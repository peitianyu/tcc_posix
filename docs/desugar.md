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
| SIMD intrinsic | `_mm_add_ps(a,b)` 等 | **原样透传**(已是标准C) | 单模式 `simd.h`: tcc 与 clang 同用 `__m128` 家族 | 无 |
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

### 4.2 扩展点改写钩子 (SIMD 已闭环; operator/defer/model 已落地)

SIMD intrinsic **不需要改写**: 单模式 `lib/simd.h` 让 tcc 与 clang/gcc 共用 `__m128`
家族类型与 immintrin 标准名, 脱糖时 `_mm_add_ps` 等原样透传, clang/gcc 原生编成
`addps`(VEX/AVX 编码)。`__m128` 原生运算符(tcc 侧曾支持)已于 2026-08-25 M2 删除,
写法与 clang 交集一致。
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
- SIMD 已闭环 `tests/t046_simd.c`(2026-08-25 M2 标准交集化): 标准 `_mm_*` 内建
  交集 + `__m128` 家族类型; TCC 编译运行 PASS, `--emit-c` 脱糖(产物纯透传: 标准 C +
  immintrin.h)交 clang/gcc 编译运行 PASS 且数字一致。`__m128` 原生运算符/私有名
  已删除(见 docs/simd-standard.md §9)。
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
| P0 | SIMD 标准 intrinsic 透传(单模式 simd.h: `__m128` 家族 + immintrin 交集) | tests/t046_simd.c 闭环 | ✅ 已落地 |
| P0 | operator 脱糖: 定义改名 + 赋值右值忠实改写(类型上推 + 优先级爬升) | t050 + t053 闭环 | ✅ 已落地 |
| P1 | operator 泛化(嵌套/混合优先级完整表达式改写) | t053 + t058 闭环 | ✅ 已落地 |
| P1 | defer 脱糖(作用域逆序重放 + return 早退) | t055 + t056 闭环 | ✅ 已落地 |
| P1 | model 泛型实例化排出标准 C | t031/t032/t054 | ✅ 已落地 |
| P1 | model 函数体完整捕获(宏展开大函数不截断) | t074 int 主流程 gcc -O3 闭环 | ✅ 已落地 (见 §4.3) |
| P1 | clang 驱动脚本(win/linux) + 性能对照(tcc vs clang -O3) | script/desugar.ps1 + docs/desugar-perf.md | ✅ 已落地 (≈37×) |

## 4.3 model 函数体捕获截断修复(2026-08-24)

**现象**: `lib/stl/unordered_map.h` 的 `set/at` 方法体含 `STL_UMAP_REHASH` 大宏,
脱糖后函数体仅剩尾部几行(rehash/探查/墓碑回填尽失), 功能残缺。

**根因**: 捕获链路两处容量/锚点缺陷叠加:
1. `dg_splitw` 拆分 model 体 token 上限 **512** —— `set` 展开后 `n=549` 超限被截,
   锚点扫描失配(`FINFAIL fp=8 openb=24 n=512`, 尾 token 停在 `e->state == 1 && e ->`)。
2. `DgModelDef.ntxt` 仅 **24** 字符, 长方法名 `stl_unordered_map_contains`(25) 被截。

**修复**(均在 `src/tccpp.c`):
- token 缓冲上限 512 → `DG_TOKENCAP_N=2048`(统一常量); `set` 现完整捕获 `n=549 blen=1661`。
- `ntxt[24]` → `ntxt[64]`, 长方法名完整存储。
- 锚点式解析 + 相对深度基准(`dg_mbase`, 形体开括号处的包围深度为闭合判定基准)已落地,
  宏内花括号嵌套不再破坏配对。(此前已含 openb 定位在参数表右括号后的首个 `{`。)

**验证**: t074 全部方法体完整捕获(`set n=549 / at n=582 / contains n=176 ...`),
脱糖产物经 mingw64 `gcc -O3` 编译运行 exit 0 = 全断言通过; t063(list)/t068(string) 同绿。

**已闭环**: `operator==` 自定义类型键(`struct Mid`)脱糖改写缺陷已修复——
根因在 `dg_gbody_op_left` 的成员前缀分支只判 `"."` 未接 `"->"`, 导致 `e->key` 左操作数只取到 `key`,
`e ->` 漂移到改写调用之外, 生成 `e->operator_eq_Mid(key,key)`。
修复为 `->` 与 `.` 同等待遇(配合 `dg_gbody_obj_start` 的访问器链收全对象基址)后,
脱糖正确输出 `operator_eq_Mid(e -> key, key)`。
验证: t074(unordered_map)/t075(unordered_set) 从 FAIL 转 PASS(clang 输出 == tcc -run)。
已知剩余(与本修复无关的既有问题): t053/054/058(struct Vec3 `+` operator 改写未触发,
`invalid operands`)、t076(迭代器 `STL_Iter_*` 模型未实例化)、t052/t064(测试自报 PASS,
仅尾随空白差异)。

## 4.4 model 实例化路径统一精简(2026-08-25)

**目标**: 消除语句级与函数定义级两套嵌套 typedef 累积实现的重复。

**改动**(均在 `src/tccpp.c`):
- `dg_model_expand_src` 移除 `CString *typedefs` 出参 —— 此前函数泛型定义体走
  `typedefs` 分支手动拼 typedef 行, 语句级走 `dg_model_emit` 直打 ppfp, 两套路径
  各维护一份实现。现统一: 体内嵌套实例一律调用 `dg_model_emit`, 去重累积到文件
  作用域全局池 `dg_fout_td`, 引用以 `struct <sn>`/`union <sn>` tag 内联。
- 顺带将源文本参数由 CString(`m->body`/`m->ret`/`m->fparams`)收敛为 token 区间
  (`m->bbody`/`m->bret`/`m->bfp` + 结束下标), 免压串/回切。
- 提取 `dg_model_inst_end` 统一三条模型实例化点(verbatim 预扫、verbatim 主扫、
  fdefs 函数泛型登记)。

**构建**: `tcc-dg6.exe` 为 musl-only 自举, 需
`-Isrc -Ibin\include -Ilib\bt-inc -DONE_SOURCE=1 -DCONFIG_TCC_MUSL=1
-DCONFIG_TCC_SEMLOCK=0 -DCONFIG_TCC_PREDEFS=1`(SEMLOCK=0 去掉 winapi
CRITICAL_SECTION; PREDEFS=1 保证产物顶部不残留 `#include <tccdefs.h>`)。

**验证**: 25 个扩展回归测试 `--emit-c` 产物与重构前逐字节一致; clang 闭环
17 通过 / 8 失败, 失败集合与重构前相同(t029/t050 首次纳入 clang 集即失败,
t053/054/058/076/t052/t064 为既有问题), 无新增回归。

## 4.5 闭环补齐: defer 早退 / vptr 保真 / reflect 脱糖 / SIMD 边界 (2026-08-25)

**现状**: 全量 clang 闭环 **27 通过 / 0 失败** (t046_simd 已于 M2 闭环, 见下)。

**1) defer 早退补齐 (t029_defer 闭环)** — `src/tccpp.c`
- 原 `dg_is_defer_line` 只认"行首 defer"。换为行内任意位置 (`dg_has_defer`),
  `dg_defer_pick` 提取单个 `defer f(...);` 调用文本入栈。
- 新增 `dg_flush_defer_line` 处理含 defer 的整行: 同行多 defer 正确入栈、
  `{`/`}` 行内深度追踪、闭块逆序重放; 对 return 用 `{ emit; return; }` 包块发射
  存活 defer, 避免条件 return 破坏控制流。
- 主流程 `st.isexit`(行首 goto/break/continue) 在跳出作用域前发射存活 defer
  (t029 的 goto 跨块与 for 内 break 均依赖此)。

**2) vptr 间接调用保真 (t076_stl_iterator 闭环)** — `src/tccpp.c`
- `dg_sugar_rewrite` 对 arrow 左侧为成员访问(前置 `.` / `->`)的调用跳过, 不再把
  `it.ops->eq(&it,&end)` 误当泛型方法糖重写成 `it.eq(&ops,&it,&end)`。
- EOF 回放拆 `dg_fdefs_typedefs` / `dg_fdefs_funcs` 两段: 结构 typedef 前置到
  "最后一个回到主文件行标记之后", 使顶层用户函数(i_incr)在使用前先见合成类型,
  解决 `STL_Iter_int` "使用先于定义"。

**3) reflect 脱糖 (t051_reflect 闭环)** — `src/tccpp.c`
- 收集 struct/union/enum 定义字段(`dg_reflect_collect_line`), EOF 前置生成
  tcc-reflect.h 的 `__refl` 静态表(`dg_reflect_emit`): 含嵌套 `sub` 引用、数组
  `count`、offsetof/sizeof/_Alignof 计算、同一类型地址复用。
- verbatim 遇 `__builtin_reflect(struct T)` 改写为 `(&T_refl)`; `dg_region_rewrite`
  遇该 builtin 直接回退 verbatim(不识别)。仅当文件确实用到 reflect 才发射表,
  避免普通 struct(t076 的 inode)误连 `__refl_field`。

**4) t046_simd — 已闭环 (2026-08-25 M2, 见 docs/simd-standard.md §9)**
旧判定"语法边界不进 clang 闭环"基于双模式 struct v4f + `.x` 字段访问。M2 单模型化后
消除: `__m128` 为内建向量类型(VT_VECTOR), t046 迁标准 intrinsic 名(`_mm_loadu_si128`/
`_mm_mullo_epi32`/`_mm_setzero_si128`), 字段访问改 `((float*)&v)[0]`, tcc 独有扩展
(整型除法/原生运算符)已删除。脱糖产物纯透传(标准 C + immintrin.h), clang 编译运行与
tcc -run 数值一致: `desugar.ps1 tests/t046_simd.c` PASS。