# TODO / 路线图

> 待办与后续方向。README 只做概览。状态图例: `[x]` 完成, `[ ]` 未做。

## P0 已完成 / 稳定性

- [x] 全部系统模块 R1-R12 (socket/process/statfs/pwdgrp/select/termios/dlfcn/
      timer/mq/ipc/aio) — docs/system-modules.md
- [x] `-b` 边界检查 + `-bt` 回溯 + 越界反查变量名 (t045/t046, probe_b4)
- [x] TLS via emutls (t047)
- [x] 内存治理 5 层 (t-own/epoch/esc/refcnt, memtrack)
- [x] 语言扩展: defer (t029)、model (t031-t032b)、operator (t050/t058/t059)、reflect (t051)、
      SIMD 标准 intrinsic 交集 (t046 + x86_64-simd; 2026-08-25 M2 收敛, 原生运算符/私有名已删)
- [x] operator 补全: 比较 (== != < <= > >=)、一元 (! ~)、自增自减 (++ --)、
      复合赋值 (+= -= *= /= %=) — t059; 已同步 `--emit-c` 脱糖
- [x] 脱糖闭环: `--emit-c` + operator/defer/model + clang 驱动 + 性能对照 (≈37×)
      — docs/desugar.md / docs/desugar-perf.md
- [x] `@listfile` 编译描述: 注释/引号/嵌套/`%if` 编译选择/glob 通配 (内置 winapi
      实现, 2026-08-25 恢复)/`%out` 输出指令 (路径基准 = listfile 目录) — 已用于
      自举与测试 (build/selfhost-*.list, tests-common.list); **`%dep` 包管理仍被
      CONFIG_TCC_MUSL 门控而 MUSL 形态已删 → 当前不可用** (KNOWN_ISSUES §3)

## P1 / Q 待办

- [x] **operator 脱糖产物 clang 验证** — 2026-08-25 完成: WSL clang 10 全量脱糖
      闭环 **32/32** (含新运算符比较/一元/自增减/复合赋值 t059, model 泛型+operator
      组合 t079)。期间修复: 泛型体算术 operator 改写 (arith=1)、同名变量跨作用域
      误改写 (dg_var_del)、desugar.ps1 UTF-8 编码。

- [ ] **termios console E2E 确认** — R6 已实现 (t040 通过)，需在真交互终端跑一次
      t040 确认 tcgetattr/TIOCGWINSZ/TCSETS 实际数值。
- [x] **cpu-prof 报告接入 `-bt` 符号渲染 vs 独立构建双路径覆盖确认** — 2026-08-25 完成:
      双平台 `-bt` (编译+链接均带) 报告渲染 `func@file:line`, 独立构建回退裸地址;
      t049 自断言两形态 + test.sh 增 `-bt` 变体 (win 走 tcc 自带 bt-exe.o/bt-log.o;
      linux 手动链接补 bt-exe/bt-log/btstub-lnx.o)。期间发现并修:
      - **linux 手动链接丢 .stab**: tccelf.c 仅 `do_debug` 时合并输入 .o 的调试节,
        `-nostdlib` 链接必须带 `-bt` 才能保留; 且 `-nostdlib` 跳过 tcc_add_btstub,
        新增 tests/btstub-lnx.c 复刻 (rt_info + 构造函数, 用 tcc 自动生成的
        `__start_stab/__stop_stab`)。
      - **纠正 3 处过期注释** (cpu-prof.c/h、tcc-own.h): "PE 后端无弱符号绑定"
        不成立 —— 中间对象是 ELF, PE/ELF 链接共用 tccelf.c 解析, 弱引用可被强定义覆盖。
- [x] **function/model 泛型脱糖稳定性收敛** — 2026-08-25 完成: 新增
      tests/t032c_model_edge.c 覆盖多类型参数 (Pair2/Tri + 嵌套实参)、嵌套实例化
      2/3 层 (Array(Box(int))/Wrap(Array(Box(int))))、泛型递归 (struct 关键字/裸名/
      双自引用字段)、常量参数组合 (Grid→Mat 常量传嵌套、2+2 归一化)、函数泛型嵌套
      实例类型、缓存一致性。期间修复 4 处边界缺陷 (tcc 编译期 + 脱糖):
      - **泛型递归自引用**: `struct Node(T) { T v; struct Node(T) *next; }` 实例化时
        字段里的 `Node(int)` 无限重实例化 → 缓存查重放宽到实例化中占位 (c==-1
        亦命中); TOK_STRUCT/UNION 分支识别 `struct Node(` 走 model_instantiate。
      - **嵌套实例化实参逗号误判**: 定义期"类型参数与成员名冲突"检查无括号深度,
        Grid 的 `Mat(T, N, 2) row` 里 T 后逗号被当成员分隔 → 括号内跳过检查。
      - **脱糖双 struct 关键字**: `struct Node(T)` 展开为 `struct struct Node_int`
        → phase2 展开遇 struct/union 后跟嵌套实例时吞掉关键字。
      - **脱糖嵌套实例化栈溢出**: expand_src 的 w/w2 各 256KB 栈数组 ×2 层嵌套
        (Grid→Mat) 击穿 1MB 栈 → 改堆分配 (基线即崩, 非本次引入)。
- [x] **反射 v2** — bitfield / VLA / 嵌套递归链字段 kind — 2026-08-25 完成:
      __refl_field 扩到 48B (bit_off/bit_size 插于 align 与 sub 间); 命名 bitfield
      入表 (存储单元偏移 + 位宽), 匿名成员跳过; FAM/VLA (`T a[]`) size=0/count=0;
      指针→struct 字段链接 sub (父表先分配 + 提前缓存破环, 自引用/互引用可用);
      脱糖侧同步: 位域跳过 (标准 C 禁 offsetof 位域, t051 断言 __TCC_DESUGAR__ 保护),
      FAM size=0、指针 sub、前向声明破互引用、仅发被引用链 (used 传播防 unused-const)。
- [x] **`emit-c` 产物 LTO/符号可见性清理** — 2026-08-25 完成: 正式产物质量门禁
      -Wall -Werror 纳入 desugar.ps1; 独立库导出验收 script/lib-export.sh (clang
      -flto -fvisibility=hidden 编库 + nm 导出符号集断言 + 消费端链接运行)。期间修:
      - **STL_STATIC 丢 unused**: tcc 不定义 __GNUC__ → 宏退化为裸 static, 脱糖产物
        交 clang -Wall 报 -Wunused-function; 改无条件 `static __attribute__((unused))`
        (tcc 亦支持该 attribute, 见 tcctok.h TOK_ATTRIBUTE2)。
      - **无 main 库反射表顺序**: EOF 回放 ins==hdr → 反射表发在用户 struct 定义前,
        clang 报 "offsetof of incomplete type"; 改 nomain 分支: 函数泛型先落地 →
        用户内容 → 反射表末尾 (含前向声明, 用户函数体引用 _refl 表)。
      - 测试源 hygiene: t054 死变量、t056 缺 string.h、t071 死变量 (门禁暴露)。

## P2 / 探索

- [ ] 完整 LLVM 后端 (替代脱糖为直接 AST→LLVM IR)。当前判断: 仅做自定 LLVM pass
      或跨语言时才需要，暂不立项 (docs/desugar.md 决策记录)。
- [ ] 类型化内存所有权注解 (`mut/owned/temp` + 编译期借用检查)。
- [ ] Windows+psxscl 的 clang 正式产物驱动 (复用 `.a` + 头, 盯 `-femulated-tls`
      与结构体 ABI)。