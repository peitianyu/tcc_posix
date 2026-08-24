/* tcc-stl deque.h - model STL_Deque(T) (双端队列, self-contained)
 *
 * 块式双端队列: 两端 push/pop 均摊 O(1)(环形缓冲), 随机访问 O(1)
 * (逻辑下标 (begin+k)%cap)。元素限 POD 值语义; 生命周期随 arena 整池回收。
 *
 * 约束(docs/stl.md §13): 方法 `model (T)` 泛型、各方法**自包含**(扩容宏展开内联,
 * 无泛型互调); 调用对象方法糖 `d->stl_deque_push_back(int)(x)`。
 * 分配走 self-contained `stl_arena_alloc`(musl malloc; 仅 musl 标准头)。
 */
#ifndef STL_DEQUE_H
#define STL_DEQUE_H

#include "allocator.h"

#define STL_DQIDX(d, k) (((d)->begin + (k)) % ((d)->cap ? (d)->cap : 1))

model struct STL_Deque(T) {
    T *data;            /* 元素数组(arena); 空 deque 为 0 */
    int begin;          /* 逻辑 0 号元素物理下标 */
    int len;
    int cap;
    STL_Arena *ar;
};

model (T) void stl_deque_init(STL_Deque(T) *self, STL_Arena *ar) {
    self->data = 0; self->begin = 0; self->len = 0; self->cap = 0; self->ar = ar;
}
model (T) int stl_deque_size(const STL_Deque(T) *self)  { return self->len; }
model (T) int stl_deque_empty(const STL_Deque(T) *self) { return self->len == 0; }
model (T) void stl_deque_clear(STL_Deque(T) *self) { self->begin = 0; self->len = 0; }

/* 随机访问(越界由 STL_CHECKS 断言) */
model (T) T stl_deque_at(STL_Deque(T) *self, int i) {
    STL_ASSERT(self && i >= 0 && i < self->len);
    return self->data[STL_DQIDX(self, i)];
}
model (T) T stl_deque_front(const STL_Deque(T) *self) {
    STL_ASSERT(self->len > 0);
    return self->data[self->begin];
}
model (T) T stl_deque_back(const STL_Deque(T) *self) {
    STL_ASSERT(self->len > 0);
    return self->data[STL_DQIDX(self, self->len - 1)];
}

/* 需要扩容时内联: 分配 nc, 按逻辑序拷贝到连续新块, begin 归零.
   返回 1=已扩(或无需扩返回0), 0=分配失败; 供 push_* 内联. */
#define STL_DEQUE_GROW(d) do { \
    if ((d)->len >= (d)->cap) { \
        int _nc = (d)->cap ? (d)->cap * 2 : 4; \
        T *_nd = (T *)stl_arena_alloc((d)->ar, (size_t)_nc * sizeof(T), STL_ALIGN); \
        if (_nd) { \
            for (int _i = 0; _i < (d)->len; _i++) _nd[_i] = (d)->data[STL_DQIDX((d), _i)]; \
            (d)->data = _nd; (d)->cap = _nc; (d)->begin = 0; \
        } \
    } \
} while (0)

model (T) void stl_deque_push_back(STL_Deque(T) *self, T x) {
    STL_DEQUE_GROW(self);
    if (self->len >= self->cap) return;      /* 分配失败 */
    self->data[STL_DQIDX(self, self->len)] = x;
    self->len++;
}
model (T) void stl_deque_push_front(STL_Deque(T) *self, T x) {
    STL_DEQUE_GROW(self);
    if (self->len >= self->cap) return;
    self->begin = (self->begin - 1 + self->cap) % self->cap;
    self->data[self->begin] = x;
    self->len++;
}
model (T) void stl_deque_pop_back(STL_Deque(T) *self) {
    if (self->len > 0) self->len--;
    if (self->len == 0) self->begin = 0;
}
model (T) void stl_deque_pop_front(STL_Deque(T) *self) {
    if (self->len > 0) {
        self->begin = (self->begin + 1) % (self->cap ? self->cap : 1);
        self->len--;
        if (self->len == 0) self->begin = 0;
    }
}

#undef STL_DQIDX
#undef STL_DEQUE_GROW

#endif /* STL_DEQUE_H */