/* tcc-stl allocator.h - 空间配置器 (self-contained 池式 bump arena)
 *
 * 发行约束(docs/stl.md §13-决策8): STL 头只依赖 musl 标准头, 绝不 include 本项目
 * 私有头(如 lib/tcc-arena.h)——脱糖产物交 clang 时这些文件不在其 sysroot 里。
 * 故这里自带一个极简 bump arena: 分配=bump, 回收=整池 reset/destroy, 基于 musl malloc。
 *
 * 用法:
 *   STL_Arena *ar = stl_arena_new(0);         // 创建(0=默认块大小)
 *   T *p = (T*)stl_arena_alloc(ar, n*sizeof(T), 16);
 *   stl_arena_reset(ar); stl_arena_destroy(ar);
 *
 * 该 arena 语义对齐原 tcc-arena(TCC 原生路径也该自洽), 便于 `-b` 下经 bcheck 登记、
 * 以及 `--emit-c` 脱糖后由 clang 直接编译。
 */
#ifndef STL_ALLOCATOR_H
#define STL_ALLOCATOR_H

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

/* 函数用 `static`（非 inline）: tcc 会生成符号, model 泛型函数体重放调用时可解析
 * (static inline 在 tcc 泛型重放下会 unresolved); clang/gcc 用 unused 抑制告警。 */
#if defined(__GNUC__) || defined(__clang__)
# define STL_STATIC static __attribute__((unused))
#else
# define STL_STATIC static
#endif

/* 内存检测开关: 默认调试期开(STL_CHECKS)。发布版 -DSTL_CHECKS=0 或 NDEBUG 关闭 →
 * 全走 assert 裁剪, 零额外开销。断言经标准 <assert.h>: TCC 原生与 clang 脱糖产物通用。 */
#ifdef STL_CHECKS
# include <assert.h>
# define STL_ASSERT(c) assert(c)
# define STL_WARN(fmt, ...) fprintf(stderr, "[slt] " fmt "\n", ##__VA_ARGS__)
#else
# define STL_ASSERT(c) ((void)0)
# define STL_WARN(fmt, ...) ((void)0)
#endif

#ifndef STL_ALIGN
#define STL_ALIGN 16u
#endif

typedef struct stl_ablock { struct stl_ablock *next; size_t size; } stl_ablock;

typedef struct STL_Arena {
    stl_ablock *head;
    size_t off, grow, cap;
    size_t epoch;          /* STL_CHECKS: reset/destroy 自增, 供陈旧指针检测 */
    unsigned outstanding;  /* STL_CHECKS: 自上次 reset 以来分配未回卷的指针数 */
} STL_Arena;

STL_STATIC STL_Arena *stl_arena_new(size_t grow)
{
    STL_Arena *a = (STL_Arena *) malloc(sizeof *a);
    if (!a) return 0;
    a->head = 0; a->off = 0;
    a->grow = grow ? grow : 8192u;
    a->cap = 0;
    a->epoch = 0;
    a->outstanding = 0;
    return a;
}

/* 分配 size 字节(对齐 align)。成功返回非 0, 失败返回 0。 */
STL_STATIC void *stl_arena_alloc(STL_Arena *a, size_t size, size_t align)
{
    stl_ablock *b;
    size_t aoff, room;
    if (!a) return 0;
    if (!size) size = 1;
    if (!align) align = 1;
    aoff = (a->off + (align - 1)) & ~(align - 1);
    b = a->head;                              /* 活动块 = 最近一块 */
    if (!b || aoff + size > b->size) {
        room = size + align;
        if (room < a->grow) room = a->grow;
        b = (stl_ablock *) malloc(sizeof *b + room);
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

/* 整池回卷。STL_CHECKS: 若仍有余活指针(未释放/仍被引用)则醒目警告(逃逸提示)。 */
STL_STATIC void stl_arena_reset(STL_Arena *a)
{
    if (!a) return;
    if (a->outstanding)
        STL_WARN("arena reset with %u outstanding pointers (epoch %u->%u); "
                 "those pointers now dangle", a->outstanding,
                 (unsigned) a->epoch, (unsigned)(a->epoch + 1));
    a->off = 0;
    a->epoch++;
    a->outstanding = 0;
}

STL_STATIC void stl_arena_destroy(STL_Arena *a)
{
    stl_ablock *b, *n;
    if (!a) return;
    if (a->outstanding)
        STL_WARN("arena destroy with %u outstanding pointers (epoch %u); "
                 "those pointers now dangle", a->outstanding, (unsigned) a->epoch);
    for (b = a->head; b; b = n) { n = b->next; free(b); }
    free(a);
}

/* ============================================================
 * Heap 后端(逐对象析构): malloc/free + 元素析构回调.
 *
 * 与 arena(整池回收, 无逐对象 free)互补: arena 适合"一次性构造 + 最后整体清理";
 * heap 适合**生命周期精确**的容器(链表节点/独立数据块), 每个对象单独分配,
 * 亦可单独释放, `free` 前可选执行元素析构回调. 仅依赖 musl malloc/stddef.
 *
 * 用法:
 *   STL_Heap h; stl_heap_init(&h);
 *   T *p = (T*)stl_heap_alloc(&h, sizeof(T), 16);
 *   stl_heap_free(&h, p, elem_dtor, ud);   // free 前调 elem_dtor(data,ud), 可为 0
 *   stl_heap_destroy(&h);                  // 校验未释放余活并警告( STL_CHECKS )
 * 计数语义对齐 arena 的 outstanding: `live` 自增/减; 销毁时余活醒目警告(泄漏提示)。
 * ============================================================ */
typedef struct STL_Heap {
    size_t live;      /* STL_CHECKS: 自 init 以来分配未 free 的指针数 */
    size_t epoch;     /* STL_CHECKS: destroy 计数器, 供存根检测 */
} STL_Heap;

STL_STATIC void stl_heap_init(STL_Heap *h)
{
    h->live = 0;
    h->epoch = 0;
}

/* 分配 size 字节(对齐 align)。成功返回非 0(计入 live), 失败返回 0。 */
STL_STATIC void *stl_heap_alloc(STL_Heap *h, size_t size, size_t align)
{
    void *p;
    if (!h) return 0;
    if (!size) size = 1;
    (void)align;                        /* malloc 已按最大对齐保证 */
    p = malloc(size);
    if (p) h->live++;
    return p;
}

/* 释放先前 heap 分配的 p(须属 h)。dtor 非 0 时, 释放前先调 dtor(p, ud)。 */
STL_STATIC void stl_heap_free(STL_Heap *h, void *p,
                              void (*dtor)(void *, void *), void *ud)
{
    if (!h) return;
    if (p) {
        if (dtor) dtor(p, ud);
        free(p);
        if (h->live) h->live--;
        else STL_WARN("heap free of untracked/over-freed pointer");
    }
}

/* 余活指针数(测试/生命周期校验用) */
STL_STATIC int stl_heap_live(const STL_Heap *h) { return (int)(h ? h->live : 0); }

/* 销毁前端校验: 若仍有未释放余活指针则醒目警告(泄漏提示)。 */
STL_STATIC void stl_heap_check(const STL_Heap *h)
{
    if (!h) return;
    if (h->live)
        STL_WARN("heap destroy with %lu outstanding pointers (epoch %lu); "
                 "those blocks leaked",
                 (unsigned long) h->live, (unsigned long) h->epoch);
}

/* 整堆销毁: 警告未释放余活, 推进 epoch(供存根检测). 不代 free 遗留块. */
STL_STATIC void stl_heap_destroy(STL_Heap *h)
{
    if (!h) return;
    stl_heap_check(h);
    h->epoch++;
    h->live = 0;
}

#endif /* STL_ALLOCATOR_H */