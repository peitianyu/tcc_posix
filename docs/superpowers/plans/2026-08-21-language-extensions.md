# TCC 语言扩展实现计划(defer / 对象方法 / model 泛型)

> **面向 AI 代理的工作者:** 必需子技能:使用 superpowers:subagent-driven-development(推荐)或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框(`- [ ]`)语法来跟踪进度。

**目标:** 为 tcc_posix 的 TCC 0.9.28rc 实现三个语言特性:defer(Go 式作用域清理)、对象方法(`v.func()` 命名约定语法糖)、model 泛型(struct/union/function 类型模板)。

**架构:** 三特性独立分层。defer 复用现有 scope cleanup 机制(`cur_scope->cl` 链 + `leave_scope` 全路径触发);对象方法在 unary() 的 `.` 分支加方法回退;model 泛型复用 TokenString + `begin_macro/end_macro` token 保存/重放(宏展开同款机制),实例化 = 类型参数 token 替换 + 重放走标准解析。

**技术栈:** TCC 0.9.28rc 单遍编译器(src/),Windows PE + Linux ELF 双目标,musl libc 测试套件。

**规格:** `docs/superpowers/specs/2026-08-21-language-extensions-design.md`

**关键代码位置(已勘察):**
- 关键字表:`src/tcctok.h`(DEF 宏;TOK 枚举在 tcc.h:1199 自动生成)
- 语句分发:`src/tccgen.c:7189` 起 block();TOK_RETURN 7253 / TOK_BREAK 7281 / TOK_CONTINUE 7286
- scope cleanup:`struct scope`(tccgen.c:113,`cl` 字段)、`try_call_scope_cleanup()`(6987)、`leave_scope()`(7115)、`new_scope()`(7073)、`save_lvalues()`(6975)
- `.` 成员访问:tccgen.c:6169;`find_field()`(4139,传 `v|SYM_FIELD` 时静默返回 NULL 可作无报错探测)
- `parse_btype()`:4721;TOK_STRUCT/UNION 分支 4826;`struct_decl()` 4447
- unary() 函数调用解析:`tok_identifier:` 6119;post operations 循环 6168 起;`gfunc_call(nb_args)` ~6290
- token 重放:`TokenString`(tcc.h:652)、`tok_str_add`(tccpp.c:1056)、`begin_macro/end_macro`(tccpp.c:1068)、`tok_str_add_tok`(tccpp.c:1157)

**开发循环(每个任务末尾):**
```bash
cd /d/work/tcc_posix
# 自举重建双目标 TCC
build/tcc-win.exe -o build/tcc-win.exe src/tcc.c -I src -DONE_SOURCE=1
build/tcc-win.exe -DTCC_TARGET_X86_64 -DCONFIG_TCC_PREDEFS=1 -o build/tcc-linux.exe src/tcc.c -I src -DONE_SOURCE=1
bash install.sh          # 更新 bin/tcc.exe (test.sh 消费)
./test.sh && ./test.sh -run && ./test.sh -linux   # 全回归 3 模式
```
测试自动收集 `tests/tNNN_*.c`;退出码 0 = PASS。新测试先写(编译应失败=特性未实现),实现后编译通过且 rc=0。

---

## 任务 1:defer

**文件:**
- 修改:`src/tcctok.h`(关键字)
- 修改:`src/tccgen.c`(defer 解析 + 记录布局 + cleanup 重放)
- 创建:`tests/t029_defer.c`

- [ ] **步骤 1:编写失败测试 `tests/t029_defer.c`**

覆盖:单/多 defer 逆序、块级作用域、return 路径、goto 跨块、参数快照语义(注册后改值不影响)。模式参考现有 t0xx 测试(退出码 0 = 通过,返回值区分失败点):

```c
/* 测试: defer 延迟执行 (Go 式: 注册点求值, 离开作用域逆序调用) */
#include <stdio.h>
static int order[16], n;
static void rec(int v) { order[n++] = v; }
static void setp(int *p, int v) { *p = v; }

int main(void) {
    /* 1. 基本逆序: 注册 f(1) f(2) f(3) → 执行 3 2 1 */
    { defer rec(1); defer rec(2); defer rec(3); }
    if (n != 3 || order[0] != 3 || order[1] != 2 || order[2] != 1) return 1;

    /* 2. 块级: 内层块 defer 在块退出时执行, 外层不受影响 */
    n = 0;
    { defer rec(10); { defer rec(11); } if (n != 1 || order[0] != 11) return 2; }
    if (n != 2 || order[1] != 10) return 3;

    /* 3. return 路径: 函数返回前执行 defer */
    n = 0;
    { int r = ret_test(); if (r != 0) return 5; if (n != 1 || order[0] != 20) return 6; }
    /* 4. 参数快照: 注册点求值, 之后改变量不影响 */
    { int x = 5; defer setp(&x, 99); x = 1; if (x != 1) return 4; }
    /* 上面块退出时 setp(&x, 99) 执行 — 需要观察 x, 故用指针记录 */

    printf("defer ok\n");
    return 0;
}
static int ret_test(void) { defer rec(20); return 0; }
```

- [ ] **步骤 2:运行确认失败**

运行:`bin/tcc.exe -c tests/t029_defer.c -o /tmp/t.o -I src/posix/musl-nt64/include -I src/posix/musl-nt64/arch/nt64 -std=c99`
预期:编译错误(`defer` 未定义 / "expression expected")。注意:测试 3 的 return 路径需用辅助函数验证,把 return 测试放进独立函数。

- [ ] **步骤 3:实现 — 加关键字**

`src/tcctok.h` 在 `DEF(TOK_DEFAULT, "default")` 后加:`DEF(TOK_DEFER, "defer")`。TOK 编号自动生成,无需改 tcc.h。

- [ ] **步骤 4:实现 — block() 语句分发**

`src/tccgen.c` block() 中 TOK_RETURN 分支(7253)前加 TOK_DEFER 分支,调用新函数 `defer_statement()`(本任务步骤 5-6 实现,声明加在 tccgen.c 顶部静态原型区 ~140 行附近):

```c
} else if (t == TOK_DEFER) {
    defer_statement();
} else if (t == TOK_RETURN) {
```

- [ ] **步骤 5:实现 — defer 记录布局与注册**

新增静态函数 `defer_statement(void)`,逻辑:
1. 校验:tok 必须为标识符且查得函数符号(否则 `tcc_error("defer requires a function call")`);函数外(`!cur_scope || !local_stack`)报错
2. 模拟 unary() 调用解析:函数符号 `vset` 压栈后,遇 `(` 逐参数 `expr_eq()` + `gfunc_param_typed()`(参考 unary() 中 `(` 分支的参数循环 ~6250-6290)
3. 此时 vstack 上 = 函数指针 + 各参数值(顺序与 unary 调用前一致)。为每条 defer 分配记录区:累计各参数 `type_size()` 求总大小与对齐,用 `get_temp_local_var(size, align, &r2)`(tccgen.c:6975 同款)在帧上分配 D;把函数指针 + 各参数按偏移 store 进 D(参考 save_lvalues 的 vset/vstore 用法)
4. 构建 defer 描述(静态结构,含:参数类型序列、D 的偏移):挂到 `cur_scope->cl` 链 — 创建 Sym 插入链头(`cur_scope->cl.s` 前插),Sym 的 `v` 字段标记 `SYM_FIELD | DEFER_MARKER`(新增 #define,如 `#define SYM_DEFER (SYM_FIELD|1)` 形式,避开现有标志位;检查 tcc.h 中 SYM_* 标志定义后选未用位),`cleanup_sym` 指向 D 的 Sym
5. `skip(';')`

**边界(实现时校验)**:defer 参数类型必须定长 —— 解析参数后逐个检查 `type_size` 时遇 VLA(类型带 VT_VLA 标志或 size 依赖运行时)即 `tcc_error("defer argument must not be VLA")`;函数外使用 defer 报错。

- [ ] **步骤 6:实现 — cleanup 退出点重放**

`try_call_scope_cleanup()`(6987)遍历 cl 链时,遇 DEFER_MARKER 的 Sym:不调用 cleanup_func,而是生成内联重放 —— 按描述中参数类型序列逆序 load 到 vstack(值已在 D 中,`vset(&type, VT_LOCAL|VT_LVAL, D_off+i)` + `indir()` 取回),最后 load 函数指针,`gfunc_call(nb_args)`。attribute cleanup 走原路径不变。

- [ ] **步骤 7:运行测试验证通过**

运行:`bash install.sh && ./test.sh`
预期:t029_defer PASS;现有 28 测试全 PASS(回归无破坏)。

- [ ] **步骤 8:Commit**

```bash
git add src/tcctok.h src/tccgen.c tests/t029_defer.c
git commit -m "feat: defer 语句 (Go 式作用域清理, 复用 scope cleanup 机制)"
```

---

## 任务 2:对象方法 (C++ 式, 无关键字)

**设计 (2026-08-21 与伙伴讨论后重写)**:struct 体内函数定义 = 方法, 无需 `method` 关键字 (C++ 同款语法)。成员解析后类型为 VT_FUNC 且后跟 `{` → 方法。隐式 self 参数 (编译器注入, 名字 `self`, 类型 = `struct*`), 方法体可直接引用字段 (token 替换为 `self->字段`)。调用 `v.func()` / `pp->func()` 自动注入 self。方法编译为内部函数 `__method_<id>_<方法名>` (static, 编译器合成名, 用户不可见), 方法表 `{struct sym → id}` 查表调用。

**示例:**
```c
struct Point {
    int x, y;
    int sum(void) { return x + y; }          /* 方法: 隐式 self, 直接写字段 */
    void set(int a, int b) { x = a; y = b; }
    int combo(int k) { return self->sum() + self->mul(k); }  /* 方法互调 */
};
Point p = { 3, 4 };
p.sum();        /* ≡ __method_0_sum(&p) */
pp->sum();      /* 指针同形 */
```

**已知限制** (文档化):
- 方法引用的字段必须声明在方法之前 (收集时替换需要字段集合)
- 方法体局部变量/参数不得与字段同名 (会被替换成 self->字段)
- 方法体内调用其他方法须 `self->` (self 是注入参数, 可直接用)
- 方法必须带函数体 (纯声明不支持);self 为方法保留参数名
- 方法随类型作用域:局部 struct 的方法符号只在定义它的函数内可调

**文件:**
- 修改:`src/tccgen.c`(方法数据结构 + struct_decl 拦截 + gen_function 重放 + unary 调用注入)
- 创建:`tests/t030_method.c`

- [x] **步骤 1:编写失败测试 `tests/t030_method.c`**

覆盖:基本调用+返回值、self 修改字段、`->` 调用、参数+字段引用、方法互调 (self->)、匿名 struct+typedef、递归方法、字段与方法同名时字段优先:

```c
/* 测试: 对象方法 (C++ 式: struct 体内函数定义, 隐式 self, . 与 -> 调用) */
#include <stdio.h>
struct Point {
    int x, y;
    int sum(void) { return x + y; }
    void set(int a, int b) { x = a; y = b; }
    int mul(int k) { return x * k; }
    int combo(int k) { return self->sum() + self->mul(k); }
};
typedef struct { int v; int get(void) { return v; } } Box;
struct Fact { int f(int n) { return n <= 1 ? 1 : self->f(n - 1) * n; } };
int main(void) {
    struct Point p = { 3, 4 };
    if (p.sum() != 7) return 1;
    p.set(10, 20);
    if (p.x != 10 || p.y != 20) return 2;
    struct Point *pp = &p;
    if (pp->sum() != 30) return 3;
    if (p.mul(3) != 30) return 4;
    if (p.combo(2) != 50) return 5;
    Box b = { 9 };
    if (b.get() != 9) return 6;
    struct Fact f;
    if (f.f(5) != 120) return 7;
    struct S { int sum; };
    struct S s = { 5 };
    if (s.sum != 5) return 8;
    printf("method ok\n");
    return 0;
}
```

- [x] **步骤 2:运行确认失败**

运行:`bin/tcc.exe -c tests/t030_method.c ...` 预期:成员循环对函数类型报 "invalid type" 或类似。

- [ ] **步骤 3:实现 — 数据结构与方法表**

tccgen.c 在 struct_decl 前新增 (方法用 token 流 + gen_function 重放):

```c
typedef struct MethodDef {
    struct MethodDef *next;
    int name;              /* 方法名 tok */
    CType sig;             /* 函数类型 (返回 + 参数链 ref->next, 不含 self) */
    TokenString body;      /* 方法体 token 流 (字段已替换为 self->字段) */
    Sym *sym;              /* 内部函数符号 */
    int func_tok;          /* 内部函数名 token (__method_<id>_<名>) */
} MethodDef;
typedef struct MethodType {
    struct MethodType *next;
    Sym *ref;              /* struct 符号 */
    int id;                /* -1 = 未分配 (收集期) */
    MethodDef *methods;
} MethodType;
static MethodType *method_types;   /* 方法表: 调用点查 ref → id */
static int method_id_counter;
static int method_pending_self;    /* '(' 分支的 self 注入标记 */
static int tok_self;               /* "self" 标识符 token (惰性) */
```

辅助函数:`method_type_get(Sym*)`(查/建)、`method_register`、`method_type_id(CType*)`(调用点查表)、`method_is_field`、`tok_self()`、`method_parse(Sym*, int name, CType*)`、`method_emit_all(Sym*)`、`method_lookup(int ident)`。

- [ ] **步骤 4:实现 — struct_decl 成员循环拦截**

type_decl 之后 (现有 `(type1.t & VT_BTYPE) == VT_FUNC → tcc_error` 之前, 且必须在 `type_size` 不完整检查之前):

```c
if ((type1.t & VT_BTYPE) == VT_FUNC) {
    /* C++ 式方法: 需函数体 */
    if (tok != '{')
        tcc_error("method '%s' needs a body", get_tok_str(v, NULL));
    method_parse(s, v, &type1);
    if (tok == ';')   /* 方法后可选 ';' */
        next();
    goto method_member_done;   /* 跳过字段布局与外层 skip(';') */
}
```
(内层 while(1) 后的 `skip(';')` 处加标签 `method_member_done:`。)

struct_decl 收尾 (`struct_layout` 之后) 调 `method_emit_all(s)`。

**method_parse 收集方法体** (tok 在 `{` 上):
1. 花括号深度计数收集 token 到 `m->body`
2. 字段替换:tok >= TOK_IDENT 且在 `s->next` 字段链 (方法前的字段) → 输出 `self` `TOK_ARROW` 字段名 三 token (注意 tokc.i 同步:标识符 token 的 tokc.i = tok 值)
3. 方法名重复检测在 emit 时做

**method_emit_all 两阶段** (struct 完成后):
- 阶段 1 (符号注册, 支持方法互调):合成函数名 `__method_<id>_<名>` (tok_alloc_const),在 `m->sig.ref->next` 参数链头插入 self 参数符号 (`tcc_malloc` 构造,`v = tok_self()`,type = struct 指针,next/prev 双向链接),`external_sym(func_tok, &sig, 0, &ad)` 注册
- 阶段 2 (方法体编译):逐个 `begin_macro(&m->body, 1); next(); gen_function(m->sym); end_macro();` — gen_function 要求 tok 在 `{` 上,自包含 (参数由 sym_push_params 从参数链推入)。**必须保存/恢复 cur_scope/root_scope/local_scope** (gen_function 会改它们, 局部 struct 场景会破坏外层作用域)

- [ ] **步骤 5:实现 — 调用点注入 (unary `.`/`->` 分支 + `(` 分支)**

`.`/`->` 分支:find_field 探测失败 (SYM_FIELD 静默模式) 且标识符且后跟 `(` → `method_lookup(ident)`:

```c
next();
s = find_field(&vtop->type, tok | SYM_FIELD, &cumofs);
if (!s && tok >= TOK_UIDENT) {
    int ident = tok, t1 = next();
    if (t1 == '(' && method_lookup(ident))
        continue;              /* tok = '(' → '(' 分支完成调用 */
    tok = t1;
    s = find_field(&vtop->type, tok, &cumofs);   /* 报错路径 */
}
```

**method_lookup**:查方法表得 id → 拼 `__method_<id>_<名>` → sym_find;无 → tcc_error;有 → `gaddrof(); mk_pointer();` 得 self 指针 → `vset(&m->type, m->r, m->c)` 压函数值 (vtop->sym = m,VT_SYM 时 c.i = 0)→ `vswap()` 得 [func, self] → `method_pending_self = 1` → 返回 1。

`(` 分支开头 (unary 内, 局部取 method_pending_self 并清零):
```c
if (method_self) vswap();          /* vtop: self→func, 供类型检查 */
...类型检查 + s = vtop->type.ref + next()...
if (method_self) {
    vswap();                       /* 还原 [func, self], vtop = self */
    nb_args = 1;                   /* self 已入栈 */
    if (sa) sa = sa->next;         /* 跳过 self 原型参数 */
}
```

- [ ] **步骤 6:运行测试验证通过**

运行:`bash install.sh && ./test.sh` 预期:t030_method PASS;全回归 PASS (含 t029_defer)。

- [ ] **步骤 7:Commit**

```bash
git add src/tccgen.c tests/t030_method.c
git commit -m "feat: 对象方法 (C++ 式 struct 体内定义, 隐式 self, . 与 -> 自动注入)"
```

## 任务 3:model struct/union

**文件:**
- 修改:`src/tcctok.h`(关键字)
- 修改:`src/tccgen.c`(model 定义解析 + 实例化)
- 创建:`tests/t031_model.c`

- [ ] **步骤 1:编写失败测试 `tests/t031_model.c`**

```c
/* 测试: model 泛型 struct/union */
#include <stdio.h>
model struct Array(T) { T *data; int len; };
model union Val(T) { T v; int tag; };

int main(void) {
    /* 1. 实例化 + 字段访问 */
    float buf[3] = { 1.5f, 2.5f, 3.5f };
    Array(float) a = { buf, 3 };
    if (a.len != 3 || a.data[1] != 2.5f) return 1;
    /* 2. 缓存复用: 同参类型一致 (sizeof 相同) */
    Array(float) b;
    if (sizeof a != sizeof b) return 2;
    /* 3. 多类型参数 / 不同实例互不影响 (T 直接作成员, size 随 T 变) */
    model struct Box(T) { T v; };
    if (sizeof(Box(double)) == sizeof(Box(int))) return 3;  /* 8 vs 4 */
    /* 4. union 实例化 */
    Val(int) u;
    u.v = 42;
    if (u.v != 42) return 4;
    /* 5. typedef 复用 */
    typedef Array(double) DArr;
    DArr d;
    if (sizeof d != sizeof(Array(double))) return 5;
    printf("model ok\n");
    return 0;
}
```

- [ ] **步骤 2:运行确认失败**

运行:`bin/tcc.exe -c tests/t031_model.c -o /tmp/t.o -I src/posix/musl-nt64/include -I src/posix/musl-nt64/arch/nt64 -std=c99`
预期:编译错误 `model` 未定义。

- [ ] **步骤 3:实现 — 加关键字 + model 记录结构**

`src/tcctok.h`:`DEF(TOK_MODEL, "model")`。tccgen.c 新增静态数据结构:

```c
typedef struct ModelDef {
    int kind;              /* VT_STRUCT / VT_UNION / VT_FUNC */
    int name;              /* 模板名 tok */
    int nparams;           /* 类型参数个数 */
    int *params;           /* 类型参数名 tok 数组 */
    TokenString body;      /* 成员声明/函数体 token 流 */
    struct ModelDef *next; /* 全局模型表 */
} ModelDef;
static ModelDef *model_list;
/* 实例化缓存: 合成名 -> ModelDef, 查表即查重 */
```

- [ ] **步骤 4:实现 — parse_btype 识别 model 定义**

`parse_btype()`(4721)的 while(1) switch 开头加 `case TOK_MODEL:` 分支:
1. `next()` 后须 TOK_STRUCT/TOK_UNION(`tcc_error` 否则)
2. 解析模板名 + `(T1, T2, ...)` 类型参数列表(token 值存入 params)
3. 进入**记录模式**:调用 `tok_str_new()` 建 TokenString,循环 `tok_str_add_tok(&ts)`(追加当前 token + `next()`)直到 `}`(括号深度计数,嵌套 struct 成员也计入);含 `}` 前最后一个 token
4. 校验:类型参数名不与成员名重复(记录时已可扫描);登记 ModelDef 到 model_list
5. **不生成任何代码**,返回 0(表示 btype 未完成,由调用方继续解析后续声明)

- [ ] **步骤 5:实现 — 类型位置实例化**

`parse_btype()` 的 TOK_IDENT 分支(现有 typedef 处理附近,~4850):识别 `Name(` 且 `Name` 在 model_list → 调用 `model_instantiate(name, ...)`:
1. 解析实参类型 token(递归:`parse_btype` + `type_decl` 得 CType,再把 CType 转回 token 序列存入实参 TokenString;实参为泛型实例时其内部名已定,直接入 token)
2. 合成内部名:`Name` + `_` + 各实参名拼接(如 `Array_float`);查缓存(全局表或 `sym_struct` 查重)——已存在直接复用其 CType
3. 复制模板 body 到新 TokenString,扫描替换:TOK_IDENT == 类型参数名的 token 换成实参 token 序列(嵌套实例化时实参 token 里含 `Name(` 序列,重放时自然递归实例化)
4. `begin_macro(&ts, 1)` 重放:`parse_btype` + 走 struct_decl 正常布局(如同 `struct 合成名 { 替换后成员 };`);`end_macro()`
5. 返回合成 struct 的 CType

**边界(实现时校验)**:成员类型必须定长 —— 记录模式下扫描到 VLA 成员即 `tcc_error("model member must not be VLA")`;类型参数名与成员名重复报错。

- [ ] **步骤 6:运行测试验证通过**

运行:`bash install.sh && ./test.sh`
预期:t031_model PASS;全回归 PASS。

- [ ] **步骤 7:Commit**

```bash
git add src/tcctok.h src/tccgen.c tests/t031_model.c
git commit -m "feat: model 泛型 struct/union (token 重放 + 类型参数替换)"
```

---

## 任务 4:model function

**文件:**
- 修改:`src/tcctok.h`(function 关键字)
- 修改:`src/tccgen.c`(函数记录 + 实例化调用)
- 创建:`tests/t032_model_fn.c`

- [ ] **步骤 1:编写失败测试 `tests/t032_model_fn.c`**

```c
/* 测试: model function 泛型 */
#include <stdio.h>
model function (T) T max2(T a, T b) { return a > b ? a : b; }
model function (T) void swap2(T *a, T *b) { T t = *a; *a = *b; *b = t; }

int main(void) {
    /* 1. 实例化调用 + 返回值 */
    if (max2(int)(3, 7) != 7) return 1;
    if (max2(double)(2.5, 1.5) != 2.5) return 2;
    /* 2. void 返回 + 函数体内 T 局部变量 */
    int x = 1, y = 2;
    swap2(int)(&x, &y);
    if (x != 2 || y != 1) return 3;
    /* 3. 嵌套 struct 实参 */
    model struct Pair(T) { T a, b; };
    model function (T) T first2(Pair(T) *p) { return p->a; }
    Pair(int) p = { 9, 8 };
    if (first2(int)(&p) != 9) return 4;
    printf("model fn ok\n");
    return 0;
}
```

- [ ] **步骤 2:运行确认失败**

运行:`bin/tcc.exe -c tests/t032_model_fn.c -o /tmp/t.o -I src/posix/musl-nt64/include -I src/posix/musl-nt64/arch/nt64 -std=c99`
预期:编译错误(`model function` 未识别)。

- [ ] **步骤 3:实现 — model function 定义记录**

`src/tcctok.h`:`DEF(TOK_FUNCTION, "function")`。

扩展任务 3 步骤 4 的 TOK_MODEL 分支:kind == VT_FUNC 时:
1. `next()` 后解析 `(T1, ...)` 类型参数列表(与 struct 一致)
2. 记录模式:保存整个函数头+体 token 流(返回类型 → 函数名 → 参数列表 → `{...}` 到匹配 `}`),括号深度计数(花括号嵌套)
3. 类型参数先注册为占位(仅记录阶段用,重放前替换,不真进符号表)——用局部临时符号或直接靠替换机制,避免污染全局符号表
4. 登记 ModelDef(kind=VT_FUNC),不生成代码

- [ ] **步骤 4:实现 — 实例化调用识别**

unary() 的 `tok_identifier:`(6119)处理:符号是泛型函数(model_list 中)时,`next()` 后若 tok == '(':
1. **尝试类型解析**:保存现场,尝试把括号内容解析为类型列表(`parse_btype` + `type_decl`,遇非类型失败则回退)
2. 成功 → 泛型实例化调用:
   - 合成内部函数名 `Name_实参名`(如 `max2_int`),查缓存
   - 未实例化过:复制函数体 token 流,类型参数 token 替换为实参 token,`begin_macro` 重放走标准函数定义(`decl` → 生成内部函数);缓存
   - 已实例化:直接取符号
   - 然后继续解析 `(调用参数)` 正常调用内部函数
3. 失败(括号内非类型)→ 回退普通调用解析(`f(x)(y)` 函数指针调用语义不变)

**边界**:递归自实例化 —— 实例化函数体重放时若遇到对同一泛型函数的自实例化调用(缓存未完成),`tcc_error("recursive model function instantiation")`;实现时实例化前先查缓存,未命中则标记"进行中",重放中再遇同参实例化即报错。

- [ ] **步骤 5:运行测试验证通过**

运行:`bash install.sh && ./test.sh && ./test.sh -run && ./test.sh -linux`
预期:t032_model_fn PASS;三模式全回归 PASS。

- [ ] **步骤 6:Commit**

```bash
git add src/tccgen.c tests/t032_model_fn.c
git commit -m "feat: model function 泛型 (函数体 token 重放, 实例化调用)"
```

---

## 收尾

- [ ] **全量验证**:`./test.sh && ./test.sh -run && ./test.sh -linux` 三模式全 PASS(win 30 + run 30 + linux 30)
- [ ] **README 更新**:已知限制移除/新增语言特性说明;测试数 28 → 30/套
- [ ] **Commit**:`git commit -m "docs: README 更新语言特性 (defer/对象方法/model 泛型)"`
