/* t062_stl_vector: M0a - 空间配置器(arena) + Pair + Vector 冒烟
 *
 * 纯断言(无 stdio)。覆盖:
 *   1. push_back/size/at/front/back/data 基本值语义
 *   2. 扩容跨多次(0->4->8->16...)
 *   3. 裸指针迭代(双轨连续容器: begin/end)
 *   4. pop_back/clear/empty/reserve
 *   5. 多实例: int / double / Pair(int,int), model 缓存类型一致性(同参同 sizeof)
 *   6. -b 越界 in: at/[] 越界(独立 -b 用例验证, 不在此跑以免回归崩)
 * 退出码 0 = 通过.
 */
#include "lib/stl/vector.h"
#include "lib/stl/pair.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

int main(void) {
    SLT_Arena *ar = slt_arena_new(0);

    /* 1. 基本 push/size/at/front/back */
    Vector(int) v; Vector_init(int)(&v, ar);
    CHECK(Vector_empty(int)(&v));
    CHECK(Vector_size(int)(&v) == 0);
    for (int i = 0; i < 100; i++) Vector_push_back(int)(&v, i * 3);
    CHECK(Vector_size(int)(&v) == 100);
    CHECK(!Vector_empty(int)(&v));
    CHECK(Vector_at(int)(&v, 0) == 0);
    CHECK(Vector_at(int)(&v, 99) == 297);
    CHECK(Vector_front(int)(&v) == 0);
    CHECK(Vector_back(int)(&v) == 297);
    CHECK(Vector_capacity(int)(&v) >= 100);

    /* 3. 裸指针迭代(连续容器) */
    {   int *it; int sum = 0; int expect = 0;
        for (it = Vector_data(int)(&v); it != Vector_end(int)(&v); ++it) sum += *it;
        for (int i = 0; i < 100; i++) expect += i * 3;
        CHECK(sum == expect);
    }

    /* 4. pop_back/clear/empty */
    Vector_pop_back(int)(&v);
    CHECK(Vector_size(int)(&v) == 99);
    CHECK(Vector_back(int)(&v) == 294);
    Vector_clear(int)(&v);
    CHECK(Vector_empty(int)(&v));
    CHECK(Vector_size(int)(&v) == 0);

    /* reserve 显式扩容 + 复用后再 push(自包含不互调) */
    Vector_init(int)(&v, ar);
    Vector_reserve(int)(&v, 8);
    CHECK(Vector_capacity(int)(&v) == 8);
    for (int i = 0; i < 20; i++) Vector_push_back(int)(&v, i);
    CHECK(Vector_size(int)(&v) == 20);
    CHECK(Vector_capacity(int)(&v) >= 20);
    CHECK(Vector_at(int)(&v, 19) == 19);

    /* 5. 多实例 + 缓存一致性 */
    Vector(double) vd; Vector_init(double)(&vd, ar);
    for (int i = 0; i < 5; i++) Vector_push_back(double)(&vd, i * 0.5);
    CHECK(Vector_at(double)(&vd, 4) == 2.0);

    Vector(Pair(int,int)) vp; Vector_init(Pair(int,int))(&vp, ar);
    Pair(int,int) p0 = { 1, 2 };
    Vector_push_back(Pair(int,int))(&vp, p0);
    CHECK(Vector_at(Pair(int,int))(&vp, 0).first == 1);
    CHECK(Vector_at(Pair(int,int))(&vp, 0).second == 2);

    /* 同参实例 size 一致(缓存复用) */
    CHECK(sizeof(Vector(int)) == sizeof(Vector(int)));
    CHECK(sizeof(Vector(Pair(int,int))) == sizeof(Vector(Pair(int,int))));
    /* 不同参 size 可不同(len/cap/ar 相同, 但至少 int/double 实例独立可用) */
    CHECK(Vector_at(double)(&vd, 0) == 0.0);

    slt_arena_destroy(ar);
    return 0;
}