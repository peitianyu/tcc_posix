# 基础版 STL（扩展 C 模拟）设计方案

日期: 2026-08-24
状态: 已确认（范围/内存模型/谓词/迭代器/命名 逐项拍板，见 §13）

## 0. 背景与目标

以 `zouxiaohang/TinySTL` 为参考（C++11 的 STD 子集+超集，练习数据结构与泛型编程），
在本 tcc_posix 的**扩展 C** 环境里实现一个"基础版 STL"：把 C++ 模板容器/算法/空间配置器
用已有的语言扩展（`model` 泛型、对象方法、`operator` 重载、`defer`）+ 内存治理头库
（arena/pool/ctmem/mmap/own）承载，逼近原生 C++ 的书写体验。

不是 C++ 移植，而是**保留扩展 C 的轻量值语义与脚本引擎定位**，做 C++ 标准库
的差异化等价物。

范围（M0）：核心**序列**组件（vector/list/pair/stack/queue）+ 基础**算法**。
关联容器（map/set/unordered_*）列为 M1。**String 设计已定稿**（见 §7.4，M1 优先实现）。
语言：**扩展 C**。内存模型：**池式 arena/pool 整池回收**（M0 元素限 POD 值语义），
heap 逐对象析构后端列为 M1。

## 1. 语言承载能力映射

| C++ 模板/STL 能力 | 扩展 C 对应 | 备注 |
|---|---|---|
| 类模板 `template<typename T> struct C` | `model struct C(T) { ... }` | 实例化为真实 struct，字节级值语义 |
| 函数模板/算法 | `model function (T) ...` | 实例化时 token 重放走标准解析 |
| 谓词/比较器默认 | **元素类型 `operator<` / `operator==`** | 实例化重放时 `a<b` 触发 operator 重载分发，免显式比较器 |
| 可传自定义比较器 | 泛型算法第二参：比较函数指针 | 仿 STL `comp` 重载 |
| 成员函数 `v.size()` | **`model (T)` 泛型函数显式调用** `Vector_size(int)(&v)` | 无对象方法糖（§13-决策6）；STL 容器方法统一显式实例化 |
| RAII/析构 | `defer` 或 **arena 整池回收**（默认路径无需析构） | 容器生命周期随 arena；heap 后端用 `defer`+`tcc_release` |
| 分配/构造切面 | allocator 头（arena/pool 后端） | `tcc_release` 统一销毁入口 |
| 运行时多态（抽象迭代器/任意类型池） | **对象内嵌 vptr / 接口表**（项目惯例） | 仅链式容器/异构场景用；连续容器走裸指针零开销 |

关键机制（决定架构）：**`model function` 实例化 = token 替换 + 标准路径重放**。
因此泛型算法体里的 `a < b`、`*it`、`it->` 在重放后解析到具体 T 的 `operator*` /
字段访问——泛型度与 C++ 模板等价，但无需显式实例化签名。

## 2. 整体架构（分层）

```
┌────────────────────────────────────────────┐
│  Algorithm  (model function 泛型):          │
│   sort/find/copy/for_each/minmax/reverse  │
│   ← 经 operator< / operator== 默认驱动     │
├────────────────────────────────────────────┤
│  Container  (model struct 泛型):           │
│   pair / vector / list / string /         │
│   stack / queue                            │
├────────────────────────────────────────────┤
│  Iterator:                                 │
│   连续容器 → 裸指针 (零开销)               │
│   链式容器 → 抽象迭代器 (对象内嵌 vptr)    │
├────────────────────────────────────────────┤
│  Memory / Lifecycle:                       │
│   allocator = self-contained 池式 SLT_Arena│
│     (仅依赖 musl malloc) + arena 整池回收   │
└────────────────────────────────────────────┘
```

依赖方向：算法 → 迭代器 → 容器 → allocator；allocator 不依赖上层。
发行约束：STL 头**只依赖 musl 标准头**（stddef/stdlib/string/…），不 include 本项目
私有头（tcc-arena/tcc-own/tcc-esc…）——脱糖交 clang 时那些文件不在其 sysroot。
分配结果一律判空（警告级）；指针生命周期随 arena，禁止跨 reset 留用。

## 3. 空间配置器 (allocator)

self-contained 池式 bump arena（`SLT_Arena`，仅用 musl `malloc`），头内内联实现，
不依赖本项目 `lib/tcc-arena.h`。容器持一个 `SLT_Arena*`，元素数组从中 bump 分配，
生命周期 = arena 生命周期：`slt_arena_reset/destroy` 一回收，无逐对象 free/析构。

```c
#define SLT_ALIGN 16u        /* 保守统一对齐, 覆盖 double/指针/max_align_t 级 POD */
typedef struct SLT_Arena SLT_Arena;
SLT_Arena *slt_arena_new(size_t grow);               /* 0 ⇒ 默认块大小 */
void  *slt_arena_alloc(SLT_Arena*, size_t size, size_t align);
void   slt_arena_reset  (SLT_Arena*);                /* 整池回卷, 复用容量 */
void   slt_arena_destroy(SLT_Arena*);                /* 归还全部块 */
```

- **M0 纯池式**：容器数据从 arena bump；整池回收，元素限 POD / 值语义 struct。
- **heap 逐对象析构后端**（`malloc/free` + 元素析构回调）整体列为 M1。
- 对齐：统一 `SLT_ALIGN=16`；M0 不依赖 `_Alignof(T)`（模型泛型重放路径上尽量避免）。
- 容器方法内直接 `slt_arena_alloc(...)`（不经 static inline 包装，规避模型重放时
  static inline 辅助 unresolved 的坑）。

## 4. 值类型与生命周期模型

- M0 元素限定：**值类型（POD/值语义 struct）**，无自构建/析构。容器语义 = 纯值拷贝
  （struct 位拷贝）。深拷贝元素（含指针资源）列 M1。
- 容器全部就地构造于 arena（`Vector(T) v;` → 内部 data 从 allocator 取，结构体本身仍可
  局部/全局）。生命周期 = arena 生命周期（`slt_arena_reset/destroy` 一回收）。
- 越界/生命周期检测（双轨，因发行需 clang 可编译）：
  - TCC 原生侧：`-b`（bcheck 数组越界兜底）+ slate 断言。
  - clang 发布侧：`-fsanitize=address`（ASan）兜底（脱糖产物可被 ASan 覆盖）。

## 4.5 内存检测与防护（一等公民）

STL 把内存检测当作**核心能力**（非事后 `-b`）。因发行走脱糖 clang（决策8，产物无
bcheck/memtrack/tcc-esc 私有符号），检测分两档：

**① 内在检测层 `SLT_CHECKS`（纯 C + musl 头，默认开）**
- `SLT_ASSERT` → `<assert.h>`（NDEBUG/SLT_CHECKS 关闭即零开销）；TCC 原生与 clang 脱糖
  产物都生效。
- allocator 自检：`SLT_Arena` 带 `epoch`/`outstanding`——`reset/destroy` 时仍有余活指针
  → 醒目警告（陈旧指针/逃逸提示），前后端一致、self-contained（已在 allocator.h 实现）。
- 容器不变量：`cap>=len`、空容器操作合法、迭代器/指针有效性。
- `at()/[]` 越界：`SLT_CHECKS` 下断言报错（文件:行）。
- 分配失败：返回 0 + 可设 OOM 钩子（默认经 `<stdio.h>` 报错到 stderr）。

**② 外接加固（可选，仅 TCC 原生；脱糖/发布时无）**
- `-b`（bcheck）：容器数据区经 `__bound_new_region` 登记，越界/错配 free 兜底。
- `memtrack`：长串/堆分配在 `__mem_report` 可追踪（区分 arena/heap 来源）。
- `tcc-esc`：长串/外逃指针跨 `reset` 撤销检测。
这些依赖 tcc 私有符号，仅 TCC 原生路径生效；发行侧检测由 ① 承担（发布版 NDEBUG 裁剪，
无额外开销）。

容器 API：带检测方法统一由 `SLT_CHECKS`/`NDEBUG` 编译期裁剪（断言内联在 `model (T)`
泛型函数体，随重放实例化）。

## 5. trait：类型约束（operator 驱动）

不做 `type_traits` 全套，只约定契约，由 `operator` 重载自然满足：

- **可排序**：T 提供 `operator< (T,T)`（或传比较器）
- **可判等**：T 提供 `operator== (T,T)`
- **可打印/可哈希**：M1（`operator<<` / 哈希方法）

泛型算法.compile 期检查：重放后找不到对应 `operator<` → 编译报错（模型泛型天然给出）。
默认谓词与显式回调并存：

```c
/* 泛型算法: 默认用 T 的 operator< ; 亦可传 comp */
model function (T) void slt_sort(T *a, int n) { ... }                    // 用 a[i]<a[j]
model function (T) void slt_sort2(T *a, int n, int(*comp)(T,T)) { ... } // 显式回调
```
调用：`slt_sort(int)(arr, 3)` / `sort_v(Vector(int))(&v, my_less)`。

## 6. 迭代器模型

- **连续容器（vector/string）**：裸指针即迭代器。
  `T *begin = v.data; T *end = v.data + v.len;` 零开销，`*v #[i]` 直接索引。
- **链式容器（list）及关联容器（M1）**：抽象迭代器——对象内嵌 vptr，方法表含
  `incr / deref / equal`。`model struct ListIter(T) { ... }`。为贴近 C++ 把裸指针也包一层
  "一致性迭代器"会让连续容器退化，故**双轨**：连续用指针，链式用抽象迭代器；算法分别适配。

算法接口尽量写成"接受 `(begin, end, ...)` 的哑参"，连续容器直接传指针、链式容器传迭代器包装，
避免统一抽象引入性能税。

## 7. 容器（M0）

### 7.1 pair
```c
model struct Pair(A,B) { A first; B second; };
```
- 构造/解构（结构化访问）、`operator==`（成员逐项）。

### 7.2 vector<T>
- 连续动态数组，`data/len/cap`；`size/empty/cap/at/`[]（越界 -b 兜底）`/push_back/pop_back/
  insert/erase/clear/reserve/resize`。
- 增长策略：`cap` 不足时 `cap = max(cap*2, 1)`；扩容时不搬数据（arena 内新块+拷贝，
  或预留连续区再 bump 提升为单块）。
- 迭代器 = `T*`。方法为 `model (T)` 泛型函数**显式调用**：`Vector_push_back(int)(&v,x)`
  （无 `.method()` 对象方法糖，§13-决策6）。
- 拷贝/赋值：位拷贝（POD）；M1 支持自定义元素 copy 的深拷贝。

### 7.3 list<T>
- 双向链表，node = `{prev,next,data}`，节点从 allocator 分配。
- 无随机访问；迭代器 = 抽象（内嵌节点指针）；`push_back/front/insert/erase/size/clear`。
- `splice`/归并 M1。

### 7.4 String（设计定稿 · M1 优先实现）

基于仓库基础设施（`model`/`operator`/`-b`/`defer`/`--emit-c`）的完整设计，从"待讨论"
升级为正式方案。属 M0 范围之外、**M1 优先**（可作 M0e 先行）。

#### A. 布局与 SSO（核心）
```c
model struct String {
    union { char     sso[23];   /* 内联缓冲, 存短串 + 尾部 NUL+1 位长标志 */
            struct { char *ptr; /* 堆/arena 长串指针 */
                     int  len;  /* 字节数(不含 NUL) */
                     int  cap; } long_;
    };
    unsigned char mode;          /* 0=短串(SSO), 1=长串(指针) */
    int  clen;                   /* 缓存字符数(UTF-8), 已知则用, -1 需 O(N) */
};
```
- SSO 内联 23 字节（对象 ~40B）；长串 `{ptr,len,cap}` + mode。短串阈值 15/23 二选，
  对象总大小控制在 32/40 字节内（M0 验证后定）。
- 显式区分 **字节数 `size`（O(1)）** 与 **字符数 `string_length`（O(N) UTF-8）**。

#### B. 双内存后端
- 默认 **Arena**：`String_new(a)`，经 `slt_arena_alloc` bump，整池回收，无逐对象析构。
- **Heap** 版：`String_hn(h)`，长串接入 `tcc_own`+`defer` 逐对象释放（M1 引入 `tcc-own`）。

#### C. 运算符重载（具体类型, 非泛型）
- `String operator+ (String, String)` 拼接、按**值返回**（新串）。
- `String operator+=`（原地追加入既有对象）、`int operator_eq` / `operator_lt`（字典序）。
- 进 `--emit-c` 脱糖白名单（见 E）。

#### A4/… UTF-8 迭代器
- `StringIter`（对象内嵌 vptr 抽象迭代器或裸指针+偏移）按**码点**推进；
  `string_length(s)` O(N) 数码点；`size=` 字节(O(1))、`length=` 字符(O(N))、`c_str()`。

#### C6. 算法适配
- `slt_sort`/`slt_find` 实例化后自动走 `String` 的 `operator<`/`operator==`，
  支持 `Vector(String)` 排序与查找（谓词免费, 无需比较器）。

#### C7. 自由函数（`string_extra.h`, 复用 arena 免临时分配）
- `string_split` / `string_trim` / `string_join`。

#### D. 调试与运行时安全（`-b` 增强）
- `string_at(s,i)` 宏在 `-b` 下触发索引检查，越界配合 `-bt` 输出调用栈。
- 逃逸：`tcc-esc` 登记长串堆指针；arena `reset` 时仍持有未撤销引用 → 醒目警告（epoch）。

#### E. `--emit-c` 脱糖与跨编译器
- 运算符脱糖同步：`operator+ / == / <` 接入 `dg_op_tbl`，输出 `operator_add/eq/lt`
  标准函数调用，`gcc/clang -O3` 可内联。
- 跨编译器兼容头：`string.h` 加 `#ifndef __TCC__` 分支，为 clang/gcc 映射 `String` 为普通
  结构体 + 函数声明（脱糖产物直接编译，不依赖 model/operator）。

#### F. 性能基准与监控
- `string_append` 热路径插 `CPU_BEGIN/END`（`cpu-prof.h`），量化 SSO 命中率与长串扩容成本。
- 长串分配接入 `memtrack`，`__mem_report` 可追踪；区分 Arena/Heap 来源。

#### 依赖/前置（生态）：String 是**具体类型**，自身无 model 泛型坑；但 `Vector(String)`
需修复 B5（model 方法名解析回退），`Vector(String)` 排序需 C6 算法；脱糖需 E 的
`dg_op_tbl` 同步。优先级：E（跨编译器+operator 脱糖）> C（operator）> B5（编译器）> D。

### 7.5 stack / queue
- 适配器：默认底层 vector（stack）/list 或 deque（queue）。M0 用现有容器折叠，
  不新增存储。

（deque：M1，需块式索引；M0 的 queue 用 list 兜底。）

## 8. 算法（model function 泛型，命名仿 STL 加前缀避免冲突）

- 命名：`slt_sort/slt_find/slt_copy/slt_for_each/slt_minmax/slt_reverse/slt_fill/...`
- 排序：简单选择/插入排序（M0 正确性优先；快排/归并 M1 性能）。排序接口吃裸指针
  或迭代器区间；比较默认 `operator<`。
- 查找：`slt_find(begin,end,val)` 返回指针/迭代器（默认 `operator==`）。
- 复制/填充：`slt_copy/slt_fill/slt_reverse`。
- 遍历：`slt_for_each(begin,end,fn)`。
- 极值：`slt_minmax`。
- M1：`remove/sort(快排)/binary_search/unique/accumulate` 等。

泛型算法写一次，容器可复用（传区间指针）；链式容器经迭代器接口复用同一算法骨架（M1）。

## 9. 目录结构

```
lib/stl/
  allocator.h   # 空间配置器 (arena/pool/ctmem 后端 + tcc-own 接入)
  trait.h       # 比较/判等契约 (operator 驱动说明 + comp 回调类型)
  iterator.h    # 抽象迭代器 (vptr/itab) + 连续容器裸指针约定
  pair.h        # model Pair
  vector.h      # model Vector
  list.h        # model List + ListIter
  string.h      # model String — 设计已定稿(§7.4), M1 优先(M0e); string_extra.h(自由函数)
  stack.h queue.h
  algorithm.h   # 泛型算法 (model function)
tests/
  t062_stl_vector.c
  t063_stl_list.c
  t064_stl_string.c
  t065_stl_algorithm.c
  ...
```
全头文件库（`SLT_STATIC` + `model` 模板），与 lib/tcc-*.h 同构，免编译期安装。

## 10. 接口约定

- 类型名大写（`Vector(T)`）；方法是 `model (T)` 泛型函数，命名 `<Type>_<ascii方法>`，
  一律**显式实例化调用** `Vector_push_back(int)(&v, x)`——**不提供 `.method()` 对象方法糖**
  （§13-决策6），避免 tccgen 泛型方法回退崩溃。
- 容器方法 all 左值接收 `&self`。
- 越界访问（`at`/`[]`）：`SLT_CHECKS` 下断言报错（文件:行）；关闭则为 UB（与 C 一致）；
  TCC 原生另可叠加 `-b`（bcheck）兜底（见 §4.5）。
- 空容器 `data==0` 合法；`size()` 等对空安全。

## 11. 分阶段实施

| 阶段 | 交付 | 验证 |
|---|---|---|
| M0a | allocator + trait + iterator + Pair + Vector | t062_stl_vector ✅ |
| M0b | List + 抽象迭代器 + Stack/Queue | t063_stl_list / t066 ✅ |
| M0c | 基础算法(sort/find/copy/for_each/reverse/minmax…) | t066 / t070 ✅ |
| M0d | 全量回归 + SLT_CHECKS 内在检测(at/front/back 边界断言) | t067 ✅ |
| M0e | String 基础 → 全功能(SSO/UTF-8/operator/concat) | t068 ✅ |
| M1 | Map(有序,仅 operator<,getor/at)/Set/Deque/深拷贝/快排/二分 | t069/t071/t073/t072/t070 ✅ |
| M1-待办 | unordered(哈希)、抽象迭代器算法骨架、heap 后端、--emit-c 脱糖 clang 闭环 | — |

已落地的命名/调用规范（替代 §13 决策4/6，见 §13-10/11）：统一 `STL_`/`stl_` 前缀避免
与 libc 冲突；容器方法一律用**对象方法糖** `m->stl_map_set(int,int)(k,v)`（编译器已支持
model 泛型方法糖 + 大 struct sret 返回）。

## 12. 测试计划

- 纯断言（无 stdio）回归用例，对齐 tests/t0xx 惯例：每个容器一组冒烟 + 边界。
- **TCC 原生侧**：`-b` 越界（`at`/`[]`）兜底验证；对每个容器跑边界用例。
- **clang 发布侧（脱糖验证）**：`tcc --emit-c t0xx.c > t0xx.desug.c` ＋ `clang -O2
  -fsanitize=address -g t0xx.desug.c` → 运行：既验证 `--emit-c` 能忠实脱糖 model/operator，
  又用 ASan 覆盖越界/泄漏。这是本 STL 的**发行检验**。（`--emit-c` 对 model/operator/
  SSO-string 的完整改写仍为 P1，未闭环。）
- 泛型数与 C++ `Template 督导`：list<int>/list<double>、vector<Pair(int,float)> 等
  model 缓存类型一致性（同参同 sizeof）。

## 13. 已确认决策（2026-08-24 逐项拍板）

1. **内存模型**：**池式 arena 整池回收**（self-contained `SLT_Arena`，仅 musl malloc），
   无逐对象析构；M0 元素限 POD / 值语义。heap 后端列为 M1。
2. **谓词默认机制**：泛型算法实例化时**自动用元素 `operator<` / `operator==` 分发**，
   免显式比较器；同时保留"第二参比较回调"重载作为覆盖手段。
3. **迭代器**：**双轨**——连续容器（vector/string）裸指针零开销；链式/关联容器
   （list/…）用 vptr 抽象迭代器。不引入统一抽象迭代器。
4. **命名**：算法 `slt_` 前缀（`slt_sort`/`slt_find`/…），容器大写类型名
   （`Vector(T)`/`List(T)`），方法 `<Type>_<lower>` 泛型函数、显式实例化调用
   （`Vector_push_back(int)(&v,x)`；无对象方法糖），规避 libc 冲突。
5. **String 设计定稿**（2026-08-24 追加）：M0 不含 string，但 §7.4 已据本项目基础设施
   （model/operator/-b/defer/--emit-c）完整设计（SSO、双内存后端、operator、UTF-8 迭代器、
   自由函数、越界/逃逸、脱糖跨编译器、cpu-prof/memtrack）；**实现列 M1 优先**。
6. **对象方法糖对泛型实例不可用**（试点发现）：`v.push_back(x)` 会触发 tccgen 方法回退
   分支对 `model` 泛型实例类型名的解析而编译崩溃（0xC0000005）。M0 容器方法一律用
   `model (T)` 泛型函数**显式实例化调用** `Vector_push_back(int)(&v, x)`；对象方法糖的
   泛型支持列为编译器增强项（tccgen 方法回退需识别泛型实例合成名）。
7. **model 泛型 + `-b` 编译 bug（已修复并验证 2026-08-24）**：`-b` 成型时向 tccgen 注入
   `__bt_init(__rt_info,1)`（[tccelf.c](file:///d:/work/tcc_posix/src/tccelf.c)）。因 tccpp 每 pass 重置
   `tok_ident`（tccpp.c:3749）而 model_list 按 token id 存名且跨 pass 存活，合成 pass 新标识符
   复用与已登记 model 相同 token id，`model_find` 误判 `__bt_init` 为泛型实例化，报
   `too many type arguments for model`。**修复已写入 tccgen.c**：新增 `model_scope_reset()`，
   在 `tccgen_init()`（每 pass 起始）清空 model_list。**已用 `-DCONFIG_TCC_MUSL` 自举重建验证**：
   `-b`+model 泛型不再报误判、正常到链接阶段（仅剩 musl 缺 libtcc1/msvcrt/kernel32 导入库，
   属链接配置，与误判无关，需 build.sh 全量）。
8. **发行策略（2026-08-24）：扩展写 + musl 头 + 脱糖 clang**。STL 头用 `model`/`operator`
   扩展编写（脚本引擎内 TCC 原生跑、贴近 C++），但**只依赖 musl 自带标准头**
   （`<stddef.h>/<stdlib.h>/<string.h>/<stdint.h>` 等），**绝不 include 本项目私有头**
   （如 `lib/tcc-arena.h`——脱糖产物交 clang 时这些文件不在其 sysroot 里）。
   发布走 `--emit-c` 脱糖成标准 C → clang 编译。因此 M0a allocator 改为 **self-contained
   内联 bump arena（基于 musl malloc）**，不引用 tcc-arena.h。验证：TCC 原生跑 t0xx 回归
   + `--emit-c` 脱糖后 clang 编译运行。

9. **model 泛型函数体内调辅助函数 → 须 `static`（非 inline）**（试点发现）：tcc 对
   `static inline` 不生成可寻址符号，model 泛型函数体重放调用它时 `unresolved`；而
   `static`（非 inline）会生成符号可解析（与 tcc-arena.h 的 plain `static` 一致）。
   故 STL 头用 `SLT_STATIC`（`static __attribute__((unused))`）定义分配等辅助，
   clang/gcc 侧亦可用。

10. **对象方法糖可用于 model 泛型方法**（2026-08-24 升级决策6）：编译器已实现泛型方法糖
    `m->stl_map_set(int,int)(k,v)` —— 方法糖识别 model 泛型模板名、经 model_function_call
    实例化（消费类型实参表 `(int,int)`）、自动注入 receiver 为首参；并支持**大 struct
    (sret) 返回**（`v->stl_vector_copy(int)()` 返回 24B 容器）。容器方法一律用该糖风格，
    替代早期的显式 `stl_map_set(int,int)(&m,k,v)` 与"放弃方法糖"的决策6注。

11. **operator 一律用 C++ 直觉写法**（`operator<` / `operator==`，弃 `operator_lt/eq`）：
    编译器把符号拼成内部 `operator_lt/eq/...`，两写法等价；`operator<`（无括号包裹）可直接
    声明。STL 元素比较契约 = `operator<`（可排序）即可（map/set/sort 用严格弱序，等价由
    `!(a<b)&&!(b<a)` 推导，不要求 `operator==`）。

12. **model 泛型自包含约束（实测）**：泛型方法体**不得调用另一同泛型方法**（跨泛型互调
    → `invalid type`/`unresolved`，如曾以 find 辅助被 get/set 调）；须在各方法体内联
    逻辑（map/set 的二分、deque/list 的扩容/拷贝都内联）。同泛型自递归（stl_qsort）可用。
    另：model 类型实参不支持**指针类型**（`char*`）与**嵌套泛型实例作值类型实参**（如
    `STL_Map(Key, STL_Vector(int))`）；值类型用 int/struct/ext等。

后续决策（string 语义拍板）落定后追加到本清单。

→ 现状（2026-08-24）：M0a–M0d、M0e(String 全功能)、M1(有序 Map/Set/Deque/深拷贝/快排/
   二分/二分查找) 已落地并全绿(73 测试)。待办：unordered(哈希)、抽象迭代器算法骨架、
   String heap 后端、`--emit-c` 脱糖 clang 闭环（发行检验）。
```