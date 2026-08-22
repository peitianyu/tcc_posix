/* tcc-mmap.h - 内存管理第 4 层: 持久化 · mmap 文件映射 bump 区域
 *
 * 提供一块映射到真实文件的数据区, bump 分配; 数据经 msync 落盘, 下次
 * 以同一文件重新打开时, 前一轮写入的字节与已用游标都原样恢复 —— 这是
 * 「进程间/跨运行持久化」的核心: 不必序列化, 内存即文件。
 *
 * 两种打开方式:
 *   tcc_mmap_open_file(path, cap)   打开/创建文件并映射 (MAP_SHARED, 可持久化)
 *   tcc_mmap_open_anon(cap)         匿名映射 (MAP_ANONYMOUS, 仅进程内大区)
 *
 * 数据布局: 文件头部一个 tcc_mmap_hdr (记录已用偏移 off), 之后是 bump 区。
 * API:
 *   tcc_mmap_alloc(m, size, align)  在持久区 bump; 超限返回 0
 *   tcc_mmap_sync(m)                把整个映射 msync 刷盘 (持久化点)
 *   tcc_mmap_reset(m)               回卷 off (不清盘)
 *   tcc_mmap_close(m)               sync + munmap + close + 释放元数据
 *
 * 该层直接向 OS 申请页, 不经过 malloc, 因此也不出现在 memtrack 报告里
 * (它的成本是文件映射页, 由 OS 记账)。全部 static, 无链接依赖。
 */
#ifndef TCC_MMAP_H
#define TCC_MMAP_H

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

/* bcheck 区域登记 (弱引用): 运行时 -b 下把 mmap 映射登记进边界区域表,
   越界写即可被 bcheck 拦截; 缺省(无 -b)为空实现。 */
#if defined(__has_attribute)
#  if __has_attribute(weak)
#    define TCC_MMAP_WEAK __attribute__((weak))
#  endif
#endif
#ifndef TCC_MMAP_WEAK
#define TCC_MMAP_WEAK
#endif
TCC_MMAP_WEAK void __bound_new_region(void *p, size_t size);

typedef struct tcc_mmap_hdr {
    size_t off;         /* 持久化的已用偏移 */
} tcc_mmap_hdr;

typedef struct tcc_mmap {
    void *base;         /* 映射基地址 (含 hdr) */
    size_t cap;         /* 数据区字节数 (不含 hdr) */
    int fd;
    char *path;         /* 文件路径, 匿名时为 0 */
} tcc_mmap;

/* musl-nt64 无 ftruncate: 用 lseek 到末尾再写 1 字节把文件扩到 total
   (中间零填充); 已够大则保持不动。 */
static int tcc_mfile_extend(int fd, size_t total)
{
    off_t cur, end;
    char zero = 0;
    cur = lseek(fd, 0, SEEK_END);
    if (cur < 0)
        return -1;
    end = (off_t) total - 1;
    if (cur > end)
        return 0;                       /* 已有内容更满, 保留 */
    if (lseek(fd, end, SEEK_SET) < 0)
        return -1;
    return write(fd, &zero, 1) == 1 ? 0 : -1;
}

/* 把整个映射 (含 hdr) 登记进 bcheck 区域表, 使越界可被 -b 拦截。
   仅当 -b 时 __bound_new_region 链接到 bcheck.o 的真实符号, 否则为 0。 */
static void tcc_mmap_register_bound(tcc_mmap *m)
{
    if (__bound_new_region)
        __bound_new_region(m->base, sizeof(tcc_mmap_hdr) + m->cap);
}

static tcc_mmap *tcc_mmap_open_file(const char *path, size_t cap)
{
    tcc_mmap *m;
    size_t total;
    void *base;
    int fd;

    m = (tcc_mmap *) malloc(sizeof *m);
    if (!m)
        return 0;
    fd = open(path, O_CREAT | O_RDWR, 0644);
    if (fd < 0) {
        free(m);
        return 0;
    }
    total = sizeof(tcc_mmap_hdr) + cap;
    if (tcc_mfile_extend(fd, total) != 0) {
        close(fd);
        free(m);
        return 0;
    }
    base = mmap(0, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) {
        close(fd);
        free(m);
        return 0;
    }
    m->base = base;
    m->cap = cap;
    m->fd = fd;
    m->path = (char *) path;    /* 外界保证字符串存活或静态 */
    tcc_mmap_register_bound(m);
    return m;
}

static tcc_mmap *tcc_mmap_open_anon(size_t cap)
{
    tcc_mmap *m;
    size_t total;
    void *base;

    m = (tcc_mmap *) malloc(sizeof *m);
    if (!m)
        return 0;
    total = sizeof(tcc_mmap_hdr) + cap;
    base = mmap(0, total, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) {
        free(m);
        return 0;
    }
    m->base = base;
    m->cap = cap;
    m->fd = -1;
    m->path = 0;
    tcc_mmap_register_bound(m);
    return m;
}

static void *tcc_mmap_data(const tcc_mmap *m)
{
    return (char *) m->base + sizeof(tcc_mmap_hdr);
}

static void *tcc_mmap_alloc(tcc_mmap *m, size_t size, size_t align)
{
    tcc_mmap_hdr *h = (tcc_mmap_hdr *) m->base;
    size_t aoff;
    if (!size)
        size = 1;
    if (!align)
        align = 1;
    aoff = (h->off + (align - 1)) & ~(align - 1);
    if (aoff + size > m->cap)
        return 0;
    h->off = aoff + size;       /* 持久化槽: 一旦分配就占位 */
    return (char *) tcc_mmap_data(m) + aoff;
}

static int tcc_mmap_sync(tcc_mmap *m)
{
    return msync(m->base, sizeof(tcc_mmap_hdr) + m->cap, MS_SYNC);
}

static void tcc_mmap_reset(tcc_mmap *m)
{
    ((tcc_mmap_hdr *) m->base)->off = 0;
}

static size_t tcc_mmap_used(const tcc_mmap *m)
{
    return ((tcc_mmap_hdr *) m->base)->off;
}

static void tcc_mmap_close(tcc_mmap *m)
{
    if (!m)
        return;
    tcc_mmap_sync(m);
    munmap(m->base, sizeof(tcc_mmap_hdr) + m->cap);
    if (m->fd >= 0)
        close(m->fd);
    free(m);
}

#endif /* TCC_MMAP_H */