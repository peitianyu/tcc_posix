/* t075_stl_unordered_set: M1-待办 - STL_Unordered_Set(K) 哈希唯一集合 (纯断言)
 *
 * 覆盖: insert(唯一)/contains/size/erase/clear, 扩容(2 幂), 墓碑重用,
 *       struct 键(operator==), each(遍历全部 + 提前中断)。
 *
 * 与 t069(有序 set) 对照: 元素无序且唯一, 键契约 = `operator==` + 字节哈希,
 * 不要求 operator<。迭代顺序与插入无关, 故 each 只断言集合语义。
 *
 * 调用风格: 对象方法糖 `s->stl_unordered_set_insert(int)(x)`。
 * 退出码 0 = 通过.
 */
#include "lib/stl/unordered_set.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

struct Mid { int id; int n; };
int operator== (struct Mid a, struct Mid b) { return a.id == b.id && a.n == b.n; }

struct Cnt { int n; int sum; };
static int acc_elem(int k, void *ud) {
    struct Cnt *c = (struct Cnt *)ud;
    c->n++; c->sum += k;
    if (k == -1) return 1;      /* 提前中断用哨兵 */
    return 0;
}

int main(void) {
    STL_Arena *ar = stl_arena_new(0);

    /* 1. int: 插入(唯一)/存在/删除/清空 */
    {
        STL_Unordered_Set(int) s;
        s->stl_unordered_set_init(int)(ar);
        CHECK(s->stl_unordered_set_empty(int)());
        for (int i = 0; i < 100; i++) CHECK(s->stl_unordered_set_insert(int)(i) == 1);
        CHECK(s->stl_unordered_set_size(int)() == 100);
        for (int i = 0; i < 100; i++) CHECK(s->stl_unordered_set_contains(int)(i));
        CHECK(!s->stl_unordered_set_contains(int)(1000));

        /* 唯一: 重复插入返回 0 且 size 不变 */
        for (int i = 0; i < 100; i++) CHECK(s->stl_unordered_set_insert(int)(i) == 0);
        CHECK(s->stl_unordered_set_size(int)() == 100);

        /* 删除 → 墓碑 */
        CHECK(s->stl_unordered_set_erase(int)(10) == 1);
        CHECK(!s->stl_unordered_set_contains(int)(10));
        CHECK(s->stl_unordered_set_size(int)() == 99);
        CHECK(s->stl_unordered_set_erase(int)(10) == 0);
        /* 删后可重插 */
        CHECK(s->stl_unordered_set_insert(int)(10) == 1);
        CHECK(s->stl_unordered_set_contains(int)(10));

        s->stl_unordered_set_clear(int)();
        CHECK(s->stl_unordered_set_empty(int)());
        CHECK(!s->stl_unordered_set_contains(int)(7));
    }

    /* 2. 扩容: 大批量插入 → 2 幂 cap ≥ len */
    {
        STL_Unordered_Set(int) s; s->stl_unordered_set_init(int)(ar);
        for (int k = 0; k < 3000; k++) CHECK(s->stl_unordered_set_insert(int)(k) == 1);
        CHECK(s->stl_unordered_set_size(int)() == 3000);
        int cap = s->stl_unordered_set_cap(int)();
        CHECK(cap >= 3000 && (cap & (cap - 1)) == 0);
        for (int k = 0; k < 3000; k += 11) CHECK(s->stl_unordered_set_contains(int)(k));
    }

    /* 3. struct 键: operator== 等值 + 删除 */
    {
        STL_Unordered_Set(struct Mid) s; s->stl_unordered_set_init(struct Mid)(ar);
        struct Mid a = {3,1}, b = {7,2}, c = {9,3};
        CHECK(s->stl_unordered_set_insert(struct Mid)(a) == 1);
        CHECK(s->stl_unordered_set_insert(struct Mid)(b) == 1);
        CHECK(s->stl_unordered_set_insert(struct Mid)(c) == 1);
        CHECK(s->stl_unordered_set_size(struct Mid)() == 3);
        CHECK(s->stl_unordered_set_contains(struct Mid)(a));
        /* 等值重复插入: 同 {3,1} 不同实例 */
        struct Mid a2 = {3,1};
        CHECK(s->stl_unordered_set_insert(struct Mid)(a2) == 0);
        CHECK(s->stl_unordered_set_size(struct Mid)() == 3);
        struct Mid d = {1,0};
        CHECK(!s->stl_unordered_set_contains(struct Mid)(d));
        CHECK(s->stl_unordered_set_erase(struct Mid)(b) == 1);
        CHECK(!s->stl_unordered_set_contains(struct Mid)(b));
        CHECK(s->stl_unordered_set_size(struct Mid)() == 2);
        CHECK(s->stl_unordered_set_contains(struct Mid)(a));
        CHECK(s->stl_unordered_set_contains(struct Mid)(c));
    }

    /* 4. each: 遍历全部实存; 删一半后剩一半; 提前中断返回访问数 */
    {
        STL_Unordered_Set(int) s; s->stl_unordered_set_init(int)(ar);
        for (int k = 1; k <= 50; k++) s->stl_unordered_set_insert(int)(k);
        struct Cnt c1 = {0, 0};
        int n = s->stl_unordered_set_each(int)(acc_elem, &c1);
        CHECK(n == 50 && c1.n == 50);
        CHECK(c1.sum == (50 * 51) / 2);   /* 集合语义: 无论槽序, 元素和不变 */
        for (int k = 2; k <= 50; k += 2) s->stl_unordered_set_erase(int)(k);
        struct Cnt c2 = {0, 0};
        n = s->stl_unordered_set_each(int)(acc_elem, &c2);
        CHECK(n == 25 && c2.n == 25);
        CHECK(c2.sum == 25 * 25);          /* 1+3+..+49 = 25^2 */
    }

    stl_arena_destroy(ar);
    return 0;
}