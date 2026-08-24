/* tcc-stl stack.h - model Stack(T) (自包含动态数组栈)
 *
 * LIFO 适配器。**自包含**(不折叠 Vector)——规避"泛型函数内调另一泛型实例 unresolved"
 * (docs/stl.md §13-6)。分配走 self-contained slt_arena_alloc(musl malloc)。
 * 元素限 POD 值语义; 生命周期随 arena 整池回收。
 *
 * 方法 `model (T)` 泛型函数、显式实例化调用 `Stack_push(int)(&s, x)`。
 */
#ifndef SLT_STACK_H
#define SLT_STACK_H

#include "allocator.h"

model struct Stack(T) {
    T *data;            /* 元素数组(arena); 空栈为 0 */
    int len;
    int cap;
    SLT_Arena *ar;
};

model (T) void Stack_init(Stack(T) *self, SLT_Arena *ar) {
    self->data = 0; self->len = 0; self->cap = 0; self->ar = ar;
}
model (T) int Stack_size(const Stack(T) *self)  { return self->len; }
model (T) int Stack_empty(const Stack(T) *self) { return self->len == 0; }
model (T) T Stack_top(const Stack(T) *self) {
    SLT_ASSERT(self->len > 0);
    return self->data[self->len - 1];
}

model (T) void Stack_push(Stack(T) *self, T x) {
    if (self->len >= self->cap) {         /* 自包含扩容 */
        int nc = self->cap ? (self->cap * 2) : 4;
        T *nd = (T *)slt_arena_alloc(self->ar, (size_t)nc * sizeof(T), SLT_ALIGN);
        if (!nd) return;
        for (int i = 0; i < self->len; i++) nd[i] = self->data[i];
        self->data = nd; self->cap = nc;
    }
    self->data[self->len++] = x;
}

model (T) void Stack_pop(Stack(T) *self) {
    if (self->len > 0) self->len--;
}

#endif /* SLT_STACK_H */