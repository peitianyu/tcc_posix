/* tcc-stl unordered_map.h - model STL_Unordered_Map(K,V) (哈希关联容器, self-contained)
 *
 * std::unordered_map 语义: 键**无序**、平均 O(1) 查找/插入/删除, 通过哈希 + 等值。
 * 与关联家族其余成员(map/set 用 operator<)区分: 这里键契约 = `operator==`(等值)
 * + 字节哈希, **不要求 operator<**(无序即无序)。
 *
 * 数据结构: 开放寻址 + 线性探测, 单平铺槽表(便于枚举)。槽状态 0=空 1=占 2=墓碑。
 * 装载阈值 ≈75%((len+tomb) >= nb - nb/4) 触发再哈希(扩容 2 幂并压缩墓碑)。
 *
 * 哈希契约(docs/stl.md §13):
 *   - 等值: `k == slot.key` 分发到具体 K 的 `operator==`(编译器泛型重放支持;
 *     与 stl_find 的 `*b == val` 先例一致)。
 *   - 哈希: 32 位 FNV-1a, 取 `sizeof(K)` 连续字节。对 int/指针/无填充 POD 严格正确;
 *     含填充 struct 键需等值键字节一致(值语义 POD 经确定初始化通常满足)。
 *     绝大场景(int/自定义枚举)免费可用, 无需显式哈希回调。
 *
 * 约束(docs/stl.md §13-9/12): 方法 `model (K,V)` 泛型、显式实例化/对象方法糖调用;
 * 各方法**自包含**(探查/再哈希内联, 不互调同泛型方法); 哈希辅助用 `static`(非 inline)
 * 全局符号, 供 model 体重放解析; 分配走 self-contained `stl_arena_alloc`。
 * 元素限 POD 值语义; 生命周期随 arena 整池回收。
 */
#ifndef STL_UNORDERED_MAP_H
#define STL_UNORDERED_MAP_H

#include "allocator.h"

#define STL_UMAP_SLOT_EN() \
    struct __stl_umap_e { int state; K key; V val; }   /* 0=空 1=占 2=墓碑 */

/* 32 位 FNV-1a 字节哈希(static 符号, model 体重放可解析; §13-9) */
STL_STATIC unsigned stl_uwhash(const unsigned char *p, size_t n)
{
    unsigned h = 2166136261u;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

model struct STL_Unordered_Map(K,V) {
    void *slots;        /* struct __stl_umap_e* 槽表, 长度 nb(2 幂), 初始 0 */
    int  len;           /* 已占键数(state=1) */
    int  tomb;          /* 墓碑数(state=2) */
    int  nb;            /* 槽数(2 幂); 0=未分配 */
    STL_Arena *ar;
};

model (K,V) void stl_unordered_map_init(STL_Unordered_Map(K,V) *self, STL_Arena *ar) {
    self->slots = 0; self->len = 0; self->tomb = 0; self->nb = 0; self->ar = ar;
}
model (K,V) int stl_unordered_map_size(const STL_Unordered_Map(K,V) *self)  { return self->len; }
model (K,V) int stl_unordered_map_empty(const STL_Unordered_Map(K,V) *self) { return self->len == 0; }
model (K,V) int stl_unordered_map_cap(const STL_Unordered_Map(K,V) *self)   { return self->nb; }

/* 清空(仅重置槽态; arena 整池回收, 无逐对象析构) */
model (K,V) void stl_unordered_map_clear(STL_Unordered_Map(K,V) *self) {
    STL_UMAP_SLOT_EN();
    struct __stl_umap_e *s = (struct __stl_umap_e *)self->slots;
    for (int i = 0; i < self->nb; i++) s[i].state = 0;
    self->len = 0; self->tomb = 0;
}

/* 再哈希: 扩容至 2 幂(首插 16)并压缩墓碑。仅 set/at(写路径)在装载近阈值时调用。
 * 内联宏(§13-9: 泛型方法不得互调同泛型方法 → 各写方法自包含再哈希)。 */
#define STL_UMAP_REHASH(failret) do { \
    STL_UMAP_SLOT_EN(); \
    int nb2 = self->nb ? self->nb * 2 : 16; \
    struct __stl_umap_e *s2 = (struct __stl_umap_e *) \
        stl_arena_alloc(self->ar, (size_t)nb2 * sizeof(struct __stl_umap_e), STL_ALIGN); \
    if (!s2) return (failret); \
    for (int q = 0; q < nb2; q++) s2[q].state = 0; \
    struct __stl_umap_e *o = (struct __stl_umap_e *)self->slots; \
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

#define STL_UMAP_ROOM() ((self->nb == 0) || \
    ((self->len + self->tomb) >= (self->nb - self->nb / 4)))

/* 线性探查定位: 声明 s/h/e, 计算 `fo`(命中槽下标, 未命中 -1)、`ft`(首个墓碑
 * 下标, 无则 -1) 与 `et`(首个空槽下标, 无则 -1, 即 break 处)。读/写方法共用
 * 同一探查段, 消除 4 处重复循环。写路径插入位 = ft>=0?ft:et。
 * 前置: STL_UMAP_SLOT_EN 已展开, self/key/nb 有效; 空表由调用方先判(nb==0)。 */
#define STL_UMAP_SPAN() \
    struct __stl_umap_e *s = (struct __stl_umap_e *)self->slots; \
    int fo = -1, ft = -1, et = -1; \
    int h = (int)(stl_uwhash((const unsigned char *)&key, sizeof(K)) & (unsigned)(self->nb - 1)); \
    for (int p = 0; p < self->nb; p++) { \
        int i = (h + p) & (self->nb - 1); \
        struct __stl_umap_e *e = &s[i]; \
        if (e->state == 0) { et = i; break; } \
        if (e->state == 1 && e->key == key) { fo = i; break; } \
        if (e->state == 2 && ft < 0) ft = i; \
        }

/* 取键对应值指针(无则 0)。只读: 不触发再哈希(空表直接 0)。 */
model (K,V) V *stl_unordered_map_get(STL_Unordered_Map(K,V) *self, K key) {
    STL_UMAP_SLOT_EN();
    if (self->nb == 0) return 0;
    STL_UMAP_SPAN();
    if (fo >= 0) return &s[fo].val;
    return 0;
}
model (K,V) int stl_unordered_map_contains(const STL_Unordered_Map(K,V) *self, K key) {
    STL_UMAP_SLOT_EN();
    if (self->nb == 0) return 0;
    STL_UMAP_SPAN();
    return fo >= 0;
}

/* 置值: 已存在→覆盖返回 0; 新插入返回 0; 内存失败 -1。写路径先确保装载余量。 */
model (K,V) int stl_unordered_map_set(STL_Unordered_Map(K,V) *self, K key, V val) {
    STL_UMAP_SLOT_EN();
    if (STL_UMAP_ROOM()) STL_UMAP_REHASH(-1);
    STL_UMAP_SPAN();
    if (fo >= 0) { s[fo].val = val; return 0; }       /* 覆盖 */
    {   int ins = (ft >= 0) ? ft : et;                /* 墓碑复用, 否则首空槽 */
        s[ins].key = key; s[ins].val = val; s[ins].state = 1;
        if (ft >= 0) self->tomb--;
        self->len++;
    }
    return 0;
}

/* get 键值, 未设置时返回默认值 dflt (不改表) */
model (K,V) V stl_unordered_map_getor(STL_Unordered_Map(K,V) *self, K key, V dflt) {
    STL_UMAP_SLOT_EN();
    if (self->nb == 0) return dflt;
    STL_UMAP_SPAN();
    if (fo >= 0) return s[fo].val;
    return dflt;
}

/* operator[] 语义: 缺键自动插入**零值默认槽**并返回其值指针, 可写. */
model (K,V) V *stl_unordered_map_at(STL_Unordered_Map(K,V) *self, K key) {
    STL_UMAP_SLOT_EN();
    if (STL_UMAP_ROOM()) STL_UMAP_REHASH(0);
    STL_UMAP_SPAN();
    if (fo >= 0) return &s[fo].val;
    {   int ins = (ft >= 0) ? ft : et;
        s[ins].key = key; s[ins].state = 1;
        if (ft >= 0) self->tomb--;
        self->len++;
        { char *vp = (char *)&s[ins].val;   /* 零初始化值槽 */
          for (int k = 0; k < (int)sizeof(V); k++) vp[k] = 0; }
        return &s[ins].val;
    }
}

/* 删键: 命中→置墓碑返回 1; 无→0 */
model (K,V) int stl_unordered_map_erase(STL_Unordered_Map(K,V) *self, K key) {
    STL_UMAP_SLOT_EN();
    if (self->nb == 0) return 0;
    STL_UMAP_SPAN();
    if (fo >= 0) { s[fo].state = 2; self->len--; self->tomb++; return 1; }
    return 0;
}

/* 遍历全部实存键值对(顺序与插入无关, 随机槽序): 返回访问数。
 * cb 每对调用一次; 非 0 可提前中断(此时返回已访问数)。 */
model (K,V) int stl_unordered_map_each(const STL_Unordered_Map(K,V) *self,
                                       int(*cb)(K,V,void*), void *ud) {
    STL_UMAP_SLOT_EN();
    struct __stl_umap_e *s = (struct __stl_umap_e *)self->slots;
    int n = 0;
    for (int i = 0; i < self->nb; i++)
        if (s[i].state == 1) { n++; if (cb(s[i].key, s[i].val, ud)) break; }
    return n;
}

#endif /* STL_UNORDERED_MAP_H */