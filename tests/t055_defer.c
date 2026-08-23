/* t055_defer.c — defer 脱糖: 按作用域逆序延迟执行
 *
 * 验证 defer 在闭块处的 LIFO (逆序) 调用, 以及嵌套块的先内后外顺序:
 *   order_test(): defer 1/2/3 -> 离开作用域逆序 -> 3,2,1
 *   nested_test(): 外层 defer 1、4; 内层 defer 2、3
 *                  -> 内层闭块先逆序 3,2; 函数末外层逆序 4,1 -> fired=3241
 * 退出码 0 = 通过.
 * 构建:  tcc  --emit-c tests/t055_defer.c -o build/t055.desugared.c
 *        gcc build/t055.desugared.c -o build/t055.exe   (或 tcc 直接编译)
 */
#include <stdio.h>

static int seq[8];
static int nseq = 0;
static void push(int v) { if (nseq < 8) seq[nseq++] = v; }

static int fired = 0;
static void fire(int v) { fired = fired * 10 + v; }

static void order_test(void)
{
    defer push(1);
    defer push(2);
    defer push(3);
}

static void nested_test(void)
{
    defer fire(1);
    {
        defer fire(2);
        defer fire(3);
    }                       /* 内层闭块: 3,2 */
    defer fire(4);
}                           /* 函数末: 4,1  (fired=3241) */

int main(void)
{
    order_test();
    nested_test();

    if (nseq != 3 || seq[0] != 3 || seq[1] != 2 || seq[2] != 1) {
        printf("FAIL: order %d,%d,%d\n", seq[0], seq[1], seq[2]);
        return 1;
    }
    if (fired != 3241) {
        printf("FAIL: nested fired=%d\n", fired);
        return 2;
    }
    puts("PASS: t055_defer");
    return 0;
}