/* t066_stl_algorithm: M0c - Stack/Queue 适配器 + 基础算法 (纯断言)
 * 覆盖:
 *   Stack: push/pop/top/size/empty, 跨扩容次序
 *   Queue: push_back/pop_front/front/back, 环形回绕次序 (pop 后继续 push)
 *   algorithm: slt_find/count/fill/reverse/for_each/sort/minmax (裸指针区间)
 *   用户值类型 operator_lt/eq 驱动 sort/find (编译期静态分派)
 * 退出码 0 = 通过.
 */
#include "lib/stl/stack.h"
#include "lib/stl/queue.h"
#include "lib/stl/algorithm.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

/* 用户值类型: 声明 operator< / operator== 供泛型算法默认谓词 */
struct Pt { int x, y; };
int operator_lt(struct Pt a, struct Pt b) { return a.x < b.x; }
int operator_eq(struct Pt a, struct Pt b) { return a.x == b.x && a.y == b.y; }

static void sum_cb(int *p) { *p += 100; }

int main(void) {
    SLT_Arena *ar = slt_arena_new(0);

    /* ---------------- Stack ---------------- */
    {
        Stack(int) s; Stack_init(int)(&s, ar);
        CHECK(Stack_empty(int)(&s));
        for (int i = 0; i < 40; i++) Stack_push(int)(&s, i * 3);
        CHECK(Stack_size(int)(&s) == 40);
        CHECK(Stack_top(int)(&s) == 117);       /* 39*3 */
        for (int i = 39; i >= 10; i--) { CHECK(Stack_top(int)(&s) == i * 3); Stack_pop(int)(&s); }
        CHECK(Stack_size(int)(&s) == 10);
    }

    /* ---------------- Queue (环形回绕) ---------------- */
    {
        Queue(int) q; Queue_init(int)(&q, ar);
        CHECK(Queue_empty(int)(&q));
        for (int i = 0; i < 12; i++) Queue_push_back(int)(&q, i + 1);  /* 1..12, cap=16 */
        CHECK(Queue_size(int)(&q) == 12);
        CHECK(Queue_front(int)(&q) == 1);
        CHECK(Queue_back(int)(&q) == 12);
        /* 弹 8 个(begin 前移), 再推 6 个触发环形回绕 */
        for (int k = 1; k <= 8; k++) CHECK(Queue_pop_front(int)(&q) == k);
        for (int i = 0; i < 6; i++)  Queue_push_back(int)(&q, 100 + i);
        CHECK(Queue_front(int)(&q) == 9);          /* 环形跨过物理末尾 */
        CHECK(Queue_back(int)(&q) == 105);
        for (int k = 9; k <= 12; k++) CHECK(Queue_pop_front(int)(&q) == k);
        for (int k = 0; k <= 5; k++)  CHECK(Queue_pop_front(int)(&q) == 100 + k);
        CHECK(Queue_empty(int)(&q));
        Queue_clear(int)(&q);
    }

    /* ---------------- 算法 (裸指针区间) ---------------- */
    {
        int a[10]; for (int i = 0; i < 10; i++) a[i] = (10 - i);  /* 10..1 */

        int *f = slt_find(int)(a, a + 10, 7);
        CHECK(f != a + 10 && *f == 7);
        CHECK(slt_find(int)(a, a + 10, 99) == a + 10);   /* 不存在 → e */
        CHECK(slt_count(int)(a, a + 10, 5) == 1);

        slt_fill(int)(a + 2, a + 5, 0);            /* 清零子区间 */
        CHECK(a[2] == 0 && a[4] == 0 && a[5] != 0);

        for (int i = 0; i < 10; i++) a[i] = i;
        slt_reverse(int)(a, a + 10);               /* 0..9 → 9..0 */
        CHECK(a[0] == 9 && a[9] == 0 && a[5] == 4);

        for (int i = 0; i < 10; i++) a[i] = i;
        slt_for_each(int)(a, a + 4, sum_cb);       /* 前 4 项 +=100 */
        CHECK(a[0] == 100 && a[3] == 103 && a[4] == 4);

        int r[8] = { 5, 3, 8, 1, 6, 2, 7, 4 };   /* 1..8 的无序排列 */
        slt_sort(int)(r, 8);
        for (int i = 0; i < 8; i++) CHECK(r[i] == i + 1);

        int mn, mx;
        slt_minmax(int)(r, 8, &mn, &mx);
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
        slt_sort(struct Pt)(pts, 5);               /* 默认 operator_lt(x) */
        for (int i = 0; i < 5; i++) CHECK(pts[i].x == i + 1);

        struct Pt key = { 1, 0 };
        CHECK(pts[0].x == 1);
        struct Pt *hit = slt_find(struct Pt)(pts, pts + 5, key);  /* operator_eq */
        CHECK(hit != pts + 5 && hit->x == 1);
    }

    slt_arena_destroy(ar);
    return 0;
}