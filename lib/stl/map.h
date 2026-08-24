/* tcc-stl map.h - model STL_Map(K,V) (基础关联容器, self-contained)
 *
 * 简单 map: 键值对数组(arena), 线性查找 + 元素 `operator==` 判键(见 features.md
 * §4.4)。元素限 POD 值语义; 生命周期随 arena 整池回收。
 *
 * 约束(docs/stl.md §13):
 *   - 方法 `model (K,V)` 泛型函数、显式实例化调用 `stl_map_set(int,int)(&m,k,v)`。
 *   - 每个方法**自包含**——不调用另一同泛型方法(如不在 get/set 内调 find),
 *     线性查找在各自方法体内内联, 规避"泛型内调泛型 invalid/unresolved"。
 *   - 键值对用**方法体内宏展开局部结构** `struct __stl_map_e {K key; V val;}`, 存储为 void*。
 *   - 分配走 self-contained `stl_arena_alloc`(musl malloc; 仅 musl 标准头)。
 *   - 不含哈希(O(n) 查找, 正确性优先; 哈希/红黑树版 M1)。
 */
#ifndef STL_MAP_H
#define STL_MAP_H

#include "allocator.h"

#define STL_MAP_EN() \
    struct __stl_map_e { K key; V val; }

model struct STL_Map(K,V) {
    void *data;         /* 键值对数组(struct __stl_map_e*); 空 map 为 0 */
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

/* 取键对应值指针(无则 0); 线性查找用 operator==, 内联自包含 */
model (K,V) V *stl_map_get(STL_Map(K,V) *self, K key) {
    STL_MAP_EN();
    struct __stl_map_e *d = (struct __stl_map_e *)self->data;
    for (int i = 0; i < self->len; i++)
        if (d[i].key == key) return &d[i].val;
    return 0;
}
model (K,V) int stl_map_contains(const STL_Map(K,V) *self, K key) {
    STL_MAP_EN();
    struct __stl_map_e *d = (struct __stl_map_e *)self->data;
    for (int i = 0; i < self->len; i++)
        if (d[i].key == key) return 1;
    return 0;
}

/* 置值: 键已存在→覆盖; 不存在→尾部插入。返回 0=ok, -1=分配失败 */
model (K,V) int stl_map_set(STL_Map(K,V) *self, K key, V val) {
    STL_MAP_EN();
    struct __stl_map_e *d = (struct __stl_map_e *)self->data;
    for (int i = 0; i < self->len; i++)
        if (d[i].key == key) { d[i].val = val; return 0; }
    /* 自包含扩容 */
    if (self->len >= self->cap) {
        int nc = self->cap ? (self->cap * 2) : 4;
        struct __stl_map_e *nd = (struct __stl_map_e *)
            stl_arena_alloc(self->ar, (size_t)nc * sizeof(struct __stl_map_e), STL_ALIGN);
        if (!nd) return -1;
        for (int j = 0; j < self->len; j++) nd[j] = d[j];
        self->data = nd; self->cap = nc; d = nd;
    }
    d[self->len].key = key; d[self->len].val = val;
    self->len++;
    return 0;
}

/* 删键: 存在→左移压缩并返回 1; 无→0 */
model (K,V) int stl_map_erase(STL_Map(K,V) *self, K key) {
    STL_MAP_EN();
    struct __stl_map_e *d = (struct __stl_map_e *)self->data;
    for (int i = 0; i < self->len; i++) {
        if (d[i].key == key) {
            for (int j = i; j < self->len - 1; j++) d[j] = d[j + 1];
            self->len--;
            return 1;
        }
    }
    return 0;
}

#endif /* STL_MAP_H */