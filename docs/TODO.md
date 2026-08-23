# TODO / 路线图

> 待办与后续方向。README 只做概览。状态图例: `[x]` 完成, `[ ]` 未做。

## P0 已完成 / 稳定性

- [x] 全部系统模块 R1-R12 (socket/process/statfs/pwdgrp/select/termios/dlfcn/
      timer/mq/ipc/aio) — docs/system-modules.md
- [x] `-b` 边界检查 + `-bt` 回溯 + 越界反查变量名 (t045/t046, probe_b4)
- [x] TLS via emutls (t047)
- [x] 内存治理 5 层 (t-own/epoch/esc/refcnt, memtrack)
- [x] 语言扩展: defer (t029)、model (t031-t032b)、operator (t050)、reflect (t051)、
      SIMD 原生运算符 (t046 + x86_64-simd, simd_demo)
- [x] 脱糖闭环: `--emit-c` + operator/defer/model + clang 驱动 + 性能对照 (≈37×)
      — docs/desugar.md / docs/desugar-perf.md
- [x] `@listfile` 编译描述 P0-P2 (`%dep`/glob/`%if`/嵌套) — docs/listfile.md

## P1 / Q 待办

- [ ] **termios console E2E 确认** — R6 已实现 (t040 通过)，需在真交互终端跑一次
      t040 确认 tcgetattr/TIOCGWINSZ/TCSETS 实际数值。
- [ ] **cpu-prof 报告接入 `-bt` 符号渲染 vs 独立构建双路径覆盖确认**。
- [ ] **function/model 泛型脱糖稳定性收敛** — 多类型参数 + 嵌套实例化 + 常量参数
      组合用例扩充，确保实例化点展开与标准 C 编译在所有边界下一致。
- [ ] **反射 v2** — bitfield / VLA / 嵌套递归链字段 kind。
- [ ] **operator 泛化** — 一元与比较运算符、多候选/重载决议 (README 标注暂不做)。
- [ ] **`emit-c` 产物 LTO/符号可见性清理** — 正式产物面向独立库导出时的符号控制。

## P2 / 探索

- [ ] 完整 LLVM 后端 (替代脱糖为直接 AST→LLVM IR)。当前判断: 仅做自定 LLVM pass
      或跨语言时才需要，暂不立项 (docs/desugar.md 决策记录)。
- [ ] 类型化内存所有权注解 (`mut/owned/temp` + 编译期借用检查)。
- [ ] Windows+psxscl 的 clang 正式产物驱动 (复用 `.a` + 头, 盯 `-femulated-tls`
      与结构体 ABI)。