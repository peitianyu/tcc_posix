/* 测试: 对象方法 (C++ 式: struct 体内函数定义, 隐式 self, . 与 -> 调用) */
#include <stdio.h>
struct Point {
    int x, y;
    int sum(void) { return x + y; }
    void set(int a, int b) { x = a; y = b; }
    int mul(int k) { return x * k; }
    int combo(int k) { return self->sum() + self->mul(k); }
};
typedef struct { int v; int get(void) { return v; } } Box;
struct Fact { int f(int n) { return n <= 1 ? 1 : self->f(n - 1) * n; } };

int main(void) {
    struct Point p = { 3, 4 };
    /* 1. 基本调用 + 返回值 */
    if (p.sum() != 7) return 1;
    /* 2. self 修改 (方法体内字段赋值) */
    p.set(10, 20);
    if (p.x != 10 || p.y != 20) return 2;
    /* 3. -> 调用形式 */
    struct Point *pp = &p;
    if (pp->sum() != 30) return 3;
    /* 4. 参数 + 字段引用 */
    if (p.mul(3) != 30) return 4;
    /* 5. 方法互调 (self->) */
    if (p.combo(2) != 50) return 5;
    /* 6. 匿名 struct + typedef */
    Box b = { 9 };
    if (b.get() != 9) return 6;
    /* 7. 递归方法 */
    struct Fact f;
    if (f.f(5) != 120) return 7;
    /* 8. 字段与方法同名: 字段优先 */
    struct S { int sum; };
    struct S s = { 5 };
    if (s.sum != 5) return 8;
    printf("method ok\n");
    return 0;
}
