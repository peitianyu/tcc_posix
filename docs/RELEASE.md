# Release 记录 (CHANGELOG)

> 里程碑级变更摘要。README 只做概览。按时间序, 最新在上。
> 完整逐提交历史用 `git log --oneline`。

## 2026-08-25 — 反射 v2 + emit-c 独立库导出 (TODO P1 两项关闭)

- **反射 v2** (`__builtin_reflect`, t051): `__refl_field` ABI 40B→48B
  (`bit_off`/`bit_size` 插于 align 与 sub 之间)。命名 bitfield 入表 (存储单元偏移
  + 位宽), 匿名成员跳过; FAM/VLA (`T a[]`) size=0/count=0; **嵌套递归链**
  (指针→struct 字段链 sub, 父表先分配 + 提前缓存破环, 自引用 L.next→L / 互引用
  A↔B 均验证)。脱糖侧同步 (位域跳过 + __TCC_DESUGAR__ 保护, FAM/递归链支持,
  前向声明破互引用, used 传播仅发被引用链)。
- **emit-c 正式产物质量门禁**: desugar.ps1 加 `-Wall -Werror`; 新增
  script/lib-export.sh 独立库导出验收 (clang `-O2 -flto -fvisibility=hidden`
  编库 → nm 断言导出符号集 = 用户 API + operator 定义, model 实例/反射表全
  static → 消费端 -flto 直链运行数字一致)。修复: STL_STATIC 无条件
  `__attribute__((unused))` (tcc 不定义 __GNUC__ 曾致脱糖产物缺保护); 无 main 库
  EOF 回放顺序 (反射表须在用户 struct 定义后 + 前向声明)。
- **编译器自举修正**: build.sh 的 `[ -f ]` 短路使 build/tcc-win.exe 长期为陈旧
  二进制; 按 BOOT_TCC 配方重建 (非 MUSL 形态, -run 正常; MUSL 形态 -run 坏已确认)。
- 套件: win **81/81**, linux **161/161**, -run **158/158**, desugar **32/32**
  (-Wall -Werror), lib-export **3/3**。

## 2026-08-25 — cpu-prof 报告 `-bt` 符号渲染双路径覆盖 (t049)

- t049 内置双形态自断言: `-bt` (编译+链接均带) → 报告渲染 `func@file:line`;
  独立构建 → 回退裸地址 @0x。test.sh 增 `-bt` 变体 (win 走 tcc 自带 bt-exe.o;
  linux 手动链接补 bt-exe/bt-log/btstub-lnx.o, 链接带 -bt 才保留 .stab 调试节)。
- 纠正 3 处过期注释 (cpu-prof.c/h、tcc-own.h): "PE 后端无弱符号绑定" 不成立
  (中间对象是 ELF, 弱引用可被强定义覆盖)。
- 套件: win **81/81** (含 t049 -bt), linux **161/161**, -run **158/158**。

## 2026-08-25 — `-b` 边界检查修复 (bcheck.o 入主构建链)

- **断点**: script/build_bt.sh 编译 bcheck.o/bt-exe.o/bt-log.o 但不在主构建链
  (build.sh/install.sh 均未调用) → `tcc -b` 报 `bcheck.o not found`, 而
  features.md 宣称 -b 可用 (文档分裂)。
- **修复**: install.sh 新增 [3/3.5] 调用 build_bt.sh, 部署三个运行时到 bin/lib;
  test.sh 新增 bound_smoke (-b 越界上报) 冒烟防回归; 套件 79 → **80/80**。
- 实测: `tcc -run -b` / 独立 exe `-b` 越界均报 `BCHECK: ... outside of the
  region` + `文件:行 by main` ✓。

## 2026-08-25 — @listfile glob 恢复 + `%out` 输出指令

- **glob 通配恢复** (此前被 CONFIG_TCC_MUSL 门控而不可用, KNOWN_ISSUES §3):
  改为**内置实现** — winapi FindFirstFileA 枚举 + 自实现 fnmatch (`*` `?` `[..]`
  字符类, 大小写不敏感), 零外部运行时依赖 (编译器自身链 msvcrt, 无 POSIX glob)。
  支持最后路径段通配; 无匹配原样保留 (同 GLOB_NOCHECK)。`tests/*.c`、
  `t0[34]*.c` 现可用。
- **新增 `%out <file>` 输出指令**: 注入 `-o <path>`, 路径基准 = **listfile 所在
  目录** (origin 参数落实; 自包含构建不受调用 cwd 影响); 绝对路径/盘符原样;
  受 `%if` 控制 (条件输出名)。
- **`%dep` 仍仅 CONFIG_TCC_MUSL 启用** (需 POSIX system/access), 当前形态不可用。
- 验证: glob/%out 端到端 (不同 cwd 调用产物落 listfile 旁); test.sh **79/79**
  (win), **158/158** (linux) 无回归。
- 文档: listfile.md §2 语法表 + §9.3/§9.4; KNOWN_ISSUES/TODO 同步。

## 2026-08-25 — abort/assert 修复 + 构建链路断点修复

- **abort()/assert 挂死 → 正确终止** (KNOWN_ISSUES §1): nt64 abort.c 原
  `raise(SIGABRT)` 后 `for(;;)` (端口 raise 不派发默认动作) — 断言失败挂死无法
  判退出。改为 `_Exit(134)` (128+SIGABRT): 有处理器先调 (longjmp 可拦截),
  否则按 SIGABRT 语义终止。abort/assert 退出码现为 134 (实测)。
- **构建链路断点修复**: build_musl.sh 只产出 build/win-musl-obj/libc.a, 但
  install.sh 消费 lib/libc-win.a — build.sh [4/4] 注释误称"无需固化", 改 musl
  源后 install 仍链接旧库 (abort 修复一度不生效即因此)。补 `cp
  win-musl-obj/libc.a → lib/libc-win.a`。
- 回归: test.sh **79/79** (win), **158/158** (linux)。

## 2026-08-25 — 脱糖 clang 全量闭环 32/32 (WSL clang 实测)

- **desugar.ps1 全量扩展集 (32 测试) 交 WSL clang 10 -O3 编译运行, 与 tcc -run
  逐字节比对: 32 通过 / 0 失败** (含 t059 新运算符、t079 model 泛型+operator 组合)。
  期间修复 3 个问题:
  1. **model 泛型体内算术 operator 未改写** (t079): `dg_gbody_oprewrite` 硬编码
     arith=0 (仅比较) → `stl_accumulate(struct Pt)` 的 `init + *b` 未改写;
     改 arith=1 (与语句级一致), 改写条件仍要求两侧为 opbase 类型变量。
  2. **同名变量跨作用域误改写** (t079 `r == a+3` 被误改 operator_eq_Pt): 变量表
     按名全局首注册优先, `int *r` 与 operator+ 内 `struct Pt r` 冲突 → 声明收集
     遇非 op 类型声明 (前/前前 token 为类型关键字) 注销该名 (dg_var_del)。
  3. **desugar.ps1 编码**: tcc -run 输出 UTF-8 但 PS 5.1 按 ANSI 捕获 (`>` 又写
     UTF-16) → 捕获前设 [Console]::OutputEncoding=UTF8 + 显式写 UTF-8 无 BOM
     (t052 中文输出验证)。
- 回归: test.sh **79/79** (win), **158/158** (linux), desugar **32/32**。
  TODO P1 "operator 脱糖 clang 验证" 关闭。

## 2026-08-25 — `-run` 模式修复 + KNOWN_ISSUES/TODO 同步

- **run_run 改用 @build/tests-common.list** (listfile 化遗漏): 原 `-run` 编译缺
  `-I lib -I .` → t062-t079 (STL) + t049 全部 `lib/stl/... not found`;
  修复后 -run 全量 140/18 → **156/156**。
- **-run 已知局限 (t041/t049 SKIP)**: tccrun 内存执行无模块加载 → dladdr 返回 0
  (t041); 单文件模式不含 extra 源 → cpu-prof.c 未定义 (t049)。独立 exe / linux
  目标均已覆盖。
- **文档同步**: KNOWN_ISSUES — operator 边界更新 (比较/一元/自增减/复合赋值已支持),
  CONFIG_TCC_MUSL 形态标注已删 (M2), syscall 未注册验证注明 psxscl 专有,
  新增 -run 局限; TODO — clang 验证项更新 (WSL clang 10 可用, 闭环 27/0),
  @listfile 标注 %dep/glob 不可用。
- 三模式全绿: test.sh **79/79** (win), **158/158** (linux), **156/156** (-run)。

## 2026-08-25 — linux 目标测试 12 项既有失败全部修复 (test.sh -linux 158/158)

- **编译器 bug (t059)**: operator 后缀 `++/--` 在 SysV 寄存器返回路径下实参传成
  地址 — gen_incdec_operator 的 old 槽用 `vset(..., VT_LOCAL, addr)` (非 lvalue
  的 VT_LOCAL 是"指针值"语义), SysV gfunc_call 对 struct 实参转 LLONG 后 gv 生成
  `lea` 而非 load → operator++ 收到垃圾参数 (反汇编对比: 前缀 `mov -0xc(%rbp),%rax`
  传值 vs 后缀 `lea -0x34(%rbp),%rax` 传址)。改 `VT_LOCAL|VT_LVAL`。win (PE struct
  参数走栈 memcpy) 不受影响 — 故此前仅 linux 挂。
- **ucontext 基座 (t060/t061)**: musl 1.1.11 只有头无实现, 新增 linux x86_64 SysV
  版 src/posix/musl-1.1.11/src/thread/x86_64/ucontext.s + makecontext.c (参数
  rdi/rsi/rdx/rcx/r8/r9; mcontext 偏移 0x28 = flags8+link8+stack_t24 — 曾误算 0x20,
  实测修正)。自动入 linux libc.a。
- **emutls (t047)**: linux 版运行时 (同 nt64 语义, 单线程懒分配) 入 libc.a。
- **dlfcn (t041)**: 静态 ELF 无动态加载器 — src/posix/musl-1.1.11/src/dlfcn/dlfcn.c
  stub (dlopen 返回 NULL, 同 musl 静态语义), 测试 linux SKIP。
- **t049_cpu**: run_linux 补 extra 源 (lib/cpu-prof.c 先编译再入链); 教训: extra
  .o 须在 libc.a **前** (tcc 单遍 alacarte 提取, 库后文件引用不到库符号)。
- **平台语义适配**: t034/t039 (psxscl vtbl 专有, linux SKIP — 真实内核已实现
  getrandom), t040 (select 非法 fd: linux 立即 -1 EBADF vs psxscl 返回 0), t042/
  t043/t044 (WSL1 限制: timer_create 失败/mq ENOSYS/msgget 挂起, 均经
  tests/wsl1.h uname 检测 SKIP, 真实 Linux/WSL2 仍全量断言)。
- 双平台全绿: test.sh **79/79** (win), **158/158** (linux)。

## 2026-08-25 — 自举与测试改用 @listfile (参数集中, 脚本精简)

- **新增 build/selfhost-win.list / selfhost-linux.list**: 自举参数集中 (BOOT 发行 TCC
  与自举产物通用, 纯参数格式兼容 BOOT 基础 `@`)。build.sh `[1/4]` / install.sh `[1/3]`
  改为 `BOOT_TCC @build/selfhost-win.list`; install.sh 顺带去掉冗余 `-DONE_SOURCE=1`。
- **新增 build/tests-common.list**: 测试公共编译选项用 `%if @os == win` 一个文件服务
  win/linux 双目标; test.sh run_win/run_linux 编译命令改为 `@build/tests-common.list`
  (脚本开头 cd "$BASE" 固定相对路径基准)。
- **修复 linux 目标 STL 编译**: linux 分支此前缺 `-I lib -I .` → t062-t079/t046/t049/
  t051 在 linux 目标全部编译失败; 补上后全部可编, `test.sh -linux` 全量 128→**146**
  通过, 无回归 (剩余 12 项既有失败: ucontext 为 nt64 专有实现无 linux 基座 / t049
  缺 extra 源 / WSL 运行时差异, 记 docs/KNOWN_ISSUES.md §3)。
- **验证**: test.sh (win) 79/79; install.sh 开箱即用通过。
- **文档**: docs/listfile.md §9 记录仓库内实际用法; 标注 glob 通配与 `%dep` 被
  CONFIG_TCC_MUSL 门控 → 当前 POSIX 构建不可用 (KNOWN_ISSUES §3); features.md §3
  与 README "纯 musl 编译器" 描述同步为现状 (产物零 winapi, 编译器自身为
  CONFIG_TCC_POSIX 构建)。

## 2026-08-25 — STL M4 补全 + operator 小 struct 返回修复

- **String 自由函数** (lib/stl/string_extra.h, §7.4 C7): `STL_string_split` (含空段/
  长段 SSO 溢出/max 截断)、`STL_string_trim` (首尾空白)、`STL_string_join` (一次
  reserve) — t078。
- **算法补全** (§8 M1): `stl_remove` / `stl_unique` / `stl_accumulate` (含 struct
  operator+ 累加) — t079。
- **编译器修复**: operator 小 struct (≤8B) 寄存器返回三处 vstack 错乱 (operator_call/
  unary 调用/method sugar 的 packed struct return): vset 原地替换后单条目 vswap 越界
  (vstack leak)、gfunc_call 后原地写 vtop 覆盖外层赋值左值 (cannot convert)、vstore
  结果=src 需 vpop。`struct Pt {int x;} s = a+b;` 与 `s = a+b;` 现可编译运行。
- 套件 77 → **79/79**。

## 2026-08-25 — SIMD 标准交集化 (M2) + 编译选项瘦身
**SIMD 收敛到 immintrin 标准交集** (docs/simd-standard.md §9):

- 内核新增 `__m128/__m128d/__m128i` 内建向量类型 (VT_VECTOR), 打通聚合值路径
  (gv/vstore/返回/初始化/函数实参), 值模型与 struct 一致 (16B 内存语义)。
- 内建名收敛到标准: `_mm_load/storeu_si128`(movdqu)、`_mm_load/store_si128`、
  `_mm_mullo_epi32/16`、`_mm_setzero_si128` 等; lib/simd.h 单模式 (`v4f/v2d/v4i/
  v8h/v16b` = `__m128` 别名)。
- **删除 tcc 独有扩展**: `__m128` 原生运算符 (+ - * /)、整型除法 `_mm_div_epi*`/
  `_mm_div_epu*`、`_mm_mul_epi8`、全部私有名别名。`a+b` 现报 invalid operand
  types (与 clang/gcc 一致)。
- **t046 进 clang 闭环**: `desugar.ps1 tests/t046_simd.c` PASS (脱糖产物纯透传:
  标准 C + immintrin.h), 全量 clang 闭环 26 通过/1 失败 → **27 通过/0 失败**。
- **编译选项瘦身**: 逐一实验验证, 必要集 = `CONFIG_TCC_POSIX=1` +
  `CONFIG_TCC_PREDEFS=1`。删除 `CONFIG_TCC_MUSL=1` 及配套 (SEMLOCK=0/TCCDIR/
  MUSL_STDIO, musl 线路 tccrun 内存模式在 psxscl mmap ≥1GB reserve 下重定位
  溢出, -run 不可用) 与冗余的 ONE_SOURCE。install.sh 直接部署 [1/3] 产物。
- 全量回归: test.sh **77/77**, desugar 10/10 (含 t046)。

## 2026-08-24 — ucontext 协程基座移植 (nt64)

musl 头声明但缺失的 4 个协程原语补全到 nt64 (x86_64)：

- **新增实现**: `src/thread/nt64/ucontext.s` (getcontext/setcontext/swapcontext/
  __uc_finish) + `src/thread/nt64/makecontext.c` (C, 按 Windows x64 寄存器布置参数)。
- **调用约定**: 关键为 tcc 的 PE x86_64 用 **Windows x64 (首参 `%rcx`)** 而非
  SysV `%rdi` (同 nt64 setjmp.s) —— 纠正了初期按 rdi 存取的野指针崩溃。
- 寄存器槽偏移按 `arch/nt64/bits/signal.h` (NT CONTEXT 布局) offsetof 实测。
- **验证**: t060 协程冒烟 (makecontext 传参 + swapcontext 双向切换 + get/set
  往返) 5/5 PASS。docs/features.md §4.6。
- **修复 (2026-08-24)**: makecontext 栈顶未预留参数 spill 空间——带参协程把前几个
  参数 spill 到入口栈顶上方(越界写坏相邻全局), exit 清理经被污染的指针槽间接调用
  崩溃。已预留 ≥0x40; 带参协程现可正常 `return` 退出 (无需 `_Exit`)。

## 2026-08-23 — 运算符重载补全 + 脱糖同步 (operator → gcc/clang 产物闭环)

- **新增运算符类别** (tccgen `operator_name_token`, t059 全面回归):
  - 比较 `== != < <= > >=` → `operator_eq/ne/lt/le/gt/ge` (返回 `int`)
  - 一元 `! ~` → `operator!` / `operator~`
  - 自增自减 `++ --` → `operator++` / `operator--` (前后缀值语义一致, 存回操作数)
  - 复合赋值 `+= -= *= /= %=` → 改写为 `a = a op b`
- **脱糖同步**: tccpp `dg_*` 表驱动重构 (dg_op_tbl cover 全部可重载运算符), 表达式
  `--emit-c` 完整改写 (比较/一元/自增减/复合赋值/if-while 条件)。修 scalar 污染:
  形参 `int x` 等标量不再被误当作 operator 变量 → `x += 2` 保持原样。
- **验证**: 5 个 operator 用例 (t050/t053/t054/t058/t059) 脱糖产物全部能以 gcc 编译
  并运行通过 (环境无 clang)。docs/features.md §4.4。

## 2026-08-23 — `tcc -ar` 工具自足 (长文件名 + @listfile + r/d)

- **GNU/BSD 长名表**: 成员名 >14 字符写入 `//` 伪成员字符串表, 成员名记
  `/offset`, 读、写双向支持 — 摆脱 15 字符截断造成的静态库成员名冲突
  (libtcc1.a 内 chkstk.o 等)。读取端 (t/提取/替换) 一并还原。
- **`@listfile`**: 从文件读对象/待删列表, 规避命令行长度限制, 新建/r/d 均支持。
- **`r` 追加/替换** 与 **`d` 删除**: 读入现有归档, 同名(basename)替换保持原位、
  按名删除, 重写符号表; 目标归档延迟到全部成员处理成功后才截断, 失败保留原档。
- **目标自足**: 不再依赖外部 mingw ar 即能正确打包含长名成员的静态库。

## 2026-08-23 — 脱糖输出 → clang/LLVM 正式产物 完整闭环 (v-next)

编译管线确定"标准 C 为底层": TCC 魔改前端做**快速验证**, 正式产物由
`--emit-c` 脱糖输出标准 C → clang/LLVM 生成, 吃满 AVX/FMA。

- **`--emit-c` 输出模式**: 与 `-E` 共用 preprocess_loop 引擎但 include 处走不同
  分支 — 保留系统 `#include` 不展开、预定义头不落地 (`-DCONFIG_TCC_PREDEFS=1`)。
- **扩展点改写闭环** (tccpp `dg_*` 模块, token/precedence 级):
  - operator 完整表达式忠实改写 (嵌套/混合优先级, t058)
  - defer 作用域逆序重放 + `return` 早退深度栈 LIFO (t056)
  - model 泛型实例化排出具体 struct/函数 (含常量参数, t032b/t054)
  - SIMD `v4f a+b` 原样透传 → clang 侧 `__m128` 原生 addps (双模式 simd.h)
- **script/desugar.ps1**: host tcc emit-c + WSL clang -O3 + 数字比对一键验收。
- **性能对照** (build/simd_bench.c): tcc 2.264s vs clang -O3 ≈0.06s, **≈37×**, 校验和一致。
- **新增** docs/desugar.md (设计), docs/desugar-perf.md (性能)。

## 2026-07/08 — 系统模块补齐 (R1-R12)

- socket/process/statfs/pwdgrp/select-poll/termios-console/dlfcn/timer/mq/System-V
  ipc/aio 全落地 (t033-t045)。aio 后台线程段错误根因 (`__psx_vtbl` 未初始化) 修复。
- ENOSYS 兜底 (`__syscall_vtbl[n]==NULL → -ENOSYS`, t034/t039)。
- 均按"接口层/编译器层"合入, 不改 musl 语义。docs/system-modules.md。

## 2026-08 — 运行时内存治理 + 调试体验

- `-b` 边界检查 + `-bt` 回溯 (bcheck.o / bt-exe.o / bt-log.o): 越界报 `文件:行`,
  反查全局/static 变量名 (`.btsym`), 命中块执行/缩小分为 `t045/probe_b*`。
- 统一销毁 `tcc_release`、mmap 登记、arena epoch、逃逸声明、memtrack 泄漏明细、
  可插拔输出 sink (`__tccmem_writer`)。docs/memory-governance.md。
- `-run -b` 透明回退临时 exe (规避内存执行自举缺陷)。

## 2026-08 — 纯 musl 编译器 (零 winapi)

- CONFIG_TCC_MUSL 自举, 编译器代码去全部 winapi 依赖, PE 导入表为空, 系统调用
  经 psxscl→ntdll 直通。install.sh `[3/3]` 产出 `bin/tcc.exe`。

## 2026-08 — TLS via emutls

- `__thread/_Thread_local` 打通: 编译器生成 `__emutls_object` + 运行时懒分配器,
  关键字引用拦截 → `__emutls_get_address`。t047。

## 2026-08 — 语言扩展系列

- C 运算符重载 `operator` 语法 (t050), 编译期静态分派零开销。
- 结构体反射 `__builtin_reflect` (t051)。
- x86_64-simd 模块拆分 + 原生运算符 (simd_demo, 双模式 simd.h)。
- cpu-prof rdtsc 周期插桩 + memtrack 可插拔 sink (t049)。

## 2026-08 — `@listfile` 编译描述 (P0-P2)

- `tcc @build.txt`: `%dep` 包管理 + glob 选源 + `%if` 编译选择 + 嵌套子文件。
  docs/listfile.md。