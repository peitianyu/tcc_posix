/* 测试: 数值转换与数学 (stdlib strtol/atoi/rand + math 基本) */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
int main(void) {
    /* strtol 各种进制 */
    char *end;
    errno = 0;
    long v = strtol("  1234", &end, 10);
    if (v != 1234 || *end) return 1;
    v = strtol("0x1F", &end, 16);
    if (v != 31) return 2;
    v = strtol("101", &end, 2);
    if (v != 5) return 3;
    v = strtol("-777", &end, 8);
    if (v != -511) return 4;
    /* 溢出检测 */
    errno = 0;
    v = strtol("99999999999999999999999", &end, 10);
    if (errno != ERANGE) return 5;
    if (v != LONG_MAX) return 6;
    /* strtoul 负数环绕 */
    unsigned long uv = strtoul("-1", &end, 10);
    if (uv != (unsigned long)-1) return 7;
    /* strtod */
    errno = 0;
    double dv = strtod("3.14159", &end);
    if (dv < 3.141 || dv > 3.142) return 8;
    if (strtod("1e10", &end) != 1e10) return 9;
    /* atoi/atol/atof */
    if (atoi("  -42") != -42) return 10;
    if (atol("2147483647") != 2147483647L) return 11;
    if (atof("2.5") != 2.5) return 12;
    /* rand 可重复 (srand 固定种子) */
    srand(42);
    int r1 = rand();
    srand(42);
    if (rand() != r1) return 13;
    /* abs/labs */
    if (abs(-5) != 5) return 14;
    if (labs(-123456789L) != 123456789L) return 15;
    /* div */
    div_t d = div(17, 5);
    if (d.quot != 3 || d.rem != 2) return 16;
    /* 数学函数 (musl 完整 math) */
    if (floor(2.7) != 2.0) return 17;
    if (ceil(2.1) != 3.0) return 18;
    if (fabs(-3.5) != 3.5) return 19;
    if (sqrt(16.0) != 4.0) return 20;
    if (fmod(7.5, 2) != 1.5) return 21;
    if (scalbn(1.5, 3) != 12.0) return 22;
    return 0;
}
