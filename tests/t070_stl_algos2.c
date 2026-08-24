/* t070_stl_algos2: M1 - 快排 / 二分查找 / 下界 (纯断言)
 * 覆盖: stl_qsort(int) 原地排序(含重复)、struct 键快排(operator<)、
 *       stl_lower_bound / stl_binary_search 于有序数组(找到/未找到/边界)。
 * 退出码 0 = 通过.
 */
#include "lib/stl/algorithm.h"

#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)

static int sorted(const int *a, int n) {
    for (int i = 1; i < n; i++) if (a[i - 1] > a[i]) return 0;
    return 1;
}

struct Pt { int x; };
int operator<(struct Pt a, struct Pt b) { return a.x < b.x; }

int main(void) {
    /* 1. 快排 int(含负/重复) */
    {
        int a[12] = { 3, -1, 7, 2, 7, 5, 0, -4, 9, 1, 8, 6 };
        stl_qsort(int)(a, 0, 11);
        CHECK(sorted(a, 12));
        CHECK(a[0] == -4 && a[1] == -1 && a[3] == 1 && a[5] == 3);
        CHECK(a[8] == 7 && a[9] == 7 && a[11] == 9);   /* 重复 7 在 8/9 */
    }

    /* 2. 快排 struct(operator<) */
    {
        struct Pt p[6] = { {5},{1},{4},{2},{3},{0} };
        stl_qsort(struct Pt)(p, 0, 5);
        for (int i = 0; i < 6; i++) CHECK(p[i].x == i);
    }

    /* 3. 二分查找 / 下界(有序) */
    {
        int a[10]; for (int i = 0; i < 10; i++) a[i] = i * 2;   /* 0,2,..,18 */
        int *hit = stl_binary_search(int)(a, 10, 14);
        CHECK(hit != a + 10 && *hit == 14);
        CHECK(stl_binary_search(int)(a, 10, 15) == a + 10);     /* 不存在 */
        CHECK(stl_binary_search(int)(a, 10, 0) == a);
        CHECK(stl_binary_search(int)(a, 10, 18) == a + 9);

        /* lower_bound: 首个 >= key */
        int *lb = stl_lower_bound(int)(a, 10, 15);              /* 16 处未存在, lb=16 */
        CHECK(lb == a + 8 && *lb == 16);
        CHECK(stl_lower_bound(int)(a, 10, 18) == a + 9);
        CHECK(stl_lower_bound(int)(a, 10, 0) == a);
        CHECK(stl_lower_bound(int)(a, 10, 100) == a + 10);      /* 全小于 → e */
    }

    return 0;
}