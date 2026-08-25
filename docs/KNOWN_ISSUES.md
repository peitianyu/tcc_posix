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

- **`__builtin_ia32_rdtsc` 不受支持** → cpu-prof 退化为内联汇编 `rdtsc`/`lfence`。
  **归属**: docs/cpu-prof.md。

- **`abort()` / `assert` 不终止进程 (Windows nt64 musl)**: nt64 `abort.c` 用
  `raise(SIGABRT)`, 但本端口的 raise 对 SIGABRT 不派发, 落到 `for(;;)` **死循环挂死**
  (不返回, 不产生 SIGABRT 信号)。故 `assert(false)` 只打印 "Assertion failed: ..."
  到 stderr 后挂死, 无法用 `waitpid`/`WIFSIGNALED` 判信号。
  **影响**: STL 内在检测层的 `SLT_ASSERT` 在越界时打印但**不终止**; 依赖信号
  (如 tcc -b bcheck 的越界上报路径) 在此环境不可据此判定进程终止。
  **规避**: 越界判据改用 `__assert_fail` 的 stderr 输出, 或在产品代码提供自收敛
  的 `SIGABRT` 处理器; STL 用例 (t067) 以 allocator 自检 (epoch/outstanding) 作
  确定性判据, 不依赖 abort。
  **归属**: docs/stl.md §4.5-①, src/posix/musl-nt64/src/exit/abort.c。

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
  兜底。t039 断言用 getrandom=318 / fanotify_init=300 / bpf=321 验证该兜底。
  **归属**: docs/system-modules.md R1。

## 3. 编译链路 / 平台约束

- **@listfile 的 glob 通配与 `%dep` 当前不可用**: 两者被 `#ifdef CONFIG_TCC_MUSL` 门控
  (libtcc.c; MUSL 形态 tcc.exe 链 POSIX libc 才有 glob/access/system), 而 M2 已删除
  CONFIG_TCC_MUSL 形态 (docs/simd-standard.md §9) → 当前 POSIX 构建的 tcc.exe 仅支持
  `@listfile` 的 注释/引号/`@`嵌套/`%if` 编译选择。`src/*.c` 通配与 `%dep 拉依赖` 会
  原样保留/忽略, 与 docs/listfile.md §2/§4 的 P2 验收标注存在分裂。
  **规避**: 脚本侧 glob (bash `tests/t*.c`) + 显式路径; 仓库内用法见 docs/listfile.md §9。

- **linux 目标测试 (test.sh -linux) 12 项既有失败** (2026-08-25 实测 146/12):
  - 链接失败 (5): t041_dlfcn、t047_tls、t060/t061 ucontext (src/thread/nt64/ 为 Windows
    专有实现, linux 目标无 ucontext 基座)、t049_cpu (run_linux 无 extra 源逻辑,
    cpu-prof.c 未入链 — run_win 有)。
  - 运行时失败 (7): t034_enosys/t039_unsupported/t040_select_poll/t042_timer/t043_mq/
    t044_ipc/t059_operator_more — WSL 环境系统调用/等待语义差异, 与编译参数无关。
  **注意**: 编译层已修复 (build/tests-common.list 补 -I lib/-I . 后 STL/simd/reflect
  t046/t049/t051/t062-t079 全部可编, 净增 18 通过)。

- **纯 musl 编译器 (CONFIG_TCC_MUSL)** 走 POSIX 接口, 不含任何 winapi 依赖:
  PE 导入表为空, 系统调用经 psxscl→ntdll 直通。
  **代价**: musl 线路无 `GetModuleFileNameA` 定位私有目录, `CONFIG_TCCDIR` 需在
  编译期 `-D` 固定 (install.sh 已写好)。无需支持原生 win32 编译。

- **winapi 不可 shim 到 musl(msvcrt 风格 stdio)**: 因 `__iob_func` 指针算术陷阱,
  msvcrt 风格 stdio 无法 shim 到 musl。

## 4. 语言扩展边界

- **operator 重载**: 仅 `+ - * / %`, 精确单一匹配, 无隐式转换/成员/ADL 重载决议;
  一元与比较暂不做; 不能显式写 `operator+(a,b)` (在表达式层是关键字)。
  **归属**: docs/features.md §4.4。

- **model 泛型**: 模板名与成员/参数名不得重复(实例化替换会冲突); 函数模板实参
  必须是类型 (非类型模板参数不支持)。
  **归属**: docs/features.md §4.2。

- **反射 `__builtin_reflect`**: 仅 struct/union 平铺字段 + 标量/枚举/指针 kind;
  bitfield/VLA/嵌套递归链为 v2。
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

- **TLS 相关**: emutls 为单线程懒分配器; 函数内 `__thread` 局部变量按普通自动变量
  处理 (天然 per-thread, 不拦截不报错)。
  **归属**: docs/features.md §2。

## 5. 已知无碍的行为说明

- **`-run` 走完整 musl 链(非 msvcrt)**: printf/malloc/time/opendir/pthread 全套
  可用, futex 基于 ntdll RtlWaitOnAddress/RtlWakeAddressAll 真阻塞。