/* t062_stl_vector: M0a - 空间配置器(arena) + STL_Pair + STL_Vector 冒烟
 *
 * 纯断言(无 stdio)。覆盖:
 *   1. push_back/size/at/front/back/data 基本值语义
 *   2. 扩容跨多次(0->4->8->16...)
 *   3. 裸指针迭代(双轨连续容器: data/end)
 *   4. pop_back/clear/empty/reserve
 *   5. 多实例: int / double / STL_Pair(int,int), model 缓存类型一致性(同参同 sizeof)
 * 调用风格: 对象方法糖 `v->stl_vector_push_back(int)(x)`(仅指针注入)。
 * 退出码 0 = 通过.
 */
#include "lib/stl/vector.h"
#include "lib/stl/pair.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

int main(void) {
    STL_Arena *ar = stl_arena_new(0);

    /* 1. 基本 push/size/at/front/back */
    STL_Vector(int) v; v->stl_vector_init(int)(ar);
    CHECK(v->stl_vector_empty(int)());
    CHECK(v->stl_vector_size(int)() == 0);
    for (int i = 0; i < 100; i++) v->stl_vector_push_back(int)(i * 3);
    CHECK(v->stl_vector_size(int)() == 100);
    CHECK(!v->stl_vector_empty(int)());
    CHECK(v->stl_vector_at(int)(0) == 0);
    CHECK(v->stl_vector_at(int)(99) == 297);
    CHECK(v->stl_vector_front(int)() == 0);
    CHECK(v->stl_vector_back(int)() == 297);
    CHECK(v->stl_vector_capacity(int)() >= 100);

    /* 3. 裸指针迭代(连续容器) */
    {   int *it; int sum = 0; int expect = 0;
        for (it = v->stl_vector_data(int)(); it != v->stl_vector_end(int)(); ++it) sum += *it;
        for (int i = 0; i < 100; i++) expect += i * 3;
        CHECK(sum == expect);
    }

    /* 4. pop_back/clear/empty */
    v->stl_vector_pop_back(int)();
    CHECK(v->stl_vector_size(int)() == 99);
    CHECK(v->stl_vector_back(int)() == 294);
    v->stl_vector_clear(int)();
    CHECK(v->stl_vector_empty(int)());
    CHECK(v->stl_vector_size(int)() == 0);

    /* reserve 显式扩容 + 复用后再 push(自包含不互调) */
    v->stl_vector_init(int)(ar);
    v->stl_vector_reserve(int)(8);
    CHECK(v->stl_vector_capacity(int)() == 8);
    for (int i = 0; i < 20; i++) v->stl_vector_push_back(int)(i);
    CHECK(v->stl_vector_size(int)() == 20);
    CHECK(v->stl_vector_capacity(int)() >= 20);
    CHECK(v->stl_vector_at(int)(19) == 19);

    /* 5. 多实例 + 缓存一致性 */
    STL_Vector(double) vd; vd->stl_vector_init(double)(ar);
    for (int i = 0; i < 5; i++) vd->stl_vector_push_back(double)(i * 0.5);
    CHECK(vd->stl_vector_at(double)(4) == 2.0);

    STL_Vector(STL_Pair(int,int)) vp; vp->stl_vector_init(STL_Pair(int,int))(ar);
    STL_Pair(int,int) p0 = { 1, 2 };
    vp->stl_vector_push_back(STL_Pair(int,int))(p0);
    CHECK(vp->stl_vector_at(STL_Pair(int,int))(0).first == 1);
    CHECK(vp->stl_vector_at(STL_Pair(int,int))(0).second == 2);

    /* 同参实例 size 一致(缓存复用) */
    CHECK(sizeof(STL_Vector(int)) == sizeof(STL_Vector(int)));
    CHECK(sizeof(STL_Vector(STL_Pair(int,int))) == sizeof(STL_Vector(STL_Pair(int,int))));
    /* 不同参 size 可不同(len/cap/ar 相同, 但至少 int/double 实例独立可用) */
    CHECK(vd->stl_vector_at(double)(0) == 0.0);

    stl_arena_destroy(ar);
    return 0;
}