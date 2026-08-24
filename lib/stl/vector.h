/* tcc-stl vector.h - model Vector(T)
 *
 * 连续动态数组(POD 值语义)。数据从 allocator(默认 arena) 取, 生命周期随 arena 整池回收。
 *
 * 限制(基于实验):
 *   - 方法必须**自包含**: `model (T)` 泛型函数内部不再调用另一同名泛型函数的实例化
 *     (tcc 只实例化外层, 内层符号 unresolved)。扩容逻辑在 reserve/push_back 内各自内联。
 *   - 迭代器 = 裸指针: `Vector_data(int)(&v)` = begin, `Vector_end(int)(&v)` = 后界;
 *     空容器 data/end 均为 0(不产生 null 指针算术)。
 *   - at/[] 越界在 `-b` 下报错, 无 -b 为 UB(与 C 数组一致)。
 *
 * 命名: 方法名 `Vector_*`, 显式实例化调用 `Vector_push_back(int)(&v, x)`。
 * (对象方法糖 v.push() 对泛型实例暂不可用 — 见 docs/stl.md §13-决策6)
 */
#ifndef SLT_VECTOR_H
#define SLT_VECTOR_H

#include "allocator.h"

model struct Vector(T) {
    T *data;            /* 元素数组(arena); 空容器为 0 */
    int len;
    int cap;
    SLT_Arena *ar;      /* 从哪个 arena 分配(self-contained, 仅 musl malloc) */
};

/* --- 构造 / 查询(无分配, 可安全互调残留? 否 —— 全是独立函数, 不复用) --- */

model (T) void Vector_init(Vector(T) *self, SLT_Arena *ar) {
    self->data = 0; self->len = 0; self->cap = 0; self->ar = ar;
}
model (T) int Vector_size(const Vector(T) *self) { return self->len; }
model (T) int Vector_empty(const Vector(T) *self) { return self->len == 0; }
model (T) int Vector_capacity(const Vector(T) *self) { return self->cap; }
model (T) T *Vector_data(Vector(T) *self) { return self->data; }
model (T) T *Vector_end(Vector(T) *self) {
    return self->data ? self->data + self->len : (T *)0;
}
/* at/[i]/front/back 在 SLT_CHECKS 下做边界/非空断言(文件:行); 关闭则近 C 语义(UB)。
 * data[0] 在空容器时为 0 且不参与算术(bcheck/musl 均安全), 断言在断言前先判空自卫。 */
model (T) T Vector_at(Vector(T) *self, int i) {
    SLT_ASSERT(self && i >= 0 && i < self->len);
    return self->data[i];
}
model (T) T Vector_front(Vector(T) *self) {
    SLT_ASSERT(self && self->len > 0);
    return self->data[0];
}
model (T) T Vector_back(Vector(T) *self) {
    SLT_ASSERT(self && self->len > 0);
    return self->data[self->len - 1];
}

/* --- 变更(自包含分配) --- */

model (T) void Vector_clear(Vector(T) *self) { self->len = 0; }

/* 预留容量(自包含扩容; 调用方显式触发扩容, push_back 内联同逻辑但不调本函数) */
model (T) void Vector_reserve(Vector(T) *self, int n) {
    if (n <= self->cap) return;
    T *nd = (T *)slt_arena_alloc(self->ar, (size_t)n * sizeof(T), SLT_ALIGN);
    if (!nd) return;                   /* 分配失败: 保持原位(判空警告级) */
    if (self->len > 0 && self->data)
        for (int i = 0; i < self->len; i++) nd[i] = self->data[i];
    self->data = nd;
    self->cap = n;
}

model (T) void Vector_push_back(Vector(T) *self, T x) {
    if (self->len >= self->cap) {
        int nc = self->cap ? (self->cap * 2) : 4;
        T *nd = (T *)slt_arena_alloc(self->ar, (size_t)nc * sizeof(T), SLT_ALIGN);
        if (!nd) return;
        if (self->len > 0 && self->data)
            for (int i = 0; i < self->len; i++) nd[i] = self->data[i];
        self->data = nd;
        self->cap = nc;
    }
    self->data[self->len++] = x;
}

model (T) void Vector_pop_back(Vector(T) *self) {
    if (self->len > 0) self->len--;
}

#endif /* SLT_VECTOR_H */