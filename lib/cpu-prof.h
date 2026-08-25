/* cpu-prof.h — 用户端 CPU 周期插桩 (精确 cycle, rdtsc)
 *
 * 让程序任意代码段(通常是热点函数)用宏包住, 得到每段的 {总周期, 次数, 每调用
 * 周期(avg), 单次最长(max)} 三份数, 把 CPU 时间归因到具体段. 纯 POSIX,
 * 零 winapi, 零编译器改动, 不依赖 bcheck.o.
 *
 * 用法:
 *   #include "cpu-prof.h"
 *   void hot_fn(void) {
 *       CPU_BEGIN(hot_fn);           // 读 TSC, 登记 site(按调用点自动建槽)
 *       ... 纯计算段 ...
 *       CPU_END(hot_fn);             // 读 TSC, 累进该 site
 *   }
 *   int main(void) {
 *       ...
 *       cpu_prof_report();               // 打 stderr (默认 sink)
 *       cpu_prof_report_to(my_printer, ctx); // 或交给用户 sink
 *   }
 *
 * 编译一并在链接处带上运行时源 (lib/cpu-prof.c), t049_cpu 的 test.sh 做法:
 *   tcc tests/x.c ../../lib/cpu-prof.c -I <本头所在目录> -o x.exe
 *
 * 关键点:
 *  - site 以 CPU_BEGIN 处的返回地址为键自动登记(open array, 固定容量, 零分配,
 *    同 memtrack 骨架); `name` 仅作报告可读标签 (经 #name 字符串化).
 *  - 递归: 内部用深度栈匹配 BEGIN/END 对; 每对累加自身跨幅(含嵌套段, 即
 *    inclusive). 纯计算段外(io/锁/休眠)不应包进去, 它们测的是"该段周期数"
 *    而非 CPU 忙用.
 *  - 多线程/多核: TSC 各核基准可能不同, 且表是进程级; 精度要求高时按线程
 *    单独跑实例, 或只用单线程再对比. v1 不据此严格归位.
 *  - sink 可插拔: 报告"一行"交给用户回调 (out, ctx), 不硬编码 stderr;
 *    cpu_prof_report() 只是内置一个 stderr sink 的便捷入口.
 *  - sink 是可赋值/可传参的指针 (运行时改道); 符号渲染钩子 __bt_resolve_addr
 *    是弱函数, 实测 PE/ELF 链接均支持弱引用绑定 (bt-exe.o 强定义覆盖),
 *    未链接 -bt 时归零回退裸地址 (t049 双路径覆盖确认).
 */
#ifndef CPU_PROF_H
#define CPU_PROF_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- 周期读取原语 ---------- */

/* 快路径: 无围栏, 可能被乱序重排; 用于相对比较可接受. */
static inline unsigned long long cpu_rdtsc(void)
{
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((unsigned long long)hi << 32) | lo;
}

/* 慢但准: lfence 围栏防乱序; 用于精确单段差值. */
static inline unsigned long long cpu_rdtsc_fence(void)
{
    unsigned int lo, hi;
    __asm__ __volatile__("lfence" ::: "memory");
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    __asm__ __volatile__("lfence" ::: "memory");
    return ((unsigned long long)hi << 32) | lo;
}

/* ---------- site 与 sink 类型 ---------- */

typedef struct cpu_site {
    unsigned long long caller;      /* 插桩点返回地址 (CPU_BEGIN 处) */
    const char        *name;        /* CPU_BEGIN 的标签 (字符串字面量) */
    unsigned long long cycles;      /* 累计周期 (inclusive)          */
    unsigned long long calls;       /* 进入次数                       */
    unsigned long long max_cycles;  /* 单次最长                       */
    int                in_use;      /* 槽是否被占用                   */
} cpu_site;

/* 用户自定义输出 sink: 报告每行经它回调; ctx 由用户自行解释. */
typedef void (*cpu_printer)(void *ctx, const char *line);

/* ---------- 运行时 API (lib/cpu-prof.c) ---------- */
void cpu_prof_begin(const char *name, unsigned long long pc);
void cpu_prof_end(void);

void cpu_prof_clear(void);
void cpu_prof_report(void);                         /* 内置 stderr sink */
void cpu_prof_report_to(cpu_printer out, void *ctx);

int  cpu_prof_sites(cpu_site **out);                /* 返回 site 表与数量 */
unsigned int cpu_prof_overflow(void);               /* site 槽溢出标志 */

/* ---------- 宏 (手动标记版) ---------- */

/* CPU_TIMER 仅作意图声明/文档占位; site 由首次 CPU_BEGIN(name) 自动登记. */
#define CPU_TIMER(name)

#define CPU_BEGIN(name) \
    do { cpu_prof_begin(#name, (unsigned long long)__builtin_return_address(0)); } while (0)
#define CPU_END(name)   do { cpu_prof_end(); } while (0)

#define CPU_CLEAR()        cpu_prof_clear()
#define CPU_REPORT()       cpu_prof_report()
#define CPU_REPORT_TO(o,c) cpu_prof_report_to((o),(c))

#ifdef __cplusplus
}
#endif

#endif /* CPU_PROF_H */