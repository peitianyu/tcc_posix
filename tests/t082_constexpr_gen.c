/* t082_constexpr_gen: 受限 constexpr (TCC 扩展) — 编译期常量对象
 *
 * 仅认可「单遍内能折叠的纯表达式」: 初始化式走 expr_const64, 非编译期
 * 常量即报错 (无循环/递归, 结构性终止保证, 见 docs/comptime.md §6)。
 * constexpr 变量按枚举常量登记 (VT_CONST|VT_ENUM_VAL + enum_val),
 * 不分配存储, 引用处单遍常量传播 → 可作数组尺寸 / switch case /
 * _Static_assert / 位运算等一切整型常量表达式。
 *
 * 同时演示 constexpr 与 model + _Generic 的类型级编译期分派配搭。
 * 注意: constexpr 初始化式只接受纯常量表达式, 不能调用 model 函数。
 * 退出码 0 = 通过.
 * 本测试为 TCC 扩展语法, 不作 clang 兼容 (与 STL 测试 t080/t081 相同).
 */
#include <stdio.h>

/* —— 受限 constexpr: 编译期整型常量对象 —— */
constexpr int N         = 4;                 /* 字面量 */
constexpr int BITS      = 8 * sizeof(int);   /* sizeof/算术折叠 */
constexpr unsigned MASK = ((1u << (BITS - 1)) - 1u);  /* 位运算 */

/* —— constexpr 与 model + _Generic 协同: 类型级编译期分派 —— */
model (T) int type_id(T v)
{
    return _Generic(v, int: 1, float: 2, double: 3, default: 9);
}

int main(void)
{
    /* 1. constexpr 常量进入数组尺寸 (编译期, 非 VLA) */
    int a[N];                       /* N = 4 */
    int b[BITS];                    /* BITS = 32 */
    if (sizeof(a) != 4 * sizeof(int)) return 1;
    if (sizeof(b) != 32 * sizeof(int)) return 2;

    /* 2. constexpr 常量在 switch case 中 */
    int v = N + 1;                  /* 5 */
    switch (v) {
    case N:    return 3;            /* 4 ≠ 5 */
    case N + 1: break;              /* 5 */
    default:   return 4;
    }

    /* 3. constexpr 常量在编译期断言 (_Static_assert) */
    _Static_assert(N > 0, "N must be positive");
    _Static_assert(MASK == 0x7FFFFFFFu, "MASK = 2^31 - 1");

    /* 4. constexpr 参与编译期/运行算式 */
    if (N * 10 != 40) return 5;
    if (BITS / 8 != 4) return 6;

    /* 5. 运行期算术引用 constexpr 常量 */
    a[0] = 0;                       /* 先清零, 避免读未初始化栈 (linux 上残留为负→假失败) */
    int x = a[0] + N + BITS;        /* 仅验引用不报错 */
    if (x < 0) return 7;

    /* 6. model + _Generic 类型分派 (与 constexpr 互补: 类型级 vs 常量级) */
    int    i = 0;
    float  f = 0;
    double d = 0;
    if (type_id(int)(i)    != 1) return 8;
    if (type_id(float)(f)  != 2) return 9;
    if (type_id(double)(d) != 3) return 10;

    printf("constexpr ok\n");
    return 0;
}