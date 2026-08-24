/* tcc-stl queue.h - model Queue(T) (自包含环形队列)
 *
 * FIFO 适配器。**自包含**——规避"泛型函数内调另一泛型实例 unresolved"
 * (docs/stl.md §13-6)。分配走 self-contained slt_arena_alloc(musl malloc)。
 * 元素限 POD 值语义; 生命周期随 arena 整池回收。
 *
 * 内部环形缓冲: 逻辑下标 k → 物理 (begin + k) % cap。pop_front 移动 begin,
 * 清空时归零基准避免 begin 溢出; 扩容时重排到新块连续存储。
 *
 * 方法 `model (T)` 泛型函数、显式实例化调用 `Queue_push_back(int)(&q, x)`。
 */
#ifndef SLT_QUEUE_H
#define SLT_QUEUE_H

#include "allocator.h"

#define SLT_QIDX(s, k) (((s)->begin + (k)) % ((s)->cap ? (s)->cap : 1))

model struct Queue(T) {
    T *data;            /* 元素数组(arena); 空队列为 0 */
    int begin;          /* 逻辑 0 号元素物理下标 */
    int len;
    int cap;
    SLT_Arena *ar;
};

model (T) void Queue_init(Queue(T) *self, SLT_Arena *ar) {
    self->data = 0; self->begin = 0; self->len = 0; self->cap = 0; self->ar = ar;
}
model (T) int Queue_size(const Queue(T) *self)  { return self->len; }
model (T) int Queue_empty(const Queue(T) *self) { return self->len == 0; }

model (T) T Queue_front(const Queue(T) *self) {
    SLT_ASSERT(self->len > 0);
    return self->data[self->begin];
}
model (T) T Queue_back(const Queue(T) *self) {
    SLT_ASSERT(self->len > 0);
    return self->data[SLT_QIDX(self, self->len - 1)];
}

model (T) void Queue_push_back(Queue(T) *self, T x) {
    if (self->len >= self->cap) {         /* 自包含扩容: 重排到连续新块 */
        int nc = self->cap ? (self->cap * 2) : 4;
        T *nd = (T *)slt_arena_alloc(self->ar, (size_t)nc * sizeof(T), SLT_ALIGN);
        if (!nd) return;
        for (int i = 0; i < self->len; i++) nd[i] = self->data[SLT_QIDX(self, i)];
        self->data = nd; self->cap = nc; self->begin = 0;
    }
    self->data[SLT_QIDX(self, self->len)] = x;
    self->len++;
}

model (T) T Queue_pop_front(Queue(T) *self) {
    SLT_ASSERT(self->len > 0);
    T v = self->data[self->begin];
    self->begin++; self->len--;
    if (self->len == 0) self->begin = 0;
    else if (self->begin >= self->cap) self->begin = 0;  /* 环形回绕, begin 恒 < cap */
    return v;
}

model (T) void Queue_clear(Queue(T) *self) { self->begin = 0; self->len = 0; }

#undef SLT_QIDX

#endif /* SLT_QUEUE_H */