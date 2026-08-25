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
- [x] `@listfile` 编译描述: 注释/引号/嵌套/`%if` 编译选择 (P0/P1) — 已用于自举与测试
      (build/selfhost-*.list, tests-common.list); **`%dep` 包管理与 glob 通配被
      CONFIG_TCC_MUSL 门控而 MUSL 形态已删 → 当前 POSIX 构建不可用** (KNOWN_ISSUES §3)

## P1 / Q 待办

- [x] **operator 脱糖产物 clang 验证** — 2026-08-25 完成: WSL clang 10 全量脱糖
      闭环 **32/32** (含新运算符比较/一元/自增减/复合赋值 t059, model 泛型+operator
      组合 t079)。期间修复: 泛型体算术 operator 改写 (arith=1)、同名变量跨作用域
      误改写 (dg_var_del)、desugar.ps1 UTF-8 编码。

- [ ] **termios console E2E 确认** — R6 已实现 (t040 通过)，需在真交互终端跑一次
      t040 确认 tcgetattr/TIOCGWINSZ/TCSETS 实际数值。
- [ ] **cpu-prof 报告接入 `-bt` 符号渲染 vs 独立构建双路径覆盖确认**。
- [ ] **function/model 泛型脱糖稳定性收敛** — 多类型参数 + 嵌套实例化 + 常量参数
      组合用例扩充，确保实例化点展开与标准 C 编译在所有边界下一致。
- [ ] **反射 v2** — bitfield / VLA / 嵌套递归链字段 kind。
- [ ] **`emit-c` 产物 LTO/符号可见性清理** — 正式产物面向独立库导出时的符号控制。

## P2 / 探索

- [ ] 完整 LLVM 后端 (替代脱糖为直接 AST→LLVM IR)。当前判断: 仅做自定 LLVM pass
      或跨语言时才需要，暂不立项 (docs/desugar.md 决策记录)。
- [ ] 类型化内存所有权注解 (`mut/owned/temp` + 编译期借用检查)。
- [ ] Windows+psxscl 的 clang 正式产物驱动 (复用 `.a` + 头, 盯 `-femulated-tls`
      与结构体 ABI)。