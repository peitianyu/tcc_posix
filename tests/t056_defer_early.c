/* t056_defer_early.c — defer 早退(return)脱糖验收:
 *
 * 验证"作用域逆序执行"在 `return` 提前离开函数时也能触发(而非仅闭块),
 * 且顺序 = 内层先 / 每层 LIFO:
 *   f(1): defer '1'(函数体) → if{ defer '2'; { defer '3'; } /* 内块立刻闭?no*\
 *         内层 {defer '3'} 正常闭块先发 '3'; 随后 `return 7` 早退再发 '2'→'1'.
 *         期望 buf="321", 返回值 7.
 *   f(0): 不进入 if, 直接底部 `return 9` → 只发 '1'. 期望 buf="1", 返回值 9.
 * 同时用"无早退"路径对照, 确认闭块重放未回归.
 * 退出码 0 = 通过.
 */
#include <stdio.h>
#include <string.h>

static char buf[8];
static int  n = 0;
static void push(char c) { buf[n++] = c; }

int f(int k)
{
    defer push('1');
    if (k) {
        defer push('2');
        {
            defer push('3');
        }
        return 7;
    }
    return 9;
}

int g(void)
{
    defer push('a');
    {
        defer push('b');
        {
            defer push('c');
            n = 0;
        }
    }
    return 0;
}

int main(void)
{
    int r, fail = 0;

    n = 0; r = f(1);
    buf[n] = 0;
    if (r != 7 || strcmp(buf, "321")) { printf("FAIL f(1): r=%d buf=%s\n", r, buf); fail++; }

    n = 0; r = f(0);
    buf[n] = 0;
    if (r != 9 || strcmp(buf, "1"))    { printf("FAIL f(0): r=%d buf=%s\n", r, buf); fail++; }

    n = 0; r = g();
    buf[n] = 0;
    if (r != 0 || strcmp(buf, "cba"))  { printf("FAIL g(): r=%d buf=%s\n", r, buf); fail++; }

    if (fail) { printf("t056 FAILED (%d)\n", fail); return 1; }
    printf("PASS: t056_defer_early\n");
    return 0;
}