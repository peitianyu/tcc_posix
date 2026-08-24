/* t072_stl_copy: M1 - 容器深拷贝 (纯断言)
 * 覆盖: Vector copy(独立数据区, 源/副本互不影响)、List copy(逐节点重建)、
 *       快排 stl_qsort 收尾(深拷贝出的数据排序)。
 * 调用风格: 对象方法糖.
 * 退出码 0 = 通过.
 */
#include "lib/stl/vector.h"
#include "lib/stl/list.h"
#include "lib/stl/algorithm.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

int main(void) {
    STL_Arena *ar = stl_arena_new(0);

    /* 1. Vector deep copy: 源/副本独立 */
    {
        STL_Vector(int) v; v->stl_vector_init(int)(ar);
        for (int i = 0; i < 10; i++) v->stl_vector_push_back(int)(i * 3);
        STL_Vector(int) c = v->stl_vector_copy(int)();
        CHECK(c->stl_vector_size(int)() == 10);
        for (int i = 0; i < 10; i++) CHECK(c->stl_vector_at(int)(i) == i * 3);
        /* 改源不改副本 */
        *v->stl_vector_data(int)() = 999;
        CHECK(c->stl_vector_at(int)(0) == 0);
        CHECK(v->stl_vector_at(int)(0) == 999);
    }

    /* 2. List deep copy: 逐节点重建 */
    {
        STL_List(int) l; l->stl_list_init(int)(ar);
        for (int i = 0; i < 8; i++) l->stl_list_push_back(int)(i);
        STL_List(int) c = l->stl_list_copy(int)();
        CHECK(c->stl_list_size(int)() == 8);
        int expect = 0;
        for (void *n = c->stl_list_begin(int)(); n; n = c->stl_list_next(int)(n))
            CHECK(*c->stl_list_data(int)(n) == expect++);
    }

    /* 3. 快排收尾: 深拷贝副本可独立排序, 源不受影响 */
    {
        STL_Vector(int) v; v->stl_vector_init(int)(ar);
        int seq[6] = {5,1,4,2,3,0};
        for (int i = 0; i < 6; i++) v->stl_vector_push_back(int)(seq[i]);
        STL_Vector(int) c = v->stl_vector_copy(int)();
        stl_qsort(int)(c->stl_vector_data(int)(), 0, 5);
        for (int i = 0; i < 6; i++) CHECK(c->stl_vector_at(int)(i) == i);
        CHECK(v->stl_vector_at(int)(0) == 5);   /* 源未变 */
    }

    stl_arena_destroy(ar);
    return 0;
}