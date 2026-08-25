/* t054_operator_return.c — operator 脱糖: return 表达式改写
 *
 * 验证 return 表达式位置的 operator 改写 (泛化自顶层赋值右值):
 *   return a + b;      → return operator_add(a, b);
 *   return (a+b)*b;    → return operator_mul(operator_add(a,b), b);
 * 函数形参 (struct Vec3 a, struct Vec3 b) 也须登记为 operator 类型变量.
 * 退出码 0 = 通过. 构建: bin/tcc.exe tests/t054_operator_return.c -o t054.exe
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

/* return a+b : 形参 a,b 都是 operator 类型 */
static struct Vec3 add(struct Vec3 a, struct Vec3 b) {
    return a + b;
}

/* return 混合优先级 + 局部 operator 变量 */
static struct Vec3 mix(struct Vec3 a, struct Vec3 b) {
    struct Vec3 c = { 2, 2, 2 };
    return a + b * c;          /* operator_add(a, operator_mul(b, c)) */
}

/* return (a+b)*b : 嵌套括号 */
static struct Vec3 paren(struct Vec3 a, struct Vec3 b) {
    return (a + b) * b;
}

int main(void) {
    struct Vec3 a = { 1, 2, 3 };
    struct Vec3 b = { 4, 5, 6 };
    struct Vec3 c = { 2, 2, 2 };
    (void)c;                     /* 保持声明的 operator 类型变量登记, 避免 -Wunused */

    struct Vec3 r1 = add(a, b);            /* {5,7,9} */
    struct Vec3 r2 = mix(a, b);            /* b*c={8,10,12}, a+ = {9,12,15} */
    struct Vec3 r3 = paren(a, b);          /* (a+b)*b={20,35,54} */

    /* 标量仍不被改写 */
    float s = a.x + b.y;                   /* 1 + 5 = 6 */
    if (s != 6)                          { puts("FAIL: scalar");    return 8; }

    if (r1.x != 5 || r1.y != 7 || r1.z != 9)       { puts("FAIL: add a+b");       return 1; }
    if (r2.x != 9 || r2.y != 12 || r2.z != 15)     { puts("FAIL: mix a+b*c");     return 2; }
    if (r3.x != 20 || r3.y != 35 || r3.z != 54)    { puts("FAIL: paren (a+b)*b"); return 3; }

    puts("PASS: t054_operator_return");
    return 0;
}