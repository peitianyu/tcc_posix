/* cpu-prof.c — CPU 周期插桩运行时 (rdtsc, 手动标记版)
 *
 * 复刻 bcheck.c memtrack 的「固定容量 open array + 零分配 + 插入排序 + 可选
 * 符号渲染」骨架, 字段语义换成周期统计. 纯 POSIX, 不依赖 bcheck.o(插桩无需
 * 信号驱动), 不触碰编译器.
 *
 * 报告输出可插拔: 每行交给调用方传入的 cpu_printer sink, 不写死 stderr;
 * cpu_prof_report() 仅是内置一个 stderr sink 的便捷入口.
 *
 * 符号渲染 (可选): 弱引用 __bt_resolve_addr (bt-exe.c 提供), 把 caller 返回
 * 地址渲染成 func@file:line; 未 link -bt 时衰减为裸地址 + 用户标签 name.
 * 说明: 用可赋值/可传参的 sink, 不用弱函数 —— 本工具链 PE 后端无弱符号绑定.
 */
#include <stdio.h>
#include <string.h>
#include "cpu-prof.h"

#define CPU_PROF_MAXSITES  128
#define CPU_PROF_MAXDEPTH  64

/* 可选: bt-exe.o 提供, 把返回地址解析为 "func@file:line"; 缺省返回错误/留空 */
__attribute__((weak))
int __bt_resolve_addr(unsigned long long pc, char *buf, unsigned long len);

static cpu_site cpu_sites[CPU_PROF_MAXSITES];
static int nb_cpu_sites;
static int cpu_overflow;

typedef struct cpu_frame {
    unsigned long long tsc;   /* BEGIN 时刻 */
    int                site;  /* 对应槽索引 */
} cpu_frame;

static cpu_frame cpu_stack[CPU_PROF_MAXDEPTH];
static int cpu_depth;

/* 按返回地址查找 site, 找不到且槽不满则新建. 返回槽索引; 失败 -1. */
static int cpu_site_find(const char *name, unsigned long long pc)
{
    int i;

    for (i = 0; i < nb_cpu_sites; i++)
        if (cpu_sites[i].caller == pc)
            break;
    if (i == nb_cpu_sites) {
        if (nb_cpu_sites < CPU_PROF_MAXSITES) {
            cpu_sites[i].caller  = pc;
            cpu_sites[i].name    = name;
            cpu_sites[i].cycles  = 0;
            cpu_sites[i].calls   = 0;
            cpu_sites[i].max_cycles = 0;
            cpu_sites[i].in_use  = 1;
            nb_cpu_sites++;
        } else {
            cpu_overflow = 1;
            return -1;
        }
    }
    return i;
}

void cpu_prof_begin(const char *name, unsigned long long pc)
{
    int idx = cpu_site_find(name, pc);

    if (idx < 0 || cpu_depth >= CPU_PROF_MAXDEPTH) /* 槽满/栈满: 丢弃该层 */
        return;

    cpu_stack[cpu_depth].tsc  = cpu_rdtsc_fence();
    cpu_stack[cpu_depth].site = idx;
    cpu_depth++;
}

void cpu_prof_end(void)
{
    unsigned long long now, d;
    int i, idx;

    if (cpu_depth <= 0)
        return;                       /* 配平错误: 多 END, 忽略 */
    i  = --cpu_depth;
    idx = cpu_stack[i].site;
    now = cpu_rdtsc_fence();
    d  = now - cpu_stack[i].tsc;
    cpu_sites[idx].cycles += d;
    cpu_sites[idx].calls++;
    if (d > cpu_sites[idx].max_cycles)
        cpu_sites[idx].max_cycles = d;
}

void cpu_prof_clear(void)
{
    nb_cpu_sites = 0;
    cpu_depth    = 0;
    cpu_overflow = 0;
}

int cpu_prof_sites(cpu_site **out)
{
    if (out)
        *out = cpu_sites;
    return nb_cpu_sites;
}

unsigned int cpu_prof_overflow(void)
{
    return (unsigned int)cpu_overflow;
}

/* ---------- 报告 (可插拔 sink) ---------- */

static void stderr_printer(void *ctx, const char *line)
{
    (void)ctx;
    fputs(line, stderr);
}

void cpu_prof_report_to(cpu_printer out, void *ctx)
{
    static cpu_site snap[CPU_PROF_MAXSITES];
    char line[192], where[160];
    int i, j, n, shown;
    cpu_site key;

    if (!out)
        return;
    if (nb_cpu_sites == 0) {
        out(ctx, "[cpuprof] no sites instrumented\n");
        return;
    }

    memcpy(snap, cpu_sites, (size_t)nb_cpu_sites * sizeof(cpu_site));
    n = nb_cpu_sites;

    /* 插入排序: 按累计周期降序 (n 小, 直排即可) */
    for (i = 1; i < n; i++) {
        key = snap[i];
        j = i - 1;
        while (j >= 0 && snap[j].cycles < key.cycles) {
            snap[j + 1] = snap[j];
            j--;
        }
        snap[j + 1] = key;
    }

    snprintf(line, sizeof line,
             "[cpuprof] %d site(s): sorted by total cycles\n", n);
    out(ctx, line);
    shown = (n > 16) ? 16 : n;
    for (i = 0; i < shown; i++) {
        unsigned long long avg = snap[i].calls
            ? (snap[i].cycles / snap[i].calls) : 0;
        where[0] = 0;
        if (__bt_resolve_addr)
            __bt_resolve_addr(snap[i].caller, where, sizeof where);
        else
            snprintf(where, sizeof where, "0x%llx", snap[i].caller);
        snprintf(line, sizeof line,
                 "[cpuprof] %-20s total=%12llu calls=%6llu avg=%8llu "
                 "max=%10llu @%s\n",
                 snap[i].name ? snap[i].name : "?",
                 snap[i].cycles, snap[i].calls, avg,
                 snap[i].max_cycles, where[0] ? where : "?");
        out(ctx, line);
    }
    if (cpu_overflow)
        out(ctx, "[cpuprof] (note: site table overflow, some segments omitted)\n");
}

void cpu_prof_report(void)
{
    cpu_prof_report_to(stderr_printer, NULL);
}