/* t052_desugar.c — 脱糖输出(--emit-c)管线验收
 *
 * 本文件是 **标准 C**(不依赖任何 tcc 扩展), 是脱糖"透传"路径的最小闭环验证:
 *   1. TCC 直接解释/编译运行 (-run / -o)   -> 作为检索基准
 *   2. TCC --emit-c 脱糖输出 -> 标准C(.desugared.c)
 *   3. clang 编译脱糖产物 (Linux/musl sysroot): 结果应与 TCC 一致
 *
 * 构建(Windows 验证前端):
 *   bin/tcc.exe tests/t052_desugar.c -o t052_desugar.exe
 *   bin/tcc.exe -run tests/t052_desugar.c
 *
 * 正式产物(Linux/musl, 需要 clang):
 *   bin/tcc.exe --emit-c -o t052.desugared.c tests/t052_desugar.c
 *   clang -O3 -mavx2 -mfma --sysroot=<musl> t052.desugared.c -o t052_release
 *   [TCC 数字结果]  t052_desugar.exe 输出
 *   [clang 数字结果] ./t052_release 输出      (应一致)
 *
 * 退出码 0 = 通过.
 */
#include <stdio.h>

static int fails = 0;
static void chk(int c, const char *m)
{
    printf("  %-24s %s\n", m, c ? "ok" : "FAIL");
    if (!c) fails++;
}

int main(void)
{
    printf("t052_desugar: 纯标准C脱糖透传基准\n");

    /* 加/减/乘/除 + 优先级 */
    int a = 12, b = 7;
    chk((a + b) * 2 == 38, "int (+)*");
    chk((a - b) / a == 0, "int - /");
    chk(a * b == 84, "int *");
    chk(a % b == 5, "int %");

    /* 浮点 */
    double x = 3.5, y = 2.0;
    chk((x + y) == 5.5, "double +");
    chk((x * y) == 7.0, "double *");
    chk(x / y == 1.75, "double /");

    /* 位运算/移位 */
    unsigned u = 0xF0;
    chk((u & 0x0F) == 0, "bit &");
    chk((u | 0x0F) == 0xFF, "bit |");
    chk((u >> 4) == 0x0F, "shift >>");
    chk((1u << 8) == 256, "shift <<");

    /* 三元/比较 */
    int m = a > b ? a : b;
    chk(m == 12, "ternary");

    if (fails) {
        printf("t052 FAILED (%d)\n", fails);
        return 1;
    }
    printf("PASS: t052_desugar (TCC); 再以 --emit-c 脱糖后交 clang 复现同值\n");
    return 0;
}