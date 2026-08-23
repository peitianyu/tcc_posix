# 脱糖输出 → clang/LLVM 正式产物

> 状态: 2026-08-23 立项. 目标: 魔改 TCC 做**快速验证前端**, 正式产物由
> **脱糖输出标准 C → clang/LLVM** 生成, 吃满 AVX/FMA 性能(补 TCC 无 AVX 短板).
> 本文把 附录 C.6 的策略固化为可执行蓝图, 含 P0 实施范围.

---

## 1. 定位与角色

| 角色 | 工具 | 职责 |
|---|---|---|
| 验证前端 | 魔改 TCC | `-run`/`-b -bt` 内存治理; operator/model/SIMD/defer 扩展验证; 秒级迭代 |
| 正式产物 | clang/LLVM | `clang -O3 -mavx2 -mfma --sysroot=<musl> out.c` 出高性能可执行 |

关键前提: **标准 C(+`<immintrin.h>` intrinsic)是底层**。用户源码主体就是标准 C,
TCC 扩展(operator/model/defer)是上部抽象, 脱糖 = 把扩展"降级"回标准 C。

---

## 2. 为什么"标准C为底层"可行且最省

- `_mm_*` 内建在 TCC 里已直接映射 SSE 指令, **命名/语义与 `immintrin.h` 一一对应**。
  SIMD 部分本就"近似标准 C", 脱糖只差 `v4f a+b`→`_mm_add_ps(a,b)` 这类(需要类型信息)。
- 无完整 AST 可回放, 从 token/解析层"透传 + 扩展点改写"最小侵入编译器。
- clang 认 `immintrin.h`, 能对 intrinsic 自由折叠/FMA, 高性能产物的优化落在 clang。

## 3. 脱糖规则表 (扩展→标准C映射)

| 扩展 | 源码 | 脱糖落点 | 需要编译器动作 | 难度 |
|---|---|---|---|---|
| SIMD 原生运算符 | `v4f a=b+c` | **原样透传**(`a=b+c`) | 双模式 `simd.h`: gcc 侧 `v4f=__m128`, 运算符原生编成 addps | 无(已落地) |
| `_mm_*` 内建 | `_mm_add_ps(a,b)` | 原样(已是标准C) | 透传 | 无 |
| operator | `a+b`(struct) | `operator+(a,b)` 函数调用 | gen_op 判 struct+存在 operator→改写 | 低 |
| model 泛型 | `model struct Vec(T)` | 实例化后具体 struct/函数 | 实例化点展开出标准C | 中 |
| defer | `defer cleanup(x)` | `__attribute__((cleanup))` / goto 展开 | defer 注册点展开 | 中 |

> operator 已在 gen_op 处**改写为调用**(编译期静态分派), 脱糖等价于**把这个改写
> 同时吐成 C 文本**(`operator+(a,b)`), 而不用恢复运算符原形。

## 4. P0 实施范围 (先打通最小闭环, 证明管线成立)

目标: 一个能跑通的 demo —— TCC 编译含 SIMD `v4f` + operator 的源码, **同时脱糖出
标准 C**, clang 编出可执行, 数字结果与 TCC 一致。

### 4.1 新增输出模式 `--emit-c` (已实现, P0 最小闭环)
新增输出开关: 不做代码生成, 而在 tccpp 的文本通道**透传**脱糖后 C 文本到
`-o <out>` / stdout。命令行长选项 `--emit-c`(单个前导 `-`, 无分离参数, 输出经 `-o` 或默认 stdout)。
```sh
tcc --emit-c -o out.desugared.c in.c
```
实作要点(2026-08-23 落地):
- `TCC_OUTPUT_DESUGAR`(6) 输出类型; 走 `preprocess_loop(s1, 1)` 与 `-E` 共用引擎,
  `desugar` 标志注入产物来源标记 `/* __TCC_DESUGAR__ ... */`。
- **保留系统 `#include`**: `parse_include` 在 desugar 分支只发射 `#include <...>` 指令、
  不随 .c 展开 —— 产物交给 clang 时由它用自己的头文件树(如 musl sysroot)重新解析,
  避免 tcc 解析后的私有类型(`fpos_t` union/`__builtin_va_list`)/绝对路径泄漏进正式产物。
  (`#include_next` 顺序敏感, 脱糖不保留)
- **预定义头不落地**: 编译需 `-DCONFIG_TCC_PREDEFS=1`, 否则产物顶部会残留
  `#include <tccdefs.h>`(tcc 私有头, clang 无此文件)。
- 产物为标准 C + 行标记(`# 1 "file"`), clang 可直接编译。

### 4.2 扩展点改写钩子 (SIMD 已落地; operator/defer/model 待 P1)

SIMD 原生运算**不需要改写**: 通过双模式 `include/simd.h` 把 `v4f` 在 gcc/clang 侧映射为
`__m128` 原生向量类型, 脱糖时 `a+b` 原样透传, 由 clang/gcc 原生编成 `addps`(VEX/AXV 编码),
无需改写成 `_mm_add_ps`。`_mm_*` 内建本就透传(命名/语义与 `immintrin.h` 一一对应)。
其余扩展落地处仍需在 codegen 之外**追加发射标准 C 文本**:

- operator 命中 → 发射 `operator+(a, b)` 调用文本。
- defer → `__attribute__((cleanup))` / goto 展开。
- model 泛型 → 实例化点展开出具体 struct/函数。

### 4.3 验收 (tests/)
- `t052_desugar.c`(已添加): 纯标准 C 透传基准。TCC `-run` 得预期结果(本机可验);
  再 `--emit-c` 脱糖, 产物 `#include <stdio.h>` 保留、正文为标准 C。
- 数字一致终验需在 **Linux/musl**(装有 clang)执行:
  `clang -O3 -mavx2 -mfma --sysroot=<musl> t052.desugared.c -o t052_release`,
  其输出与 TCC `-run` 输出逐项一致。
- SIMD 运算已闭环 `build/simd_demo.c`: 原生运算符 + 标准 `_mm_*` 内建交集; TCC 编译运行 PASS,
  `--emit-c` 脱糖(`#include "simd.h"` 保留、`a+b` 透传)交 `gcc -O3 -mavx2` 编译运行 PASS,
  且 `objdump` 确认生成 `vaddps/vmulps/vsubps/vdivps`(AVX 矢量指令, 非标量降级)。
- operator、defer 两条扩展用例待 keyword 级改写(P1)后补入。
- **operator 脱糖已闭环** `tests/t050_operator.c` + `tests/t053_operator_expr.c`(已生效):
  `--emit-c` 在 `tccpp` 的语句级缓冲(`dg_*` 模块)做 token 级改写; 两类验证均 PASS。
  - 定义改名: `operator+`(TOK_OPERATOR `operator` 关键字 + 独立 `+` token)组合识别后改名
    `operator_add`(同理 `*`→`operator_mul`、`-`→`operator_sub`), 定义签名一并保留。
  - 调用改写: `c = a + b` → `c = operator_add(a,b)`。
  - **类型向上传播 + 优先级爬升**: 右值先解析成 DGNode 树(`dg_expr` 用 precedence-climbing,
    left-assoc), 二元节点 `tag` 仅在 op 已注册且**两侧都 operator 类型**时向上继承 —— 因此
    `(a+b)*b → operator_mul(operator_add(a,b),b)`、`a+b*b-a → operator_sub(operator_add(a,operator_mul(b,b)),a)`
    完全括号/优先级忠实保留, 而 `a.x*a.x+b.y`(字段成员访问, 非 operator 运算数)原样透传不误改。
  - 文本快照(`dg_txt` 缓冲时拷贝) `+` `CString` 按 `out.size` 精确 `fwrite` 写出, 消除脏字节。
  - TCC 本地编译 PASS; `--emit-c` 产物交 WSL `gcc -O0`/`-O2 -Wall` 编译运行 PASS, 数字一致。

## 5. musl 兼容 (链接层, 非语法层)

- Linux+musl: clang 原生 `--sysroot=<musl>` 一套。
- Windows+psxscl: 编译无关的 `.a`+头,lld 可直接用; 需 clang 驱动, 盯
  `-femulated-tls`(musl-nt64 走 emutls) 与结构体 ABI。
- 源码层 POSIX/musl `#include` 在脱糖产物中照旧, 无损。

## 6. 决策记录

- **标准C为底层**(2026-08-23): 上层扩展脱糖降级, 非"全扩展语法重建"。省掉从 AST
  重建的庞大成本, 也贴合 TCC 单遍哲学 —— 脱糖 = 透传 + 扩展点改写。
- **脱糖输出 ≠ `-E`**(2026-08-23 修正): `-E` 内联展开系统头(带 tcc 私有类型与绝对
  路径), 产物 clang 无法直接食用; 脱糖**保留 `#include` 指令、不展开**, 交 clang 用
  自身 sysroot 重新解析。为满足此语义, `--emit-c` 与 `-E` 共用 `preprocess_loop` 引擎
  但在 include 处走不同分支。
- **不做完整 AST→LLVM IR 后端**: 仅做自定 LLVM pass 或跨语言时才需要, 暂不立项。

## 7. Roadmap

| 阶段 | 内容 | 交付 | 状态 |
|---|---|---|---|
| P0 | `--emit-c` 开关 + 标准C 透传 + 保留 include + 预定义头不落地 | t052 标准C基准 | 已落地 |
| P0 | SIMD 原生运算符透传(双模式 simd.h: v4f→gcc __m128, a+b 原生 addps) | build/simd_demo.c 闭环 | ✅ 已落地 |
| P0 | operator 脱糖: 定义改名 + 赋值右值忠实改写(类型上推 + 优先级爬升) | t050 + t053 闭环 | ✅ 已落地 |
| P1 | operator 泛化(嵌套/混合优先级完整表达式改写) | t053 + t058 闭环 | ✅ 已落地 |
| P1 | defer 脱糖(作用域逆序重放 + return 早退) | t055 + t056 闭环 | ✅ 已落地 |
| P1 | model 泛型实例化排出标准 C | t031/t032/t054 | ✅ 已落地 |
| P1 | clang 驱动脚本(win/linux) + 性能对照(tcc vs clang -O3) | script/desugar.ps1 + docs/desugar-perf.md | ✅ 已落地 (≈37×) |