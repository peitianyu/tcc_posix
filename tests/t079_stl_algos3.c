/* t079_stl_algos3: §8 M1 补全 - stl_remove / stl_unique / stl_accumulate (纯断言)
 * 覆盖: remove(移除多个/全部/无)、unique(有序去重/单元素/空)、
 *       accumulate(int 求和/struct operator+ 累加)。
 * 退出码 0 = 通过.
 */
#include "lib/stl/algorithm.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

struct Pt { int x; };
struct Pt operator+ (struct Pt a, struct Pt b) { struct Pt r = { a.x + b.x }; return r; }

int main(void)
{
    /* 1. stl_remove: 移除多个目标, 保序压缩 */
    {
        int a[8] = { 1, 2, 3, 2, 4, 2, 5, 6 };
        int *e = stl_remove(int)(a, a + 8, 2);
        CHECK(e == a + 5);                              /* 移除 3 个 2 */
        CHECK(a[0] == 1 && a[1] == 3 && a[2] == 4 && a[3] == 5 && a[4] == 6);
    }

    /* 2. stl_remove: 全部移除 / 无命中 */
    {
        int a[4] = { 7, 7, 7, 7 };
        CHECK(stl_remove(int)(a, a + 4, 7) == a);       /* 全移 → 空 */
        int b[4] = { 1, 2, 3, 4 };
        CHECK(stl_remove(int)(b, b + 4, 9) == b + 4);   /* 无命中 → 原样 */
    }

    /* 3. stl_unique: 有序去重 */
    {
        int a[10] = { 1, 1, 2, 3, 3, 3, 4, 5, 5, 6 };
        int *e = stl_unique(int)(a, a + 10);
        CHECK(e == a + 6);
        for (int i = 0; i < 6; i++) CHECK(a[i] == i + 1);
    }

    /* 4. stl_unique: 单元素 / 空 / 全同 */
    {
        int a[1] = { 9 };
        CHECK(stl_unique(int)(a, a + 1) == a + 1);
        CHECK(stl_unique(int)(a, a) == a);              /* 空区间 */
        int b[5] = { 4, 4, 4, 4, 4 };
        CHECK(stl_unique(int)(b, b + 5) == b + 1);
    }

    /* 5. stl_accumulate: int 求和 */
    {
        int a[6] = { 1, 2, 3, 4, 5, 6 };
        CHECK(stl_accumulate(int)(a, a + 6, 0) == 21);
        CHECK(stl_accumulate(int)(a, a + 6, 100) == 121);
        CHECK(stl_accumulate(int)(a, a, 7) == 7);       /* 空区间 → init */
    }

    /* 6. stl_accumulate: struct (operator+) */
    {
        struct Pt p[4] = { {1},{2},{3},{4} };
        struct Pt init = { 0 };
        struct Pt s = stl_accumulate(struct Pt)(p, p + 4, init);
        CHECK(s.x == 10);
    }

    /* 7. 组合: remove + unique 链式(先排序保证 unique 语义) */
    {
        int a[10] = { 3, 1, 2, 3, 1, 3, 2, 4, 1, 3 };
        stl_qsort(int)(a, 0, 9);
        int *u = stl_unique(int)(a, a + 10);            /* 排序后 [1,1,1,2,2,3,3,3,3,4] */
        int *r = stl_remove(int)(a, u, 3);              /* 去 3 → [1,2,4] */
        CHECK(r == a + 3);
        CHECK(a[0] == 1 && a[1] == 2 && a[2] == 4);
    }

    return 0;
}
