/* t069_stl_map: M1 - STL_Map(K,V) 基础关联容器 (纯断言)
 * 覆盖: set/get/contains/size/erase/clear,
 *       覆盖更新(同键改值)、插入扩容、多类型实例(int 键 / struct 键 operator==)。
 * 注: model 类型实参不支持指针类型(char* 键已在探针验证不可用, 见 docs/stl.md)。
 * 退出码 0 = 通过.
 */
#include "lib/stl/map.h"
#include "lib/stl/vector.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

/* 自定义键类型: 用 operator== 判键 */
struct Key { int id; };
int operator_eq(struct Key a, struct Key b) { return a.id == b.id; }

int main(void) {
    STL_Arena *ar = stl_arena_new(0);

    /* 1. int-int: 插入/查找/覆盖/删除 */
    {
        STL_Map(int,int) m; stl_map_init(int,int)(&m, ar);
        CHECK(stl_map_empty(int,int)(&m));
        for (int i = 0; i < 50; i++) CHECK(stl_map_set(int,int)(&m, i, i * 10) == 0);
        CHECK(stl_map_size(int,int)(&m) == 50);
        for (int i = 0; i < 50; i++) {
            CHECK(stl_map_contains(int,int)(&m, i));
            int *v = stl_map_get(int,int)(&m, i);
            CHECK(v && *v == i * 10);
        }
        CHECK(!stl_map_contains(int,int)(&m, 99));
        CHECK(stl_map_get(int,int)(&m, 99) == 0);

        /* 覆盖更新 */
        CHECK(stl_map_set(int,int)(&m, 7, 777) == 0);
        CHECK(stl_map_size(int,int)(&m) == 50);
        { int *v = stl_map_get(int,int)(&m, 7); CHECK(v && *v == 777); }

        /* 删除 */
        CHECK(stl_map_erase(int,int)(&m, 10) == 1);
        CHECK(!stl_map_contains(int,int)(&m, 10));
        CHECK(stl_map_size(int,int)(&m) == 49);
        CHECK(stl_map_erase(int,int)(&m, 10) == 0);   /* 已删 */

        stl_map_clear(int,int)(&m);
        CHECK(stl_map_empty(int,int)(&m));
    }

    /* 2. int 键多实例隔离 */
    {
        STL_Map(int,int) a, b; stl_map_init(int,int)(&a, ar); stl_map_init(int,int)(&b, ar);
        stl_map_set(int,int)(&a, 1, 100);
        stl_map_set(int,int)(&b, 1, 200);
        CHECK(*stl_map_get(int,int)(&a, 1) == 100);
        CHECK(*stl_map_get(int,int)(&b, 1) == 200);
        CHECK(stl_map_size(int,int)(&a) == 1 && stl_map_size(int,int)(&b) == 1);
    }

    /* 3. struct 键: operator== 判键 (model 类型实参不支持嵌套泛型实例,
     *    故值类型用 int 而非 Vector 实例) */
    {
        STL_Map(struct Key, int) m; stl_map_init(struct Key, int)(&m, ar);
        struct Key k7 = {7};
        CHECK(stl_map_set(struct Key, int)(&m, k7, 77) == 0);
        CHECK(stl_map_contains(struct Key, int)(&m, k7));
        int *g = stl_map_get(struct Key, int)(&m, k7);
        CHECK(g && *g == 77);
        /* 键字段不同的新实例不命中 */
        struct Key k8 = {8};
        CHECK(!stl_map_contains(struct Key, int)(&m, k8));
    }

    stl_arena_destroy(ar);
    return 0;
}