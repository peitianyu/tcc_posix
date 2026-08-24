/* tcc-stl vector.h - model STL_Vector(T)
 *
 * 连续动态数组(POD 值语义)。数据从 allocator(默认 arena) 取, 生命周期随 arena 整池回收。
 *
 * 限制(基于实验):
 *   - 方法必须**自包含**: `model (T)` 泛型函数内部不再调用另一同名泛型函数的实例化
 *     (tcc 只实例化外层, 内层符号 unresolved)。扩容逻辑在 reserve/push_back 内各自内联。
 *   - 迭代器 = 裸指针: `stl_vector_data(int)(&v)` = begin, `stl_vector_end(int)(&v)` = 后界;
 *     空容器 data/end 均为 0(不产生 null 指针算术)。
 *   - at/[] 越界在 `-b` 下报错, 无 -b 为 UB(与 C 数组一致)。
 *
 * 命名: 方法名 `stl_vector_*`, 显式实例化调用 `stl_vector_push_back(int)(&v, x)`。
 * (对象方法糖 v.push() 对泛型实例暂不可用 — 见 docs/stl.md §13-决策6)
 */
#ifndef STL_VECTOR_H
#define STL_VECTOR_H

#include "allocator.h"

model struct STL_Vector(T) {
    T *data;            /* 元素数组(arena); 空容器为 0 */
    int len;
    int cap;
    STL_Arena *ar;      /* 从哪个 arena 分配(self-contained, 仅 musl malloc) */
};

/* --- 构造 / 查询(无分配, 可安全互调残留? 否 —— 全是独立函数, 不复用) --- */

model (T) void stl_vector_init(STL_Vector(T) *self, STL_Arena *ar) {
    self->data = 0; self->len = 0; self->cap = 0; self->ar = ar;
}
model (T) int stl_vector_size(const STL_Vector(T) *self) { return self->len; }
model (T) int stl_vector_empty(const STL_Vector(T) *self) { return self->len == 0; }
model (T) int stl_vector_capacity(const STL_Vector(T) *self) { return self->cap; }
model (T) T *stl_vector_data(STL_Vector(T) *self) { return self->data; }
model (T) T *stl_vector_end(STL_Vector(T) *self) {
    return self->data ? self->data + self->len : (T *)0;
}
/* at/[i]/front/back 在 STL_CHECKS 下做边界/非空断言(文件:行); 关闭则近 C 语义(UB)。
 * data[0] 在空容器时为 0 且不参与算术(bcheck/musl 均安全), 断言在断言前先判空自卫。 */
model (T) T stl_vector_at(STL_Vector(T) *self, int i) {
    STL_ASSERT(self && i >= 0 && i < self->len);
    return self->data[i];
}
model (T) T stl_vector_front(STL_Vector(T) *self) {
    STL_ASSERT(self && self->len > 0);
    return self->data[0];
}
model (T) T stl_vector_back(STL_Vector(T) *self) {
    STL_ASSERT(self && self->len > 0);
    return self->data[self->len - 1];
}

/* --- 变更(自包含分配) --- */

model (T) void stl_vector_clear(STL_Vector(T) *self) { self->len = 0; }

/* 预留容量(自包含扩容; 调用方显式触发扩容, push_back 内联同逻辑但不调本函数) */
model (T) void stl_vector_reserve(STL_Vector(T) *self, int n) {
    if (n <= self->cap) return;
    T *nd = (T *)stl_arena_alloc(self->ar, (size_t)n * sizeof(T), STL_ALIGN);
    if (!nd) return;                   /* 分配失败: 保持原位(判空警告级) */
    if (self->len > 0 && self->data)
        for (int i = 0; i < self->len; i++) nd[i] = self->data[i];
    self->data = nd;
    self->cap = n;
}

model (T) void stl_vector_push_back(STL_Vector(T) *self, T x) {
    if (self->len >= self->cap) {
        int nc = self->cap ? (self->cap * 2) : 4;
        T *nd = (T *)stl_arena_alloc(self->ar, (size_t)nc * sizeof(T), STL_ALIGN);
        if (!nd) return;
        if (self->len > 0 && self->data)
            for (int i = 0; i < self->len; i++) nd[i] = self->data[i];
        self->data = nd;
        self->cap = nc;
    }
    self->data[self->len++] = x;
}

model (T) void stl_vector_pop_back(STL_Vector(T) *self) {
    if (self->len > 0) self->len--;
}

/* 深拷贝(元素逐项拷贝到独立 arena 数据区). 按值返回新容器, 不改 self.
 * M0 元素为 POD 值语义 → 逐元素位拷贝即"深"拷贝; 新 data 从 self->ar 分配. */
model (T) STL_Vector(T) stl_vector_copy(const STL_Vector(T) *self) {
    STL_Vector(T) r;
    r.data = 0; r.len = 0; r.cap = 0; r.ar = self->ar;
    if (self->len > 0 && self->data) {
        T *nd = (T *)stl_arena_alloc(self->ar, (size_t)self->len * sizeof(T), STL_ALIGN);
        if (nd) {
            for (int i = 0; i < self->len; i++) nd[i] = self->data[i];
            r.data = nd; r.len = self->len; r.cap = self->len;
        }
    }
    return r;
}

#endif /* STL_VECTOR_H */