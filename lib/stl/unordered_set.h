/* tcc-stl unordered_set.h - model STL_Unordered_Set(K) (哈希唯一集合, self-contained)
 *
 * std::unordered_set 语义: 元素**无序**且**唯一**、平均 O(1) 存在性/插入/删除,
 * 通过哈希 + 等值。键契约 = `operator==` + 字节哈希, **不要求 operator<**(无序即无序)。
 *
 * 数据结构同 unordered_map: 开放寻址线性探测 + 墓碑, 单平铺槽表; 装载阈值 ≈75%
 * 触发再哈希(扩容 2 幂并压缩墓碑)。槽状态 0=空 1=占 2=墓碑。
 *
 * 哈希契约与边界: 等值 `k == s[i].key` 分发到具体 K 的 `operator==`; 哈希 32 位
 * FNV-1a 取 `sizeof(K)` 字节。int/指针/无填充 POD 严格正确; 含填充 struct 键需
 * 等值键字节一致(值语义 POD 确定初始化通常满足)。元素限 POD; 生命周期随 arena。
 *
 * 约束(docs/stl.md §13-9/12): 方法 `model (K)` 泛型、显式实例化/对象方法糖调用;
 * 各方法自包含(探查/再哈希内联, 不互调); 哈希辅助用 `static` 全局符号供 model 重放;
 * 分配走 self-contained `stl_arena_alloc`(与 unordered_map 共享 stl_uwhash)。
 */
#ifndef STL_UNORDERED_SET_H
#define STL_UNORDERED_SET_H

#include "allocator.h"
#include "unordered_map.h"       /* 复用 stl_uwhash */

#define STL_USET_SLOT_EN() \
    struct __stl_uset_e { int state; K key; }   /* 0=空 1=占 2=墓碑 */

model struct STL_Unordered_Set(K) {
    void *slots;        /* struct __stl_uset_e* 槽表, 长度 nb(2 幂), 初始 0 */
    int  len;
    int  tomb;
    int  nb;
    STL_Arena *ar;
};

model (K) void stl_unordered_set_init(STL_Unordered_Set(K) *self, STL_Arena *ar) {
    self->slots = 0; self->len = 0; self->tomb = 0; self->nb = 0; self->ar = ar;
}
model (K) int stl_unordered_set_size(const STL_Unordered_Set(K) *self)  { return self->len; }
model (K) int stl_unordered_set_empty(const STL_Unordered_Set(K) *self) { return self->len == 0; }
model (K) int stl_unordered_set_cap(const STL_Unordered_Set(K) *self)   { return self->nb; }

/* 清空(仅重置槽态; arena 整池回收) */
model (K) void stl_unordered_set_clear(STL_Unordered_Set(K) *self) {
    STL_USET_SLOT_EN();
    struct __stl_uset_e *s = (struct __stl_uset_e *)self->slots;
    for (int i = 0; i < self->nb; i++) s[i].state = 0;
    self->len = 0; self->tomb = 0;
}

#define STL_USET_REHASH(failret) do { \
    STL_USET_SLOT_EN(); \
    int nb2 = self->nb ? self->nb * 2 : 16; \
    struct __stl_uset_e *s2 = (struct __stl_uset_e *) \
        stl_arena_alloc(self->ar, (size_t)nb2 * sizeof(struct __stl_uset_e), STL_ALIGN); \
    if (!s2) return (failret); \
    for (int q = 0; q < nb2; q++) s2[q].state = 0; \
    struct __stl_uset_e *o = (struct __stl_uset_e *)self->slots; \
    for (int q = 0; q < self->nb; q++) if (o[q].state == 1) { \
        int h = (int)(stl_uwhash((const unsigned char *)&o[q].key, sizeof(K)) \
                      & (unsigned)(nb2 - 1)); \
        for (int p = 0; p < nb2; p++) { \
            int j = (h + p) & (nb2 - 1); \
            if (s2[j].state == 0) { s2[j] = o[q]; break; } \
        } \
    } \
    self->slots = s2; self->nb = nb2; self->tomb = 0; \
} while (0)

#define STL_USET_ROOM() ((self->nb == 0) || \
    ((self->len + self->tomb) >= (self->nb - self->nb / 4)))

/* 线性探查定位(同 unordered_map STL_UMAP_SPAN): fo=命中槽, ft=首墓碑, et=首空槽 */
#define STL_USET_SPAN() \
    struct __stl_uset_e *s = (struct __stl_uset_e *)self->slots; \
    int fo = -1, ft = -1, et = -1; \
    int h = (int)(stl_uwhash((const unsigned char *)&key, sizeof(K)) & (unsigned)(self->nb - 1)); \
    for (int p = 0; p < self->nb; p++) { \
        int i = (h + p) & (self->nb - 1); \
        struct __stl_uset_e *e = &s[i]; \
        if (e->state == 0) { et = i; break; } \
        if (e->state == 1 && e->key == key) { fo = i; break; } \
        if (e->state == 2 && ft < 0) ft = i; \
        }

/* 是否含 key(只读, 不触发再哈希; 空表直接 0) */
model (K) int stl_unordered_set_contains(const STL_Unordered_Set(K) *self, K key) {
    STL_USET_SLOT_EN();
    if (self->nb == 0) return 0;
    STL_USET_SPAN();
    return fo >= 0;
}

/* 插入(唯一): 已存在→0; 新插入→1; 内存失败 -1 */
model (K) int stl_unordered_set_insert(STL_Unordered_Set(K) *self, K key) {
    STL_USET_SLOT_EN();
    if (STL_USET_ROOM()) STL_USET_REHASH(-1);
    STL_USET_SPAN();
    if (fo >= 0) return 0;                              /* 已存在 */
    {   int ins = (ft >= 0) ? ft : et;
        s[ins].key = key; s[ins].state = 1;
        if (ft >= 0) self->tomb--;
        self->len++;
    }
    return 1;
}

/* 删除: 命中→置墓碑返回 1; 无→0 */
model (K) int stl_unordered_set_erase(STL_Unordered_Set(K) *self, K key) {
    STL_USET_SLOT_EN();
    if (self->nb == 0) return 0;
    STL_USET_SPAN();
    if (fo >= 0) { s[fo].state = 2; self->len--; self->tomb++; return 1; }
    return 0;
}

/* 遍历全部实存元素(随机槽序): cb 每元一次(非 0 提前中断), 返回访问数 */
model (K) int stl_unordered_set_each(const STL_Unordered_Set(K) *self,
                                     int(*cb)(K,void*), void *ud) {
    STL_USET_SLOT_EN();
    struct __stl_uset_e *s = (struct __stl_uset_e *)self->slots;
    int n = 0;
    for (int i = 0; i < self->nb; i++)
        if (s[i].state == 1) { n++; if (cb(s[i].key, ud)) break; }
    return n;
}

#endif /* STL_UNORDERED_SET_H */