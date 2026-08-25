# 用户端矩阵库方案 (mat)

> 状态: 2026-08-23 设计
> 范围: 一块运行在 tcc_posix 之上的用户端线性代数库, C 实现,
>       手写平铺 GEMM 内核 + 复用编译器 SIMD 打包指令 (v4f / _mm_*).
> 依据: 关于「SIMD 内核融合能否做到 Eigen 程度 / 是否必须编译器做」的讨论结论.
> 结论: 矩阵层走库级手写内核 (对标 BLAS/oneDNN/OpenBLAS), 编译器零改动;
>       编译器 SIMD 懒只保留给 v4f 向量表达式层 — 那层因 C 无运算符重载,
>       不得已才必须编译器做.

---

## 1. 背景与目标

上一轮讨论确立了分工:
- **矩阵内核层 (GEMM/元素/归约)**: 用户端手写, 可达 BLAS 级, 编译器零改动.
- **向量表达式懒熔合 (v4f + v4f 透明熔合)**: 编译器做 (C 无运算符重载, 只有
  `gen_op` 钩子能拦截 `+`), 已在 `src/x86_64-simd.c` 落地.

本方案把**矩阵层**固化为可执行设计. 目标:
1. 最小立住 `C = A*B + C` 闭环, 验证平铺 GEMM 手感与性能;
2. 复用既有 `v4f`/`_mm_*` 打包指令作为累加积木, 不再发明新机制;
3. 编译期常量形状走 `model` 泛型 (t032b), 动态形状走运行时.

设计原则: 不发明新运算类型, 不触碰编译器, 全部落在库头文件 + 一个 .c.

---

## 2. 结论综述 (为何库级而非编译器级)

| 判据 | 库级手写内核 (用户端) | 编译器生成内核 |
|---|---|---|
| 表达式透明性 | 需显式 API / 宏糖 | 直接 `C = A*B + C` |
| 内核质量 | 可达 BLAS 级 (手调平铺+FMA) | 难超手写 (GCC/Clang 同) |
| 工作量 | 中, 一次写好长期复用 | 高 (平铺器+模式识别) |
| 编译器改动 | 零 | 大, 侵入前端+后端 |
| 覆盖模式 | 有限集, 命中主力 | 慢, 需逐步扩展 |

矩阵层选库级. 编译器懒熔合的价值与矩阵层正交, 仍保留给 v4f 表达式.

---

## 3. 值模型

### 3.1 运行时形状 (动态)

```c
typedef struct mat {
    size_t m, n;          /* 行 x 列 */
    size_t stride;        /* 行字节跨度 (含填充), >= n*esize */
    float *d;             /* 元素存储, 16 字节对齐 */
    /* 所有元素视为行主序连续; 布局在 mat_alloc 时固定 */
} mat;
```

v2 扩展双精度/整型时, 用元素类型参数化 (见 §7 泛型方向).

### 3.2 编译期形状 (model 泛型)

复用已有 `model struct Mat(T,R,C)` (t032b), 让小尺寸固定形状在栈上落体:

```c
model struct Mat(float, int R, int C) { float d[R*C] __attribute__((aligned(16))); };
Mat(float, 3, 3) m3;   /* 栈上, 零动态分配 */
```

两者共存: 大/动态矩阵走 `mat`, 小/编译期固定走 model.

---

## 4. 内存与对齐

- `mat_alloc`: 用 musl `posix_memalign` (16 或 32B) 对齐分配, 满足 `_mm_*`
  movaps/movdqa 的对齐要求.
- 不依赖 winapi; 全程 POSIX (mmap/posix_memalign 已验证可链入).
- 所有权: 每个 `mat` 记录 `owned`; `mat_free` 走统一销毁语义, 防止与裸堆错配
  (衔接 tcc-own 的归属思想, 但 mat 自管无需外部登记).

---

## 5. 平铺 GEMM 内核 (核心)

签名: `void mat_mul_add(mat *C, const mat *A, const mat *B);`
即 `C += A * B`.

### 5.1 分块层次 (两/三级平铺)

| 层 | 分块 | 目的 |
|---|---|---|
| 第一级 (i-loop) | A 行块 × B 列块 | 复用 C 累加块在寄存器 |
| 第二级 (k-loop, L2) | K 轴分块 (如 KC=256) | B 块驻 L2 缓存 |
| 微内核 (MC×NC) | 如 8×8 float | 寄存器阻塞, 全 FMA |

### 5.2 微内核 (寄存器阻塞)

微内核固定 4×4 累加器由 4 路 XMM 承载 (每路 = 1 行 A 逐列 4 个 C 元素),
k 循环内 `_mm_load_ps` A 行 + B 列打包乘累加:

```c
/* 伪码: 微内核一行 × 四列的 FMA 累加 */
v4f a_row = _mm_load_ps(&A[d][kk]);      /* 打包取 A 的一行 */
v4f c0 = _mm_load_ps(&C[d][jj]);         /* 各列累加器 */
    ...
for (k = 0; k < KC; k++) {
    v4f b = /* B 当前行的 4 列打包 */;
    c0 = _mm_add_ps(c0, _mm_mul_ps(a_row, b));   /* 逐列乘累加 (intrinsic 写法) */
}
_mm_store_ps(&C[d][jj], c0);
```

- 2026-08-25 M2 后 tcc 删除 `__m128` 原生运算符 (与 clang/gcc 交集一致),
  一律用 `_mm_add_ps/_mm_mul_ps` 等标准 intrinsic; 无整型除法 intrinsic。
- 本方案浮点透传处理器 FMA 指令 (`vfmadd`), 编译器已能经内建发射.

### 5.3 已有 SIMD 原语即积木

微内核不发明指令, 复用 `src/x86_64-simd.c` 已支持的打包操作:
`_mm_load/_store/_setzero/_add/_sub/_mul`(v4f, 标准 intrinsic). 由编译器内建直接
发射成 movaps/addps/mulps — 与手写汇编平铺具备同构描述力. (tcc 无 `_mm_fma*`
内建; FMA 由脱糖产物在 clang -O3 侧编出, 见 desugar-perf.md.)

---

## 6. 其他算子 (模式集)

| 算子 | 形式 | 备注 |
|---|---|---|
| 元素映射 | `mat_map(C, A, op)` / 宏糖 | 逐元素 + v4f 打包遍历 |
| 归约 | `float mat_dot(A, B);` `float mat_norm(A);` | 打包乘累加 + 横向 reduce |
| 广播 | `mat_adds(C, A, float s);` | 标量 + 打包加 |
| 转置 | `mat_trans(C, A);` | 分块转置 |

首期只立 GEMM + dot 最小闭环, 横向扩展后续.

---

## 7. 泛型 / 扩展方向

- **元素类型**: 现定 float; 用 `tcc-own` 细粒度泛型或 model 参数化扩展到
  double (v2d) 与整型 (v4i/v8h/v16b). model 常量参数 (t032b) 已能承载
  `Mat(T,R,C)` 形状参数.
- **向量长度**: 现 128-bit (XMM). 需要时升 256/512 (YMM/ZMM), 内核里把
  常量与宽收集原语换成对应宽度即可, 寄存器阻塞数跟着调.
- **超标量**: 微内核同时开多行累加器提升 ILP (与手动向量长度无关, 属寄存器
  管用上的安排).

---

## 8. 文件布局 (交付物)

```
include/mat.h        # 公开 API + 值模型 (全头即可用, 免安装)
src/posix/.../mat/   # (可选) 拆到组件目录共享给其它工具
tests/t048_mat.c     # 回归: GEMM 数值随机对比朴素三重循环 / dot / 对齐
```

- mat.h 头文件内联大部分算子 (同 LLVM 头文件库思路), 微内核可选 .c 或全内联.
- 测试对比朴素 O(n^3) 参考实现, 校验分块后数值一致 (相对误差阈值).

---

## 9. 已知限制 (诚实边界)

- 无自动内核选择 (未做 cpuinfo 分派, x86_64 目标固定基线).
- 无符号无关全指令: 整型除法上标量兜底, 打包吞吐低于浮点 GEMM.
- 小矩阵 (< 分块阈) 平铺反而慢, mat GEMM 走朴素+仍然 SIMD 的快路径.
- model 固定形状不可变维数后伸缩, 属编译期定死 (既有限制).

---

## 10. 落地顺序建议

1. **mat.h 值模型 + posix_memalign 对齐分配** (无内核).
2. **朴素 SIMD 版 `mat_mul`** (不分块, 先正确): 对照 t046 原语.
3. **KC/MC×NC 平铺** (首次对齐 BLAS 手感): +dot/norm/broadcast/dot 归约.
4. **model 固定形状路径** 小矩阵栈上落体.
5. **tests/t048_mat.c**: 随机数值回归 + 朴素对照 + (可选) 计时.
6. Python/脚本侧性能抽样对比 (本地 x86_64).

> 首期验收: t048_mat 通过, `C += A*B` 相对朴素浮点误差可接受, 未触碰编译器.

---

## 附录 A. C 语言使用方式 (用户视角)

按「当前即可用」→「可选的编译器扩展」排三级用法定法, 避免把未来特性当现有能力:

### A.1 现状可用 — 显式 API (动态形状)

```c
#include "mat.h"

/* 创建: m2×m3 矩阵, 16 字节对齐 */
mat *A = mat_alloc(2, 3);
mat *B = mat_alloc(3, 4);
mat *C = mat_alloc(2, 4);

/* 清 0, 填数 (行主序) */
mat_zero(C);
A->d[0*3+1] = 3.0f;   /* A[0][1] */
B->d[1*4+2] = 2.0f;   /* B[1][2] */

/* C = C + A*B  (平铺 GEMM, 复用 v4f/FMA) */
mat_mul_add(C, A, B);

float dot = mat_dot(A, A);   /* 归约: ||A||² */
mat_free(A);  mat_free(B);  mat_free(C);  /* 统一销毁, 防错配 */
```

### A.2 现状可用 — model 泛型固定形状 (编译期尺寸, 栈上落体)

小矩阵免动态分配, 形状在编译期定死:

```c
#include "mat.h"

model struct Mat(float, int R, int C) { float d[R*C] __attribute__((aligned(16))); };
typedef Mat(float, 3, 3) M33;

M33 A = {0}, B = {0}, C = {0};
A.d[0*3+1] = 1.f;  B.d[1*3+1] = 2.f;

mat_mul_add_s(3, 3, 3, C.d, A.d, B.d);   /* 定尺寸内核重载 */
```

### A.3 可选扩展 (需编译器改动) — 表达式重载

> 依赖 gen_op 钩子把 mat/Mat 的 `*`/`+` 组装成**懒表达式节点**, 消费时才平铺求值.
> **当前编译器未实现**, 列出以明确"目标长什么样", 不应视为已可用.
> 与 C++ 的关键差异: C 无 `auto`/一等表达式, --- 懒表达式**只能**同一条语句内
> 构造并消费; 赋值给 mat / `.d` 访问 / 传参处是被强制物化的点.

```c
/* 语义函数: 返回"表达式"(延迟), 不是算好的 mat */
model (T) T _op_mul(T a, T b);
model (T) T _op_add(T a, T b);

/* 整条表达式赋给真 mat → 唯一物化点, 一次性平铺 GEMM */
M33 D = A * B + C;

/* 中间结果继续运算 → 不产生临时全矩阵, 懒树生长 */
M33 E = (A * B) * (C1 + C2);

/* .d 访问是强制物化点 */
float s = (A * B).d[0];

/* 不支持: auto e = A*B (无一等表达式); 跨语句复用 A*B
   (第二个 = 已强制物化为真 mat, 不再懒)                     */
```

分级小结: A.1/A.2 是当前交付范围; A.3 是语言扩展方向的"使用样貌", 与 mat 库正交.

---

## 附录 B. C 语言运算符重载: `operator` 语法 (独立语言扩展)

> 与 mat 库正交的**编译器扩展**, 已实现并回归 (`tests/t050_operator.c`, 50/50).
> 目标是把 C++ 的 `operator` 语法移植到 C, 让 `a*b` 对自定义 struct 直接可写.
> 实现要点与现状修正见本附录 B.3。

### B.1 动机与定位

- C 无运算符重载, `struct + struct` 原本非法. 现有 `v4f+v4f` 是编译器对 SIMD
  的**硬编码特化** ([simd_gen_op]), 只认识写死的向量类型.
- `operator` 语法把它**泛化**: 用户声明 `operator+`, 编译器按操作数静态类型在
  gen_op 查表, 改写成一次普通函数调用. 属**编译期静态分派, 零运行时开销** —
  与 vptr 运行时多态 (间接寻址) 是同一系统里的两个极端.

### B.2 语法 (声明侧)

```c
struct Vec3 { float x, y, z; };

/* operator 关键字 + 运算符字符 = 函数名; 之后是正常参数与函数体 */
Vec3 operator+ (Vec3 a, Vec3 b) {
    Vec3 r = { a.x+b.x, a.y+b.y, a.z+b.z };
    return r;
}
Vec3 operator* (Vec3 a, Vec3 b) { ... }

Vec3 c = a + b;     /* → operator+(a, b)  */
Vec3 d = a * b;     /* → operator*(a, b)  */
```

### B.3 编译器识别与实现要点 (**已实现**, t050)

- `operator` 设为**保留字** (`tcctok.h` 的 `TOK_OPERATOR`);声明符 (type_decl 的
  标识符分支) 遇 `operator` 后读一个运算符 char, 拼成单一函数名 token
  (`operator+`), 使 `Vec3 operator+ (Vec3, Vec3){}` 就是一个名字为
  `operator+` 的普通全局函数定义.
- `gen_op` 遇二元算术 (`+ - * / %`) 且操作数 `VT_BTYPE == VT_STRUCT` 时,
  合成 `operator<op>` 名并 `sym_find` 查**全局同名函数符号** (精确匹配, 无
  隐式转换/ADL);命中则把 `[a, b]` 重排成 `[f, a, b]` 后 `gfunc_call(2)` 改写,
  返回 1;否则回落 combine_types 报 invalid operand.
- 返回值: struct 依 x86-64 ABI —— 大 struct 走 **sret 隐藏指针** (分配返回槽,
  压隐藏指针为第0参数, `gfunc_call(3)`, 结果在槽), 小 struct 走寄存器 (仿 unary
  逐 reg 落槽);标量/float 直接 `PUT_R_RET` 设返回寄存器.
- 与内置运算符优先级一致: 由语法树 (`a+b*c` = `a+(b*c)`) 天然处理, 无需换序.
- 内置类型 (`VT_INT/VT_FLOAT/指针`) 与 SIMD (simd_gen_op 优先) 永远走原语义,
  不参与重载查询. (顺带修复: simd_vector_kind 对未 include <simd.h> 的普通
  struct 不再误报 required.)
- 已知限制: 表达式层 `operator+` 是关键字, 不能显式写 `operator+(a, b)` 调用;
  operator 声明须在使用之前 (普通 C 序, sym_find 才找得到).

### B.4 与备选方案的取舍

| 方案 | 识别方式 | 新 token | 真支持 `a+b` | 函数名 |
|---|---|---|---|---|
| `operator+` (本设计) | 保留字拼 token | 有 | ✅ | 强绑定, 最清晰 |
| `_op_add` (方案F) | 名字前缀约定 | 无 | ✅ | 锁死 `_op_` |
| `__overload__ '+'` | 显式标注 | 有 | ✅ | 随便 |
| 宏 / X-Macro | 宏展开 | 无 | ❌ (只能函数别名) | — |

选 `operator` 的理由: 显式、无 `_op_` 命名约定、`a+b` 直接可写、对 C++ 用户最
熟悉. 代价: 引入 `operator` 保留字 (占用一个普通标识符名).

> 注: 单名 `_op`(不带后缀) 或函数指针 `Vec3(*_op)(Vec3,Vec3)` 均**不可行** —
> 一个名字无法区分不同运算符, 仍需第二个维度 (关键字/登记) 才知 `+` 还是 `*`.

### B.5 诚实边界 (不做清单)

- **精确类型匹配**, 无隐式转换 / ADL / 成员函数 / 默认参数重载 — 完整重载决议
  成本非线性, 明确排除.
- 同一运算符对同一类型**多个候选**不做; 只支持精确单一匹配.
- 一元 `-` / 比较 `< ==` 为可选后置, 首期仅二元算术.
- **性能红线**: 重载是语法糖, 必须转发到手写内核 (如 mat.h 的平铺 GEMM),
  **不得**让 `A*B+C` 生成标量 `for` 循环 — 否则"能编译但慢一个量级".
- 懒/延迟求值 (表达式作物化) 与 operator 正交: 需另做 VT_SIMDEXPR 那套懒节点
  机制; operator 默认按急切函数调用求值.

### B.6 分级小结

- A.1/A.2 (mat 库) 为当前交付范围, 零编译器改动.
- 附录 B `operator` 语法 **已实现** (需编译器改动的语言扩展), 回归 t050, 与
  矩阵层正交 (mat 表达式重载 A.3 可声明 operator 转发到手写 GEMM 内核).

---

## 附录 C. 性能对照与懒运算可达性归档

> 状态: 2026-08-23 讨论结论固化. 覆盖: 懒运算能用户端做到哪层、Eigen 对照、
> 加编译器特性能否追平、传统 BLAS 如何高效、我们在 tcc_posix 上能否做到.
> 目标: 为「矩阵层是否立项 + VT_SIMDEXPR 编译器特性时机」留下可执行依据.

### C.1 懒运算能用户端实现到哪一层

懒/惰性求值分两级, **用户端只够得到算法/内存级**, 寄存器级必须编译器:

| 级 | 能做什么 | 省什么 | 归属 |
|---|---|---|---|
| 算法/内存级 | operator 返回 `Expr{op,l,r}` 表达式树, 赋值/取元素时**单遍物化** | 中间临时**整块分配 + 整体搬运**(大向量/矩阵大头) | ✅ 用户端可做 (表达式模板) |
| 寄存器级 | `(a*b+c)*d` 在 xmm0 链上算完, 只 load 叶子、最后 store | 少量 load/store 指令(若干 movaps) | ❌ 需编译器 (VT_SIMDEXPR) |

- 结论: 长向量/大矩阵收益大头在内存级 → **用户端表达式模板就够**(复用 operator);
  4–8 元素小 SIMD 的寄存器级熔合收益小, 且用户端 Expr 树开销可能抵消 → 不优先.

### C.2 Eigen 对照

| 维度 | Eigen (C++) | 用户端 C (operator) |
|---|---|---|
| `A*B+C` 语法 | C++ 运算符重载 | operator 扩展 (编译期改写为调用) |
| 表达式编码 | **编译期类型** (每步不同模板类型携带整棵树) | **运行时 Expr 值**, 物化遍历 |
| 求值决策时机 | 编译期经 traits 挑 lazy/eager + 派发内核 | 运行时 `switch(op)` 单遍 |
| 代数/恒等式优化 | ✅ (traits 特化: inv×B→solve、乘积 vs 系数式) | ❌ (只能用户手写内核) |
| 向量化 | 模板内联后后端自动向量化/显式 SIMD | 需**手动** v4f/_mm |
| 临时寿命 | C++ 引用延长 + RVO/移动 | C 无引用寿命 → Expr 指针须活得比物化点久 |
| 性能上限 | 逼近手写 BLAS | 靠手写内核, 结构上低于 Eigen |

### C.3 加编译器特性能否追平 Eigen —— 三档

Eigen 性能来源拆三条, 逐条判定:

| 特性 | 追平对象 | 判定 |
|---|---|---|
| SIMD 表达式折叠 (VT_SIMDEXPR 惰性节点) | 系数式 + 寄存器级熔合 | ✅ 可追平 · 编译器本职 · **推荐后续做** |
| 自动向量化 pass | SIMD 生成 | 🟡 部分 · TCC 无, 加=重工程 · 暂以 operator/_mm 手动替代 |
| 编译期类型分流 (type-traits/特化) | `A·inv(B)→solve` 等恒等式 | ❌ 结构受限 · 需类 C++ 元编程, 违背 TCC 单遍架构, 不建议 |

- **关键边界**: 矩阵总性能上限由**手写内核质量**决定, 编译器特性追不平内核.
  Eigen 快一半靠表达引擎、一半靠 BLAS 级 GEMM.

### C.4 传统 BLAS (SGEMM) 如何高效

朴素 `for i,j,k` 三重标量循环 = bandwidth-bound (性能惨). 高效实现按三层拆,
目标转成 **compute-bound (受 FMA 吞吐限制)**:

1. **缓存分块 + 打包**: 外层 `jj/ii` 块到 cache 尺寸, 内层 `kk` 块到 L1; 把
   A/B 子块**拷贝进连续缓冲**(packing) → 消除 TLB 缺失、跨步访存、重复读 B.
2. **寄存器微内核 (microkernel)**: 对 `r×c` 子块(如 4×8), 累加器**钉在 r×c 个
   寄存器**里, `for k: acc[][]+=a[r]·b[c]`; A 每行只读一次, 几乎无中间 store/load.
3. **向量化 SIMD**: 微内核用 FMA 一次算 8 float (AVX2), 累加器即一组矢量寄存器.

   → GotoBLAS/OpenBLAS/BLIS 路线, SGEMM 可达 ~90%+ 峰值.

### C.5 我们在 tcc_posix 上能否做到

**关键事实**: TCC 的 x86_64 SIMD 只到 **SSE (128 位 XMM)**, 无 AVX/YMM/vfmadd.

- ✅ **能做到**: 缓存分块 + packing + `v4f` 寄存器微内核 + SSE 向量化 —— **全用户端,
  零编译器改动**. 三者叠加可达 **SSE compute-bound 峰值**(等价 2005–2011 SSE BLAS).
  - 微内核累加用 operator 糖或 `_mm_mul_ps/_mm_add_ps` 组合(无单条 FMA, 用 mul+add).
- ❌ **追不平**: AVX2/FMA(256 位) 不存在 → 每周期只算 4 float, **绝对 flops 峰值约为
  AVX2 版 BLAS 的一半**(SIMD 宽度硬上限, 非写法问题). 极致峰值(>90% SSE)还依赖手写
  微内核 + 逐指令微调, 且热循环由 TCC 生成, 寄存器分配质量是变量.

- **是否值得做**: 目标若是「亲手搭一个真实可用的 GEMM、把 BLAS 原理落地、验证能否
  打到 SSE 峰值」→ 值得, 即 A.1/A.2 手写平铺 GEMM 内核; 若想拼现代 OpenBLAS 绝对
  峰值 → 不可达(卡在 TCC 缺 AVX/FMA). SIMD 表达式折叠 (VT_SIMDEXPR) 是后续
  优先级最高的编译器特性切入点 (C.3).

### C.6 TCC 验证前端 + 脱糖输出 → clang/LLVM 正式产物 (立项)

> 目标: TCC 做**前期代码验证**(秒级 `-run`、`-b -bt` 内存治理、operator/model/
> SIMD/defer 提前验证); **正式产物由 clang/LLVM 出**(吃满 LLVM 优化: 自动向量化/
> FMA/内联, 补 TCC 无 AVX/FMA 的短板). 起因: clang **不认 TCC 魔改语法** → 正式
> 产物只能由 TCC 前端「吐出来」.

**架构 (路径 D, 已定)**: TCC 前端(可单遍 hook, 不必先建完整 AST) → **脱糖输出
标准 C (`gnu11` + intrinsic)** → `clang/LLVM` 编译:

| 魔改特性 | 脱糖落点 | 需编译器动作 |
|---|---|---|
| `operator a+b` | `operator+(a,b)` 普通函数调用 | gen_op 处改写(已实现) + C 文本发射 |
| `model` 泛型 | 实例化后的具体 `struct`/`typedef`/函数 | 实例化后在 parse 处出 C |
| SIMD `v4f` 运算 | `_mm_*` SSE intrinsic (`__m128`) | simd_emit_* 处映射 intrinsic |
| `defer` | 作用域退出清理 (`__attribute__((cleanup))` 或 goto 展开) | defer 注册点展开 C |

- **C 标准落点**: `gnu11` 优先(C11 才有 `_Thread_local`; GNU 扩展 `__thread`/
  `__attribute__`/`typeof` 少一层脱糖); **C99 只是迁移下限, 不作为默认**. SIMD 走
  `<immintrin.h>`(vendor 头) + `-msse*/-mavx2 -mfma`.
- **驱动**: `clang -std=gnu11 -O3 -mavx2 -march=native out.c <链接> -o prod`,
  收敛进一份 Makefile/脚本.
- **与 C.1/C.5 关系**: 懒运算仍走 gen_op/用户端表达式模板 (C.1); 矩阵内核写成
  标准 C + 可移植 intrinsic, 正式产物吃 LLVM 的 AVX/FMA (补 C.5 的 SSE 上限).

**musl 兼容性 (修正)**: musl 是 libc, 非语法. 源码层 POSIX/musl 用法在脱糖产物中
照样 `#include`, 无损. 真正的门槛在**构建/链接**:
- **Linux + musl**: clang 原生 `--sysroot=<musl>` 一套, 无需 psxscl.
- **Windows + psxscl**: psxscl/musl 是**编译无关的 `.a` + 头**(TCC 能用,lld 同样能用),
  不用另做一份; 只需写一个 **clang 驱动**(target/flag/链接参数). 需盯**TLS 对齐**
  (`-femulated-tls`, 因 musl-nt64 走 emutls) 与调用约定/结构体 ABI.

**关于 AST 输出层的定位**: 本方案证明 AST/中间层不再是绕远 —— 它的真实价值是作为
**「脱糖 → 标准 C 的输出端」**, 而非直接到 LLVM IR. 完整 AST→LLVM IR 后端(保留扩展
直达优化器)任务更重, 仅在做自定义 LLVM pass/跨语言时才需要, 暂不立项.