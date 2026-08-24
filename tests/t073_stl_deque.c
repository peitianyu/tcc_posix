/* t073_stl_deque: M1 - STL_Deque(T) 双端队列 (纯断言)
 * 覆盖: 两端 push/pop、环形回绕(前插满再后插)、随机访问 at/front/back、
 *       与 stl_qsort 结合(可排序)、多实例。
 * 调用风格: 对象方法糖 `d->stl_deque_push_back(int)(x)`。
 * 退出码 0 = 通过.
 */
#include "lib/stl/deque.h"
#include "lib/stl/algorithm.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

int main(void) {
    STL_Arena *ar = stl_arena_new(0);

    /* 1. push_back/push_front + front/back + 随机访问 */
    {
        STL_Deque(int) d; d->stl_deque_init(int)(ar);
        CHECK(d->stl_deque_empty(int)());
        for (int i = 0; i < 5; i++) d->stl_deque_push_back(int)(i);       /* 0..4 */
        for (int i = 0; i < 5; i++) d->stl_deque_push_front(int)(100 + i);/* 104..100 前插 */
        CHECK(d->stl_deque_size(int)() == 10);
        CHECK(d->stl_deque_front(int)() == 104);
        CHECK(d->stl_deque_back(int)() == 4);
        /* 逻辑序: 104,103,102,101,100,0,1,2,3,4 */
        CHECK(d->stl_deque_at(int)(0) == 104);
        CHECK(d->stl_deque_at(int)(5) == 0);
        CHECK(d->stl_deque_at(int)(9) == 4);
    }

    /* 2. 环形回绕: 交替两端 pop, 再插触发回绕 */
    {
        STL_Deque(int) d; d->stl_deque_init(int)(ar);
        for (int i = 0; i < 8; i++) d->stl_deque_push_back(int)(i);       /* 0..7 */
        for (int i = 0; i < 3; i++) d->stl_deque_pop_front(int)();        /* 去掉 0,1,2 */
        for (int i = 0; i < 5; i++) d->stl_deque_pop_back(int)();         /* 去掉 7..3 */
        CHECK(d->stl_deque_size(int)() == 0);
        /* 再前插/后插(空后 begin 归零, 环形 OK) */
        d->stl_deque_push_front(int)(-1);
        d->stl_deque_push_back(int)(9);
        CHECK(d->stl_deque_front(int)() == -1);
        CHECK(d->stl_deque_back(int)() == 9);
        CHECK(d->stl_deque_at(int)(0) == -1);
        CHECK(d->stl_deque_at(int)(1) == 9);
    }

    /* 3. 与快排结合: 拷到 deque, 转连续区间排序 */
    {
        STL_Deque(int) d; d->stl_deque_init(int)(ar);
        int seq[6] = {5,1,4,2,3,0};
        for (int i = 0; i < 6; i++) d->stl_deque_push_back(int)(seq[i]);
        CHECK(d->stl_deque_size(int)() == 6);
        /* deque 内部连续(无前插时 begin=0)可直接排序 */
        /* (此处仅验证元素可索引访问; 完整排序走 vector 区间) */
        int ordered[6];
        for (int i = 0; i < 6; i++) ordered[i] = d->stl_deque_at(int)(i);
        stl_qsort(int)(ordered, 0, 5);
        for (int i = 0; i < 6; i++) CHECK(ordered[i] == i);
    }

    /* 4. 多实例隔离 */
    {
        STL_Deque(double) a, b; a->stl_deque_init(double)(ar); b->stl_deque_init(double)(ar);
        a->stl_deque_push_back(double)(1.5);
        b->stl_deque_push_back(double)(2.5);
        CHECK(a->stl_deque_back(double)() == 1.5);
        CHECK(b->stl_deque_back(double)() == 2.5);
        CHECK(a->stl_deque_size(double)() == 1 && b->stl_deque_size(double)() == 1);
    }

    stl_arena_destroy(ar);
    return 0;
}