/* t069_stl_map: M1 - STL_Map(K,V) 有序关联容器 (纯断言)
 * 覆盖: set/get/contains/size/erase/clear, 覆盖更新(同键改值)、插入扩容、
 *       多类型实例、**键有序**(乱序插入后按键升序迭代 stl_map_key_at)。
 * 调用风格: 对象方法糖 `m->stl_map_set(int,int)(k,v)`(仅指针注入, 无 &m 实参)。
 * 注: model 类型实参不支持指针类型(char* 键探针不可用, 见 docs/stl.md)。
 * 退出码 0 = 通过.
 */
#include "lib/stl/map.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

/* struct 键: 仅 operator<(无 operator==)——证明 map 只需"可排序"(等价由 < 推导) */
struct Cmp { int id; };
int operator< (struct Cmp a, struct Cmp b) { return a.id < b.id; }

int main(void) {
    STL_Arena *ar = stl_arena_new(0);

    /* 1. int-int: 插入/查找/覆盖/删除 */
    {
        STL_Map(int,int) m;
        m->stl_map_init(int,int)(ar);
        CHECK(m->stl_map_empty(int,int)());
        for (int i = 0; i < 50; i++) CHECK(m->stl_map_set(int,int)(i, i * 10) == 0);
        CHECK(m->stl_map_size(int,int)() == 50);
        for (int i = 0; i < 50; i++) {
            CHECK(m->stl_map_contains(int,int)(i));
            int *v = m->stl_map_get(int,int)(i);
            CHECK(v && *v == i * 10);
        }
        CHECK(!m->stl_map_contains(int,int)(99));
        CHECK(m->stl_map_get(int,int)(99) == 0);

        /* 覆盖更新(键数不变) */
        CHECK(m->stl_map_set(int,int)(7, 777) == 0);
        CHECK(m->stl_map_size(int,int)() == 50);
        { int *v = m->stl_map_get(int,int)(7); CHECK(v && *v == 777); }

        /* 删除 */
        CHECK(m->stl_map_erase(int,int)(10) == 1);
        CHECK(!m->stl_map_contains(int,int)(10));
        CHECK(m->stl_map_size(int,int)() == 49);
        CHECK(m->stl_map_erase(int,int)(10) == 0);   /* 已删 */

        m->stl_map_clear(int,int)();
        CHECK(m->stl_map_empty(int,int)());
    }

    /* 2. 乱序插入 → 键升序迭代 (有序语义) */
    {
        int seq[8] = { 5, 3, 8, 1, 7, 2, 6, 4 };
        STL_Map(int,int) m; m->stl_map_init(int,int)(ar);
        for (int i = 0; i < 8; i++) CHECK(m->stl_map_set(int,int)(seq[i], seq[i] * 100) == 0);
        CHECK(m->stl_map_size(int,int)() == 8);
        for (int i = 0; i < 8; i++) {              /* 键序 = 1..8 */
            CHECK(*m->stl_map_key_at(int,int)(i) == i + 1);
            CHECK(*m->stl_map_val_at(int,int)(i) == (i + 1) * 100);
        }
        /* 删除中间键后仍有序: 删 4 → 1,2,3,5,6,7,8 */
        m->stl_map_erase(int,int)(4);
        int keys[7] = {1,2,3,5,6,7,8};
        for (int i = 0; i < 7; i++) CHECK(*m->stl_map_key_at(int,int)(i) == keys[i]);
    }

    /* 3. struct 键: 仅 operator<(可排序契约), 有序 + 覆盖 + 删除 */
    {
        STL_Map(struct Cmp, int) m; m->stl_map_init(struct Cmp, int)(ar);
        struct Cmp c7 = {7}, c3 = {3}, c9 = {9};
        CHECK(m->stl_map_set(struct Cmp, int)(c7, 77) == 0);
        CHECK(m->stl_map_set(struct Cmp, int)(c3, 33) == 0);
        CHECK(m->stl_map_set(struct Cmp, int)(c9, 99) == 0);
        CHECK(m->stl_map_contains(struct Cmp, int)(c7));
        int *g = m->stl_map_get(struct Cmp, int)(c7);
        CHECK(g && *g == 77);
        /* 按 id 升序: 3,7,9 */
        CHECK(m->stl_map_key_at(struct Cmp, int)(0)->id == 3);
        CHECK(m->stl_map_key_at(struct Cmp, int)(1)->id == 7);
        CHECK(m->stl_map_key_at(struct Cmp, int)(2)->id == 9);
        struct Cmp c8 = {8};
        CHECK(!m->stl_map_contains(struct Cmp, int)(c8));
        /* 删除中间键后仍有序: 3,9 */
        CHECK(m->stl_map_erase(struct Cmp, int)(c7) == 1);
        CHECK(m->stl_map_size(struct Cmp, int)() == 2);
        CHECK(m->stl_map_key_at(struct Cmp, int)(0)->id == 3);
        CHECK(m->stl_map_key_at(struct Cmp, int)(1)->id == 9);
    }

    /* 4. getor: 未设置的键返回默认值(不改表) */
    {
        STL_Map(int,int) m; m->stl_map_init(int,int)(ar);
        CHECK(m->stl_map_set(int,int)(5, 55) == 0);
        CHECK(m->stl_map_getor(int,int)(5, -1) == 55);   /* 已设置 → 值 */
        CHECK(m->stl_map_getor(int,int)(9, -1) == -1);   /* 未设置 → 默认 */
        /* 默认查询不插入 */
        CHECK(!m->stl_map_contains(int,int)(9));
        CHECK(m->stl_map_size(int,int)() == 1);
    }

    /* 5. at(operator[] 语义): 缺键自动插入零值槽并可写入 */
    {
        STL_Map(int,int) m; m->stl_map_init(int,int)(ar);
        int *p = m->stl_map_at(int,int)(7);          /* 缺键 → 插入零值槽 */
        CHECK(p && *p == 0);                          /* 默认零值 */
        CHECK(m->stl_map_contains(int,int)(7));
        CHECK(m->stl_map_size(int,int)() == 1);
        *m->stl_map_at(int,int)(7) = 777;             /* 写回既有键 */
        CHECK(*m->stl_map_at(int,int)(7) == 777);
        *m->stl_map_at(int,int)(3) = 33;              /* 再插新键 */
        CHECK(*m->stl_map_at(int,int)(3) == 33);
        CHECK(m->stl_map_size(int,int)() == 2);
    }

    stl_arena_destroy(ar);
    return 0;
}