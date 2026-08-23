/* t058_operator_deep.c — 运算符重载脱糖: 深层嵌套 / 混合优先级 / 括号内调用
 *
 * 作为 t053 的加强版, 专攻更深/更复式的重写, 验证"完整表达式忠实改写"策略
 * (括号结构与优先级在反推时被完整重建):
 *   h = ((a + b) * (c - d)) % a      → operator_mod(operator_mul(operator_add(a,b), operator_sub(c,d)), a)
 *   i = a + b * c - d / a            → operator_sub(operator_add(a, operator_mul(b,c)), operator_div(d,a))
 *   (a + b) 作为函数实参 (括号内非模式调用, 实为表达式) 必须被改写
 *   j = mul2(a + b, c - d)           → 但这是普通函数调用, 实参保持原样 (不写 operator_)
 *   k = ((a))                        → 括号不改变优先级, 折叠为 operator_add(a, b) 语义不变
 * 标量字段始终不误改 (含深层: ((s.x * s.y) + s.z) 原样).
 * 退出码 0 = 通过.
 */
#include <stdio.h>

struct Vec3 { float x, y, z; };

struct Vec3 operator+ (struct Vec3 a, struct Vec3 b) {
    struct Vec3 r = { a.x + b.x, a.y + b.y, a.z + b.z };
    return r;
}
struct Vec3 operator- (struct Vec3 a, struct Vec3 b) {
    struct Vec3 r = { a.x - b.x, a.y - b.y, a.z - b.z };
    return r;
}
struct Vec3 operator* (struct Vec3 a, struct Vec3 b) {
    struct Vec3 r = { a.x * b.x, a.y * b.y, a.z * b.z };
    return r;
}
struct Vec3 operator/ (struct Vec3 a, struct Vec3 b) {
    struct Vec3 r = { a.x / b.x, a.y / b.y, a.z / b.z };
    return r;
}
struct Vec3 operator% (struct Vec3 a, struct Vec3 b) {
    struct Vec3 r = { (int)a.x % (int)b.x, (int)a.y % (int)b.y, (int)a.z % (int)b.z };
    return r;
}

/* 普通函数: 签名带 operator 类型参数 (值传递), 验证跨函数改写 */
struct Vec3 mul2(struct Vec3 a, struct Vec3 b) {
    struct Vec3 r = { a.x * b.x, a.y * b.y, a.z * b.z };
    return r;
}

int main(void) {
    struct Vec3 a = { 6, 9, 12 };   /* 除/取模用大数, 避免除零 */
    struct Vec3 b = { 2, 3, 4 };
    struct Vec3 c = { 8, 5, 7 };
    struct Vec3 d = { 3, 2, 2 };

    /* 1. 深层多层: ((a+b)*(c-d)) % a
     *    a+b={8,12,16}, c-d={5,3,5}, *={40,36,80}, %a={4,0,8} */
    struct Vec3 h = ((a + b) * (c - d)) % a;
    if (h.x != 4 || h.y != 0 || h.z != 8) { puts("FAIL: deep nesting"); return 1; }

    /* 2. 混合优先级 (纯加/乘/减, 期望整数): a + b*c - d
     *    b*c={16,15,28}, a+={22,24,40}, 减d => {19,22,38} */
    struct Vec3 i = a + b * c - d;
    if (i.x != 19 || i.y != 22 || i.z != 38) { puts("FAIL: mixed prty"); return 1; }

    /* 3. 括号折叠: ((a)) 双层括号不影响优先级 */
    struct Vec3 k = ((a)) + b;      /* = a+b = {8,12,16} */
    if (k.x != 8 || k.y != 12 || k.z != 16) { puts("FAIL: paren fold"); return 1; }

    /* 3b. 除法改写为 operator_div: (a+b)/b = {8/2,12/3,16/4} = {4,4,4} */
    struct Vec3 kd = (a + b) / b;
    if (kd.x != 4.0f || kd.y != 4.0f || kd.z != 4.0f) { puts("FAIL: div rewrite"); return 1; }

    /* 4. 普通函数调用: 实参是 operator 表达式, 但函数名非 operator, 不得改写 */
    struct Vec3 j = mul2(a + b, c - d);   /* mul2({8,12,16},{5,3,5})={40,36,80} */
    if (j.x != 40 || j.y != 36 || j.z != 80) { puts("FAIL: fn-call arg"); return 1; }

    /* 5. 逗号 / 三目内的 operator 表达式不误伤 (三元保持原样) */
    struct Vec3 m = (h.x > 0) ? (a + b) : (c - d);  /* a+b={8,12,16} */
    if (m.x != 8 || m.y != 12 || m.z != 16) { puts("FAIL: ternary"); return 1; }

    /* 6. 标量字段深层不误改 */
    float t = ((a.x * b.x) + c.x) - d.x;   /* 12+8-3=17 */
    if (t != 17.0f) { puts("FAIL: scalar deep"); return 1; }

    puts("PASS: t058_operator_deep");
    return 0;
}