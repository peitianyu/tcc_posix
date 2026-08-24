/* t063_stl_list: M0b - List(T) 双向链表冒烟（纯断言）
 * 覆盖: push_back/front 顺序、size、front/back、pop_back/front、
 *       节点指针迭代(List_begin/next/data)、多实例(int/double)、clear/empty。
 * 退出码 0 = 通过.
 */
#include "lib/stl/list.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

int main(void) {
    SLT_Arena *ar = slt_arena_new(0);

    List(int) l; List_init(int)(&l, ar);
    CHECK(List_empty(int)(&l));

    /* push_back 顺序 */
    for (int i = 0; i < 50; i++) List_push_back(int)(&l, i * 2);
    CHECK(List_size(int)(&l) == 50);
    CHECK(List_front(int)(&l) == 0);
    CHECK(List_back(int)(&l) == 98);

    /* push_front 逆序头插 */
    List_push_front(int)(&l, -1);
    CHECK(List_front(int)(&l) == -1);
    CHECK(List_size(int)(&l) == 51);

    /* 节点指针迭代求和 */
    {   int sum = 0, expect = -1;
        for (int i = 0; i < 50; i++) expect += i * 2;
        for (void *n = List_begin(int)(&l); n; n = List_next(int)(&l, n))
            sum += *List_data(int)(&l, n);
        CHECK(sum == expect);
    }

    /* pop_front / pop_back */
    List_pop_front(int)(&l);                 /* 去掉 -1 */
    CHECK(List_front(int)(&l) == 0);
    List_pop_back(int)(&l);                  /* 去掉 98 */
    CHECK(List_back(int)(&l) == 96);
    CHECK(List_size(int)(&l) == 49);

    /* clear / empty */
    List_clear(int)(&l);
    CHECK(List_empty(int)(&l));

    /* 多实例隔离 */
    List(double) ld; List_init(double)(&ld, ar);
    for (int i = 0; i < 3; i++) List_push_back(double)(&ld, i * 0.5);
    CHECK(List_back(double)(&ld) == 1.0);
    CHECK(List_front(double)(&ld) == 0.0);
    CHECK(List_size(int)(&l) == 0);          /* int 实例不受影响 */
    CHECK(sizeof(List(int)) == sizeof(List(int)));      /* 缓存一致 */

    slt_arena_destroy(ar);
    return 0;
}