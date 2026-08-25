# SIMD 标准 intrinsic 单模型实现方案

> 目标：让 TCC 的 SIMD 支持与 clang 语法一致——以标准 `__m128 / __m128d / __m128i`
> 为唯一向量载体、`_mm_*` 收敛到标准 intrinsic 交集，使 `--emit-c` 对 SIMD **纯透传**
> （零定向改写），并让 t046 等 SIMD 验收进入 clang 闭环。
>
> 原则：不追求 TCC 全实现 clang/GCC vector 扩展（过度），只求「标准交集」单模型；私有
> 扩展分流（可映射者做标准组合，不可映射者明确标为 tcc 独有、不进 clang 闭环）。

---

## 1. 现状剖析（已探查，`src/` 落点）

| 关注点 | 现状 | 文件 |
|---|---|---|
| 类型系统 | `VT_BTYPE` 为 4bit mask(0x0f)，已用 0–11，**12–15 空闲** | src/tcc.h:1060 |
| 无 `__m128` | TCC 无 `__m128`/`VT_VECTOR`/vector 类型 | src/tcc.h |
| 识别方式 | 按名反射 `<simd.h>` 里 `typedef struct v4f` 的符号判定向量 | src/x86_64-simd.c `simd_vtype`/`simd_vector_kind` |
| 内建 | `TOK_mm_*` 一整族 tcc 自有内建，作用于上述 struct | src/tcctok.h:179-`;` src/x86_64-simd.c |
| 运算符 | `v4f+v4f` → `gen_op()` → `simd_gen_op()` 发射 SSE | src/x86_64-simd.c |
| 17 字节槽 | `simd_emit_vzero`/`simd_finish_result` 用 get_temp_local_var(16,16) 槽 | src/x86_64-simd.c |

## 2. 设计总览

标准 `__m128` 家族作为内建**基础类型**（新增 `VT_VECTOR`），三种以 `t->ref` 区分
（f=4×float / d=2×double / i=16×byte整数）。`simd_vtype`/`simd_vector_kind` 从"反射
struct"改为按 `VT_VECTOR + ref` 识别；`_mm_*` 内建与原生运算符作用其上。simd.h 单模式化
为 `__m128`（`v4f/v2d/v4i/v8h/v16b` 保留为兼容别名），并迁入库受控路径入 git。

收益：脱糖时类型名/函数名/运算符在 tcc 与 clang 目标上**逐字一致** → `--emit-c` 纯透传。

## 3. 分步实现

### 3.0 前置：simd.h 归属迁移（独立可先行）
- 新建 `lib/simd.h`（受控、入 git），内容为单模式标准版（见 3.5）。
- `src/posix/musl-nt64/include/simd.h`（当前在 git-ignored 目录）随后废弃。
- `script/desugar.ps1` 的 INC 已含 `-I lib`（本轮已加），无需改；`tests` 的
  `#include "simd.h"` 沿 -I lib 解析。
- 清理 `__TCC_DESUGAR__` 分支（单模式后不再需要双模式）。

### 3.1 内核新增 `VT_VECTOR` + `__m128` 家族内建类型（核心）
- `src/tcc.h`
  - `#define VT_VECTOR 12`（用 VT_BTYPE 空闲位；`__m128i` 走整型语义）。
  - 类型判定宏：`IS_VECTOR(t)`、`vector_kind(t->t & VT_BTYPE)`。
- `src/tccgen.c`（声明与内建类型解析）
  - 把 `__m128 / __m128d / __m128i` 注册为内建标识符，映射到 `{VT_VECTOR, ref→预设 kind 符号}`。
  - `type_size()` / type 对齐路径：VECTOR 返回 16 / 16 对齐（复用或新增分支）。
  - 三种以 `ref` 指向的 `Sym`（名字 `__m128`/`__m128d`/`__m128i`）区分。
- 明确简化：不引入完整 GCC `vector_size` 扩展语法，只登记这三个固有名。

### 3.2 `simd_vtype` / `simd_vector_kind` 改认标准类型（x86_64-simd.c）
- `simd_vector_kind(CType*)`：`(t->t & VT_BTYPE)==VT_VECTOR` 时按 `t->ref == g_v4f/g_v2d/g_v4i/g_v8h/g_v16b`（内建符号）返回对应 kind；unconditionally 对普通 struct 返回 -1。
- `simd_vtype(kind)` 返回对应内建 `__m128` CType（替代原先 `sym_find("v4f")` 反射）。
- `simd_kind[]` 表：`name` 字段改为 `__m128/__m128d/__m128i`（v4i/v8h/v16b 同为 `__m128i`，靠 kind 区分语义）；`esize/add_op/sub_op/mul_op` 保留。

### 3.3 `_mm_*` 内建对齐标准名（tcctok.h + x86_64-simd 派发）
- 保留已与标准同名的（`_mm_load_ps/_mm_add_ps/_mm_min_ps/_mm_cmpeq_ps` 等），这些现在作用于 `__m128`。
- 改名到标准 intrinsic：
  - `_mm_load_epi32/16/8` → `_mm_loadu_si128`（或 `_mm_load_si128`，对齐场景）；`_mm_store_epi32/16/8` → `_mm_storeu_si128` / `_mm_store_si128`。
  - `_mm_mul_epi32` → `_mm_mullo_epi32`（逐元素 int32 乘积语义）。
  - `_mm_mul_epi16` → `_mm_mullo_epi16`；`_mm_mul_epi8` → tcc 私有(8 位低字节乘积)，按 3.6 处理。
- 私有/无标准等价者（见 §4）分流。

### 3.4 原生运算符在 `__m128`
- `gen_op` 对 `VT_VECTOR` 操作数路由 `simd_gen_op`（现有 `simd_vector_kind` 判定替换为 `IS_VECTOR`）。
- 支持 `+ - * /`（f/d 用 pack 指令；整型 `*` 用 `pmulld/pmullw`，`/` 走 §4 决策）。

### 3.5 simd.h 单模式化（lib/simd.h）
```c
#include <immintrin.h>
typedef __m128  v4f;   /* 兼容别名 */
typedef __m128d v2d;
typedef __m128i v4i, v8h, v16b;
```
- clang 与 TCC 读同一份；tcc 因 3.1 的 `__m128` 支持可直接编译标准写法。

### 3.6 t046 迁移到标准交集（tests/t046_simd.c）
- `_mm_load/store_epi*` → `_mm_load/storeu_si128((const __m128i*)p)`。
- `_mm_mul_epi32` → `_mm_mullo_epi32`。
- `dacc.x` → `dacc[0]`（`__m128` clang 支持下标；tcc 的 `__m128` 下标访问经 3.1 一并支持，或 tcc 侧用 `simd_elt` 生成取低字）——也可提供访问宏 `VEC_X(v)`。
- 整型除法 `_mm_div_epi32/16/8/epu16`：见 §4 决策 ②。

## 4. intrinsic 标准迁移映射表

| tcc 私有名 | 标准 intrinsic | 备注 |
|---|---|---|
| `_mm_load_epi32/16/8` | `_mm_load_si128/loadu_si128` | 载荷 |
| `_mm_store_epi32/16/8` | `_mm_store_si128/storeu_si128` | 存储 |
| `_mm_mul_epi32` | `_mm_mullo_epi32` | 逐元素 int32 乘积 |
| `_mm_mul_epi16` | `_mm_mullo_epi16` | pmullw |
| `_mm_mul_epi8` | 无标准 | 私有，归 ② |
| `_mm_div_epi32` | 无直接 | 归 ② |

### 决策 ②：私有扩展处理（整型除法 `_mm_div_*`、`_mm_mul_epi8`）
- **int32 除法**：可映射 `_mm_cvtps_epi32(_mm_div_ps(_mm_cvtepi32_ps(a),_mm_cvtepi32_ps(b)))`
  （对目标小整数精度成立），作为 tcc `__m128i` 的原子内建保留但**不进标准交集**。
- **epi16/8 / epu 除法、mul_epi8**：无标准路径且扩宽复杂 → **保留为 tcc 独有扩展**，
  t046 中对应断言裁剪或改标 `TCC_SIMD_EXT`（进 tcc 本地验收，不进 clang 闭环）。
- 明确边界：**标准闭不含这组**；`--emit-c` 遇之透传（clang 编译失败即标记为
  tcc-only），与 KNOWN_ISSUES 现有口径一致。

## 5. 回归计划

1. 构建：`build.sh` 或增量自举 `tcc-win → tcc-dg8`（musl flags 同前）。
2. SIMD 原生验收：`tests/t046_simd.c`（tcc `-run`）保持语义不变（迁移到标准名后数字一致）。
3. clang 闭环：`desugar.ps1 tests/t046_simd.c` —— 目标从 **CLANG-FAIL → PASS**（标准交集部分）。
4. 回归集：`tests/t001–t077`（test.sh, 51 项）+ `desugar.ps1` 全量 26+1。
5. 逐字节产物比对（无 SIMD 的头文件改动不影响既有 26 个非 SIMD 用例）。

## 6. 风险与回滚

- **VT_BTYPE 扩展**：仅新增枚举位，不影响既有类型；回滚 = 撤销该位与声明解析分支。
- **`_mm_*` 改名**：牵动 t046 与可能其它 SIMD 用例 → 以「别名保留旧名」过渡，避免破坏。
- **`__m128` 下标/取字**：tcc 需对齐 clang `e[0]`；节假日可先用访问宏规避。
- 全程以 `t046` 单一验收为准 + 全量回归，任意阶段失败立即回退该阶段。

## 7. 里程碑

- M0：simd.h 迁 lib + 入 git + desugar.ps1 INC 指向（1 次 commit，可先行）。
- M1：内核 `VT_VECTOR` + `__m128` 家族类型 + `type_size`/对齐（tcc 可编译 `__m128`）。✅ 已落地。
- M2：`simd_vtype`/`simd_vector_kind`/`gen_op` 改认 + 内建改名 + t046 迁标准交集。
- M3：t046 clang 闭环 PASS + 全量回归通过 + 文档更新（simd-standard / desugar §4.6）。

## 8. M2 实证与 "VT_VECTOR 聚合值路径"并入点清单 (2026-08-25)

**实证**：tcc 值模型用 `bt == VT_STRUCT` 判定"聚合/内存型"，VT_VECTOR 未纳入任何一处，
导致把 VECTOR 值当标量 `gv()` 压寄存器 → `src/x86_64-gen.c load():558` 断言
（仅认 INT/LLONG/PTR/FUNC）。类型识别层（parse_btype / simd_vtype / simd_vector_kind）
已通；**卡点在 tcc 聚合值路径的一致性贯穿**。

**核心判断**：tcc 对聚合值（struct）本就走"按 `type_size` memcpy / 地址"而非寄存器；
VECTOR 的 `type_size` 已返回 16，故**让 VT_VECTOR 复用 struct 聚合路径**（扩展
`bt==VT_STRUCT` → `bt==VT_STRUCT || bt==VT_VECTOR`）即可绕开 load 寄存器化，指令发射
（simd_emit_*）可整体复用不动。

**关键约束 / 风险**：VECTOR 的 `ref` 是 `g_simd_tag` 伪指针（非真 struct 布局符号），
凡"结构布局解引用 / 成员访问 / 数组元素步进 / offsetof-field"路径**绝不能**含 VECTOR，
只能落入"纯 size 搬运 / 地址化 / 整体拷贝"路径。

### 需扩展 `bt==VT_STRUCT`（并入 VT_VECTOR）的聚合路径点（tccgen.c 为主）

| 文件:行(近似) | 上下文 | 处理方式 |
|---|---|---|
| tccgen.c:1912,7220,7457 等 `is_float(...)` 前 | 值类型分流 | 新增 `IS_AGGREGATE(t)=STRUCT\|VECTOR`，VECTOR 走地址/搬运 |
| tccgen.c `gen_assign` / 聚合赋值 | 大类型赋值 → `memcpy` | 并入 VECTOR（按 size=16） |
| tccgen.c 函数实参 by-value `push` | 结构按值压栈 | VECTOR 同样 16B 压栈 |
| tccgen.c `ret_nregs` / 返回值 | 结构返回 | VECTOR 走聚合返回路径 |
| tccgen.c 初始化器 / 声明 `has_init` | 结构初始化 | VECTOR 16B 搬运 |
| x86_64-gen.c `load()`:522-533 | VT_STRUCT 转小标量 | **勿并入**；VECTOR 大类型不得进寄存器，走地址 |
| x86_64-gen.c `store`/`move` 聚合 | 16B 搬运 | 并入 VECTOR |

**前提验证**：任意改动前先确认 `type_size(VECTOR)==16` 且搬运路径只按 size（不查 ref
布局）；用 `simd_t.c`（`_mm_load_ps` + `a*b` + `_mm_store_ps`）作最小回归；通过后再扩 t046。

### 已完成的 M2 前半（工作区已 stash，专项可 restore 续用）
- `x86_64-simd.c`：`simd_vtype` 改构造 `VT_VECTOR`(ref=g_simd_tag[kind])；`simd_vector_kind`
  按 `VT_VECTOR+ref` 识别；新增 `simd_kind_ref()`。
- `tccgen.c parse_btype`：`__m128/__m128d/__m128i` 设 `ref=simd_kind_ref(f/d/i)`。
- `lib/simd.h`：单模式（tcc 免 immintrin，clang include immintrin；`v4f/v2d/v4i/v8h/v16b`
  = `__m128` 别名）。
- `tcc.h`：声明 `simd_kind_ref`。
## 9. M2 落地与扩展收敛 (2026-08-25)

### M2 完成: VT_VECTOR 聚合值路径贯穿
- tccgen.c 并入点（`bt==VT_STRUCT || bt==VT_VECTOR`）: gv() 早退(1934)、compare_types(2925)、
  combine_types(3032)、verify_assign_cast(3916)、vstore memcpy(3943)、unary 调用 sret(7770)、
  expr_cond islv(8224)、gfunc_return(8354)、decl_initializer(9705)、add_local_bounds(1746)。
- 未并入（正确排除）: `type_size` ref 布局遍历、`find_field`/`check_fields` 成员访问、
  `decl_initializer_alloc` flex-array 检测(9955)、`classify_x86_64_inner` 字段遍历(SysV 分支)、
  x86_64 `load()` 小标量化 —— VECTOR ref 是身份标签, 不可解引用。
- `simd_ensure_slot(n)`: 操作数非 16 对齐本地槽(LLOCAL 参数/全局/解引用指针)时经 vstore
  复制到临时槽 —— 修复 `movdqa [rbp+bo]` 未对齐 #GP (LLOCAL 参数槽 8 对齐 + 内容是指针)。
- SysV 分支: `classify_x86_64_inner` 加 `case VT_VECTOR: return x86_64_mode_memory`。

### 标准名收敛 (删除 tcc 私有名/扩展, 2026-08-25 决策)
按"不要 tcc 独有扩展"收敛到 immintrin 标准交集:
- 删除内建: `_mm_div_epi32/16/8`、`_mm_div_epu32/16/8`(标量除法)、`_mm_mul_epi8`(无标准);
  `simd_gen_op`(__m128 原生 + - * /)与 tccgen.c 路由一并删除 → `__m128 a+b` 报
  "invalid operand types"(与 clang 一致)。
- 删除私有名别名: `_mm_load/store_epi32/16/8` → `_mm_load/storeu_si128`(movdqu 未对齐)/
  `_mm_load/store_si128`; `_mm_mul_epi32/16` → `_mm_mullo_epi32/16`; `_mm_setzero_epi32/16/8`
  → `_mm_setzero_si128`。
- 删除 `gen_vec_div`/`gen_vec_mul8`(x86_64-gen.c) 与 tcc.h 声明。
- `__m128i` 单类型归一: I4/W16/B8 共享 ref tag(`simd_kind_tag`), kind 语义由内建名承载
  (v8h sav = _mm_loadu_si128(...) 类型兼容); W16/B8 仅剩 add/sub。

### M3 验收结果
- tcc -run: `tests/t046_simd.c` PASS; 全量 `test.sh` **77/77**。
- clang 闭环: `desugar.ps1 tests/t046_simd.c` PASS (CLANG-FAIL → PASS); desugar 核心 10/10。
- 脱糖产物纯透传: `__m128` 类型 + 标准 `_mm_*` 名 + `#include <immintrin.h>`(simd.h
  `__TCC_DESUGAR__` 分支恢复用于产物透传)。

### 基础设施教训 (bin/tcc.exe 形态与编译选项逐一验证, 2026-08-25)
- bin/tcc.exe 必须是 `CONFIG_TCC_POSIX=1` 构建(默认链 ELF libc.a 而非 msvcrt);
  a813951 后 install.sh 的 musl 自编译因 environ(weak 符号 alacarte) 失败 fallback 到
  原生 build → 链接 musl 测试报 `unresolved stdout`/dllimport。
- **编译选项最终必要集 (逐一实验验证)**:
  - `CONFIG_TCC_POSIX=1` 必要 (tccpe.c:2089 唯一使用点: 默认链 libc.a)。
  - `CONFIG_TCC_PREDEFS=1` 必要 (编译期嵌入 tccdefs_.h; 缺它产物引用 tccdefs.h,
    desugar 产物被 clang 编译失败, bin/ 无法自足)。
  - `CONFIG_TCC_MUSL=1` + 配套(SEMLOCK=0/TCCDIR/MUSL_STDIO) **不必要且有害**:
    musl 线路 tccrun 内存模式在 psxscl mmap(强制 reserve ≥1GB, musl realloc 依赖)
    下映射落 2GB 上沿 0x7fff0000, 布局符号越过 0x80000000 → R_X86_64_32S 重定位
    溢出 (tcc -run 报 relocation out of range); psxscl 无 clone/posix_spawn 实现,
    临时 exe fallback 不可用。无 MUSL 形态 -run/链接/测试全部正常。
  - `ONE_SOURCE=1` 冗余 (tcc.c:21 默认已是 1)。
  - `TCC_TARGET_X86_64` 冗余但防御性保留 (宿主自动定义, 交叉构建保险)。
- install.sh [3/3] 已改为直接部署 [1/3] 产物 (无 MUSL 形态); build.sh [1/4] 的
  tcc-win 构建命令为 `BOOT_TCC -DCONFIG_TCC_POSIX=1 -DCONFIG_TCC_PREDEFS=1
  -o build/tcc-win.exe src/tcc.c -I src`, build/lib/libc.a 就位。
