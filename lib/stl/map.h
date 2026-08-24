/* tcc-stl map.h - model STL_Map(K,V) (有序关联容器, self-contained)
 *
 * std::map 语义: 键**有序**(按元素 `operator<` 严格弱序), 查找二分(O(log n)),
 * 迭代按键序。键只需提供 `operator<`(与 stl_sort 等"可排序"契约一致):
 * 等价关系由 `!(a<b) && !(b<a)` 推导, 不要求 `operator==`。
 * 元素限 POD 值语义; 生命周期随 arena 整池回收。
 *
 * 约束/契约(docs/stl.md §13):
 *   - 方法 `model (K,V)` 泛型函数、显式实例化调用 `stl_map_set(int,int)(&m,k,v)`。
 *   - 每个方法**自包含**——不调用另一同泛型方法(lower_bound 在各方法体内内联;
 *     泛型互调会 invalid/unresolved)。
 *   - 键值对用**方法体内宏展开局部结构** `struct __stl_map_e {K key; V val;}`, 存储为 void*。
 *   - 分配走 self-contained `stl_arena_alloc`(musl malloc; 仅 musl 标准头)。
 */
#ifndef STL_MAP_H
#define STL_MAP_H

#include "allocator.h"

#define STL_MAP_EN() \
    struct __stl_map_e { K key; V val; }

model struct STL_Map(K,V) {
    void *data;         /* 有序键值对数组(struct __stl_map_e*, 按 key 升序) */
    int  len;
    int  cap;
    STL_Arena *ar;
};

model (K,V) void stl_map_init(STL_Map(K,V) *self, STL_Arena *ar) {
    self->data = 0; self->len = 0; self->cap = 0; self->ar = ar;
}
model (K,V) int stl_map_size(const STL_Map(K,V) *self)  { return self->len; }
model (K,V) int stl_map_empty(const STL_Map(K,V) *self) { return self->len == 0; }
model (K,V) void stl_map_clear(STL_Map(K,V) *self) { self->len = 0; }

/* 有序迭代: 第 i 个(升序)键/值指针 */
model (K,V) K *stl_map_key_at(STL_Map(K,V) *self, int i) {
    STL_MAP_EN(); STL_ASSERT(self && i >= 0 && i < self->len);
    return &((struct __stl_map_e *)self->data)[i].key;
}
model (K,V) V *stl_map_val_at(STL_Map(K,V) *self, int i) {
    STL_MAP_EN(); STL_ASSERT(self && i >= 0 && i < self->len);
    return &((struct __stl_map_e *)self->data)[i].val;
}

/* 取键对应值指针(无则 0); 仅 operator<: lower_bound 定位首个 !(<key) 位, 再判等价 */
model (K,V) V *stl_map_get(STL_Map(K,V) *self, K key) {
    STL_MAP_EN();
    struct __stl_map_e *d = (struct __stl_map_e *)self->data;
    int lo = 0, hi = self->len;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (d[mid].key < key) lo = mid + 1; else hi = mid;
    }
    if (lo < self->len && !(key < d[lo].key)) return &d[lo].val;
    return 0;
}
model (K,V) int stl_map_contains(const STL_Map(K,V) *self, K key) {
    STL_MAP_EN();
    struct __stl_map_e *d = (struct __stl_map_e *)self->data;
    int lo = 0, hi = self->len;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (d[mid].key < key) lo = mid + 1; else hi = mid;
    }
    return lo < self->len && !(key < d[lo].key);
}

/* 置值: 仅 operator<。lower_bound 定位插入点, 等价(exists)→覆盖, 否则有序插入。 */
model (K,V) int stl_map_set(STL_Map(K,V) *self, K key, V val) {
    STL_MAP_EN();
    struct __stl_map_e *d = (struct __stl_map_e *)self->data;
    int lo = 0, hi = self->len;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (d[mid].key < key) lo = mid + 1; else hi = mid;
    }
    if (lo < self->len && !(key < d[lo].key)) { d[lo].val = val; return 0; }  /* 覆盖 */
    if (self->len >= self->cap) {            /* 自包含扩容 */
        int nc = self->cap ? (self->cap * 2) : 4;
        struct __stl_map_e *nd = (struct __stl_map_e *)
            stl_arena_alloc(self->ar, (size_t)nc * sizeof(struct __stl_map_e), STL_ALIGN);
        if (!nd) return -1;
        for (int j = 0; j < self->len; j++) nd[j] = d[j];
        self->data = nd; self->cap = nc; d = nd;
    }
    for (int j = self->len; j > lo; j--) d[j] = d[j - 1];       /* 右移腾位 */
    d[lo].key = key; d[lo].val = val;
    self->len++;
    return 0;
}

/* 删键: 仅 operator<。等价命中→左移压缩返回 1; 无→0 */
model (K,V) int stl_map_erase(STL_Map(K,V) *self, K key) {
    STL_MAP_EN();
    struct __stl_map_e *d = (struct __stl_map_e *)self->data;
    int lo = 0, hi = self->len;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (d[mid].key < key) lo = mid + 1; else hi = mid;
    }
    if (lo >= self->len || key < d[lo].key) return 0;
    for (int j = lo; j < self->len - 1; j++) d[j] = d[j + 1];
    self->len--;
    return 1;
}

#endif /* STL_MAP_H */