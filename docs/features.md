# 特性深水区 (语言扩展 / 编译器 / 运行时工具)

> 本文件收纳各类特性/扩展的**详细设计**。README 只保留一页总览与模块索引，
> 每个功能的实现细节在此展开。专题设计另有独立文档的 (desugar / memory /
> cpu-prof / reflect / matrix-library / system-modules / listfile) 不在本文重复，
> 只做指引。

## 1. 调试体验 (-b 边界检查 + -bt 回溯)

编译时加 `-b`(数组/指针/堆越界检查)与 `-bt`(崩溃回溯, 文件:行 + 调用栈),
让 C 脚本的调试体验接近 Python traceback —— 越界时报 `文件:行: RUNTIME ERROR`
并附完整调用链:

```bash
build/tcc-win.exe -b -bt hello.c -o hello.exe
./hello.exe     # 越界时: probe.c:9: at oob_fn: RUNTIME ERROR: invalid memory access
                #           probe.c:22: by main
```

支持的越界检测:全局数组、栈上局部数组、堆 `malloc`、`memcpy`/字符串跨区,
以及对齐分配 (`posix_memalign` / `aligned_alloc` / `memalign`)。运行时由
`lib/bcheck.c`(CONFIG_TCC_MUSL 分支, 无 winapi/pthread/dlfcn 依赖)实现,
`lib/bt-exe.c`/`tccrun.c` 提供基于 NT x64 固定偏移 (Rip=0xf8/Rbp=0xa0/Rsp=0x98)
的堆栈回溯与向量异常处理。由 `script/build_bt.sh` 生成 `bcheck.o`/`bt-exe.o`/`bt-log.o`。

**越界对象反查变量名**:独立编译的 exe 会把最终 ELF 符号表+字符串表(一个 `.btsym`
数据段)一并链接进去,`-b` 越界时报错时会把命中的**全局/static 数组变量名**及其
实际越界字节数一并报出(堆块/栈局部无符号名则不报):

```bash
./probe_b2.exe
# BCHECK: 0x46f9a8 (size 4) is outside of the region (0x46f9a0..0x46f9a7)
#   (this is the variable 'g': address 0x46f9a0..0x46f9a8; access overran its end by 4 bytes)
```

已知限制:`-run` 与 `-b` 组合在内存执行路径上存在 tcc 深水区缺陷(经 `tcc_add_support`
链接的 bcheck.o 自举在 `-run` 下会非确定性崩溃, 排查细节见 docs/system-modules.md)。
为使 `-run -b` 可用, 现**透明回退**: 自动编译成临时 exe 再运行(边界检查走请求的独立 exe
路径, 完全正常), 用毕删除, 并透传程序参数与退出码; 故 `-run -b` 现可正常使用。

## 2. TLS (thread-local storage) — ✅ 已完成

`__thread`/`thread_local` 已通过 emutls 完整打通(编译器 + 运行时), 回归 `t047_tls`。

**设计(runtime)**: `musl-nt64/arch/nt64/src/crt_tls.c` 的
`__emutls_get_address` 为**单线程懒分配器**: 进程级布局游标(初始 `used=1`, 保证
offset=0 始终是"未分配"哨兵)给每个 `__emutls_object` 分配对齐 offset, 首次访问把
`defval`(初始值) 物化进自管 region(64KiB)。单线程语义完整正确、不越界进 musl/psx
内部 TLS 块、多个 TLS 对象互不别名。任何时刻都返回同一地址(offset 持久化)。

**设计(编译器, `src/tccgen.c`)**: `__thread` 全局不再放 `.tdata/.tbss`:
- 声明期: 在 `.data` 生成 `struct __emutls_object __emutls_v_<name>`
  `{ size, align, offset=0, defval }`。带初始化的 `__thread`, `defval` 通过
  `R_DATA_PTR` 重定位指向占位的初始值拷贝, 供运行时首次访问时物化。
- 标识符引用: 拦截 `VT_TLS` 符号, 生成
  `rax = __emutls_get_address(&__emutls_v_<name>)`; 标量/结构体设 `[rax]` 为 lvalue
  (`REG_IRET|VT_LVAL`), 数组则 `REG_IRET`(rax 即基址)。
- 已知坑: `gfunc_call` 会弹掉函数+参数, 结果需 `vpushi(0)` 占位后填 `REG_IRET`;
  描述符 offset 用 `tls_desc_ofs>=0` 作哨兵(首个描述符 offset 恰为 0)。
- 函数内 `__thread` 局部变量按普通自动变量处理(无需 emutls)。

**验证**: `tests/t047_tls.c` 覆盖初始化物化 / 数组 / 复合赋值 / 跨函数 / 取址 /
结构体 TLS / 多对象不冲突; 全部通过。

## 3. 纯 musl 编译器 (零 winapi 依赖)

`bin/tcc.exe` 是用自身按 **CONFIG_TCC_MUSL** 线路自举的纯 musl 编译器:
代码里所有 winapi 调用 —— 进程 (`_spawnvp`/`GetTickCount`/`GetCurrentProcessId`)、
内存 (`VirtualAlloc`/`VirtualFree`/`VirtualProtect`)、动态库 (`LoadLibraryA`/
`GetProcAddress`/`FreeLibrary`)、路径 (`GetModuleFileNameA`/`GetSystemDirectoryA`)、
异常 (`AddVectoredExceptionHandler`) —— 全部换成 POSIX 对等项, 编译出的 PE **导入表
为空**, 系统调用经 psxscl→ntdll 直通, 不依赖 msvcrt/kernel32/user32。

| 原生 winapi | musl 等价 |
|---|---|
| `_spawnvp` | `posix_spawn` + `waitpid` |
| `GetTickCount` | `gettimeofday` |
| `GetCurrentProcessId` / `getpid` | `getpid` |
| `DeleteFileA` | `unlink` |
| `VirtualAlloc`/`VirtualFree`/`VirtualProtect` | `mmap`/`munmap`/`mprotect` |
| `LoadLibraryA`/`GetProcAddress`/`FreeLibrary` | `dlopen`/`dlsym`/`dlclose` (psxscl dlfcn→ntdll LDR) |
| `GetModuleFileNameA` 定位 tcc 目录 | 编译期 `CONFIG_TCCDIR` 固定 |
| `GetSystemDirectoryA` | 无 (musl 线路不搜索 windows 系统目录) |
| `AddVectoredExceptionHandler` (VEH) | `sigaction` (POSIX 信号 + ucontext) |
| `RtlAddFunctionTable`/`RUNTIME_FUNCTION` | 弃用 (崩溃回溯走 sigaction) |
| `VirtualQuery` 取模块基址 (bt-exe) | `dlopen(NULL)` |

自举链路 (install.sh `[3/3]`): 用已构建的发行 TCC 以 `-DCONFIG_TCC_MUSL`
及 `CONFIG_TCCDIR` 重编自身并链 `libc-win.a` (musl libc), 产出即 `bin/tcc.exe`。

## 4. 语言扩展一览

各扩展的完整语义/边界/设计文档见 docs/matrix-library.md 附录 B、docs/reflect.md。

### 4.1 defer (t029)

Go 式作用域清理: 注册点求值、离开作用域逆序调用, 与
`__attribute__((cleanup))` 共用 scope cleanup 机制, 支持 return/goto 全路径:

```c
{ struct File f = open_file("x"); defer f.close(); }
```

### 4.2 model 泛型 (t031-t032, 常量参数 t032b)

编译期类型工厂 —— `model` 关键字定义类型/函数模板, 使用处提供具体类型参数,
编译器克隆模板并替换, 生成与手写特定类型完全一致的零开销代码。struct/union
与 function 两类均支持:

```c
model struct Array(T) { T *data; int len; };        /* 结构体模板 */
model (T) T max2(T a, T b) { return a > b ? a : b; } /* 函数模板 */

Array(float) a = { buf, 3 };        /* 实例化: 合成内部类型 Array_float */
Array(double) b;                    /* 同参缓存复用, sizeof 一致 */
if (max2(int)(3, 7) != 7) ...       /* 实例化调用: 生成内部函数 max2_int */
```

支持多类型参数、嵌套实例化 (`Array(Box(int))`)、返回类型为模板实例、泛型内嵌
结构/递归。常量参数 (t032b): `model (int N,T) T f(T a[N])` 编译期归一化数值实参,
内部名按数值而非文本签名确定。**已知限制**: 模板名与成员/参数名不得重复 (实例化
替换会冲突); 函数模板实参必须是类型 (非类型模板参数不支持)。

### 4.3 SIMD 打包内建 + 原生运算符 (t046 + x86_64-simd)

128-bit 向量类型 + 对应 SSE 内建(`_mm_*`),值直接落内存槽,内建发射原生打包 SSE
指令。双模式 `include/simd.h`: TCC 侧定义 16B 对齐 struct 由 `simd_vector_kind()`
识别、`simd_gen_op()` 钩子在 gen_op 直接发射 addps/addpd; gcc/clang 侧映射为
`__m128/__m128i` 原生向量类型。处理独立成 `src/x86_64-simd.c` 模块 (`simd_gen_op` /
`simd_builtin_dispatch` 两个钩子)。

```c
v4f a = { 1,2,3,4 }, b = { 5,6,7,8 };
v4f c = a + b;                    /* addps (float 无需 66 前缀) */
v4i r;  _mm_store_ps((float*)&r, _mm_cvttps_epi32(c));  /* 截断 f→i */
```

类型族 `v4f/v2d/v4i/v8h/v16b`。整型/double 前缀细节与除法符号差异见 README 历史
或 docs/matrix-library.md 附录。

### 4.4 运算符重载 `operator` 语法 (t050 / t058 / t059)

自定义 struct 可声明 operator 函数, 编译器在续写处按操作数静态类型改写为一次普通
函数调用 —— **编译期静态分派, 零运行时开销**。语法(二元算术/一元用 C++ 风格运算符,
比较/自增自减用派生名):

```c
struct Vec3 operator+ (struct Vec3 a, struct Vec3 b) { ... }  /* a+b */
struct Vec3 operator! (struct Vec3 a)                         /* !a */
struct Vec3 operator++ (struct Vec3 a)                        /* ++a / a++ */
int operator_eq (struct Vec3 a, struct Vec3 b)                /* a==b */
struct Vec3 c = a + b;      /* → operator+(a, b) */
```

**支持类别** (命名约定 `operator_name_token`, tccgen.c):

| 类别 | 运算符 | 函数名 | 返回 |
|---|---|---|---|
| 二元算术 | `+ - * / %` | `operator+ ... operator%` | struct |
| 一元 | `! ~` | `operator!` / `operator~` | struct |
| 比较 | `== != < <= > >=` | `operator_eq/ne/lt/le/gt/ge` | `int` |
| 自增自减 | `++ --` | `operator++` / `operator--` | struct |

- **实现**: `operator` 保留字拼单 token;`struct` 二元/一元算术与自增自减命中全局同名
  函数 (精确匹配, 无隐式转换/ADL); 比较运算符返回 `int`。`++x/x++/--x/x--` 统一按
  operator 传入副本、结果存回操作数 (前后缀值语义与 `-run` 一致)。返回 struct 依
  x86-64 ABI 走 sret/寄存器落槽。
- **复合赋值**: `a += b` 等编译期改写为 `a = a + b` (经 `operator<op>`)。
- **边界**: 精确单一匹配, 无隐式转换/成员/重载决议。
- **性能红线**: 重载是语法糖, 应转发到手写内核(如矩阵平铺 GEMM), 不得退化成标量
  `for` 循环。

### 4.5 结构体反射 `__builtin_reflect` (t051)

编译期把类型信息生成只读元数据表, 单遍友好、不建 AST (设计见 docs/reflect.md):

```c
#include "tcc-reflect.h"
const struct __refl *r = __builtin_reflect(struct Vec3);
for (int i = 0; i < (int)r->nfield; i++)
    printf("%s @%u %uB align%u\n", r->fields[i].name,
           r->fields[i].offset, r->fields[i].size, r->fields[i].align);
```

支持 struct/union 平铺字段 + 标量/枚举/指针 kind;bitfield/VLA/嵌套递归链为 v2。

### 4.6 ucontext 协程基座 (t060)

`getcontext/setcontext/makecontext/swapcontext` 由 musl 头声明但**实现缺失**，
现移植补全到 nt64 (x86_64)。协程可在**自管独立栈**上布置入口函数并往返切换:

```c
static ucontext_t main_ctx, co1;
static char st[8192] __attribute__((aligned(16)));
static void co1_fn(int a, int b, int c) { ...; swapcontext(&co1, &main_ctx); ... }
co1.uc_stack.ss_sp = st; co1.uc_stack.ss_size = sizeof st;
makecontext(&co1, co1_fn, 3, 10, 20, 30);
swapcontext(&main_ctx, &co1);   /* 切到协程; 协程 swapcontext 切回 */
```

- **实现**: [ucontext.s](file:///d:/work/tcc_posix/src/posix/musl-nt64/src/thread/nt64/ucontext.s)
  (getcontext/setcontext/swapcontext/__uc_finish) + C 版 `makecontext.c`。
  寄存器槽偏移按 `arch/nt64/bits/signal.h` (Windows NT CONTEXT 布局) 实测。
- **调用约定**: tcc 的 PE x86_64 用 **Windows x64** (首参 `%rcx`), 非 SysV `%rdi`;
  角色与 nt64 `setjmp.s` 一致；makecontext 参数按 4 个寄存器 `rcx/rdx/r8/r9`。
- **值语义**: `swapcontext` 保存 old 现场 `RSP=返回地址槽, RIP=返回地址`,
  恢复 new 时 `RSP=存值+8, jmp RIP`, 与 `getcontext(返回后)` 状态等价。
- **栈顶预留 (已修复)**: makecontext 在栈缓冲上界下方预留 Windows x64 影子区/
  参数 spill 空间。曾因带参函数把前几个参数 spill 到入口栈顶上方(如 `[rbp+0x20]`),
  越界写坏相邻全局——exit 清理时经被污染的全局函数指针槽间接调用近似 null 而崩。
  现预留 ≥0x40, 带参协程也能正常 `return` 退出 (不再需要 `_Exit`)。
- **-b 协程感知 (`__bound_add_region`)**: makecontext 经**弱引用**调用 bcheck 的
  持久登记接口, 把协程栈显式登记为检查区(不随创建它的帧返回而失效, 区别于
  FP-bound 的 `__bound_new_region`)。作用: 协程体内**显式**越界访问(数组下标/指针
  解引用落在协程栈上)能被 `-b` 报出, 且回溯能跨上下文切换解析到协程函数。
  局限(结构性): makecontext 入口的参数 spill 属**非插桩的帧内写** `[rbp/rsp+off]`,
  登记也不能拦截, 只能靠上面的栈顶预留兜底。验证 `tests/t061_corob.c`:
  `-b` 报 `outside of the region ... overran its end by 1 bytes (co_fn)`, 不带 `-b`
  照常运行。线程/进程不受此问题影响(见下)。

## 5. 运行时内存治理 (memory/mmap/arena/esc/memtrack)

经 `-b`(bcheck) 在运行时逐类抓取, 细致设计见 docs/memory-governance.md。落到
编译器便利层的实现 (全头文件, 无编译器改动):

- **统一销毁入口 `tcc_release` + 归属表 (`lib/tcc-own.h`)**; 错配 free、双释拦截。
- **mmap 区域登记 (`lib/tcc-mmap.h`)** — 映射区登记进 bcheck, `-b` 下越界被拦。
- **arena epoch 核验 (`lib/tcc-arena.h`)** — reset/destroy 后陈旧指针警告。
- **显式逃逸 (`lib/tcc-esc.h`)** — `register/revoke/check`, 逃逸指针标志。
- **memtrack 泄漏明细** — `mem_lives` 表逐活体对象记录, `__mem_report(1)` 逐条列出。
- **可插拔输出 sink** — 全局函数指针 `__tccmem_writer` (`tccmem_writer_set`), 未赋值
  默认 stderr。

测试: `build/tests/t_own.c`/`t_epoch.c`/`t_esc.c`/`t_refcnt.c`。

## 6. CPU 周期插桩 (cpu-prof)

用 `rdtsc` 插桩得到每段代码的精确周期数, 把 CPU 时间归因到具体函数/段。
纯 POSIX, 零 winapi, 零编译器改动, 不依赖 bcheck。设计见 docs/cpu-prof.md。

```c
#include "cpu-prof.h"
void hot_fn(void) { CPU_BEGIN(hot_fn); ... CPU_END(hot_fn); }
int main(void) { ... cpu_prof_report(); }
```

三份数归因: **总周期 / 调用次数 / avg cycle**。site 以返回地址为键自动登记。
递归用深度栈配平。`__builtin_ia32_rdtsc` tcc 不支持 → 退化为内联汇编 `rdtsc`。
测试: `tests/t049_cpu.c`。