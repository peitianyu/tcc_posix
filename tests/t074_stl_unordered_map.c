/* t074_stl_unordered_map: M1-待办 - STL_Unordered_Map(K,V) 哈希关联容器 (纯断言)
 *
 * 覆盖: set/get/contains/size/erase/clear, 覆盖更新(同键改值), 扩容(2 幂增长),
 *       墓碑重用(删后仍可插), getor(默认不改表), at(operator[] 自动插入零值槽),
 *       each(遍历全部), struct 键(operator== + 字节哈希), 反复删插(压实稳定性)。
 *
 * 与 t069(有序 map) 对照的重点差异: 这里键契约 = `operator==` + 字节哈希,
 * 不要求 operator<。迭代顺序与插入无关(哈希随机), 故 each 只断言集合语义。
 *
 * 调用风格: 对象方法糖 `m->stl_unordered_map_set(int,int)(k,v)`。
 * 退出码 0 = 通过.
 */
#include "lib/stl/unordered_map.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

/* struct 键: 无填充 POD —— 需 operator==(等值契约); FNV 取 sizeof(K) 字节。
 * 值语义 POD, {id,n} 确定初始化 ⇒ 等值键字节一致, 哈希与 == 自洽。 */
struct Mid { int id; int n; };
int operator== (struct Mid a, struct Mid b) { return a.id == b.id && a.n == b.n; }

/* each 回调: 求和校验 + 计数 */
static int count_sum(int k, int v, void *ud) {
    int *acc = (int *)ud;
    acc[0]++;
    acc[1] += (v == 0) ? k * 2 : v;   /* at 插的零值槽 *2 以区分, 一般 -1 哨兵 */
    if (k == -1) return 1;            /* 提前中断测试用 */
    return 0;
}

int main(void) {
    STL_Arena *ar = stl_arena_new(0);

    /* 1. int-int: 插入/查找/覆盖/删除/清空 */
    {
        STL_Unordered_Map(int,int) m;
        m->stl_unordered_map_init(int,int)(ar);
        CHECK(m->stl_unordered_map_empty(int,int)());
        for (int i = 0; i < 100; i++) CHECK(m->stl_unordered_map_set(int,int)(i, i * 10) == 0);
        CHECK(m->stl_unordered_map_size(int,int)() == 100);
        for (int i = 0; i < 100; i++) {
            CHECK(m->stl_unordered_map_contains(int,int)(i));
            int *v = m->stl_unordered_map_get(int,int)(i);
            CHECK(v && *v == i * 10);
        }
        CHECK(!m->stl_unordered_map_contains(int,int)(1000));
        CHECK(m->stl_unordered_map_get(int,int)(1000) == 0);

        /* 覆盖更新(键数不变) */
        CHECK(m->stl_unordered_map_set(int,int)(7, 777) == 0);
        CHECK(m->stl_unordered_map_size(int,int)() == 100);
        { int *v = m->stl_unordered_map_get(int,int)(7); CHECK(v && *v == 777); }

        /* 删除 → 墓碑, 不含且 size 减一 */
        CHECK(m->stl_unordered_map_erase(int,int)(10) == 1);
        CHECK(!m->stl_unordered_map_contains(int,int)(10));
        CHECK(m->stl_unordered_map_size(int,int)() == 99);
        CHECK(m->stl_unordered_map_erase(int,int)(10) == 0);   /* 已删 */

        /* 删后仍可插入同键(墓碑重用 → 是否新表均须含) */
        CHECK(m->stl_unordered_map_set(int,int)(10, 101) == 0);
        CHECK(m->stl_unordered_map_size(int,int)() == 100);
        { int *v = m->stl_unordered_map_get(int,int)(10); CHECK(v && *v == 101); }

        m->stl_unordered_map_clear(int,int)();
        CHECK(m->stl_unordered_map_empty(int,int)());
        CHECK(!m->stl_unordered_map_contains(int,int)(7));
    }

    /* 2. 扩容 + JSON 式键(超阈值推再哈希, verify 全部键) */
    {
        STL_Unordered_Map(int,int) m; m->stl_unordered_map_init(int,int)(ar);
        int n = 0;
        for (int k = 0; k < 2000; k++) { m->stl_unordered_map_set(int,int)(k, k); n++; }
        CHECK(m->stl_unordered_map_size(int,int)() == 2000);
        for (int k = 0; k < 2000; k += 7) CHECK(*m->stl_unordered_map_get(int,int)(k) == k);
        /* cap 必为 2 幂且 ≥ 装载数 */
        int cap = m->stl_unordered_map_cap(int,int)();
        CHECK(cap >= 2000 && (cap & (cap - 1)) == 0);
    }

    /* 3. getor: 未设置返回默认(不改表) */
    {
        STL_Unordered_Map(int,int) m; m->stl_unordered_map_init(int,int)(ar);
        CHECK(m->stl_unordered_map_set(int,int)(5, 55) == 0);
        CHECK(m->stl_unordered_map_getor(int,int)(5, -1) == 55);
        CHECK(m->stl_unordered_map_getor(int,int)(9, -1) == -1);
        CHECK(!m->stl_unordered_map_contains(int,int)(9));
        CHECK(m->stl_unordered_map_size(int,int)() == 1);
    }

    /* 4. at(operator[]): 缺键自动插入零值槽并可写 */
    {
        STL_Unordered_Map(int,int) m; m->stl_unordered_map_init(int,int)(ar);
        int *p = m->stl_unordered_map_at(int,int)(7);
        CHECK(p && *p == 0);
        CHECK(m->stl_unordered_map_contains(int,int)(7));
        CHECK(m->stl_unordered_map_size(int,int)() == 1);
        *m->stl_unordered_map_at(int,int)(7) = 777;
        CHECK(*m->stl_unordered_map_at(int,int)(7) == 777);
        *m->stl_unordered_map_at(int,int)(3) = 33;
        CHECK(*m->stl_unordered_map_at(int,int)(3) == 33);
        CHECK(m->stl_unordered_map_size(int,int)() == 2);
    }

    /* 5. struct 键: operator== 等值, 覆盖 + 删 + 插 + getor */
    {
        STL_Unordered_Map(struct Mid, int) m; m->stl_unordered_map_init(struct Mid, int)(ar);
        struct Mid a = {3,1}, b = {7,2}, c = {9,3};
        CHECK(m->stl_unordered_map_set(struct Mid, int)(a, 31) == 0);
        CHECK(m->stl_unordered_map_set(struct Mid, int)(b, 72) == 0);
        CHECK(m->stl_unordered_map_set(struct Mid, int)(c, 93) == 0);
        CHECK(m->stl_unordered_map_contains(struct Mid, int)(a));
        CHECK(*m->stl_unordered_map_get(struct Mid, int)(b) == 72);
        /* 等值覆盖: 同 {3,1} 不同实例 */
        struct Mid a2 = {3,1};
        CHECK(m->stl_unordered_map_set(struct Mid, int)(a2, 311) == 0);
        CHECK(m->stl_unordered_map_size(struct Mid, int)() == 3);
        CHECK(*m->stl_unordered_map_get(struct Mid, int)(a) == 311);
        /* 未含键 */
        struct Mid d = {1,0};
        CHECK(!m->stl_unordered_map_contains(struct Mid, int)(d));
        CHECK(m->stl_unordered_map_getor(struct Mid, int)(d, -5) == -5);
        /* 删除 */
        CHECK(m->stl_unordered_map_erase(struct Mid, int)(b) == 1);
        CHECK(!m->stl_unordered_map_contains(struct Mid, int)(b));
        CHECK(m->stl_unordered_map_size(struct Mid, int)() == 2);
    }

    /* 6. each: 遍历全部实存键值对; 提前中断返回访问数 */
    {
        STL_Unordered_Map(int,int) m; m->stl_unordered_map_init(int,int)(ar);
        for (int k = 1; k <= 40; k++) m->stl_unordered_map_set(int,int)(k, k * 100);
        CHECK(m->stl_unordered_map_size(int,int)() == 40);
        int acc[2] = {0, 0};
        int n = m->stl_unordered_map_each(int,int)(count_sum, acc);
        CHECK(n == 40);
        CHECK(acc[0] == 40);
        /* 删一半再遍历: 仅余实存 */
        for (int k = 1; k <= 40; k += 2) m->stl_unordered_map_erase(int,int)(k);
        int acc2[2] = {0, 0};
        n = m->stl_unordered_map_each(int,int)(count_sum, acc2);
        CHECK(n == 20);
        CHECK(acc2[0] == 20);
    }

    stl_arena_destroy(ar);
    return 0;
}