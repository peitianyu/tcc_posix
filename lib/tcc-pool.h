/* tcc-pool.h - 内存管理第 3 层: 智能分配 · 对象池 (固定槽复用)
 *
 * 面向「大量同构、短生命周期、反复创建/释放」的对象 (如 AST 节点、消息、记录)。
 * 固定大小槽位按块整批取得; 释放的对象进空闲链表复用, 不归还给堆, 从而:
 *   - 分配 = 空闲链弹出 或 当前块 bump (O(1))
 *   - 释放 = 空闲链入  (O(1)), 无碎裂
 *   - 内存总量随水位单调增长后趋于稳定 (高水位即峰值), 不再反复 malloc
 *
 * API:
 *   tcc_pool_new(slot_size, per_chunk)   创建 (slot_size 自动垫到下个空指针对齐)
 *   tcc_pool_alloc(p)                    取一个槽 (未初始化)
 *   tcc_pool_free(p, ptr)                归还一个槽到空闲链
 *   tcc_pool_live(p) / tcc_pool_cap(p)   在用槽数 / 总槽容量
 *   tcc_pool_destroy(p)                  归还全部块 (堆)
 *
 * 块经 malloc 取得; `-b` 下穿过 __bound_malloc, 块的整批增长自动计入
 * memtrack (按增长调用点归因) —— 这正是「固定槽复用少分配」要揭示的成本。
 * 头文件为可复制工具, 全部 static, 无链接依赖。
 */
#ifndef TCC_POOL_H
#define TCC_POOL_H

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#ifndef TCC_POOL_DEFAULT_PER_CHUNK
#define TCC_POOL_DEFAULT_PER_CHUNK 64u
#endif

typedef struct tcc_pool_chunk {
    struct tcc_pool_chunk *next;
    size_t filled;          /* 本块已 bump 出的槽数 */
    /* 槽位数据紧接在后; 每个槽的前 sizeof(void*) 字节可作空闲链指针 */
} tcc_pool_chunk;

typedef struct tcc_pool {
    size_t slot_size;       /* 单槽字节数 (对齐后) */
    size_t per_chunk;       /* 每块槽数 */
    size_t cap;             /* 总槽容量 (各块 filled 高水位之和) */
    size_t live;            /* 当前在用槽数 */
    tcc_pool_chunk *chunks; /* 全部块的链 */
    tcc_pool_chunk *cur;    /* 正在 bump 的块 */
    void *free_list;        /* 空闲槽链表头 */
} tcc_pool;

#define POOL_ALIGN_SZ  ((size_t)sizeof(void *) > sizeof(size_t) \
                        ? sizeof(void *) : sizeof(size_t))

static tcc_pool *tcc_pool_new(size_t slot_size, size_t per_chunk)
{
    tcc_pool *p = (tcc_pool *) malloc(sizeof *p);
    if (!p)
        return 0;
    /* 槽内须能容纳一个空闲链指针 */
    slot_size = (slot_size + POOL_ALIGN_SZ - 1) & ~(POOL_ALIGN_SZ - 1);
    if (slot_size < sizeof(void *))
        slot_size = sizeof(void *);
    p->slot_size = slot_size;
    p->per_chunk = per_chunk ? per_chunk : TCC_POOL_DEFAULT_PER_CHUNK;
    p->cap = 0;
    p->live = 0;
    p->chunks = 0;
    p->cur = 0;
    p->free_list = 0;
    return p;
}

/* 新增一块: 该块只提供 bump; 既有的空闲链始终优先于开新块 */
static tcc_pool_chunk *pool_grow(tcc_pool *p)
{
    tcc_pool_chunk *c;
    c = (tcc_pool_chunk *) malloc(sizeof *c + p->slot_size * p->per_chunk);
    if (!c)
        return 0;
    c->next = p->chunks;
    c->filled = 0;
    p->chunks = c;
    p->cur = c;
    p->cap += p->per_chunk;
    return c;
}

static void *tcc_pool_alloc(tcc_pool *p)
{
    tcc_pool_chunk *c;
    void *s;
    char *base;

    if (p->free_list) {                 /* 复用已释放槽 */
        s = p->free_list;
        p->free_list = *(void **) s;
        p->live++;
        return s;
    }
    c = p->cur;
    if (!c || c->filled == p->per_chunk) {
        c = pool_grow(p);
        if (!c)
            return 0;
    }
    base = (char *) c + sizeof *c;
    s = base + c->filled * p->slot_size;
    c->filled++;
    p->live++;
    return s;
}

static void tcc_pool_free(tcc_pool *p, void *ptr)
{
    if (!p || !ptr)
        return;
    *(void **) ptr = p->free_list;      /* 槽内写入空闲链指针 */
    p->free_list = ptr;
    p->live--;
}

/* 实际向堆申请过的字节数 (块数据区) */
static size_t tcc_pool_bytes(const tcc_pool *p)
{
    return p->cap * p->slot_size;
}

static size_t tcc_pool_live(const tcc_pool *p) { return p->live; }
static size_t tcc_pool_cap(const tcc_pool *p)  { return p->cap;  }

static void tcc_pool_destroy(tcc_pool *p)
{
    tcc_pool_chunk *c, *n;
    if (!p)
        return;
    for (c = p->chunks; c; c = n) {
        n = c->next;
        free(c);
    }
    free(p);
}

#endif /* TCC_POOL_H */