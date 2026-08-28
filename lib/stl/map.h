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

/* 有序 lower_bound: 声明 d/lo/hi 并定位首个 !(key≥d[mid].key ...) 为 < key 的假位。
 * 由各写/读方法内联展开(self 已含 key 形参、STL_MAP_EN 已定义 __stl_map_e), 消除
 * 6 处重复二分区段; lo 即首个 d[lo].key >= key 的下标(溢出守卫由方法自行处理)。 */
#define STL_MAP_LB() \
    struct __stl_map_e *d = (struct __stl_map_e *)self->data; \
    int lo = 0, hi = self->len; \
    while (lo < hi) { \
        int mid = (lo + hi) / 2; \
        if (d[mid].key < key) lo = mid + 1; else hi = mid; \
        }

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
    STL_MAP_EN(); STL_MAP_LB();
    if (lo < self->len && !(key < d[lo].key)) return &d[lo].val;
    return 0;
}
model (K,V) int stl_map_contains(const STL_Map(K,V) *self, K key) {
    STL_MAP_EN(); STL_MAP_LB();
    return lo < self->len && !(key < d[lo].key);
}

/* 置值: 仅 operator<。lower_bound 定位插入点, 等价(exists)→覆盖, 否则有序插入。 */
model (K,V) int stl_map_set(STL_Map(K,V) *self, K key, V val) {
    STL_MAP_EN(); STL_MAP_LB();
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

/* get 键值, 未设置时返回默认值 dflt (不改表; 见 std::map 语义的 "or default") */
model (K,V) V stl_map_getor(STL_Map(K,V) *self, K key, V dflt) {
    STL_MAP_EN(); STL_MAP_LB();
    if (lo < self->len && !(key < d[lo].key)) return d[lo].val;
    return dflt;
}

/* operator[] 语义: 返回键对应值的引用指针; 缺键则自动插入**零值默认槽**(POD 值
   语义下零初始化)并返回其指针, 可 m->stl_map_at(int,int)(k) = v 直接写入. */
model (K,V) V *stl_map_at(STL_Map(K,V) *self, K key) {
    STL_MAP_EN(); STL_MAP_LB();
    if (lo < self->len && !(key < d[lo].key)) return &d[lo].val;
    /* 缺键: 有序插入新槽, 值零初始化 */
    if (self->len >= self->cap) {
        int nc = self->cap ? (self->cap * 2) : 4;
        struct __stl_map_e *nd = (struct __stl_map_e *)
            stl_arena_alloc(self->ar, (size_t)nc * sizeof(struct __stl_map_e), STL_ALIGN);
        if (!nd) return 0;
        for (int j = 0; j < self->len; j++) nd[j] = d[j];
        self->data = nd; self->cap = nc; d = nd;
    }
    for (int j = self->len; j > lo; j--) d[j] = d[j - 1];
    d[lo].key = key;
    {   char *vp = (char *)&d[lo].val;   /* 零初始化值槽 */
        for (int k = 0; k < (int)sizeof(V); k++) vp[k] = 0;
    }
    self->len++;
    return &d[lo].val;
}

/* 删键: 仅 operator<。等价命中→左移压缩返回 1; 无→0 */
model (K,V) int stl_map_erase(STL_Map(K,V) *self, K key) {
    STL_MAP_EN(); STL_MAP_LB();
    if (lo >= self->len || key < d[lo].key) return 0;
    for (int j = lo; j < self->len - 1; j++) d[j] = d[j + 1];
    self->len--;
    return 1;
}

#endif /* STL_MAP_H */