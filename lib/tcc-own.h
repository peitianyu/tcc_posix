/* tcc-own.h - 内存治理：统一销毁入口 + 指针归属表
 *
 * 解决「void* 通用化 vs 分配器错配」冲突：保留 void* 灵活传参，同时在分配
 * 时登记「指针 → 分配器」归属，销毁统一走 tcc_release()，运行时便能查出
 * 「把受管指针当裸堆释放、双重释放、释放非本系统指针」等错配并醒目提示。
 *
 *   void *p = tcc_arena_alloc(a, 64, 8);
 *   tcc_own_register(p, OWNER_ARENA);       登记归属
 *   tcc_release(p);                          查归属, 给出释放指引
 *
 * 语义（对受管分配器, 单点 tcc_release 大多应为"拒绝 + 指引"，因为 arena/
 * pool/mmap/ctmem 都是整块管理，逐项释放本身就是错配；真正合法单点释放
 * 只有裸堆 OWNER_HEAP → free）。
 *
 * 报错走 stderr, 前缀 [memgov]; 在 -bt 构建下可通过弱符号 __bt_resolve_addr
 * 把调用返回地址归因到 func@file:line（缺省只打印地址）。
 *
 * 全部 static, 无链接依赖, 与 bcheck/memtrack 正交。
 */
#ifndef TCC_OWN_H
#define TCC_OWN_H

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

#ifndef TCC_OWN_MAX
#define TCC_OWN_MAX 4096u
#endif

enum tcc_owner {
    OWNER_NONE   = 0,
    OWNER_HEAP,     /* 裸 malloc */
    OWNER_ARENA,
    OWNER_POOL,
    OWNER_CTMEM,
    OWNER_MMAP
};

/* 弱引用: bt-exe.o 提供, 把返回地址解析为 "func@file:line"; 缺省返回 -1 */
#if defined(__has_attribute)
#  if __has_attribute(weak)
#    define TCC_OWN_WEAK_DECL __attribute__((weak))
#  endif
#endif
#ifndef TCC_OWN_WEAK_DECL
#define TCC_OWN_WEAK_DECL
#endif
TCC_OWN_WEAK_DECL int __bt_resolve_addr(unsigned long long pc, char *buf, unsigned long len);

/* 可插拔报告 sink: bcheck.o 的 __mem_report/__mem_snapshot 逐行回调这个
 * 全局函数指针. 用户赋值(如写文件/日志/单测捕获)后报告即改道; 默认 NULL
 * 回落 stderr, 与既有行为一致.
 *
 * 注意: 不用弱函数而用可赋值指针 —— 弱引用只能回答"链接期是否提供",
 * 无法在运行期改道; 可赋值指针让用户可在运行时切换/恢复 sink. (早期注释称
 * "PE/COFF 后端不支持弱符号绑定, 弱引用恒为 NULL" 已过期: 2026-08-25 探针
 * 实测 tcc 的 .o 为 ELF 中间对象, PE/ELF 链接共用 tccelf.c 解析, 弱引用可被
 * bt-exe.o 强定义覆盖; bcheck.c/cpu-prof.c 的 __bt_resolve_addr 即走弱函数.)
 *
 *   __tccmem_writer = my_writer;      // 接管
 *   __tccmem_writer = 0;              // 恢复 stderr
 */
extern void (*__tccmem_writer)(const char *line);
#define tccmem_writer_set(fn)  (__tccmem_writer = (fn))

typedef struct tcc_own_entry {
    const void *ptr;
    int  owner;
} tcc_own_entry;

/* 开放式简单数组表, 静态分配, 不触发递归 malloc */
typedef struct tcc_own_tab {
    tcc_own_entry e[TCC_OWN_MAX];
    size_t n;
} tcc_own_tab;

static tcc_own_tab tcc_own_g;

static int tcc_own_lookup(const void *p)
{
    size_t i;
    for (i = 0; i < tcc_own_g.n; i++)
        if (tcc_own_g.e[i].ptr == p)
            return tcc_own_g.e[i].owner;
    return OWNER_NONE;
}

static int tcc_own_register(const void *p, int owner)
{
    size_t i;
    if (!p || owner <= OWNER_NONE)
        return 0;
    if (tcc_own_lookup(p) != OWNER_NONE)     /* 已登记, 覆盖即可 */
        return 1;
    if (tcc_own_g.n >= TCC_OWN_MAX)          /* 饱和: 静默退回不追踪 */
        return 1;
    tcc_own_g.e[tcc_own_g.n].ptr = p;
    tcc_own_g.e[tcc_own_g.n].owner = owner;
    tcc_own_g.n++;
    return 1;
}

static int tcc_own_unregister(const void *p)
{
    size_t i;
    for (i = 0; i < tcc_own_g.n; i++)
        if (tcc_own_g.e[i].ptr == p) {
            tcc_own_g.e[i] = tcc_own_g.e[--tcc_own_g.n]; /* 末项回填 */
            return 1;
        }
    return 0;
}

static const char *tcc_owner_name(int owner)
{
    switch (owner) {
        case OWNER_HEAP:  return "heap";
        case OWNER_ARENA: return "arena";
        case OWNER_POOL:  return "pool";
        case OWNER_CTMEM: return "ctmem";
        case OWNER_MMAP:  return "mmap";
        default:          return "unknown";
    }
}

/*
 * 统一销毁/归还入口。
 *   OWNER_HEAP     -> 撤销登记并 free
 *   OWNER_ARENA    -> 拒绝单点释放, 指引走 arena_destroy/reset (整块管理)
 *   OWNER_MMAP     -> 拒绝, 指引走 tcc_mmap_close
 *   OWNER_CTMEM    -> 拒绝 (编译期缓冲, 无运行时 free)
 *   OWNER_POOL     -> 拒绝通过共享 free 释放 (需池句柄, 应走 tcc_pool_free)
 *   OWNER_NONE     -> 违规: 释放非受管/未登记指针
 */
static void tcc_release(void *p)
{
    int owner = tcc_own_lookup(p);
    if (owner == OWNER_NONE) {
        fprintf(stderr, "[memgov] MISUSE release-of-unmanaged ptr=%p "
                "(not a tracked allocation, double-free or foreign pointer)\n", p);
        return;
    }
    if (owner == OWNER_HEAP) {
        tcc_own_unregister(p);
        free(p);
        return;
    }
    switch (owner) {
        case OWNER_ARENA:
            fprintf(stderr, "[memgov] REFUSE free-of-arena ptr=%p ; arena is "
                    "whole-block managed, use tcc_arena_reset/destroy, not per-slot free\n", p);
            break;
        case OWNER_MMAP:
            fprintf(stderr, "[memgov] REFUSE free-of-mmap ptr=%p ; "
                    "use tcc_mmap_close, mapping is one region\n", p);
            break;
        case OWNER_CTMEM:
            fprintf(stderr, "[memgov] REFUSE free-of-ctmem ptr=%p ; compile-time "
                    "buffer has no runtime free\n", p);
            break;
        case OWNER_POOL:
            fprintf(stderr, "[memgov] REFUSE shared-free-of-pool ptr=%p ; pool "
                    "slot needs its pool handle, call tcc_pool_free(pool, ptr)\n", p);
            break;
        default:
            break;
    }
}

#endif /* TCC_OWN_H */