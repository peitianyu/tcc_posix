# TCC 语言扩展设计:defer / 对象方法 / model 泛型

日期:2026-08-21
状态:已批准(用户审查通过)

## 目标

为 tcc_posix 的 TCC 0.9.28rc 分支实现三个语言特性,构成"数值计算脚本引擎"的差异化灵魂:

1. **defer** — 离开作用域时逆序执行注册的清理函数(Go 式语义)
2. **对象方法** — `v.func(args)` → `Array_func(&v, args)` 命名约定语法糖
3. **model 泛型** — `model struct Array(T)` / `model function (T) T max(T a, T b)` 类型模板,实例化时克隆成员声明/函数体替换类型

三者相互独立,分阶段实现,每阶段独立提交/测试。

## 架构约束

- TCC 0.9.28rc 是单遍编译器,**没有独立 AST**(tccgen.c 中解析与代码生成交织)
- 三个特性各自落在不同层面,互不阻塞:
  - defer → 代码生成层,复用现有 scope cleanup 机制
  - 对象方法 → 表达式解析层(`.` 成员访问分支)
  - model 泛型 → 词法 + 类型解析层,复用 TokenString/macro_ptr 重放机制(struct 成员声明与函数体共用同一套记录/替换/重放基础设施)
- 现有关键机制(已验证存在):
  - `struct scope` 的 `cl` 清理链 + `try_call_scope_cleanup()`(tccgen.c:6987)+ `leave_scope()`;return/break/continue/块尾/goto 跨块全部调用清理链
  - `TokenString`(tcc.h:652)+ `begin_macro/end_macro`(tccpp.c:1068)token 保存/重放
  - `.` 成员访问分支(tccgen.c:6169,`find_field` 失败可作方法回退)

## 阶段一:defer

### 语法

```c
defer func(args...);   // 仅函数调用形式
```

### 语义(Go 式)

- 参数在**注册点**求值并快照保存(值语义);传指针参数仍为引用语义
- 离开注册所在作用域时**逆序**执行(后注册先执行)
- 作用域含:块尾、return、break、continue、goto 跨块

### 实现

1. **词法**:tccpp.c 关键字表增加 `defer`(TOK_DEFER)
2. **解析**:block() 语句分发(TOK_RETURN 附近)遇 `defer` → 正常解析函数调用表达式(函数符号+参数值已在 vstack)
3. **记录布局**:编译期在当前函数帧分配 defer 记录局部变量 D:
   `{ void (*func)(); <arg1> <arg2> ... <argN>; }`(按各参数类型对齐)
   注册点生成 store 代码:函数指针+参数值写入 D
4. **挂链**:D 的 Sym 注册到 `cur_scope->cl` 链,打 defer 标记(区别于 attribute cleanup)
5. **退出点**:修改 `try_call_scope_cleanup()` — 遇 defer 标记生成内联重放代码(从 D load 函数指针+参数 → `gfunc_call()`);attribute cleanup 走原路径。逆序由 cl 链插入顺序天然保证(链头=最新=最后调用)
6. **退出路径全免费**:return(7253 行 `leave_scope(root_scope)`)、break/continue(7280/7286)、块尾(7246 `prev_scope`)、goto 跨块(block_cleanup)均已调用清理链

### 边界

- defer 参数类型必须定长(禁止 VLA 参数,报错)
- defer 不能在函数外使用(报错)
- 与 `__attribute__((cleanup))` 共存:defer 记录挂链不带 cleanup_func,用独立标记区分

### 改动量

tccpp.c ~10 行 + tccgen.c ~120 行

## 阶段二:对象方法

### 语法

```c
void Array_print(Array *self, ...);   // 定义:普通函数,命名约定 <类型名>_<方法名>
v.print(args);                        // 调用:自动重写为 Array_print(&v, args)
```

### 实现(集中在 unary() `.` 分支 tccgen.c:6169)

1. `next()` 取得标识符后 **优先成员访问**:`find_field` 成功 → 原路径(.field 语义零影响)
2. find_field 失败 **且** 下一 token 是 `(` → 方法解析:
   - 取 v 最终类型的类型名:具名 struct/union 用标签名;typedef 用别名;匿名无 typedef → 报错
   - 拼 `Array_<tok>` 查符号表
   - 找到 → v 的地址压为第一参数,继续正常调用解析
   - 找不到 → 报错 "`Array` has no method `tok`"
3. **左值要求**:v 必须左值;非左值报错(第一阶段)
4. **歧义**:字段与重名方法 → 字段优先(标准 C 兼容)
5. **`->` 方法调用**:与 `.` 共享同一解析分支(6169 行 `indir()` 解引用后走相同路径),方法回退分支天然覆盖,顺带支持(`p->func()` → `Array_func(p, args)`)

### 改动量

~200 行,全在 tccgen.c unary 路径。无新关键字。

## 阶段三:model 泛型

### 语法

```c
model struct Array(T) { T *data; int len; };   // struct 定义
Array(float) a;                                 // 实例化(类型位置)
model union U(T) { T v; int tag; };
model function (T) T max(T a, T b) {            // function 定义
    return a > b ? a : b;
}
float m = max(float)(x, y);                     // 实例化调用
```

**struct/union 定义**与**function 定义**共用同一套基础设施:记录 token 流 → 类型参数替换 → 重放走标准解析路径。区别只在保存范围:struct 保存成员声明到 `}`,function 保存整个函数头+体到匹配 `}`(括号深度计数)。

### struct/union/typedef 实现

1. **词法**:新增关键字 `model`(TOK_MODEL)
2. **定义解析**(parse_btype 识别 `model`):
   - 解析 `struct/union Name(T1,...)` 与类型参数列表
   - **记录模式**:成员声明 token 流原样存入 TokenString(tok_str_add_tok 逐个追加)直到 `}`;**不立即布局**
3. **实例化**(类型位置出现 `Name(`):重放成员 token 流走标准 struct 声明解析:
   - `begin_macro` 把 TokenString 设为 token 源
   - 重放前做类型参数替换:TOK_IDENT 等于 T1/T2 的替换为实参 token 序列(实参可递归为泛型实例)
   - 走标准 struct_layout 生成真实 struct
4. **歧义规避**:类型参数名全大写(如 T/U/Elem),字段/局部变量名小写——靠命名约定,不解析歧义;定义时校验类型参数名不得与成员名重复
5. **内部命名与缓存**:内部标签合成名 `Array$<实参名>`(嵌套递归拼接);相同实参重复实例化查缓存复用,保证类型一致
6. **typedef 泛型**:以"实例化结果可被 typedef 复用"为准(已有机制);`model typedef` 直接定义别名模板的语法不在本次范围
7. **边界**:成员类型必须定长,禁止 VLA 成员(报错)

### function 泛型

语法要点:**类型参数列表紧跟 `model function`**(`(T1, T2)` 在最前),解决"返回类型含 T 时 T 尚未定义"的鸡生蛋问题;之后是标准 C 函数声明。

1. **定义解析**:遇 `model function` → 解析 `(T1, T2)` 类型参数列表 → 把 T1/T2 注册为待定标识符(允许在返回类型/参数/函数体中使用)→ 解析函数声明并进入记录模式,整个函数头+体 token 流存入 TokenString(括号深度计数到匹配 `}`)→ 不生成代码
2. **实例化调用**:unary() 中识别 `Name(<实参>)(<调用参数>)` 模式:
   - `Name` 是泛型函数(符号表标记)且第一对括号内容为类型 token 序列 → 泛型实例化调用;否则走普通函数调用(兼容 `f(x)(y)` 函数指针返回调用)
   - 判定规则:括号内容全部解析为类型(实参可为泛型实例,递归)→ 实例化
3. **重放**:替换 T → 实参 token(嵌套泛型实参递归处理),走标准函数定义路径,生成内部函数名 `Name$float`;调用点直接调用内部函数
4. **缓存**:同参实例化复用(重放会重新定义同名符号,无缓存必重定义报错);函数体内自实例化(递归泛型)第一阶段报错(缓存未完成写入)
5. **自然获得**:函数体内的 `T x;`、`sizeof(T)`、`(T)expr` 在重放时全部正常解析——token 重放即重新解析,无需特殊处理

### 改动量

tccpp.c ~40 行 + tccgen.c ~400 行(含 function 泛型),总 ~440 行

## 测试计划(每阶段独立)

| 阶段 | 新增测试 | 验证点 |
|------|----------|--------|
| defer | t029_defer | 单/多 defer 逆序、块级、return 路径、goto 跨块、break/continue、参数快照语义(注册后改值不影响)、与 cleanup 属性共存 |
| 对象方法 | t030_method | 基本调用、参数传递、self 修改、`.` 与 `->` 两种调用形式、字段与方法同名歧义、无方法报错 |
| model | t031_model | struct/union 实例化、多类型参数、嵌套实例化、缓存复用(同类型一致性)、typedef 复用;function 泛型:定义/实例化调用/多参数/嵌套 struct 实参/递归自实例化报错;泛型实例与对象方法配合(`Array(float) a; a.print()`) |

每阶段完成即跑 `test.sh` 全回归(现有 84 测试不受影响——均为新增语法,不触标准 C 路径)。

## 范围外(明确不做)

- defer 优先级调度(defer_prio,opt.txt 扩展项)
- model 泛型变量(顶层 `model T x`)
- `model typedef` 别名模板直接定义语法
- 矩阵字面量等 opt.txt 其他特性
