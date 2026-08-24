/* tcc-stl allocator.h - 空间配置器 (self-contained 池式 bump arena)
 *
 * 发行约束(docs/stl.md §13-决策8): STL 头只依赖 musl 标准头, 绝不 include 本项目
 * 私有头(如 lib/tcc-arena.h)——脱糖产物交 clang 时这些文件不在其 sysroot 里。
 * 故这里自带一个极简 bump arena: 分配=bump, 回收=整池 reset/destroy, 基于 musl malloc。
 *
 * 用法:
 *   SLT_Arena *ar = slt_arena_new(0);         // 创建(0=默认块大小)
 *   T *p = (T*)slt_arena_alloc(ar, n*sizeof(T), 16);
 *   slt_arena_reset(ar); slt_arena_destroy(ar);
 *
 * 该 arena 语义对齐原 tcc-arena(TCC 原生路径也该自洽), 便于 `-b` 下经 bcheck 登记、
 * 以及 `--emit-c` 脱糖后由 clang 直接编译。
 */
#ifndef SLT_ALLOCATOR_H
#define SLT_ALLOCATOR_H

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

/* 函数用 `static`（非 inline）: tcc 会生成符号, model 泛型函数体重放调用时可解析
 * (static inline 在 tcc 泛型重放下会 unresolved); clang/gcc 用 unused 抑制告警。 */
#if defined(__GNUC__) || defined(__clang__)
# define SLT_STATIC static __attribute__((unused))
#else
# define SLT_STATIC static
#endif

/* 内存检测开关: 默认调试期开(SLT_CHECKS)。发布版 -DSLT_CHECKS=0 或 NDEBUG 关闭 →
 * 全走 assert 裁剪, 零额外开销。断言经标准 <assert.h>: TCC 原生与 clang 脱糖产物通用。 */
#ifdef SLT_CHECKS
# include <assert.h>
# define SLT_ASSERT(c) assert(c)
# define SLT_WARN(fmt, ...) fprintf(stderr, "[slt] " fmt "\n", ##__VA_ARGS__)
#else
# define SLT_ASSERT(c) ((void)0)
# define SLT_WARN(fmt, ...) ((void)0)
#endif

#ifndef SLT_ALIGN
#define SLT_ALIGN 16u
#endif

typedef struct slt_ablock { struct slt_ablock *next; size_t size; } slt_ablock;

typedef struct SLT_Arena {
    slt_ablock *head;
    size_t off, grow, cap;
    size_t epoch;          /* SLT_CHECKS: reset/destroy 自增, 供陈旧指针检测 */
    unsigned outstanding;  /* SLT_CHECKS: 自上次 reset 以来分配未回卷的指针数 */
} SLT_Arena;

SLT_STATIC SLT_Arena *slt_arena_new(size_t grow)
{
    SLT_Arena *a = (SLT_Arena *) malloc(sizeof *a);
    if (!a) return 0;
    a->head = 0; a->off = 0;
    a->grow = grow ? grow : 8192u;
    a->cap = 0;
    a->epoch = 0;
    a->outstanding = 0;
    return a;
}

/* 分配 size 字节(对齐 align)。成功返回非 0, 失败返回 0。 */
SLT_STATIC void *slt_arena_alloc(SLT_Arena *a, size_t size, size_t align)
{
    slt_ablock *b;
    size_t aoff, room;
    if (!a) return 0;
    if (!size) size = 1;
    if (!align) align = 1;
    aoff = (a->off + (align - 1)) & ~(align - 1);
    b = a->head;                              /* 活动块 = 最近一块 */
    if (!b || aoff + size > b->size) {
        room = size + align;
        if (room < a->grow) room = a->grow;
        b = (slt_ablock *) malloc(sizeof *b + room);
        if (!b) return 0;
        if (b) { b->size = room; b->next = a->head; a->head = b; a->cap += room; aoff = 0; }
    }
    if (b) {
        a->off = aoff + size;
        a->outstanding++;
        return (void *)((char *)(b + 1) + aoff);
    }
    return 0;
}

/* 整池回卷。SLT_CHECKS: 若仍有余活指针(未释放/仍被引用)则醒目警告(逃逸提示)。 */
SLT_STATIC void slt_arena_reset(SLT_Arena *a)
{
    if (!a) return;
    if (a->outstanding)
        SLT_WARN("arena reset with %u outstanding pointers (epoch %u->%u); "
                 "those pointers now dangle", a->outstanding,
                 (unsigned) a->epoch, (unsigned)(a->epoch + 1));
    a->off = 0;
    a->epoch++;
    a->outstanding = 0;
}

SLT_STATIC void slt_arena_destroy(SLT_Arena *a)
{
    slt_ablock *b, *n;
    if (!a) return;
    if (a->outstanding)
        SLT_WARN("arena destroy with %u outstanding pointers (epoch %u); "
                 "those pointers now dangle", a->outstanding, (unsigned) a->epoch);
    for (b = a->head; b; b = n) { n = b->next; free(b); }
    free(a);
}

#endif /* SLT_ALLOCATOR_H */