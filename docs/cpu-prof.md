# 用户端 CPU 周期插桩方案 (cpu-prof)

> 状态: 2026-08-23 设计 → 已落地 (include/cpu-prof.h + lib/cpu-prof.c + tests/t049_cpu.c, PASS)
> 范围: 一块运行在 tcc_posix 之上的用户端 C 库, 用 rdtsc 插桩计算**精确周期数**,
>       把 CPU 时间归因到具体代码段 (函数). 纯 POSIX, 零 winapi, 零编译器改动.
> 依据: 「CPU 使用监测 / 采样 vs 周期插桩 / 用户端 v.s. 编译器」讨论结论.
> 结论: 精致周期计数走**插桩 (rdtsc)** 而非统计采样 (SIGPROF); 先做**手动标记版**,
>       编译器全自动插桩列为二期.

---

## 1. 背景与定位

CPU 分析有两条路线, 本方案取**插桩周期计数**:

| 路线 | 触发 | 精度 | 用途 |
|---|---|---|---|
| 统计采样 (SIGPROF) | 周期信号 | 概率占比 | 全局 Top-N, 侦察热点 |
| 插桩周期 (rdtsc) | 插桩点 | 精确 cycle | 单函数净周期 / 归因 |

二者互补, 典型用法是两阶段: 先采样粗定位热点函数, 再对该函数插桩细查 `avg cycle`
与 `calls`, 定位真正优化点.

本方案专注于**精确周期计数**这一支, 目标:
1. 让人能在任意函数用宏包住, 得到 `{总周期, 调用次数, 每调用周期}` 三份数;
2. 复用 memtrack (bcheck.c) 的表 + Top-N 报告 + `__bt_resolve_addr` 渲染骨架;
3. 处理多线程/递归/乱序这三处 rdtsc 的陷阱.

设计原则: 不发明新机制, 不触碰编译器, 全部落在库头 + 一个 .c.

---

## 2. 核心能力: 把 CPU 归因成可执行结构

插桩产出 `cpu_site { cycles, calls }`, 由三份数支撑分析:

| 量 | 含义 | 对应优化动作 |
|---|---|---|
| 总周期 | 该函数及其调用整体时间 | 谁最烧 → 归因 |
| 调用次数 | 频次 | 高频 → 考虑内联/降调用 |
| 每调用周期 = 总/次数 | 单次成本 | 单次贵 → 改算法/向量化 |

三者之和 ≈ 程序 CPU 时间; per-thread 分隔后可观察各线程吃满程度与可并行性.

关键优势: **采样分不清"高频低单次"和"低频高单次", 插桩能 (avg cycle), 这是选用插桩的根本理由.**

---

## 3. 周期读取原语 (最底层)

```c
/* 快路径: 无围栏, 乱序下自身读数可能偏; 用于相对比较可接受 */
static inline unsigned long long rdtsc_now(void) {
    return __builtin_ia32_rdtsc();
}

/* 慢但准: lfence 围栏防乱序, 需编译器支持内建/内联汇编 */
static inline unsigned long long rdtsc_fence(void) {
    __builtin_ia32_lfence();
    unsigned long long t = __builtin_ia32_rdtsc();
    __builtin_ia32_lfence();
    return t;
}
```

- **`__builtin_ia32_rdtsc` 是否被 tcc 支持 → 落地时先验证**, 若不支持则退化为
  内联汇编 `rdtsc; lfence; rdtsc` (`__asm__ __volatile__`).
  **已验证**: tcc 报 `unresolved reference` 不支持内建, 已改用内联汇编
  `__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi))` (lfence 同理 `"lfence":::"memory"`),
  实测 delta 正常.
  `CPU_BEGIN(name)` 用 `#name` 字符串化站点标签, 并按键到 `__builtin_return_address(0)`
  (同函数不同调用点是独立 site, 便于按调用点归因).
- 精度依赖围栏: 无围栏 rdtsc 会与前后指令乱序重排, 两边读数错位则周期失准.
- 频率换算 (cycle→ns) 为可选项: 用 `clock_gettime(CLOCK_MONOTONIC)` 对 TSC 打数
  校准 cycles/s.

---

## 4. API 设计

```c
/* 手动标记版 —— 用户包住任意一段代码 */
CPU_TIMER(timer_name);            /* 声明一个命名定时槽 (凑着一个 site) */
CPU_BEGIN(name);                  /* 入口: 读 TSC, 记当前深度 */
CPU_END(name);                    /* 出口: 读 TSC, 累进该 site */
void cpu_prof_report(void);       /* 打印全部 site: 总/次数/avg */
void cpu_prof_report_to(cpu_printer out, void *ctx);  /* 输出交给用户 sink */
void cpu_prof_clear(void);
```

- `timer_name` 解析到唯一 `cpu_site { caller, cycles, calls, max }`.
- `CPU_BEGIN/END` 用 `do{}while(0)` 包裹, 记录到预分配 site 表 (open array, 复刻
  memtrack 的固定容量 + 零分配原则, 见 §7).
- **输出 sink 由用户决定**: 库不硬编码 `fprintf(stderr)`. 报告参数化成一个回调
  (见 §6), 用户可让其打到 cerr/cout/文件/日志/单测捕获, 甚至不打印只取数据.

---

## 5. 三处陷阱及其处理

| 陷阱 | 现象 | 处理 |
|---|---|---|
| **多线程/多核 TSC 漂移** | 不同核心 TSC 基准不同 | 记录 `cpu_id` (rdtscp 的 ecx), 或统一到主核对齐; per-thread 累加 |
| **递归** | 同函数嵌套双计 | 维护 per-thread 深深度栈: BEGIN 压入入口 TSC, END 出栈取净差 |
| **乱序执行** | rdtsc 与代码乱排, 读数失准 | 准路径加 lfence; 对比路径用相对值即可 |

- 阻塞区 (read/sleep/锁等待) 不应被包进插桩: 它们算的是"该段循环数"而非 CPU 忙.
  文档明确要求插桩点只包**纯计算段**, 否则周期数不符" CPU 使用".

---

## 6. 数据表 + 报告 (复用 memtrack 骨架)

照搬 bcheck.c 的 memtrack 结构, 字段改语义即可:

```c
typedef struct cpu_site {
    unsigned long long caller;      /* 插桩点返回地址 (BEGIN 处)  */
    unsigned long long cycles;      /* 累计周期                   */
    unsigned long long calls;       /* 累计进入次数               */
    unsigned long long max_cycles;  /* 单次最长                   */
    int cpu_id;                     /* 采到的核心 id (rdtscp)     */
} cpu_site;
```

- 排序 + Top-N 打印: 复制 [__mem_report](lib/bcheck.c) 的插入排序逻辑.
- **输出可插拔 (用户决定 sink)**: 库把"一行"作为回调触发, 不硬编码 `fprintf(stderr)`:

  ```c
  /* 库只负责组行, 交给用户自定义的回调 */
  typedef void (*cpu_printer)(void *ctx, const char *line);
  void cpu_prof_report_to(cpu_printer out, void *ctx) {
      char line[256];
      fn(out, ctx, line);          /* 组一行(函数名/总cycle/calls/avg)后回调 */
      ...
  }
  /* 默认实现可让用户一行接管到任一处: */
  cpu_prof_report_to(line_to_file, fp);      /* cerr/cout/日志文件 皆可 */
  cpu_prof_report_to(line_to_test, &capture);/* 单测捕获断言 */
  ```

  不打印、只取数据的人也可用 `cpu_site` 数组直接遍历 (report 前先快照).
- 符号渲染: 复用弱钩子 `__bt_resolve_addr` (bt-exe.c), 输出 `func@file:line`;
  未 link -bt 时衰减为裸地址. 与 memtrack 对待该钩子完全一致.
- 这也顺带修正 memtrack 的坏味道: bcheck.c 的报告是直接 `fprintf(stderr)` (L1645,
  L1653~1700), 后续可抽成同一个可插拔 hook 复用, 属可选重构 (见 §11).
- dlfcn 已补齐 (t041 通过), dladdr 可得函数名; 但**源码行 `func@file:line` 仍走
  `__bt_resolve_addr`** (dladdr 对 PE 给不出 debug_line).

---

## 7. 实现要点 (骑 bcheck 与否)

与 memtrack 的关键差异: 插桩是**被动累加**, 无需信号驱动, 比 memtrack 更独立.

- **不内嵌 bcheck.o**: 采样器需要信号, 插桩不需要. 作为独立 `lib/cpu-prof.c`
  链接即可, 用户 `#include "cpu-prof.h"` 即用.
- 仍遵守 memtrack 的**固定容量 open array + 零分配**原则 (CPU_BEGIN 会在用户内层
  调用, 不能递归进 malloc wrapper 造成污染).
- 全局锁: 渲染报告用环形缓冲快照, 排序打印做在快照上, 避免持锁排序.

---

## 8. 文件布局 (交付物)

```
include/cpu-prof.h    # 公开 API (CPU_BEGIN/END/CLEAR/REPORT) + rdtsc 原语
lib/cpu-prof.c        # site 表 + 累加/递归栈 + 报告 (复刻 memtrack 骨架)
tests/t049_cpu.c      # 插桩几个真实热函数, 验证 总/次/avg 三份数自洽
```

- 头文件内联 rdtsc + BEGIN/END 宏 (免安装, 同 LLVM 头文件库思路).
- t049 用已知循环校验: 如 `for` 算 n 次斐波那契, 断言 calls==n、avg 在合理区间.

---

## 9. 已确认 (修正) 的环境断言

- **dlfcn/dladdr 可用**: 早期「dlfcn unresolved」记忆已过时, R10c 已补齐,
  t041 通过; bt-exe.c 已用 `dlopen(NULL)` 取主模块基址. 故符号渲染有两条路
  (dladdr 取名字族, __bt_resolve_addr 取源码行).
- `__builtin_ia32_rdtsc` **不受支持**, 退化为内联汇编已落地 (见 §3) 并验证 t049 通过.

---

## 10. 诚实边界 (不做清单)

- **非编译器全自动插桩**: 一期只做手动标记; 编译器 prologue/epilogue 自动插桩 +
  per-thread TSC 栈 + 递归栈属二期 (需改 x86_64-gen.c, 本方案明确搁置).
- 不尝试替代采样做全局利用率: 插桩测的是"该段周期数", 不做 CPU 利用率报表.
- 不校准到 ns (默认给 cycle): 频率换算做可选项.
- 整型/跨核混跑时 rdtscp 的 cpu_id 只记录不据此做严格归位 (避免过度设计).

---

## 11. 落地顺序建议

1. **验证 `__builtin_ia32_rdtsc`** 在 tcc 可发射 (小样例 + 反汇编确认).
2. **rdtsc 原语 + CPU_BEGIN/END 宏** + 单 site 累加 (先对).
3. **open array site 表 + 报告排序**, 复用 __bt_resolve_addr 渲染 → t049 通过.
4. **递归栈 + per-thread 计数** (多线程样例).
5. 与 memtrack 统一为一个 header/site 渲染帮助函数 (可选重构).

> 首期验收: t049_cpu 通过; 一个真实热函数插桩后 总/次/avg 三份数自洽, 未触碰编译器.