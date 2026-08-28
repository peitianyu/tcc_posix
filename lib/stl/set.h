/* tcc-stl set.h - model STL_Set(K) (有序唯一集合, self-contained)
 *
 * std::set 语义: 键**有序**且**唯一**(按元素 `operator<` 严格弱序), 二分查找
 * (O(log n)), 迭代按键序。键只需提供 `operator<`(与 stl_sort/map "可排序"契约一致):
 * 等价由 `!(a<b) && !(b<a)` 推导。元素限 POD 值语义; 生命周期随 arena 整池回收。
 *
 * 约束(docs/stl.md §13): 方法 `model (K)` 泛型、显式实例化调用
 * `s->stl_set_insert(int)(x)` 或对象方法糖; 各方法自包含(二分内联, 无泛型互调)。
 * 分配走 self-contained `stl_arena_alloc`(musl malloc; 仅 musl 标准头)。
 */
#ifndef STL_SET_H
#define STL_SET_H

#include "allocator.h"

/* 有序 lower_bound: 声明 d/lo/hi 并定位首个 !(d[mid] < key) 位(key 形参在 self 域)。
 * 各方法内联展开, 消除 4 处重复二分区段; lo 即首个 d[lo] >= key 的下标(等值由
 * `!(key < d[lo])` 判定, 溢出守卫由方法自行处理)。 */
#define STL_SET_LB() \
    K *d = (K *)self->data; \
    int lo = 0, hi = self->len; \
    while (lo < hi) { \
        int mid = (lo + hi) / 2; \
        if (d[mid] < key) lo = mid + 1; else hi = mid; \
        }

model struct STL_Set(K) {
    void *data;         /* 有序唯一键数组(K*, 升序) */
    int  len;
    int  cap;
    STL_Arena *ar;
};

model (K) void stl_set_init(STL_Set(K) *self, STL_Arena *ar) {
    self->data = 0; self->len = 0; self->cap = 0; self->ar = ar;
}
model (K) int stl_set_size(const STL_Set(K) *self)  { return self->len; }
model (K) int stl_set_empty(const STL_Set(K) *self) { return self->len == 0; }
model (K) void stl_set_clear(STL_Set(K) *self) { self->len = 0; }

/* 有序迭代: 第 i 个(升序)键指针 */
model (K) K *stl_set_key_at(STL_Set(K) *self, int i) {
    STL_ASSERT(self && i >= 0 && i < self->len);
    return &((K *)self->data)[i];
}

/* 是否含 key: 仅 operator<, lower_bound + 等价 */
model (K) int stl_set_contains(const STL_Set(K) *self, K key) {
    STL_SET_LB();
    return lo < self->len && !(key < d[lo]);
}

/* 插入(唯一): 已存在→返回 0; 新插入有序→返回 1; 分配失败→ -1 */
model (K) int stl_set_insert(STL_Set(K) *self, K key) {
    STL_SET_LB();
    if (lo < self->len && !(key < d[lo])) return 0;        /* 已存在 */
    if (self->len >= self->cap) {
        int nc = self->cap ? (self->cap * 2) : 4;
        K *nd = (K *)stl_arena_alloc(self->ar, (size_t)nc * sizeof(K), STL_ALIGN);
        if (!nd) return -1;
        for (int j = 0; j < self->len; j++) nd[j] = d[j];
        self->data = nd; self->cap = nc; d = nd;
    }
    for (int j = self->len; j > lo; j--) d[j] = d[j - 1];
    d[lo] = key;
    self->len++;
    return 1;
}

/* 删除: 命中→左移压缩返回 1; 无→0 */
model (K) int stl_set_erase(STL_Set(K) *self, K key) {
    STL_SET_LB();
    if (lo >= self->len || key < d[lo]) return 0;
    for (int j = lo; j < self->len - 1; j++) d[j] = d[j + 1];
    self->len--;
    return 1;
}

#endif /* STL_SET_H */