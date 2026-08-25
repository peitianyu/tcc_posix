# Release 记录 (CHANGELOG)

> 里程碑级变更摘要。README 只做概览。按时间序, 最新在上。
> 完整逐提交历史用 `git log --oneline`。

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