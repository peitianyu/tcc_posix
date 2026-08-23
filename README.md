# tcc_posix — 纯 POSIX 双平台编译链

同一份 POSIX C 源码,编译出 Windows 原生 exe 与 Linux 静态 ELF,两边行为一致。

基于 TCC 0.9.28rc + musl libc + midipix psxscl(musl-libc.org / midipix 2015)。

## 快速开始

```bash
./install.sh                          # 生成自足目录 bin/ (tcc.exe + libc.a + musl 头)
bin/tcc.exe hello.c -o hello.exe      # 一条命令, 默认链 musl (零 Windows API 依赖)
```

```bash
./build.sh                            # 从源码完整重建 (自举 TCC + 后端 + musl libc)
build/tcc-win.exe examples/hello.c              # Windows exe (默认平台)
build/tcc-win.exe -platform=linux examples/hello.c  # Linux ELF
```

## 测试

```bash
./test.sh              # Windows 编译+运行 (48 测试)
./test.sh -run         # 追加 tcc -run 模式
./test.sh -linux       # 追加 Linux (WSL) 测试
```

当前 **51/51 通过** (test.sh, Windows tcc 自编译运行)。覆盖 stdio/malloc/mmap/文件/目录/时间/
宽字符/信号/tmp 映射、pthread 全套 (create/join/mutex/cond/barrier/sem/递归锁)、
线程压力、main 提前退出、tcc -run (含 futex 真阻塞)、ctype/setjmp/regex/search/
fenv/multibyte/crypt/prng 模块 (t022-t028),语言扩展 defer (t029)、
model 泛型 (t031-t032) 及 model 常量参数 (t032b)、
运算符重载 (t050, operator 语法)、结构体反射 (t051, __builtin_reflect),
SIMD 打包 SSE 内建 (t046,v4f/v2d/v4i/v8h/v16b),
CPU 周期插桩 (t049, cpu-prof rdtsc),
以及系统型回归 (t033-t045):
sched_yield、ENOSYS 兜底、socket、process、statfs/fstatfs、pwd/grp 虚拟 /etc、
unsupported、select/poll、termios console 映射、dlfcn、timer、mq、System V ipc、aio。

## 调试体验 (-b 边界检查 + -bt 回溯)

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

## TLS (thread-local storage) — ✅ 已完成

`__thread`/`thread_local` 已通过 emutls 完整打通(编译器 + 运行时), 回归 `t047_tls`。

**设计(runtime, 提交 6cc6b5c 起)**: `musl-nt64/arch/nt64/src/crt_tls.c` 的
`__emutls_get_address` 为**单线程懒分配器**: 进程级布局游标(初始 `used=1`, 保证
offset=0 始终是"未分配"哨兵)给每个 `__emutls_object` 分配对齐 offset, 首次访问把
`defval`(初始值) 物化进自管 region(64KiB), 单线程语义完整正确、不越界进 musl/psx
内部 TLS 块、多个 TLS 对象互不别名。任何时刻都返回同一地址(offset 持久化)。

**设计(编译器, `src/tccgen.c`)**: `__thread` 全局不再放 `.tdata/.tbss`:
- 声明期: 在 `.data` 生成 `struct __emutls_object __emutls_v_<name>`
  `{ size, align, offset=0, defval }`; 对应的 C 符号占据"死占位"section(存初始值)。
  带初始化的 `__thread`, `defval` 通过 `R_DATA_PTR` 重定位指向占位的初始值拷贝,
  供运行时首次访问时物化(after `put_extern_sym`)。
- 标识符引用: 拦截 `VT_TLS` 符号, 生成
  `rax = __emutls_get_address(&__emutls_v_<name>)`; 标量/结构体设 `[rax]` 为 lvalue
  (`REG_IRET|VT_LVAL`), 数组则 `REG_IRET`(rax 即基址, 不再二次解引用)。
- 已知坑: `gfunc_call` 会弹掉函数+参数, 结果需 `vpushi(0)` 占位后填 `REG_IRET`;
  描述符 offset 用 `tls_desc_ofs>=0` 作哨兵(首个描述符 offset 恰为 0)。
- 函数内 `__thread` 局部变量按普通自动变量处理(无需 emutls): 自动局部天然
  per-thread, 不拦截也不报错。

**验证**: `tests/t047_tls.c` 覆盖初始化物化 / 数组 / 复合赋值 / 跨函数 / 取址 /
结构体 TLS / 多对象不冲突; 全部通过。`bin/tcc.exe tests/t047_tls.c -o t.exe && t.exe`。

详见 docs/system-modules.md。

## 纯 musl 编译器 (零 winapi 依赖)

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

## 内存治理 (tcc-own / mmap / arena / esc / memtrack)

运行时内存错误 -- 错配 free、双释、mmap 越界、arena reset/destroy 后悬垂、
跨函数指针逃逸、泄漏 —— 由 `-b`(bcheck) 在运行时逐类抓取, 细致设计见
`docs/memory-governance.md`。落到编译器便利层的实现 (全头文件, 无编译器改动):

- **统一销毁入口 `tcc_release` + 归属表 (`lib/tcc-own.h`)**: 每个受管指针登记
  归属 (裸堆/arena/mmap/ctmem/pool), 统一入口按归属正确销毁或**拒绝并指引**,
  报错行留 `[memgov]` 前缀。错配 free、双释、对非受管指针 free 在此拦截。
- **mmap 区域登记 (`lib/tcc-mmap.h`)**: mmap 映射区经弱引用 `__bound_new_region`
  登记进 bcheck, `-b` 下越界被拦 (普通构建弱符号为 0 不生效)。
- **arena epoch 核验 (`lib/tcc-arena.h`)**: arena 带 epoch/存活计数, `reset`/`destroy`
  仍有存活指针即警告; `tcc_arena_check(a, e0)` 声明式核验陈旧指针。
- **显式逃逸 (`lib/tcc-esc.h`)**: `register(ptr,&slot,"name",epoch)/revoke/check`,
  reset 后 `check` 打出 "ESCAPED pointer ... still referenced by 'g_slot'"。
- **memtrack 泄漏明细**: bcheck 内的 `mem_lives` 表逐活体对象记录 (ptr+size+caller),
  `__mem_report(1)` 逐条列出泄漏对象 (调用点经 `__bt_resolve_addr` 归因为 func@file:line)。
- **可插拔输出 sink**: memtrack 报告去向由用户决定 —— 全局函数指针
  `__tccmem_writer` (lib/tcc-own.h 的 `tccmem_writer_set`), 用户赋值后报告每行
  经它回调 (文件/日志/单测捕获), 未赋值默认回落 stderr。注意: 不用弱函数,
  因 TCC 的 PE/COFF 后端不做弱符号绑定 (详见 memory-governance §6.5)。

测试: `build/tests/t_own.c`/`t_epoch.c`/`t_esc.c`/`t_refcnt.c` (前两个 `-b` 也跑通)。

## 系统模块可用性

已实现并通过回归 (详见 docs/system-modules.md):

| 模块 | 状态 |
|---|---|
| socket / network | ✅ t035 |
| process | ✅ t036 |
| statfs / fstatfs | ✅ t037 |
| pwd / grp (虚拟 /etc 映射) | ✅ t038 |
| select / poll | ✅ t040_select_poll |
| termios console 映射 | ✅ t040_termios (E2E 待真 console) |
| dlfcn | ✅ t041 |
| timer | ✅ t042 |
| mq | ✅ t043 |
| System V ipc (msg/sem/shm) | ✅ t044 |
| aio | ✅ t045 (后台线程退出段错误已修复) |
| ENOSYS 兜底 / sched_yield / env PATH | ✅ t034/t039 / t033 / t008 |

## CPU 周期插桩 (cpu-prof)

用户端 C 库, 用 `rdtsc` 插桩得到每段代码的**精确周期数**, 把 CPU 时间归因到具体
函数/段。纯 POSIX, 零 winapi, 零编译器改动, 不依赖 bcheck。设计见 docs/cpu-prof.md。

```c
#include "cpu-prof.h"            /* lib/ 下; 编译需有 -I <lib目录> */

void hot_fn(void) {
    CPU_BEGIN(hot_fn);            /* 入口: 读 TSC, 按调用点自动建 site */
    ... 纯计算段 ...
    CPU_END(hot_fn);              /* 出口: 读 TSC, 累进该 site */
}
int main(void) {
    ...
    cpu_prof_report();                 /* 默认 stderr */
    cpu_prof_report_to(my_printer, ctx); /* 或交给用户 sink */
}
```

- 三份数归因: **总周期 / 调用次数 / avg (每调用周期)**, 采样分不清"高频低单次"与
  "低频高单次", 插桩的 `avg cycle` 能 —— 这是选插桩的根本理由 (度 CPU 忙用, 不是
  全局利用率)。
- site 以 `CPU_BEGIN` 处返回地址为键自动登记 (固定容量 open array + 零分配,
  memtrack 骨架); `#name` 字符串化为报告标签; 同函数不同调用点是独立 site。
- 递归用深度栈配平 (BEGIN/END 对), 每对累加自身跨幅 (inclusive)。
- 陷阱: `__builtin_ia32_rdtsc` tcc 不支持 → 退化为内联汇编 `rdtsc`/`lfence`。
  插桩点只包**纯计算段** (io/锁/休眠不算 CPU 忙用)。
- 输出可插拔: 报告每行交给回调 `cpu_printer(void *ctx, const char *line)`, 不写死
  stderr, 可打到 cerr/cout/文件/日志/单测捕获, 或只取 `cpu_prof_sites` 数据。
- 符号渲染: 复用弱钩子 `__bt_resolve_addr` (link `-bt`), 输出 `func@file:line`。

测试: `tests/t049_cpu.c` — 编译: `bin/tcc.exe -I lib tests/t049_cpu.c lib/cpu-prof.c -o t.exe`。

## 语言扩展

**defer 语句** (t029):Go 式作用域清理,注册点求值、离开作用域逆序调用,与
`__attribute__((cleanup))` 共用 scope cleanup 机制,支持 return/goto 全路径:

```c
{ struct File f = open_file("x"); defer f.close(); }
```

**model 泛型** (t031-t032):编译期类型工厂 —— `model` 关键字定义类型/函数模板,
使用处提供具体类型参数,编译器克隆模板并替换,生成与手写特定类型完全一致的
零开销代码。struct/union 与 function 两类均支持:

```c
model struct Array(T) { T *data; int len; };        /* 结构体模板 */
model (T) T max2(T a, T b) { return a > b ? a : b; } /* 函数模板 (无 function 关键字) */

Array(float) a = { buf, 3 };        /* 实例化: 合成内部类型 Array_float */
if (a.data[1] == 2.5f) ...          /* 字段访问与普通 struct 无异 */
Array(double) b;                    /* 同参缓存复用, sizeof 一致 */
if (max2(int)(3, 7) != 7) ...       /* 实例化调用: 生成内部函数 max2_int */
if (max2(double)(2.5, 1.5) != 2.5) ...
```

实现机制:模板定义仅记录 token 流 (不生成代码);实例化时记录实参 token,
合成内部名 (`Array_float` / `max2_int`) 缓存查重,替换类型参数后重放走标准
解析 (struct_decl / decl)。函数体延迟到文件末尾编译 (避免插入调用方函数)。
支持多类型参数、嵌套实例化 (`Array(Box(int))`)、返回类型为模板实例、泛型内嵌
结构/递归。已知限制:model 模板名与成员/参数名不得重复 (实例化替换会冲突);
函数模板实参必须是类型 (非类型模板参数不支持)。

**model 常量参数** (t032b):model 模板的常量(非类型)参数 —— `model struct Mat(T,int R,int C)`、
`model (int N,T) T f(T a[N])`。编译期把每个常量实参归一化求值(`2+2`→`3`),
不同拼写共享同一缓存,生成的内部名按**数值**而非文本签名确定:

```c
model struct Mat(T, int R, int C) { T d[R*C]; };   /* 数值尺寸参数 */
model (int N, T) T dot(T a[N], T b[N]) { T s = 0; for (int i=0;i<N;i++) s+=a[i]*b[i]; return s; }

Mat(float, 3, 3) m;              /* 实例化: 合成 Mat_float_INT3_INT3 */
if (dot(3)(double)(a, b) != s)   /* 函数模板: 常量3 + 类型double */
```

**SIMD 打包内建** (t046):128-bit 向量类型 + 对应 SSE 内建(`_mm_*`),值直接落
内存槽,内建发射原生打包 SSE 指令:

```c
typedef void v4f __attribute__((vector_size(16)));   /* 4x float */
v4f a = { 1,2,3,4 }, b = { 5,6,7,8 };
v4f c = a + b;                    /* addps (float 无需 66 前缀) */
v4i r;  _mm_store_ps((float*)&r, _mm_cvttps_epi32(c));  /* 截断 f→i */
```

类型族 `v4f/v2d/v4i/v8h/v16b`(还有 `v4f2i` 等转 dtype):
- 向量常量参数/运算/字段都正常,支持与标量混算、转置/抽取 (`_mm_*`)。
- 打包整型与 double 指令必须带 **66 前缀**(movdqa/paddd 66 0F,movapd/addpd 66 0F),
  float 的 movaps/addps 无需前缀。
- 打包整型除法是**符号无关**的技术例外:仅除法区分有无符号
  (`_mm_div_epu8/16/32` 用 movzx+div,`_mm_div_epi8/16/32` 用 movsx+idiv),
  add/sub/mul 无论符号位完全一致。8/16-bit 的除法必须在寄存器加载除数
  (不能 `idivl [mem32]`,否则会读到相邻元素高字节)。

**运算符重载 `operator` 语法** (t050, 独立语言扩展):自定义 struct 可声明
`operator+ - * /` 等函数,编译器在 `a+b` 处按操作数静态类型改写为一次普通函数
调用 —— **编译期静态分派, 零运行时开销**(与 SIMD 硬编码特化相对, 是同一系统
里泛化的一极)。设计见 docs/matrix-library.md 附录 B:

```c
struct Vec3 { float x, y, z; };
struct Vec3 operator+ (struct Vec3 a, struct Vec3 b) {
    struct Vec3 r = { a.x+b.x, a.y+b.y, a.z+b.z }; return r;
}
struct Vec3 operator* (struct Vec3 a, struct Vec3 b) { ... }

struct Vec3 c = a + b;   /* → operator+(a, b)  */
struct Vec3 d = a * b;   /* → operator*(a, b)  */
struct Vec3 e = a + b*b; /* 优先级由语法树天然保持: a + (b*b) */
```

- **实现**: `operator` 保留字 (tcctok.h);声明符把 `operator '+'` 拼成单一函数名
  token (`operator+`);`gen_op` 遇 struct 二元算术, 查全局同名 `operator<op>`
  函数 (精确匹配, 无隐式转换/ADL), 命中则 `vpushsym+vpushv+gfunc_call(2)` 改写;
  返回 struct 依 x86-64 ABI 走 sret 隐藏指针或寄存器落槽 (仿 unary 返回)。
- **边界**: 仅 `+ - * / %`,精确单一匹配,无隐式转换/成员/重载决议;多候选与
  一元/比较暂不做;不显式调用 `operator+`(a,b)(它在表达式层是关键字)。
- **性能红线**:重载是语法糖,应转发到手写内核(如矩阵平铺 GEMM),不得退化成
  标量 `for` 循环。

**结构体反射 `__builtin_reflect`** (t051, 独立语言扩展):编译期把类型信息(名字/类型/
偏移/大小/对齐)生成只读元数据表,单遍友好、不建 AST(设计见 docs/reflect.md):

```c
#include "tcc-reflect.h"
const struct __refl *r = __builtin_reflect(struct Vec3);
for (int i = 0; i < (int)r->nfield; i++)
    printf("%s @%u %uB align%u\n", r->fields[i].name,
           r->fields[i].offset, r->fields[i].size, r->fields[i].align);
```

- 复用 TCC 已有机制 (`parse_builtin_params("t")` 解析类型、`struct_layout` 的字段偏移、
  `type_size`、写 `.rdata` + `greloca` 段内重定位),本表多类型时按段内绝对偏移落位。
- 支持 struct/union 平铺字段 + 标量/枚举/指针 kind;bitfield/VLA/嵌套递归链为 v2。

## 开发-验证 + 正式产物策略 (TCC 魔改前端 → clang/LLVM)

本项目把编译器用作 `TCC 魔改前端`(operator/model/SIMD/defer 等扩展)做**前期验证**,
正式产物交给 **clang/LLVM** 生成 —— 因 clang 不认 TCC 扩展语法, 正式产物由 TCC 前端
**脱糖输出标准 C (`gnu11` + `<immintrin.h>` intrinsic)**, 再 `clang -O3 -mavx2 -mfma`
编译, 吃满 LLVM 自动向量化/FMA/内联(补 TCC 无 AVX/FMA 的短板)。映射:

| 扩展 | 脱糖落点 |
|---|---|
| `operator a+b` | `operator+(a,b)` 函数调用 |
| `model` 泛型 | 实例化后的具体类型/函数 |
| SIMD `v4f` | `_mm_*` SSE intrinsic |
| `defer` | `__attribute__((cleanup))` 或 goto 展开 |

musl 是 libc 非语法: Linux+musl 用 clang `--sysroot` 原生; Windows+psxscl 复用
`.a`+头、另写 clang 驱动(盯 `-femulated-tls` 与 ABI)。完整决策链见
docs/matrix-library.md 附录 C。

## 已知限制

- **纯 musl 编译器 (CONFIG_TCC_MUSL)** 走 POSIX 接口, **不含任何 winapi 依赖**:
  PE 导入表为空, 系统调用经 psxscl→ntdll 直通。代价是 musl 线路无
  `GetModuleFileNameA` 定位私有目录, `CONFIG_TCCDIR` 需在编译期 `-D` 固定
  (install.sh 已写好); 无需支持原生 win32 编译。
- **pty 链路不可用** — psxscl-2015 的 /dev 分派器对 ptmx/pts 显式返回
  NOT_FOUND(存根)。termios console 映射 (TCGETS/TCSETS/TIOCGWINSZ,
  GetConsoleMode/SetConsoleMode) 已实现并通过 t040, 但本自动化环境无 console,
  需在真交互终端跑一次 t040 做 E2E 数值确认。
- **fenv 为 dummy 实现** — nt64 的 fenv.c 注释 "Dummy functions for archs
  lacking fenv implementation",feclearexcept/fetestexcept 等返回 0,真实
  舍入/异常语义不生效 (t026 只测 API 契约)。
- **`-run` 模式** 走完整 musl 链 (非 msvcrt):printf/malloc/time/opendir/pthread
  全套可用,futex 基于 ntdll RtlWaitOnAddress/RtlWakeAddressAll 真阻塞。
