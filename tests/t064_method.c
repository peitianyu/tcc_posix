/* t064_method.c - 对象方法语法糖: a->f() → receiver 指针注入
 *
 * 规则 (method-call.md):
 *   - 字段优先: 字段命中走标准 C 字段访问
 *   - '.' 仅字段访问, 不触发方法糖; '->' 字段缺失且下一 token 为 '(' → 方法糖
 *   - 方法函数第一个形参必须是 receiver 的指针 A*
 *   - 同名全局函数绑定, 首参类型核对, 无拼接
 */
#include <stdio.h>

struct Point { int x, y; };

static int Point_len2(struct Point *p)  /* 首参 A* → a->Point_len2() */
{ return p->x * p->x + p->y * p->y; }

static void Point_show(struct Point *p) /* 首参 A* → pa->Point_show() */
{ printf("show(%d,%d)\n", p->x, p->y); }

static struct Point Point_add(struct Point *a, struct Point b)
{ struct Point r = { a->x + b.x, a->y + b.y }; return r; }  /* 值返回 struct */

int main(void)
{
    struct Point a = { 3, 4 };
    struct Point *pa = &a;
    int rc = 0;

    /* '->' 值接收器 (自动取址): a->Point_len2() ≡ Point_len2(&a) */
    if (a->Point_len2() != 25) { printf("FAIL: auto-address method\n"); rc = 1; }

    /* '->' 指针接收器: pa->Point_show() ≡ Point_show(pa) */
    pa->Point_show();

    /* 字段优先: a.x 走字段访问 ('.' 不做方法糖) */
    if (a.x != 3 || a.y != 4) { printf("FAIL: field access\n"); rc = 1; }

    /* 值返回 struct 的方法 (首参仍为 A*) */
    struct Point b = { 10, 20 };
    struct Point c = a->Point_add(b);
    if (c.x != 13 || c.y != 24) { printf("FAIL: struct-return method\n"); rc = 1; }

    if (rc == 0) printf("PASS t064_method\n");
    return rc;
}