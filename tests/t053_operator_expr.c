/* t053_operator_expr.c — 运算符重载脱糖: 嵌套 / 混合优先级 / 标量不误改
 *
 * 验证 operator 脱糖的类型向上传播:
 *   h = (a + b) * b     →  operator*(operator_add(a, b), b)       (嵌套完全括号)
 *   i = a + b * b - a   →  operator_sub(operator_add(a, operator_mul(b,b)), a) (混合优先级)
 *   j = (a + b) * (a - b) → operator_mul(operator_add(a,b), operator_sub(a,b))
 * 标量表达式 (含 struct 字段成员访问) 必须原样保留, 不得改写为 operator 调用.
 * 退出码 0 = 通过. 构建: bin/tcc.exe tests/t053_operator_expr.c -o t053_operator_expr.exe
 */
#include <stdio.h>

struct Vec3 { float x, y, z; };

struct Vec3 operator+ (struct Vec3 a, struct Vec3 b) {
    struct Vec3 r = { a.x + b.x, a.y + b.y, a.z + b.z };
    return r;
}
struct Vec3 operator* (struct Vec3 a, struct Vec3 b) {
    struct Vec3 r = { a.x * b.x, a.y * b.y, a.z * b.z };
    return r;
}
struct Vec3 operator- (struct Vec3 a, struct Vec3 b) {
    struct Vec3 r = { a.x - b.x, a.y - b.y, a.z - b.z };
    return r;
}

int main(void) {
    struct Vec3 a = { 1, 2, 3 };
    struct Vec3 b = { 4, 5, 6 };

    /* (a+b)*b : (a+b)={5,7,9}, *b={4,5,6} => {20,35,54} */
    struct Vec3 h = (a + b) * b;
    /* a + b*b - a : (b*b)={16,25,36}, a+={17,27,39}, -a => {16,25,36} */
    struct Vec3 i = a + b * b - a;
    /* (a+b)*(a-b) : a-b={-3,-3,-3}, a+b={5,7,9} => {-15,-21,-27} */
    struct Vec3 j = (a + b) * (a - b);

    if (h.x != 20 || h.y != 35 || h.z != 54) { puts("FAIL: (a+b)*b"); return 1; }
    if (i.x != 16 || i.y != 25 || i.z != 36) { puts("FAIL: a+b*b-a"); return 1; }
    if (j.x != -15 || j.y != -21 || j.z != -27) { puts("FAIL: (a+b)*(a-b)"); return 1; }

    /* 标量字段表达式必须原样保留 (不可改写为 operator 调用) */
    float t = a.x * a.x + b.y;      /* 1 + 5 = 6 */
    struct Vec3 c = a + b;          /* 基础改写仍生效: {5,7,9} */
    if (t != 6)                                     { puts("FAIL: scalar field expr"); return 1; }
    if (c.x != 5 || c.y != 7 || c.z != 9)           { puts("FAIL: a+b basic"); return 1; }

    puts("PASS: t053_operator_expr");
    return 0;
}