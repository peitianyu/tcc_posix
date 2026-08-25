# 已知限制 / 未解决问题 (Known Issues & Bugs)

> 独立于 README 的"坑与边界"清单。README 只做概览引用。按领域分组,
> 每条标注: 现象 / 影响 / （若有）规避 / 归属文档。

## 1. 编译器 / 运行时缺陷

- **`-run` 与 `-b` 组合深水区缺陷**: 经 `tcc_add_support` 链接的 bcheck.o 自举在
  `-run` 内存执行路径下会非确定性崩溃。
  **影响**: 一度无法在 `-run` 下做边界检查。
  **规避**: 现在透明回退 —— 自动编译成临时 exe 再运行(边界检查走独立 exe 路径,
  完全正常), 用毕删除, 透传参数与退出码。故 `-run -b` 现可正常使用。
  **归属**: docs/features.md §1; 排查细节 docs/system-modules.md R13。

- **`-run` 内存执行局限**: tccrun 无模块加载概念 — `dladdr` 恒返回 0
  (t041 -run SKIP); 单文件模式不含 extra 源 (t049 依赖 lib/cpu-prof.c,
  -run SKIP)。独立 exe 与 linux 目标均已覆盖这两项。
  **归属**: test.sh run_run 注释。

- **`__builtin_ia32_rdtsc` 不受支持** → cpu-prof 退化为内联汇编 `rdtsc`/`lfence`。
  **归属**: docs/cpu-prof.md。

- **`abort()` / `assert` 不终止进程 (已修复 2026-08-25)**: nt64 `abort.c` 原用
  `raise(SIGABRT)` 后 `for(;;)` 挂死 (本端口 raise 不派发默认动作)。已改为
  `_Exit(134)` (128+SIGABRT): 有处理器则先调用 (longjmp 可拦截), 否则以
  SIGABRT 语义终止, `waitpid`/退出码可判定。assert 同样受益 (rc=134)。
  **归属**: src/posix/musl-nt64/src/exit/abort.c。

## 2. POSIX 子系统缺失 / 存根

- **pty 链路不可用**: psxscl-2015 的 /dev 分派器对 ptmx/pts 显式返回 NOT_FOUND(存根)。
  **影响**: `open("/dev/ptmx")` / 伪终端分配不可用。
  **归属**: docs/system-modules.md。

- **termios console E2E 待真 console**: termios console 映射 (TCGETS/TCSETS/
  TIOCGWINSZ ↔ GetConsoleMode/SetConsoleMode) 已实现并通过 t040, 但自动化环境
  无任何 console (GetStdHandle(STDIN)=INVALID)。
  **需做**: 在真交互终端跑一次 t040 做 E2E 数值确认。
  **归属**: docs/system-modules.md R6。

- **fenv 为 dummy 实现**: nt64 的 fenv.c 注释 "Dummy functions for archs lacking
  fenv implementation",feclearexcept/fetestexcept 等返回 0。
  **影响**: 真实舍入/异常语义不生效 (t026 只测 API 契约)。

- **部分 syscall 未注册**: 未注册 syscall 由 `__syscall_vtbl[n]==NULL → -ENOSYS`
  兜底。验证 (t034/t039) 用 getrandom=318 / fanotify_init=300 / bpf=321 — 这是
  **psxscl 专有语义** (真实 Linux 内核已实现 getrandom), linux 目标下 t034/t039
  跳过 (见 tests/wsl1.h 与测试内 #ifdef __linux__)。
  **归属**: docs/system-modules.md R1。

## 3. 编译链路 / 平台约束

- **@listfile 的 glob 通配已恢复, `%dep` 仍不可用** (2026-08-25): glob 原被
  `#ifdef CONFIG_TCC_MUSL` 门控 (MUSL 形态已删 → 不可用), 现已改为**内置实现**
  (winapi FindFirstFileA 枚举 + 自实现 fnmatch, 零外部运行时依赖) 对所有构建
  启用 — `tests/*.c`、`t0[34]*.c`、`?` 等可用。`%dep` 包管理仍仅 CONFIG_TCC_MUSL
  启用 (需 POSIX system/access; 当前形态下不可用)。
  **新增 `%out <file>` 输出指令**: 注入 `-o`, 路径基准 = listfile 所在目录
  (自包含构建, 不受调用 cwd 影响); 受 `%if` 控制; 绝对路径/盘符原样。
  **归属**: docs/listfile.md §2/§9。

- **linux 目标测试 12 项既有失败已全部修复** (2026-08-25, 现 test.sh -linux 158/158):
  - 编译器 bug: operator 后缀 ++/-- 在 SysV (linux) 寄存器返回路径下实参传成地址
    (gen_incdec_operator 的 old 槽用非 lvalue VT_LOCAL → gfunc_call 生成 lea 而非
    load) — 改 VT_LOCAL|VT_LVAL。
  - ucontext 基座: musl 1.1.11 无实现, 新增 linux x86_64 SysV 版
    (src/posix/musl-1.1.11/src/thread/x86_64/ucontext.s + makecontext.c;
    mcontext 偏移 0x28 = flags8+link8+stack_t24)。
  - emutls 运行时 (linux 版, t047); dlfcn stub (静态 ELF 无动态加载器, t041
    linux SKIP); run_linux 补 t049_cpu extra 源 (且 extra 须在 libc.a 前,
    tcc 单遍 alacarte 提取)。
  - 平台语义适配: t034/t039 (psxscl vtbl 专有, linux SKIP), t040 (select 非法
    fd: linux -1 EBADF vs psxscl 0), t042/t043/t044 (WSL1 限制: timer_create/
    mq/msgget, 经 tests/wsl1.h 检测 SKIP)。

- **CONFIG_TCC_MUSL 形态已删除 (M2, 2026-08-25)**: 曾以 CONFIG_TCC_MUSL 自举
  编译器自身 (内部 winapi 调用全换 POSIX, PE 导入表为空), 因 musl 线路 tccrun
  在 psxscl ≥1GB reserve 下重定位溢出 (-run 不可用) 已停用。现状:
  `bin/tcc.exe` 是 CONFIG_TCC_POSIX 构建 (常规 PE/msvcrt 链接), **编译产物**
  默认链 musl libc.a, 产物 PE 导入表为空、零 winapi 依赖。编译选项结论见
  docs/simd-standard.md §9。

- **winapi 不可 shim 到 musl(msvcrt 风格 stdio)**: 因 `__iob_func` 指针算术陷阱,
  msvcrt 风格 stdio 无法 shim 到 musl。

## 4. 语言扩展边界

- **operator 重载**: 支持 二元算术 `+ - * / %`、一元 `! ~`、自增自减 `++ --`
  (前后缀, 存回操作数)、比较 `== != < <= > >=`、复合赋值 `+= -= *= /= %=`;
  精确单一匹配, 无隐式转换/成员/ADL 重载决议; 不能显式写 `operator+(a,b)`
  (在表达式层是关键字)。**注**: 后缀 ++/-- 在 SysV 目标曾有实参传址 bug
  (2026-08-25 已修, gen_incdec_operator VT_LOCAL|VT_LVAL)。
  **归属**: docs/features.md §4.4。

- **model 泛型**: 模板名与成员/参数名不得重复(实例化替换会冲突); 函数模板实参
  必须是类型 (非类型模板参数不支持)。
  **归属**: docs/features.md §4.2。

- **反射 `__builtin_reflect` (v2 已完成)**: struct/union 平铺字段 + 标量/枚举/指针
  kind + **bitfield** (bit_off/bit_size) + **FAM/VLA** (size=0/count=0) + **嵌套递归链**
  (指针→struct sub, 自/互引用破环)。剩余边界: 匿名成员跳过 (无名不入表); 函数指针
  字段 kind=RE_OTHER; **脱糖产物不含位域字段** (标准 C 禁止 offsetof 位域, t051
  位域断言经 __TCC_DESUGAR__ 保护)。
  **归属**: docs/reflect.md。

- **脱糖 `--emit-c`**: `#include_next` 顺序敏感不保留; 需 `-DCONFIG_TCC_PREDEFS=1`
  否则产物残留 `#include <tccdefs.h>` (clang 无此文件)。SIMD 已收敛到 immintrin
  标准交集 (t046_simd desugar 闭环 PASS, 见 docs/simd-standard.md §9)。
  **归属**: docs/desugar.md。

- **SIMD 标准交集局限 (t046_simd, 已闭环)**: `__m128` 无原生运算符/无 `.x` 字段
  访问 (与 clang/gcc 一致); 无整型除法/8 位乘法 intrinsic (无标准等价, 已删除)。
  取低字用 `((float*)&v)[0]` 或 `_mm_cvtss_f32`。脱糖产物纯透传 (标准 C +
  immintrin.h), clang 编译运行与 tcc -run 数值一致。
  **归属**: docs/simd-standard.md §9 / docs/desugar.md §4.5。

- **TLS 相关**: emutls 为单线程懒分配器 (win 在 musl-nt64 crt_tls.c, linux 在
  musl-1.1.11/src/thread/x86_64/emutls.c, 语义一致); 函数内 `__thread` 局部变量
  按普通自动变量处理 (天然 per-thread, 不拦截不报错)。多线程 per-thread 副本
  超出范围。
  **归属**: docs/features.md §2。

## 5. 已知无碍的行为说明

- **`-run` 走完整 musl 链(非 msvcrt)**: printf/malloc/time/opendir/pthread 全套
  可用, futex 基于 ntdll RtlWaitOnAddress/RtlWakeAddressAll 真阻塞。