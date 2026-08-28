# 编译期语义扩展（compile-time semantics）设计方案 v2

日期: 2026-08-27
状态: **受限 constexpr（常量对象）已落地；类型级分派（model+_Generic）已就绪**（仅新增唯一 `constexpr` 关键字，其余复用）

> v1 曾提 `static_if`/`static_choose` 等新语法。v2 按用户偏好重写：
> **① 保留字增量最小；② 在已有机制上增加语义**。
> 手段：用 **C11 标准原语 + 已有 `model`/`operator`/`__builtin_*`** 组合出类型级分派，
> 仅引入**唯一** `constexpr` 关键字承载编译期常量对象（见 §0 权衡）。

---

## 0. 设计目标（v2）

1. **保留字增量最小**：传统 `static_if`/`comptime`/`@` 等一律拒绝；仅引入**一个**
   `constexpr` 关键字（C23 同风格子集）承载"编译期常量对象"，其余全部复用。
2. **在已有机制上扩语义**：优先复用已实现的 `_Generic`/`model`/`operator`/`__builtin_*`，
   需要时给它们**加语义子集**，而非加新语法。
3. **不破坏单遍哲学**：仍避开解释器/AST/第二遍（reflect.md §9 硬边界）。

> 权衡说明（相对 v1/v2 调整）：类型级分派（`_Generic`+`model`）确实能做到**零新增关键字**；
> 但"编译期常量对象"若也用 `_Generic` 表达，写起来相当绕。为了让用户能以最直觉的
> `constexpr int N = 8*sizeof(int);` 表达常量，最终**引入唯一保留字 `constexpr`**，
> 且其实现复用枚举常量机制，非新增解释器。

## 1. 关键观察：`_Generic` 已是完整的类型级编译期分派

v2 最重要的事实（已核实 tccgen.c:7539，`TOK_GENERIC` 完整实现）：
- `_Generic(expr, type: x, type2: y, default: z)`：**C11 标准原语，零新增关键词**。
- 已具备：`compare_types` 类型匹配（非串行双匹配过则报错）、`skip_or_save_block`
  选定分支才解析/生成目标码（未选中分支跳过 → **类型错误不泄漏、零代码**）。
- 它天然就是 C 版的"按类型分支"="if constexpr + 类型特化的交集"，**只是缺语义扩展**。

结论：**类型级编译期分派的核心机制早已存在，缺的是把它接到 `model` 泛型与惰求值语义上**。

## 2. 设计思想：三件套组合，无需新语法

把「编译期判定」拆成两个正交语义，分别交给已存在的原语，各自承担 **C++ 编译期三件套** 的一角：

| C++ 编译期 | C11/本项目等价物 | 缺 |
|---|---|---|
| `if constexpr` 分支 | `_Generic`（类型分派选择） | 无 |
| `static_assert` 判定 | `_Static_assert` ✅ | 无 |
| constexpr 常量对象 | `constexpr` ✅ **已实现**（唯一新增关键字） | 无（见 §6.1.1） |
| 模板类型参数 | `model` 泛型参数 ✅ | 无 |
| 模板常量参数 | `model` 常量参数 ✅（t032b） | 无 |
| operator 特化 | `operator` 静态分派 ✅ | 无 |
| constexpr 纯函数折叠 | roadmap（§6.3.2） | 语义扩展点 |
| 惰求值表达式入类型 | 缺（易，见 §7） | ⚠️ 受 C 类型上限 |

**不是"造新语言"，是"在 `_Generic`/`model`/`operator`/`constexpr` 的既有语义上补衔接"。**

## 3. 语义扩展点（增量，非新语法）

### 3a. 让 `_Generic` 认识"泛型类型形参"（推荐，最小改动）
现状：`_Generic` 的关联类型是**具体类型**。type 分派时能给的是实例化后的具体类型
（`STL_Mat(float,3,3)`），但写代码时我们希望写的是**模板形态**。

扩展：`model` 体内部，`_Generic` 的关联类型允许写 **model 形参/常量参**，编译器按
实例化后的具体类型匹配。语义仍是 `_Generic`，只是**关联类型表可在 model 体内按
形参展开**：

```c
model (T) T stl_clamp(T v, T lo, T hi) {
    /* 按实例化出的具体 T 选实现，全程标准 _Generic 语义 */
    return _Generic(v,
        float:  v < lo ? lo : (v > hi ? hi : v),
        double: v < lo ? lo : (v > hi ? hi : v),
        default: /* 整型/其它走宏通用实现 */ (v < lo ? lo : (v > hi ? hi : v))
    );
}
```

- **零新 token**：`_Generic` 已是标准 C；加的只是"允许关联类型引用 model 形参"。
- 用户要写的仍是**标准 C 语法**（`_Generic`），只是其内部可用 model 的形参。

### 3b. 为 model 增加"约束子句"作为注释级元数据（可选，最轻）
不引入可执行语法，只允许在 model 声明上加**约束注解**，供 3c 的实例化解析用。

### 3c. model 实例化时按 `_Generic` 语义挑选定义体（对标 C++ 特化，改动居中）
给 model 登记加"**按 `_Generic` 关联类型表匹配定义体**"：
```c
model (T) T stl_zero(void) { return 0; }        /* 通用 */
model (T) T stl_zero_impl_f(void) { return 0.0f; }  /* 或 */
```
特化体选择复刻 `_Generic` 的 `compare_types` 匹配，不改语法、只改匹配语义。

### 3d. operator 里让 `_Generic` 当选 param 类型（零改动）
现有 `operator` 静态分派已 OK，`_Generic` 只作其中一种判定即可。

## 4. v2 相对 v1 的取舍

| 维度 | v1（`static_if`） | v2（扩 `_Generic`/model） |
|---|---|---|
| 新增保留字 | ❌ 有（`static_if`） | ✅ **零** |
| 标准 C 兼容 | 破坏 | ✅ 纯 C11 语义扩展 |
| 复用既有机制 | 少（新造） | ✅ 多（`_Generic`/model/operator 扩语义） |
| 改动面 | 新解析分支 + 新 token | model 体内关联类型展开 + 实例化匹配 |
| desugar/`--emit-c` 闭环 | 需新脱糖 | `_Generic` 标准 C，脱糖天然通，**负担小** |
| 学习成本 | 新语法 | 已会 C11 的用户零迁移 |

## 5. 落地面（若定稿）

| 阶段 | 产出 | 验收 |
|---|---|---|
| P0 | 3a：model 体内 `_Generic` 关联类型可用形参展开 | 实例化后按 T 选实现，未选分支零代码 |
| P1 | 3c：model 实例化按获取的特化体 | 特化命中正确定义体 |
| P2 | 脱糖/`--emit-c` 回归 | clang 闭环可编译（`_Generic` 标准透传） |
| P3 | 惰求值表达式"类型落"（§7） | 表达式类型确定性 |

## 5.5 落地状态（2026-08-27，以 t082 验证）

| 能力 | 形态 | 状态 |
|---|---|---|
| `constexpr` 常量对象 | `constexpr int N = 8*sizeof(int);` | ✅ **已落地**（t082） |
| 常量进数组尺寸 / switch case / `_Static_assert` | 单遍常量传播 | ✅ 已验证 |
| `constexpr` + `model` + `_Generic` 类型分派 | 编译期类型信息 | ✅ 已验证（配搭） |
| `constexpr` 纯表达式函数折叠 | `constexpr int f(int n){return n*n;}` | ⏳ roadmap（§6.3.2） |
| 惰求值表达式"入类型"（Eigen 级） | §7 | 受 C 类型系统上限，最大逼近 |

> 落地方式（最小语义增量）：`constexpr` 作为一个受限编译期常量限定符，**复用了
> 枚举常量符号机制**（`VT_CONST` + `VT_ENUM_VAL` + `enum_val`）：
> 初始化式走 `expr_const64` 折叠成编译期常量 → 登记为常量符号 → 不分配存储 →
> 引用处单遍常量传播自动取常数值。**未引入新解释器 / AST / 第二遍**。

## 6. 受限 constexpr：纯表达式 / 纯函数内联折叠（含终止性保证）

> 风貌：不引入新的解释器。在 `constexpr` 关键字的既有语义上，只认可「单遍内能折叠」的子集。

### 6.1 范围：拿 `expr_const64` 折叠，不解释函数体

受限 constexpr 是 §2 表中「constexpr 求值器缺失」的最小补齐，分四级能力：

| 形态 | 例子 | 判定 | 依据 |
|---|---|---|---|
| 纯表达式折叠（常量对象） | `constexpr int x = 8*sizeof(int);` | ✅ **已实现** | `expr_const64`(tccgen.c:8383+4) 把整条表达式折叠为 `VT_CONST` |
| 纯表达式函数内联折叠 | `constexpr int f(int n){return n*n;}` | △ 可行(受限) | 脱糖内联 + 调用点 `expr_const64` 折叠（roadmap，§6.3.2） |
| 含循环 / 递归的通用 constexpr | `for`/`while`/自递归 | ✗ 不做 | 需解释执行 + 停机检测(不可判定)，撞单遍哲学 |

关键事实：**已实现的常量对象不涉及函数调用**（`sizeof`/字面量/算术/位运算仍是纯表达式）；
纯表达式函数没有循环结构。`expr_const64` 走 `expr_cond()` 只解析算术 / 三元 / 调用，
`VT_CONST` 折叠。循环根本进不来。

### 6.1.1 已落地：常量对象 + 单遍常量传播

语法（`constexpr` 置于声明最开头，C23 同风格子集）：

```c
constexpr int N         = 4;                 /* 字面量 */
constexpr int BITS      = 8 * sizeof(int);   /* sizeof/算术折叠 */
constexpr unsigned MASK = ((1u << (BITS - 1)) - 1u);  /* 位运算 */

int a[N];                 /* 数组尺寸编译期确定, 非 VLA */
switch (v) { case N: ... }            /* 可作 case 常量 */
_Static_assert(MASK == 0x7FFFFFFFu, "...");  /* 可作断言 */
```

语义规则（约定）：
1. **只能置于声明最开头**（`constexpr int N = ...;`）；暂不支持 `static constexpr` 等组合。
2. **只接受整型常量对象**；初始化式必须是编译期常量表达式，走 `expr_const64`，
   非折叠即 `tcc_error`（无隐式回退到运行时变量）。
3. **不得缺省初始化**（`constexpr int x;` 报错）；不分配存储，无取地址语义。
4. 未实现 `constexpr` 函数（见 §6.3.2）。

> 完整可运行示例见 [tests/t082_constexpr_gen.c](../tests/t082_constexpr_gen.c)。

### 6.2 终止性保证条款（本方案的终止性如何成立）

> 受限 constexpr 的终止性不靠「检测」无限循环/递归（不可判定），而靠**结构与资源约束**：

1. **循环被结构排除**：求值只认表达式（无 `while`/`for`），没有循环结构 →
   「循环不终止」在语法层面即不存在，无需运行时/编译期停机检测。
2. **递归被内联深度护栏约束**：纯表达式函数可自递归（`f(n)=n*f(n-1)`），
   但求值走**内联展开 + 折叠**（非解释器栈）→ 自递归撑爆内联层数。
   编译器加**内联深度上限**（如 `CONST_INLINE_MAX`），超限编译期报错终止。
3. **不引入不可判定问题**：放弃所有含循环的 constexpr → 无需通用停机检测 →
   单遍内「总有界、必终止」。

> 收益：这套保证不需要通用停机分析，只需一个深度计数器，正是受限设计相对
> 通用 comptime 的优雅之处。与 §0「避开解释器 / AST / 第二遍」的硬约束自洽。

### 6.3 实现要点（已落地部分 + roadmap）

#### 6.3.1 已落地：常量对象的单遍实现（tccgen.c `decl()`）

- **token**：`tcctok.h` 新增 `DEF(TOK_CONSTEXPR, "constexpr")`（唯一新增关键字）。
- **识别**：`decl()` 在每轮外层迭代开头 `is_constexpr = (tok == TOK_CONSTEXPR)`，
  置位则先 `next()` 消费，再交给 `parse_btype` 正常解析类型。
- **登记**：进入"逐声明成员"循环时，若 `is_constexpr`：要求 `=` 且整型，
  用 `expr_const64()` 折叠；按枚举常量登记
  `sym_push(v, &ct, VT_CONST, 0)` + `csym->enum_val = cv`，其中
  `ct.t = (整型位) | VT_STATIC | VT_ENUM_VAL`；**不分配存储、不走 `decl_initializer_alloc`**。
- **常量传播**：引用该标识符时，unary 的 `r == VT_CONST && IS_ENUM_VAL(s->type.t)`
  分支自动 `vtop->c.i = s->enum_val` → 单遍内拿到常数值（与枚举常量完全同路）。

#### 6.3.2 roadmap：`constexpr` 纯表达式函数折叠

- 复用 `constexpr` 关键字，扩展「允许引用标记 constexpr 的函数」：函数体必须是
  纯表达式（单 return），调用点若实参全为常量 → 脱糖内联后 `expr_const64` 折叠。
- 内联折叠深度用现有 `inline` / 递归计数器护栏，超限 `tcc_error`（终止保证第 2 条）。

#### 6.3.3 desugar / `--emit-c` 降级（对齐 §4 的标准 C 透传路线）

- 常量对象构造上等价于局部/全局枚举常量：`constexpr int N = 4;`
  ≡ `enum { N = 4 };`。脱糖输出时把 `constexpr` 声明渲染成对应 `enum` / 已折叠的
  `static const`，即可被 clang 等标准 C 编译器透传，**无需新增语法透传通道**。
- 类型级分派（`_Generic`）本就是标准 C 透传（§4 P2）。

## 7. 惰求值表达式"入类型"（唯一真语义扩展点）

Eigen 要 `CwiseBinaryOp<A,B>::...` 把表达式嵌入类型。C 无"类型即值"，但可用
**已有机制**做最大逼近：

- 用 `_Generic` 选表达式"结果类型携带"：按叶子类型静态决定算子结果标量类型，把我们缺的
  "表达式→结果类型"映射交给 `_Generic`，而非类型系统。
- 树上求值仍用 matrix.md §8 的指针 desc；但**结果类型分派**用 `_Generic` 静态判 →
  合成" **类型在编译期定，值在运行时求** "的 C 天然分层。
- `__builtin_reflect(T)`（已实现）把类型信息落数据，`_Generic` 消费之，形成
  "类型→数据→编译期分支"链。

**诚实边界**：C 无法做到"类型即值的嵌套模板"；`_Generic` 只到"按具体类型分支"一层。
要做到 Eigen 级别的"表达式树整棵进类型"，仍受 C 类型系统上限 → 方案不承诺，只做最大逼近。

## 8. 待定问题（交由你决策）

> 2026-08-27 定稿进展：**基线已定**（§3a 的 `_Generic` 扩展 + 唯一 `constexpr` 关键字），
> `static_if`（v1）整体搁置；`constexpr` 常量对象已落地。以下仍待决议：

1. ~基线选择~ → **已定**：以 `_Generic`（3a）+ `constexpr` 为核心，`static_if`（v1）搁置。
2. model 形参进的 `_Generic`，要支持常量参（`_Generic` 里按 `R*C` 等常量判）吗？
3. 特化（3c）与通用体（3a）的匹配优先级：新的 `_Generic` 优先，失败回通用 → 是否够用？
4. desugar/`--emit-c` 是否要求新增关联类型展开的行为对齐，还是纯透传 `_Generic` 即可？
5. `constexpr` 函数折叠（§6.3.2）是否纳入下一里程碑？深度护栏定多少（`CONST_INLINE_MAX`）？

---

> 结论：**v2 已从"方案"走向"落地"** —— 保留字增量最小（唯一 `constexpr`），类型级分派
> 零新增（`_Generic`+`model`），核心机制复用枚举常量符号与 `expr_const64` 完成单遍常量
> 折叠。当前实现见 §5.5 / §6.1.1 / §6.3.1，配 t082 验证。待 §8 决议后推进函数折叠。