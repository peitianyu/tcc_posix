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
  否则产物残留 `#include <tccdefs.h>` (clang 无此文件); SIMD 的 TCC 私有内建
  (`_mm_load_epi16/_mm_div_epi32`) 不是 immintrin.h 标准内建, 涉及其用例无法直接
  交 gcc/clang 脱糖(只能用标准内建交集)。
  **归属**: docs/desugar.md。

- **TLS 相关**: emutls 为单线程懒分配器; 函数内 `__thread` 局部变量按普通自动变量
  处理 (天然 per-thread, 不拦截不报错)。
  **归属**: docs/features.md §2。

## 5. 已知无碍的行为说明

- **`-run` 走完整 musl 链(非 msvcrt)**: printf/malloc/time/opendir/pthread 全套
  可用, futex 基于 ntdll RtlWaitOnAddress/RtlWakeAddressAll 真阻塞。