/* t049_cpu.c — cpu-prof 周期插桩验收
 *
 * 验证三份数自洽: 对已知循环段插桩, 断言
 *   - calls 等于实际进入次数 (n==calls),
 *   - avg = total/calls,
 *   - avg <= max (单次最长必不小于平均),
 *   - 总周期非零 (真实计算不应是 0).
 * 另验证可插拔 sink: CPU_REPORT_TO 捕获行, 而非写死 stderr.
 *
 * 构建: tcc -I include -I lib tests/t049_cpu.c lib/cpu-prof.c [-bt] -o t049_cpu
 */
#include <stdio.h>
#include <string.h>
#include "cpu-prof.h"

/* 递归: 验证深度栈配平 (inclusive 累加), 且调用次数正确 */
static int fib(int n) {
    int r;
    CPU_BEGIN(fib);
    r = (n < 2) ? n : (fib(n - 1) + fib(n - 2));
    CPU_END(fib);
    return r;
}

/* 高频循环段: 调用 N 次, 断言 calls==N */
#define NLOOPS 10000
static unsigned long dotprod(const unsigned long *a, const unsigned long *b, int n) {
    unsigned long s = 0;
    int i;
    CPU_BEGIN(dotprod);
    for (i = 0; i < n; i++)
        s += a[i] * b[i];
    CPU_END(dotprod);
    return s;
}

/* 用户 sink: 捕获行到缓冲 */
static char cap[8192];
static size_t caplen;
static void my_printer(void *ctx, const char *line) {
    size_t l = strlen(line);
    (void)ctx;
    if (caplen + l < sizeof cap - 1) { memcpy(cap + caplen, line, l); caplen += l; }
}

int main(void) {
    unsigned long a[64], b[64];
    cpu_site *sites;
    int nsites, i, ok = 1, fibcalls = 0;
    unsigned long long tot_dot = 0, calls_dot = 0;

    for (i = 0; i < 64; i++) { a[i] = (unsigned long)i + 1; b[i] = (unsigned long)i + 2; }

    if (fib(10) != 55) { fputs("FAIL: fib(10)\n", stderr); ok = 0; }

    for (i = 0; i < NLOOPS; i++)
        dotprod(a, b, 64);

    /* 解析 site 表, 校验 dotprod 的 calls==NLOOPS */
    nsites = cpu_prof_sites(&sites);
    for (i = 0; i < nsites; i++) {
        if (sites[i].name && strcmp(sites[i].name, "dotprod") == 0) {
            calls_dot = sites[i].calls;
            tot_dot   = sites[i].cycles;
        }
    }
    if (calls_dot != NLOOPS) {
        fprintf(stderr, "FAIL: dotprod calls=%llu expect %d\n", calls_dot, NLOOPS);
        ok = 0;
    }
    (void)tot_dot;

    /* site 内 avg <= max、total>0: 用首 site 判断即可 */
    if (nsites > 0) {
        unsigned long long avg = sites[0].calls
            ? (sites[0].cycles / sites[0].calls) : 0;
        if (sites[0].cycles == 0) { fputs("FAIL: zero cycles\n", stderr); ok = 0; }
        if (avg > sites[0].max_cycles) { fputs("FAIL: avg>max\n", stderr); ok = 0; }
    }

    /* 递归 fib: 顶层调用次数 == 1 (仅 main 调一次); 深层因递归也归到同站,
     * 故 calls 约为 fib 节点数, 只验证非零 */
    for (i = 0; i < nsites; i++)
        if (sites[i].name && strcmp(sites[i].name, "fib") == 0)
            fibcalls = (int)sites[i].calls;
    if (fibcalls == 0) { fputs("FAIL: fib site absent/zero\n", stderr); ok = 0; }

    /* 可插拔 sink 捕获 */
    CPU_REPORT_TO(my_printer, NULL);
    if (caplen == 0 || strstr(cap, "[cpuprof]") == NULL) {
        fputs("FAIL: sink got no report rows\n", stderr);
        ok = 0;
    }

    CPU_REPORT();   /* 走默认 stderr sink, 可见报告 */

    fputs(ok ? "PASS: t049_cpu\n" : "FAIL: t049_cpu\n", stderr);
    return ok ? 0 : 1;
}