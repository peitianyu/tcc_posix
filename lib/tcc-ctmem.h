/* tcc-ctmem.h - 内存管理第 1 层: 编译时内存 · 零堆静态 bump 分配
 *
 * 容量在编译期已知, 存储来自用户提供(或内嵌)的静态/栈缓冲, 完全不经 malloc,
 * 因此没有运行时堆分配、没有释放、没有 memtrack 开销 —— 属于确定性的极简层。
 * 适合: 单次解析、固定上限的临时缓冲、嵌入式式微 arena。
 *
 * 两种用法:
 *   tcc_ctmem_area:            包装一个外部静态缓冲区
 *       static char buf[4096];
 *       tcc_ctmem c = tcc_ctmem_begin(buf, sizeof buf);
 *       void *p = tcc_ctmem_alloc(&c, n, 8);    超限返回 0
 *       tcc_ctmem_reset(&c);                    回卷, 不触碰堆
 *
 *   tcc_ctmem_static:          内嵌一个编译期上限的静态池
 *       常量 TCC_CTMEM_STATIC_CAP 可在包含本头文件前覆写
 *       tcc_ctmem c = tcc_ctmem_static();       不会失败
 *
 * 因不占用堆, 该层天然与 memtrack 解耦: `-b` 报告里看不到它, 属于预期
 * (它的"成本"在编译期就已确定并内联)。全部 static, 无链接依赖。
 */
#ifndef TCC_CTMEM_H
#define TCC_CTMEM_H

#include <stddef.h>
#include <string.h>

#ifndef TCC_CTMEM_STATIC_CAP
#define TCC_CTMEM_STATIC_CAP 2048u
#endif

typedef struct tcc_ctmem {
    char *base;
    size_t cap;
    size_t off;
} tcc_ctmem;

static tcc_ctmem tcc_ctmem_begin(void *buf, size_t cap)
{
    tcc_ctmem c;
    c.base = (char *) buf;
    c.cap = cap;
    c.off = 0;
    return c;
}

/* 内嵌静态池 (翻译单元级), 规避堆 */
static tcc_ctmem tcc_ctmem_static(void)
{
    static char buf[TCC_CTMEM_STATIC_CAP];
    return tcc_ctmem_begin(buf, sizeof buf);
}

/* bump, 对齐; 空间不足或 cap 不满足对齐位数时返回 0 */
static void *tcc_ctmem_alloc(tcc_ctmem *c, size_t size, size_t align)
{
    size_t aoff;
    if (!size)
        size = 1;
    if (!align)
        align = 1;
    aoff = (c->off + (align - 1)) & ~(align - 1);
    if (aoff + size > c->cap)
        return 0;
    c->off = aoff + size;
    return c->base + aoff;
}

static void tcc_ctmem_reset(tcc_ctmem *c) { c->off = 0; }
static size_t tcc_ctmem_used(const tcc_ctmem *c) { return c->off; }
static size_t tcc_ctmem_cap(const tcc_ctmem *c)  { return c->cap; }

#endif /* TCC_CTMEM_H */