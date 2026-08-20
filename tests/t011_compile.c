/* 测试: 递归/栈/volatile/对齐 (编译正确性) */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
/* 深递归 */
static long fib(int n) { return n < 2 ? n : fib(n-1) + fib(n-2); }
/* 大栈帧 */
static int bigstack(void) {
    char buf[8192];
    memset(buf, 0x5A, sizeof buf);
    return buf[0] + buf[8191];
}
/* 对齐要求 */
typedef struct { char c; double d; } AlignT;
typedef struct { char c; long long l; } AlignL;
int main(void) {
    /* fib(25) = 75025 */
    if (fib(25) != 75025) return 1;
    /* 大栈帧 */
    if (bigstack() != 0x5A + 0x5A) return 2;
    /* 结构体对齐 */
    AlignT at;
    if ((uintptr_t)&at.d % sizeof(double)) return 3;
    AlignL al;
    if ((uintptr_t)&al.l % sizeof(long long)) return 4;
    /* 64 位运算 */
    long long a = 0x1234567890LL;
    long long b = a * 1000000;
    if (b != 0x1234567890LL * 1000000) return 5;
    long long c = b / 7, d2 = b % 7;
    if (c * 7 + d2 != b) return 6;
    /* 无符号 64 位移位 */
    unsigned long long u = 1ULL << 63;
    if (u >> 63 != 1) return 7;
    if ((u >> 62) != 2) return 8;
    /* 负数除法向零截断 */
    if (-7 / 2 != -3) return 9;
    if (-7 % 2 != -1) return 10;
    /* volatile 不优化 */
    volatile int v = 0;
    for (int i = 0; i < 1000; i++) v += i;
    if (v != 499500) return 11;
    /* 浮点混合 */
    double f = 1.5;
    f = f * 2.0 + 1;
    if (f != 4.0) return 12;
    int fi = (int)f;
    if (fi != 4) return 13;
    /* 函数指针 */
    long (*fp)(int) = fib;
    if (fp(10) != 55) return 14;
    /* 位域 */
    struct { unsigned a : 3, b : 5, c : 24; } bits;
    bits.a = 5; bits.b = 17; bits.c = 0xFFFFFF;
    if (bits.a != 5 || bits.b != 17 || bits.c != 0xFFFFFF) return 15;
    return 0;
}
