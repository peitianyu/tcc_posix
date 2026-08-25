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
- M1：内核 `VT_VECTOR` + `__m128` 家族类型 + `type_size`/对齐（tcc 可编译 `__m128`）。
- M2：`simd_vtype`/`simd_vector_kind`/`gen_op` 改认 + 内建改名 + t046 迁标准交集。
- M3：t046 clang 闭环 PASS + 全量回归通过 + 文档更新（simd-standard / desugar §4.6）。