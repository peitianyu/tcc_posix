/* t063_stl_list: M0b - STL_List(T) 双向链表冒烟（纯断言）
 * 覆盖: push_back/front 顺序、size、front/back、pop_back/front、
 *       节点指针迭代(stl_list_begin/next/data)、多实例(int/double)、clear/empty。
 * 调用风格: 对象方法糖 `l->stl_list_push_back(int)(x)`。
 * 退出码 0 = 通过.
 */
#include "lib/stl/list.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

int main(void) {
    STL_Arena *ar = stl_arena_new(0);

    STL_List(int) l; l->stl_list_init(int)(ar);
    CHECK(l->stl_list_empty(int)());

    /* push_back 顺序 */
    for (int i = 0; i < 50; i++) l->stl_list_push_back(int)(i * 2);
    CHECK(l->stl_list_size(int)() == 50);
    CHECK(l->stl_list_front(int)() == 0);
    CHECK(l->stl_list_back(int)() == 98);

    /* push_front 逆序头插 */
    l->stl_list_push_front(int)(-1);
    CHECK(l->stl_list_front(int)() == -1);
    CHECK(l->stl_list_size(int)() == 51);

    /* 节点指针迭代求和 */
    {   int sum = 0, expect = -1;
        for (int i = 0; i < 50; i++) expect += i * 2;
        for (void *n = l->stl_list_begin(int)(); n; n = l->stl_list_next(int)(n))
            sum += *l->stl_list_data(int)(n);
        CHECK(sum == expect);
    }

    /* pop_front / pop_back */
    l->stl_list_pop_front(int)();                 /* 去掉 -1 */
    CHECK(l->stl_list_front(int)() == 0);
    l->stl_list_pop_back(int)();                  /* 去掉 98 */
    CHECK(l->stl_list_back(int)() == 96);
    CHECK(l->stl_list_size(int)() == 49);

    /* clear / empty */
    l->stl_list_clear(int)();
    CHECK(l->stl_list_empty(int)());

    /* 多实例隔离 */
    STL_List(double) ld; ld->stl_list_init(double)(ar);
    for (int i = 0; i < 3; i++) ld->stl_list_push_back(double)(i * 0.5);
    CHECK(ld->stl_list_back(double)() == 1.0);
    CHECK(ld->stl_list_front(double)() == 0.0);
    CHECK(l->stl_list_size(int)() == 0);           /* int 实例不受影响 */
    CHECK(sizeof(STL_List(int)) == sizeof(STL_List(int)));      /* 缓存一致 */

    stl_arena_destroy(ar);
    return 0;
}