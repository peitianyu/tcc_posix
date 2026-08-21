/* 测试: defer 延迟执行 (Go 式: 注册点求值, 离开作用域逆序调用) */
#include <stdio.h>
static int order[16], n;
static void rec(int v) { order[n++] = v; }
static void setp(int *p, int v) { *p = v; }
static int ret_test(void);

int main(void) {
    /* 1. 基本逆序: 注册 f(1) f(2) f(3) → 执行 3 2 1 */
    { defer rec(1); defer rec(2); defer rec(3); }
    if (n != 3 || order[0] != 3 || order[1] != 2 || order[2] != 1) return 1;

    /* 2. 块级: 内层块 defer 在块退出时执行, 外层不受影响 */
    n = 0;
    { defer rec(10); { defer rec(11); } if (n != 1 || order[0] != 11) return 2; }
    if (n != 2 || order[1] != 10) return 3;

    /* 3. return 路径: 函数返回前执行 defer */
    n = 0;
    { int r = ret_test(); if (r != 0) return 5; if (n != 1 || order[0] != 20) return 6; }

    /* 4. 参数快照: 注册点求值, 之后改变量不影响 */
    { int x = 5; defer setp(&x, 99); x = 1; if (x != 1) return 4; }

    /* 5. goto 跨块: 跳出块时执行块内 defer */
    n = 0;
    {
        defer rec(30);
        if (n != 0) return 7;
        goto out;
    }
out:
    if (n != 1 || order[0] != 30) return 8;

    /* 6. break 跳出循环: 循环体内 defer 执行 */
    n = 0;
    for (;;) {
        defer rec(40);
        break;
    }
    if (n != 1 || order[0] != 40) return 9;

    printf("defer ok\n");
    return 0;
}

static int ret_test(void) { defer rec(20); return 0; }
