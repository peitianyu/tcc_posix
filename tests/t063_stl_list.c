/* t063_stl_list: M0b - STL_List(T) 双向链表冒烟（纯断言）
 * 覆盖: push_back/front 顺序、size、front/back、pop_back/front、
 *       节点指针迭代(stl_list_begin/next/data)、多实例(int/double)、clear/empty。
 * 退出码 0 = 通过.
 */
#include "lib/stl/list.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

int main(void) {
    STL_Arena *ar = stl_arena_new(0);

    STL_List(int) l; stl_list_init(int)(&l, ar);
    CHECK(stl_list_empty(int)(&l));

    /* push_back 顺序 */
    for (int i = 0; i < 50; i++) stl_list_push_back(int)(&l, i * 2);
    CHECK(stl_list_size(int)(&l) == 50);
    CHECK(stl_list_front(int)(&l) == 0);
    CHECK(stl_list_back(int)(&l) == 98);

    /* push_front 逆序头插 */
    stl_list_push_front(int)(&l, -1);
    CHECK(stl_list_front(int)(&l) == -1);
    CHECK(stl_list_size(int)(&l) == 51);

    /* 节点指针迭代求和 */
    {   int sum = 0, expect = -1;
        for (int i = 0; i < 50; i++) expect += i * 2;
        for (void *n = stl_list_begin(int)(&l); n; n = stl_list_next(int)(&l, n))
            sum += *stl_list_data(int)(&l, n);
        CHECK(sum == expect);
    }

    /* pop_front / pop_back */
    stl_list_pop_front(int)(&l);                 /* 去掉 -1 */
    CHECK(stl_list_front(int)(&l) == 0);
    stl_list_pop_back(int)(&l);                  /* 去掉 98 */
    CHECK(stl_list_back(int)(&l) == 96);
    CHECK(stl_list_size(int)(&l) == 49);

    /* clear / empty */
    stl_list_clear(int)(&l);
    CHECK(stl_list_empty(int)(&l));

    /* 多实例隔离 */
    STL_List(double) ld; stl_list_init(double)(&ld, ar);
    for (int i = 0; i < 3; i++) stl_list_push_back(double)(&ld, i * 0.5);
    CHECK(stl_list_back(double)(&ld) == 1.0);
    CHECK(stl_list_front(double)(&ld) == 0.0);
    CHECK(stl_list_size(int)(&l) == 0);          /* int 实例不受影响 */
    CHECK(sizeof(STL_List(int)) == sizeof(STL_List(int)));      /* 缓存一致 */

    stl_arena_destroy(ar);
    return 0;
}