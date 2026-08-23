/* t050_operator.c — C 运算符重载 (operator 语法, 独立语言扩展)
 *
 * 验证附录 B: struct 二元算术运算改写为对 operator<op> 的函数调用.
 *   c = a + b   →  operator+(a, b)
 *   d = a * b   →  operator*(a, b)
 *   e = a + b*b →  a + (b*b), 优先级由语法树天然保持
 * 断言逐项结果; 退出码 0 = 通过.
 * 构建: bin/tcc.exe tests/t050_operator.c -o t050_operator.exe
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

int main(void) {
    struct Vec3 a = { 1, 2, 3 };
    struct Vec3 b = { 4, 5, 6 };

    struct Vec3 c = a + b;          /* operator+ : (1+4),(2+5),(3+6) = {5,7,9} */
    struct Vec3 d = a * b;          /* operator* : (1*4),(2*5),(3*6) = {4,10,18} */
    struct Vec3 e = a + b * b;      /* a + (b*b): (b*b)={16,25,36}, then {17,27,39} */

    if (c.x != 5 || c.y != 7 || c.z != 9)      { puts("FAIL: a+b"); return 1; }
    if (d.x != 4 || d.y != 10 || d.z != 18)    { puts("FAIL: a*b"); return 1; }
    if (e.x != 17 || e.y != 27 || e.z != 39)   { puts("FAIL: a+b*b precedence"); return 1; }

    puts("PASS: t050_operator");
    return 0;
}