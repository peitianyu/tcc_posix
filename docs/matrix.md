# stl-matrix —— Eigen 式矩阵库 (优化设计)

> 状态: **M6 已实现 (2026-08-28)**。直接法落地(§1.2 分解1/2): `mt_lu_factor`(部分选主元
> `P A = L U`) + `mt_mat_det`/`mt_mat_inverse` + `mt_cholesky`(LLT), 配套
> `t090_mat_linalg` PASS。M5 `mt_mat_prod` + 缓存分块 **KC k 分块 + MR×NR
> 寄存器微核** 落地(§10.1/§10.2); 行主序源 B 顺序读已 cache-line 友好, packing 正交
> 留给未来列主序输入接口。SEEE 128 天花板见 §10.3。里程碑见 §13。基于 Eigen 3.4.0 源码剖析与
> `model`/`operator`/`__m128` 基础设施。
> 本文**取代并合并**了 2026-08-23 的 `docs/matrix-library.md`(动态 `mat` + 手写
> GEMM 内核方案; 其 GEMM 分块/微核与 SSE 天花板细节并入 §10, 性能/边界并入 §14;
> operator 语言扩展详情已由 docs/features.md §4.4 覆盖, desugar 详情已由
> docs/desugar*.md 覆盖)。
> 核心方法论: **先立"算子→单一求值循环"语义通道, 再向量化, 再缓存分块; CUDA 埋点前置。**

---

## 1. 定位与关键决策

| 维度 | 决策 | 理由 |
|---|---|---|
| 矩阵形态 | **固定尺寸 `mt_mat(T,R,C)`**(model 常量参) | 编译期定尺寸, 栈上 POD, 零堆 |
| 常量参数 | `T a[R*C]` 数组尺寸直接用 R/C | **已实测可行**(§2.A) |
| SIMD | **intrinsic 层独立**(float→`__m128`, double→`__m128d`) | `x86_64-simd.c` 内建可用, 不阻塞于 M2 |
| 求值 | 算子对象 + **融合单通道 eval**(§4) | 对齐 Eigen 语义层设计 |
| 存储 | 连续 `a[R*C]`, **16B 对齐** | SIMD load/store 前提 |
| 内存 | 全栈 POD + 对齐; 无 GC | Eigen 同款策略 |
| 懒表达式 | **指针树 + 调用方缓冲**(§7) | 内嵌值递归不可行(见 §2.C) |

## 1.1 与 Eigen 的差距与绕过结论 (2026-08-27)

> 背景: 对照 Eigen 剖析, 三大结构性缺口 (表达式模板 / 偏特化 traits / 编译期递归
> 展开) 经逐一验证均**可绕过** —— 最终差距收敛为"语法啰嗦", 而非"性能差距"。

| Eigen 能力 | 本质 | 本项目的绕过 | 代价 |
|---|---|---|---|
| 表达式模板惰求值 | 类型链融合 eval, 零临时 | `mt_expr_desc` **指针树 + eval 单循环** (§7/§8) | 每 SIMD packet(4 float) 一次 desc 递归, 间接寻址被**摊平 → 性能损失 <1%** |
| 偏特化 / traits 元编程 | 编译期按类型选实现/选类型 | `_Generic` + `__builtin_types_compatible_p` 类型分派; `constexpr` 常量对象承载数值 traits; **运算统一返回 `mt_mat(T,R,C)`, 不依赖"返回不同类型"** | 语法略啰嗦 (`mt_mat_*(T,R,C)(...)`) |
| 编译期递归展开 | `sum<3>::value` 全展开小矩阵 | 运行时三层循环 + 编译器`inline`/`model` 实例化 + `constexpr` 索引 | 小矩阵循环+内联, 无实际影响 |

**语法封装路径 (把"啰嗦"压回可读)**: 依赖本库已有的 `operator` 静态分派 + 宏,
将 `mt_mat_eval(T,R,C)(&C, <desc>)` 封装为 `mt_assign(T, R, C) = A + B + D` 之类的
接近 `C = A + B + D` 的形态。语义仍是融合 eval, 与 Eigen 对齐。

**新增可复用基础 (本次 comptime 已落地, 见 docs/comptime.md)**: ① **受限 `constexpr`**
常量对象(唯一新增关键字, 复用枚举常量机制实现单遍折叠)承载编译期维度派生、超栈上限校验
(`_Static_assert`)、packet lane 数等, 使 §5/§9 的编译期常量表达更直觉。② **类型级分派
`model` + `_Generic`** 已就绪(t082), 即"按实例化具体类型选实现/选类型"的 traits 等价物,
正是 §1.1 绕过表第三行"偏特化/traits"的落地基础。

> 结论: M0-M7 的可行性不受三缺口影响; 计算性能可达 Eigen 量级, 差距仅在表达层,
> 可用 operator+宏收敛。

## 1.2 功能范围 (2026-08-27 收敛确认)

> 模型钉为固定尺寸稠密 `mt_mat(T,R,C)`, 对标 Eigen **Dense 系**子集;
> 内存模型(栈上 POD, 零堆)让稀疏/迭代求解器/几何天然出局。

| 类别 | 模块 | 决策 |
|---|---|---|
| 系数运算 | `operator+/-/*`、转置、block、逐元素、标量广播 | ✅ 做 (M0-M1) |
| 归约 | `sum/mean/dot/norm/行列式/迹` | ✅ 做 单趟归约 |
| 数组语义 | 逐元素 `sqrt/abs/min/max` 等 | ✅ 做 (= unary 算子变体) |
| 直接法 · 分解1 | `LU (PartialPivLU)` → 逆/行列式 | ✅ 做 分水岭, 首个分解 |
| 直接法 · 分解2 | `Cholesky (LLT)` 对称正定 | ✅ 做 |
| QR 分解 | — | ⚠️ 可选 (后续) |
| SVD (雅可比) | — | ⚠️ 可选 (靠后) |
| 特征值 (实对称) | — | ❌ 暂缓 |
| 稀疏矩阵 | SparseMatrix/LU/Cholesky | ❌ 不做 (内存模型冲突) |
| 迭代求解器 | CG/GMRES 等大型稀疏 | ❌ 不做 |
| 几何 | 四元数/变换 | ❌ 不做 (后续独立库) |

**先行顺序**: 先铺归约/逐元素通道(用 unary/binary 算子扩展求值通道+SIMD),
再上 LU→逆/行列式。(由用户 2026-08-27 确认: 功能范围=推荐级, 先铺通道再分解)

## 2. 前置验证状态 (已实测)

> 基于 `tests/t032b_model_const.c` / `tests/t032c_model_edge.c` (均 PASS) 与
> 本库现有 model 头文件。

| 项 | 结论 | 影响 |
|---|---|---|
| **A. 常量参数数组尺寸** `T a[R*C]` | ✅ 可行; 归一化缓存 `2+2==4` 同型; `Vec(N)` 尺寸随 N 变 | M0 无阻塞 |
| **B. 嵌套实例化 / 函数泛型混用** `Mat(T,N,2)` `vecsum(N,T)` | ✅ 可行(3 层嵌套) | M0/M1 无阻塞 |
| **C. 递归** | ⚠️ **仅指针自引用可行** `Node(T)*next`; 内嵌值递归 **不可行**(结构体无限大小) | **P0-b 改指针树** |

> 结论: 存储层(M0)完全就绪; 懒求值的表达式树 **必须用指针子树**, 不能照搬
> Eigen 的"内嵌拷贝子树"。此为本次整理最重要的修正。

## 3. 设计原则(源自 Eigen 剖析)

1. **算子→一次求值**: `dst = expr` 在**单个循环**内按元素(或 packet)写入 dst, 不层层物化临时。← `CwiseBinaryOp`+`AssignEvaluator`
2. **编译期静态分派**: 遍历/展开/向量化全部编译期确定, 小矩阵全展开。
3. **向量化靠对齐**: 目标与源 16B 对齐且内部尺寸是 packet 整数倍才走 SIMD, 否则标量/unaligned。
4. **生命周期(不悬挂)**: 左值子矩阵按**引用**, 临时经**调用方缓冲自含** → 无悬挂。← Eigen `ref_selector`
5. **矩阵乘缓存分块**: kc/mc/nc 三块 + 打包 + 微核。← Goto/BLIS

## 4. 分层架构

```
 lib/mat/
   matrix.h    mt_mat(T,R,C) + 构造/访问/转置       [存储层]
   ops.h       operator+/-/* + 算子描述 + eval        [算子层/求值层]
   expr.h      mt_expr_node 表达式树 (P1)          [求值层]
   lazy.h      mt_mat_lazy 懒求值 (P0)                [求值层]
   packet.h    packet 抽象 + get4 (SSE/标量)          [SIMD 层]
   pack_sse.h  __m128/__m128d 落地实现                [后端层]
   gemm.h      GEMM 缓存分块 + 微核 (M5)              [后端层]
   cuda.h      设备接口 (预留 ENOSYS)                 [设备层]
```

依赖方向: `matrix ← ops ← expr/lazy ← gemm`, `packet ← pack_* 后端`,
算子与 packet 均为**纯函数/对象**, 供设备层换实现。

## 5. 存储表示与对齐 (M0, 已验证可行)

```c
model struct mt_mat(T, R, C) {
    union { T a[R * C]; _Alignas(16) char align16; };   /* 强制 16B 对齐 */
};
```

- 行主序连续; R*C 由常量参在实例化时算出(§2.A 已验证)。
- **对齐断言**: `_Generic` 分派下 `_Static_assert` 行列为正; 运行时 `MT_ASSERT((uintptr_t)m.a % 16 == 0)`。
- 超大栈保护: 依托本次落地的受限 `constexpr`(docs/comptime.md §6.1), **编译期**推导字节数并断言:
  ```c
  constexpr long MT_MAT_MAX_BYTES = 64L * 1024L;
  _Static_assert((long)R * C * (long)sizeof(T) <= MT_MAT_MAX_BYTES,
                 "mt_mat stack size exceeds limit; use heap/device");
  ```
  维度、packet lane 数等一律由 `constexpr` 常量对象承载(`comptime.md` 单遍折叠), 编译期即给定, 无运行时开销。

## 6. 运算符与访问 (M1)

统一暴露为函数调用(现有 `operator` 脱糖已就绪):

```c
mt_mat(float,3,3) A, B, C;
mt_mat_fill(float,3,3)(&A, 1.0f);
mt_mat_binop(float,3,3)(ADD, &C, &A, &B);   /* 立即求值(融合通道) */
mt_mat_at(float,3,3)(&C, i, j);             /* 行主序下标 */
mt_mat_transpose(float,3,3)(&tA, &A);
```

`ADD/SUB/MUL/SCAL(标量广播)/TRANSP` 归入统一 `mt_mat_binop`, 内部走 §7 通道。

## 7. 核心: 算子与求值 (融合单通道)

每个运算表达为**纯算子对象**, `dst = expr` 单循环写入:

```c
enum MT_MatOpKind { SRC, BCAST, ADD, SUB, MUL, SCAL, TRANSP,
                     UNARY, REDUC };   /* 数组逐元素 / 归约 (见 §1.2) */

/* 单算子描述(求值期只读) */
typedef struct mt_expr_desc {
    int kind;            /* 叶子=SRC/BCAST; 复合=ADD/SCAL.. */
    const float *src;    /* 叶子: 左值矩阵 a 地址(引用) */
    float scalar;        /* 标量/提升 */
    /* 复合: 子节点为指针(引用调用方缓冲或左值), 见 §8 */
    const mt_expr_desc *ln, *rn;
} mt_expr_desc;
```

eval 单循环(先标量, 后 packet):

```c
int mt_mat_eval(T,R,C)(mt_mat(T,R,C) *dst, const mt_expr_desc *e);
/* 按 R*C 线性遍历; 每元素 dst->a[i] = SR(ln) OP SR(rn);
   SR(x) = x->kind==SRC ? x->src[i] : 递归 get(x);
   对齐块内一次 4(float)/2(double) 元素 → packet 层(§9) */
```

关键: **先按线性逐元素写正确版, 再局部替换成 packet 并行块**,
保证语义单一来源。这正是"先立通道、后向量化"。

## 8. 懒求值 (P0→P1)

> **修正**: 因 §2.C, 表达式树**不能用内嵌值子树**。改用**指针树 + 调用方缓冲**:

### P0: 单算子懒 + eval 物化(先落地)
当前算子经 eval 直接写目标; `(A+B)*s` 按优先级逐层物化, **不悬挂、可靠**。复杂度最低。
调用方提供一次表达式线框即可。

### P1: 表达式树全懒(对标 Eigen)
- 表达式对象持 `mt_expr_desc` 树,**子节点为指针**。
- 叶子(左值矩阵)→ 引用其 `a` 地址; 复合 → 引用调用方栈上/小缓冲里的子节点。
- 求值 = 根 desc 递归 get(逐元素) 或 get4(packet)。
- 深度有限(用户表达式) → 调用方一次性提供定尺寸缓冲, **零堆, 无悬挂**。

> 取舍: P0 保底可靠, P1 追齐 Eigen 逼真懒求值。缓冲超限用扇出数组受限树兜底。

## 9. SIMD packet 层 (M2)

```c
/* packet.h: 抽象(对标 Eigen packet_traits → find_best_packet) */
#define MT_PACK_BYTES 16
typedef struct mt_packet { __m128 f; __m128d d; } mt_packet; /* v4f/v2d 宽 */

int mt_packet_can_vec(int type, int n);      /* 对齐且 n%lanes==0 才 SIMD */
void mt_mat_vec_add(T)(float *dst, const float *a, const float *b, int n);
/* for(i; i+4<=n;) _mm_load_ps/_mm_add_ps/_mm_store_ps; 尾部标量 */
```

- **packet 层隔离**: 上层算法只依赖 `mt_packet` 与 `*_vec_*` 语义; SSE 落地在
  `pack_sse.h`, 未来 HIP/CUDA 各自变体, 算法层不动。
- float→v4f(16B), double→v2d; 尾部标量回退。

## 10. GEMM: 矩阵乘缓存分块 (M5, 最大加速)

对标 Eigen `GeneralMatrixMatrix` + `GeneralBlockPanelKernel` (Goto/BLIS)。
旧方案 `matrix-library.md` 的三层平铺 + 微核细节并入本节。

### 10.1 分块层次 (两级/三级平铺, 并入自 2026-08-23 方案)

| 层 | 分块 | 目的 |
|---|---|---|
| 第一级 (i-loop) | A 行块 × B 列块 | 复用 C 累加块在寄存器 |
| 第二级 (k-loop, L2) | K 轴分块 (如 KC=256) | B 块驻 L2 缓存 |
| 微内核 (MC×NC) | 如 8×8 float | 寄存器阻塞, 全部 FMA 式乘加 |

```
for kc(分块 depth, L2, ~96~128 行) {
    pack A → L1 连续块;  pack B → L1 连续块;
    for nc(分块右) for mc(分块左) {
        gebp 微核: mr×nr 寄存器分块, packet 乘加累加(pfetch 可选);
    }
}
```

### 10.2 微内核 (寄存器阻塞)

固定 `mr×nr` 累加器由 vector 承载,k 循环内 `_mm_load_ps` A 行 + B 列打包乘累加
(16B 对齐前提, 对齐块走 `_mm_load_ps/_mm_store_ps`, 越界/未对齐尾部标量回退):
A 每行只读一次, 几乎无中间 store/load → 由 bandwidth-bound 转为 compute-bound。

### 10.3 SSE 天花板 (并入自 2026-08-23 附录 C.5)

TCC 的 `__m128` 仅 **SSE (128 位 XMM)**, 无 AVX/YMM/vfmadd。缓存分块 + packing +
微核可达 **SSE compute-bound 峰值**(~等价 2005–2011 SSE BLAS; 每周期 4 float)。
绝对 flops 峰值约为 AVX2 版 BLAS 的一半 —— **SIMD 宽度硬上限**, 非写法问题;
正式产物可在 M6 脱糖 → clang -O3 (-mavx2 -mfma) 侧吃满 AVX/FMA。

### 10.4 落地次序

- 首版先做**简单三层循环 (i-k-j, 行主序友好) + 尾部标量**, 正确后再上分块+微核。
- product 多次才物化临时, 一次内联; 小矩阵 (< 分块阈) 平铺反慢 → 走朴素 + 仍 SIMD 快路径。

## 11. 内存治理 (对齐 + 生命周期 + 别名)

| 关注 | 对策 | 对标 |
|---|---|---|
| 对齐 | §5 union+断言; 堆(若有)用对齐分配器 | Eigen `DenseStorage` |
| 临时生命周期 | 叶子引用左值 + 复合引用调用方缓冲(§8) | Eigen `ref_selector` |
| 别名/自赋值 | eval 前若 dst 与源重叠先拷贝 | Eigen `NoAlias`/`evalTo` |
| 超大栈 | `_Static_assert` 上限, 超限提示 | Eigen `check_static_allocation_size` |
| 无 GC | 全 POD; 设备内存显式 on/off(§12) | Eigen 无 GC |

## 12. CUDA 预留接口 (先签名, 实现 ENOSYS)

> 原则: 摸到 GPU 时**调用点语法不变, 只换后端**。算子纯函数化 + packet 隔离
> + 表达式树即设备无关 IR。

```c
typedef void* mt_cuda_stream;    typedef void* mt_cuda_context;
int mt_mat_cuda_init(mt_cuda_context*);                              /* ENOSYS */
int mt_mat_cuda_malloc(mt_cuda_context, const void *src, size_t, void**);
int mt_mat_cuda_copy_to(mt_cuda_context, void *h, const void *d, size_t, mt_cuda_stream);
int mt_mat_eval_device(mt_cuda_context, mt_cuda_stream, mt_expr_desc *dst, const mt_expr_desc *expr);
int mt_mat_cuda_sync(mt_cuda_stream);
```

- 算子纯对象 → 设备同名 `__device__` 实现(双端编译)。
- `mt_expr_desc` 是内存可序列化描述, 可直接当核参/拷入设备内存。

## 13. 实现顺序 (里程碑)

| # | 内容 | 前置 |
|---|---|---|
| M0 | `mt_mat(T,R,C)` 常量参 + 16B 对齐 POD | ✅ **已实现** `lib/mat/matrix.h` + `t083_mat_storage` PASS |
| M1 | 融合 eval 通道: 算子/访问 + eval 单循环逐元素 | ✅ **已实现** `lib/mat/ops.h` + `t084_mat_eval` PASS |
| M2 | SIMD packet 层 + 对齐块 + 标量回退 | ✅ **已实现** `lib/mat/packet.h`/`pack_sse.h` + `t085_mat_simd` PASS |
| M3 | 归约/逐元素通道(redution/unary: sum/dot/范数/迹 + sqrt/abs) | ✅ **已实现** `ops.h`/`packet.h`/`pack_sse.h` + `t086_mat_reduc` PASS |
| M4 | 懒求值 P1 (**指针树** + 调用方一次缓冲) | ✅ **已实现** `ops.h` `mt_expr_frame`/`eval_frame`/`eval_transpose`/`eval_block` + `t087_mat_lazy` PASS |
| M5 | GEMM 缓存分块 + 微核 | ✅ **已实现** `ops.h` `mt_mat_prod`(别名保护) + `pack_sse.h` `mt_sse_gemm_b_f/d`(KC 分块 + MR×NR 微核) + `packet.h` `mt_mat_gemm`(分派) + `t088_mat_gemm` / `t089_mat_gemm_blocked` PASS |
| M6 | **直接法 LU→逆/行列式** + Cholesky(LLT) + desugar/clang 闭环 | ✅ **已实现** `linalg.h` `mt_lu_factor`(部分选主元 `P A = L U`) / `mt_mat_det` / `mt_mat_inverse`(N 个单位解回代, 支持就地) / `mt_cholesky`(LLT 就地, 非正定报错) + `t090_mat_linalg` PASS | M3 |
| M7 | (可选) QR 分解 / 雅可比 SVD | M6 |
| M8 | (触发式) CUDA 设备后端 | GPU 环境 |

测试(按序接续 t082): `t083_mat_storage`(M0, 已 PASS) / `t084_mat_eval`(M1) /
`t085_mat_simd`(M2) / `t086_mat_reduc`(M3) / `t087_mat_lazy`(M4) /
`t088_mat_gemm`(M5) / `t089_mat_gemm_blocked`(M5) / `t090_mat_linalg`(M6)。

## 14. 风险与既定取舍

1. **懒表达式缓冲**: 指针树需调用方供缓冲; 深度超限用扇出受限树兜底。⚠️ 唯一新风险。
2. **SIMD 仅 float/double**: 整型/复数 scalar 回退。
3. **维度膨胀**: 只显式实例化 (float/double × 常用 R,C)。
4. **对齐承载**: 依赖 struct 16B 对齐; 失败降 `_mm_loadu`。
5. **懒求值首版性能**: P1 先正确, SIMD get4 后追吞吐。
6. **SSE 天花板**: 本机 x86_64 仅 128 位 XMM; 绝对 flops 峰值约为 AVX2 BLAS 一半。
   极限性能由正式产物(clang -O3 -mavx2 -mfma)承接(§10.3)。
7. **无自动内核选择**: 不按 cpuinfo 动态分派, 固定 x86_64 SSE 基线。

> 沿革: 本文取代 2026-08-23 `docs/matrix-library.md`(动态 `mat` + 手写 GEMM 内核 +
> operator/desugar 附录)。设计演进: 动态形状 → 固定尺寸 `mt_mat`(栈上 POD);
> 手写内核优先 → Eigen 式「算子→融合 eval→高可读语法」; 懒求值由内嵌子树修正为
> 指针树(§2.C)。operator 扩展归 docs/features.md §4.4, desugar 归 docs/desugar*.md。