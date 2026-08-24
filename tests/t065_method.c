/* t065_method.c - 对象方法语法糖回归: '->' 指针注入 (语义固定) */
#include <stdio.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { failures++; \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

struct Point { int x, y; };

static void point_set(struct Point *p, int x, int y) { p->x = x; p->y = y; }
static int  point_sum(struct Point *p) { return p->x + p->y; }

int main(void)
{
    struct Point pt = {0, 0};

    /* '->' 指针注入 (struct 值接收器): 自动 &pt, 可改字段 */
    pt->point_set(3, 4);
    pt->point_set(7, 9);
    CHECK(pt.x == 7 && pt.y == 9);

    /* '->' 指针注入 (struct 值接收器): 方法返回经指针读取 */
    pt->point_set(1, 2);
    int s = pt->point_sum();
    CHECK(s == 3);

    /* '->' 且 receiver 本身是指针: 直接传指针, 不再二次取址 */
    struct Point *pp = &pt;
    pp->point_set(5, 6);
    CHECK(pt.x == 5 && pt.y == 6);

    /* 字段优先: 正常 '.' 字段访问不受影响 */
    pt.x = 100;
    CHECK(pt.x == 100);

    printf(failures ? "FAILED\n" : "OK\n");
    return failures;
}