/* t066_stl_algorithm: M0c - STL_Stack/STL_Queue 适配器 + 基础算法 (纯断言)
 * 覆盖:
 *   Stack: push/pop/top/size/empty, 跨扩容次序
 *   Queue: push_back/pop_front/front/back, 环形回绕次序 (pop 后继续 push)
 *   algorithm: stl_find/count/fill/reverse/for_each/sort/minmax (裸指针区间)
 *   用户值类型 operator< / operator== 驱动 sort/find (编译期静态分派)
 * 容器调用风格: 对象方法糖 `s->stl_stack_push(int)(x)`; 算法为自由泛型函数(显式调用)。
 * 退出码 0 = 通过.
 */
#include "lib/stl/stack.h"
#include "lib/stl/queue.h"
#include "lib/stl/algorithm.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

/* 用户值类型: operator< / operator== 供泛型算法默认谓词 */
struct Pt { int x, y; };
int operator< (struct Pt a, struct Pt b) { return a.x < b.x; }
int operator==(struct Pt a, struct Pt b) { return a.x == b.x && a.y == b.y; }

static void sum_cb(int *p) { *p += 100; }

int main(void) {
    STL_Arena *ar = stl_arena_new(0);

    /* ---------------- Stack ---------------- */
    {
        STL_Stack(int) s; s->stl_stack_init(int)(ar);
        CHECK(s->stl_stack_empty(int)());
        for (int i = 0; i < 40; i++) s->stl_stack_push(int)(i * 3);
        CHECK(s->stl_stack_size(int)() == 40);
        CHECK(s->stl_stack_top(int)() == 117);       /* 39*3 */
        for (int i = 39; i >= 10; i--) { CHECK(s->stl_stack_top(int)() == i * 3); s->stl_stack_pop(int)(); }
        CHECK(s->stl_stack_size(int)() == 10);
    }

    /* ---------------- Queue (环形回绕) ---------------- */
    {
        STL_Queue(int) q; q->stl_queue_init(int)(ar);
        CHECK(q->stl_queue_empty(int)());
        for (int i = 0; i < 12; i++) q->stl_queue_push_back(int)(i + 1);  /* 1..12, cap=16 */
        CHECK(q->stl_queue_size(int)() == 12);
        CHECK(q->stl_queue_front(int)() == 1);
        CHECK(q->stl_queue_back(int)() == 12);
        /* 弹 8 个(begin 前移), 再推 6 个触发环形回绕 */
        for (int k = 1; k <= 8; k++) CHECK(q->stl_queue_pop_front(int)() == k);
        for (int i = 0; i < 6; i++)  q->stl_queue_push_back(int)(100 + i);
        CHECK(q->stl_queue_front(int)() == 9);       /* 环形跨过物理末尾 */
        CHECK(q->stl_queue_back(int)() == 105);
        for (int k = 9; k <= 12; k++) CHECK(q->stl_queue_pop_front(int)() == k);
        for (int k = 0; k <= 5; k++)  CHECK(q->stl_queue_pop_front(int)() == 100 + k);
        CHECK(q->stl_queue_empty(int)());
        q->stl_queue_clear(int)();
    }

    /* ---------------- 算法 (裸指针区间) ---------------- */
    {
        int a[10]; for (int i = 0; i < 10; i++) a[i] = (10 - i);  /* 10..1 */

        int *f = stl_find(int)(a, a + 10, 7);
        CHECK(f != a + 10 && *f == 7);
        CHECK(stl_find(int)(a, a + 10, 99) == a + 10);   /* 不存在 → e */
        CHECK(stl_count(int)(a, a + 10, 5) == 1);

        stl_fill(int)(a + 2, a + 5, 0);            /* 清零子区间 */
        CHECK(a[2] == 0 && a[4] == 0 && a[5] != 0);

        for (int i = 0; i < 10; i++) a[i] = i;
        stl_reverse(int)(a, a + 10);               /* 0..9 → 9..0 */
        CHECK(a[0] == 9 && a[9] == 0 && a[5] == 4);

        for (int i = 0; i < 10; i++) a[i] = i;
        stl_for_each(int)(a, a + 4, sum_cb);       /* 前 4 项 +=100 */
        CHECK(a[0] == 100 && a[3] == 103 && a[4] == 4);

        int r[8] = { 5, 3, 8, 1, 6, 2, 7, 4 };   /* 1..8 的无序排列 */
        stl_sort(int)(r, 8);
        for (int i = 0; i < 8; i++) CHECK(r[i] == i + 1);

        int mn, mx;
        stl_minmax(int)(r, 8, &mn, &mx);
        CHECK(mn == 1 && mx == 8);
    }

    /* ---------------- 用户值类型 operator 驱动 ---------------- */
    {
        struct Pt pts[5];
        pts[0].x = 4; pts[0].y = 0;
        pts[1].x = 2; pts[1].y = 0;
        pts[2].x = 5; pts[2].y = 0;
        pts[3].x = 1; pts[3].y = 0;
        pts[4].x = 3; pts[4].y = 0;
        stl_sort(struct Pt)(pts, 5);               /* 默认 operator< */
        for (int i = 0; i < 5; i++) CHECK(pts[i].x == i + 1);

        struct Pt key = { 1, 0 };
        CHECK(pts[0].x == 1);
        struct Pt *hit = stl_find(struct Pt)(pts, pts + 5, key);  /* operator== */
        CHECK(hit != pts + 5 && hit->x == 1);
    }

    stl_arena_destroy(ar);
    return 0;
}