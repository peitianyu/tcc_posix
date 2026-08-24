/* t071_stl_set: M1 - STL_Set(K) 有序唯一集合 (纯断言)
 * 覆盖: insert(唯一性/去重/有序维护)、contains、erase、size、有序迭代、
 *       struct 键(仅 operator<) 去重。
 * 调用风格: 对象方法糖 `s->stl_set_insert(int)(x)`。
 * 退出码 0 = 通过.
 */
#include "lib/stl/set.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

struct Cmp { int id; };
int operator< (struct Cmp a, struct Cmp b) { return a.id < b.id; }

int main(void) {
    STL_Arena *ar = stl_arena_new(0);

    /* 1. int: 乱序插入 → 有序唯一 */
    {
        STL_Set(int) s; s->stl_set_init(int)(ar);
        CHECK(s->stl_set_empty(int)());
        int seq[9] = { 5, 3, 8, 3, 1, 7, 5, 2, 6 };   /* 含重复 3,5 */
        for (int i = 0; i < 9; i++) s->stl_set_insert(int)(seq[i]);
        /* 去重并有序: {1,2,3,5,6,7,8} */
        CHECK(s->stl_set_size(int)() == 7);
        int uniq[7] = {1,2,3,5,6,7,8};
        for (int i = 0; i < 7; i++) CHECK(*s->stl_set_key_at(int)(i) == uniq[i]);
    }

    /* 2. inset 返回值: 新=1 / 已存在=0 */
    {
        STL_Set(int) s; s->stl_set_init(int)(ar);
        CHECK(s->stl_set_insert(int)(7) == 1);
        CHECK(s->stl_set_insert(int)(7) == 0);          /* 重复 */
        CHECK(s->stl_set_insert(int)(3) == 1);
        CHECK(s->stl_set_contains(int)(7));
        CHECK(s->stl_set_contains(int)(3));
        CHECK(!s->stl_set_contains(int)(9));
        /* 有序迭代 */
        int expect = 0; int keys[2] = {3,7};
        for (int i = 0; i < 2; i++) CHECK(*s->stl_set_key_at(int)(i) == keys[i]);
        /* 删除 */
        CHECK(s->stl_set_erase(int)(3) == 1);
        CHECK(!s->stl_set_contains(int)(3));
        CHECK(s->stl_set_erase(int)(3) == 0);
        CHECK(s->stl_set_size(int)() == 1);
        s->stl_set_clear(int)();
        CHECK(s->stl_set_empty(int)());
    }

    /* 3. struct 键: 仅 operator<, 去重 + 有序 */
    {
        STL_Set(struct Cmp) s; s->stl_set_init(struct Cmp)(ar);
        struct Cmp a = {4}, b = {1}, c = {4}, d = {9};   /* a 与 c 同 id=4 → 去重 */
        CHECK(s->stl_set_insert(struct Cmp)(a) == 1);
        CHECK(s->stl_set_insert(struct Cmp)(b) == 1);
        CHECK(s->stl_set_insert(struct Cmp)(c) == 0);    /* 重复 */
        CHECK(s->stl_set_insert(struct Cmp)(d) == 1);
        CHECK(s->stl_set_size(struct Cmp)() == 3);
        /* 有序: 1,4,9 */
        CHECK(s->stl_set_key_at(struct Cmp)(0)->id == 1);
        CHECK(s->stl_set_key_at(struct Cmp)(1)->id == 4);
        CHECK(s->stl_set_key_at(struct Cmp)(2)->id == 9);
        CHECK(s->stl_set_contains(struct Cmp)(a));
    }

    stl_arena_destroy(ar);
    return 0;
}