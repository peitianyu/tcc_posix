/* tcc-stl stack.h - model STL_Stack(T) (自包含动态数组栈)
 *
 * LIFO 适配器。**自包含**(不折叠 Vector)——规避"泛型函数内调另一泛型实例 unresolved"
 * (docs/stl.md §13-6)。分配走 self-contained stl_arena_alloc(musl malloc)。
 * 元素限 POD 值语义; 生命周期随 arena 整池回收。
 *
 * 方法 `model (T)` 泛型函数、显式实例化调用 `stl_stack_push(int)(&s, x)`。
 */
#ifndef STL_STACK_H
#define STL_STACK_H

#include "allocator.h"

model struct STL_Stack(T) {
    T *data;            /* 元素数组(arena); 空栈为 0 */
    int len;
    int cap;
    STL_Arena *ar;
};

model (T) void stl_stack_init(STL_Stack(T) *self, STL_Arena *ar) {
    self->data = 0; self->len = 0; self->cap = 0; self->ar = ar;
}
model (T) int stl_stack_size(const STL_Stack(T) *self)  { return self->len; }
model (T) int stl_stack_empty(const STL_Stack(T) *self) { return self->len == 0; }
model (T) T stl_stack_top(const STL_Stack(T) *self) {
    STL_ASSERT(self->len > 0);
    return self->data[self->len - 1];
}

model (T) void stl_stack_push(STL_Stack(T) *self, T x) {
    STL_ARR_GROW(self);
    if (self->len >= self->cap) return;   /* 分配失败: 保留原位 */
    self->data[self->len++] = x;
}

model (T) void stl_stack_pop(STL_Stack(T) *self) {
    if (self->len > 0) self->len--;
}

#endif /* STL_STACK_H */