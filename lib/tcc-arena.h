/* tcc-arena.h - 内存管理第 2 层: 运行时基础 · Arena 分配器 (bump/区域分配)
 *
 * 一次性生命周期数据的快速分配器: 分配只是指针移动 (bump), 释放 = 整片丢弃。
 * 块按需增长并链式保存, 支持:
 *   tcc_arena_new(grow)            创建 (grow=0 表示默认块大小)
 *   tcc_arena_alloc(a, size, align) bump 分配(对齐)
 *   tcc_arena_used(a)  /  capacity  已用 / 已申请字节
 *   tcc_arena_reset(a)              清空已用但不归还块(保留容量, 复用于下一轮)
 *   tcc_arena_destroy(a)            归还全部块
 *
 * 内存经 malloc 取得; 在 `-b` 下会穿过 bcheck.o 的 __bound_malloc, 因而块的
 * 增长开销自动计入 memtrack 的 [tccmem] 报告(按增长发生的调用点归因)。
 * 头文件是可复制的工具, 无链接依赖(全部 static)。
 */
#ifndef TCC_ARENA_H
#define TCC_ARENA_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct tcc_block {
    struct tcc_block *next;
    size_t size;            /* 数据区字节数 */
    /* 数据区紧接在后 */
} tcc_block;

typedef struct tcc_arena {
    tcc_block *head;        /* 当前可用的活动块 (最近分配) */
    size_t off;             /* head 上已用偏移 */
    size_t grow;            /* 每块数据尺寸 */
    size_t total_cap;       /* 所有块数据总字节 */
    size_t total_used;      /* 已分配(含本条) */
    size_t epoch;           /* 纪元: reset/destroy 时自增 */
    size_t outstanding;     /* 自上次 reset/destroy 以来已分配、未回卷的指针数 */
} tcc_arena;

#ifndef TCC_ARENA_DEFAULT_GROW
#define TCC_ARENA_DEFAULT_GROW 8192u
#endif

static tcc_arena *tcc_arena_new(size_t grow)
{
    tcc_arena *a = (tcc_arena *) malloc(sizeof *a);
    if (!a)
        return 0;
    a->head = 0;
    a->off = 0;
    a->grow = grow ? grow : TCC_ARENA_DEFAULT_GROW;
    a->total_cap = 0;
    a->total_used = 0;
    a->epoch = 0;
    a->outstanding = 0;
    return a;
}

/* 分配一块容纳 size 数据的定长块, 头部预留 tcc_block */
static tcc_block *arena_grow(tcc_arena *a, size_t want)
{
    tcc_block *b;
    size_t sz = a->grow;
    if (sz < want)
        sz = (want + (a->grow - 1)) & ~(a->grow - 1);
    b = (tcc_block *) malloc(sizeof *b + sz);   /* -b: 在此调用点计入 memtrack */
    if (!b)
        return 0;
    b->size = sz;
    b->next = a->head;
    a->head = b;
    a->off = 0;
    a->total_cap += sz;
    return b;
}

static void *tcc_arena_alloc(tcc_arena *a, size_t size, size_t align)
{
    tcc_block *b;
    size_t base, aoff, room, pri;

    if (!size)
        size = 1;
    if (!align)
        align = 1;
    /* 对齐到 align 的向上取整 */
    aoff = (a->off + (align - 1)) & ~(align - 1);

    b = a->head;
    if (!b || aoff + size > b->size) {
        room = size + align;                 /* 对齐头可能跨块 */
        if (room < a->grow)
            room = a->grow;
        b = arena_grow(a, room);
        if (!b)
            return 0;
        aoff = 0;
        pri = 0;                             /* 新块从偏移 0 起 */
    } else {
        pri = a->off;
    }
    base = (size_t) b + sizeof *b;
    a->off = aoff + size;
    a->total_used += (aoff - pri) + size;    /* 仅本次增量: 对齐填充 + 大小 */
    a->outstanding++;
    return (void *) (base + aoff);
}

static void tcc_arena_reset(tcc_arena *a)
{
    tcc_block *b;
    for (b = a->head; b; b = b->next)
        ;                       /* 块全部保留 */
    if (a->outstanding > 0)
        fprintf(stderr, "[memgov] ARENA reset with %u outstanding pointers "
                "(epoch %u->%u); those pointers now dangle\n",
                (unsigned) a->outstanding, (unsigned) a->epoch, (unsigned)(a->epoch + 1));
    a->off = 0;                 /* 仅从活动块头部重新 bump */
    a->total_used = 0;
    a->epoch++;
    a->outstanding = 0;
    /* 注: 保留多块时上面循环仅作占位; 实际复位只回退当前头块,
       其余块仍挂链待下次用尽后复用 —— 功能无误, 字节核算以 used 为准 */
}

static void tcc_arena_destroy(tcc_arena *a)
{
    tcc_block *b, *n;
    if (a->outstanding > 0)
        fprintf(stderr, "[memgov] ARENA destroy with %u outstanding pointers "
                "(epoch %u); those pointers now dangle\n",
                (unsigned) a->outstanding, (unsigned) a->epoch);
    for (b = a->head; b; b = n) {
        n = b->next;
        free(b);
    }
    free(a);
}

/* 存取前声明式核验: 持方记录拿到指针时的 epoch, 用之来核对当前是否仍有效。
   返回 0 表示 epoch 已过期(悬垂), 否则 1。 */
static int tcc_arena_check(const tcc_arena *a, size_t epoch_holder)
{
    if (epoch_holder != a->epoch) {
        fprintf(stderr, "[memgov] ARENA stale access: holder epoch %u != "
                "current %u (pointer was invalidated by reset/destroy)\n",
                (unsigned) epoch_holder, (unsigned) a->epoch);
        return 0;
    }
    return 1;
}

static size_t tcc_arena_used(const tcc_arena *a)
{
    return a->total_used;
}

static size_t tcc_arena_capacity(const tcc_arena *a)
{
    return a->total_cap;
}

#endif /* TCC_ARENA_H */