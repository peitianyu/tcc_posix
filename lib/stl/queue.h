/* tcc-stl queue.h - model STL_Queue(T) (自包含环形队列)
 *
 * FIFO 适配器。**自包含**——规避"泛型函数内调另一泛型实例 unresolved"
 * (docs/stl.md §13-6)。分配走 self-contained stl_arena_alloc(musl malloc)。
 * 元素限 POD 值语义; 生命周期随 arena 整池回收。
 *
 * 内部环形缓冲: 逻辑下标 k → 物理 (begin + k) % cap。pop_front 移动 begin,
 * 清空时归零基准避免 begin 溢出; 扩容时重排到新块连续存储。
 *
 * 方法 `model (T)` 泛型函数、显式实例化调用 `stl_queue_push_back(int)(&q, x)`。
 */
#ifndef STL_QUEUE_H
#define STL_QUEUE_H

#include "allocator.h"

model struct STL_Queue(T) {
    T *data;            /* 元素数组(arena); 空队列为 0 */
    int begin;          /* 逻辑 0 号元素物理下标 */
    int len;
    int cap;
    STL_Arena *ar;
};

model (T) void stl_queue_init(STL_Queue(T) *self, STL_Arena *ar) {
    self->data = 0; self->begin = 0; self->len = 0; self->cap = 0; self->ar = ar;
}
model (T) int stl_queue_size(const STL_Queue(T) *self)  { return self->len; }
model (T) int stl_queue_empty(const STL_Queue(T) *self) { return self->len == 0; }

model (T) T stl_queue_front(const STL_Queue(T) *self) {
    STL_ASSERT(self->len > 0);
    return self->data[self->begin];
}
model (T) T stl_queue_back(const STL_Queue(T) *self) {
    STL_ASSERT(self->len > 0);
    return self->data[STL_RING_IDX(self, self->len - 1)];
}

model (T) void stl_queue_push_back(STL_Queue(T) *self, T x) {
    STL_RING_GROW(self);
    if (self->len >= self->cap) return;      /* 分配失败 */
    self->data[STL_RING_IDX(self, self->len)] = x;
    self->len++;
}

model (T) T stl_queue_pop_front(STL_Queue(T) *self) {
    STL_ASSERT(self->len > 0);
    T v = self->data[self->begin];
    self->begin++; self->len--;
    if (self->len == 0) self->begin = 0;
    else if (self->begin >= self->cap) self->begin = 0;  /* 环形回绕, begin 恒 < cap */
    return v;
}

model (T) void stl_queue_clear(STL_Queue(T) *self) { self->begin = 0; self->len = 0; }

#endif /* STL_QUEUE_H */