# 内存管理治理方案

> 状态: 2026-08-22 草拟
> 范围: 内存 5 层分配器体系 (lib/tcc-{ctmem,arena,pool,mmap}.h) + memtrack/bcheck
>       的「编译期判定界限、禁止写法、自动化限制」综合方案。
> 依据: 一次关于「内存问题能否编译期判定 + 能否直接禁止」的讨论结论。

---

## 1. 背景与目标

已实现一套 5 层内存管理体系，目标是让程序「实时知道内存花在哪、花得对不对」。
但「能不能在编译期就把内存问题抓出来、乃至直接禁止错误写法」存在本质边界。

本方案的目的是**把讨论结论固化为可执行的设计**：

1. 明确**编译期能判 / 不能判**的界限（避免对 TCC 提出架构上做不到的要求）；
2. 明确 **`-b -bt` 能抓 / 抓不到** 的清单（据此设计运行时兜底）；
3. 制定**禁止写做法**（编程规约）与**自动化限制层**（编不过 / 运行时必抓）。

---

## 2. 内存 5 层体系现状

| 层 | 头文件 | 机制 | 字节经何途径 | 是否计 memtrack |
|---|---|---|---|---|
| L1 编译时 | `lib/tcc-ctmem.h` | 零堆静态 bump (外部缓冲/内嵌静态池) | 无分配 | 否 (预期) |
| L2 运行时基础 | `lib/tcc-arena.h` | bump/区域分配 | malloc | 是 |
| L3 智能分配 | `lib/tcc-pool.h` | 固定槽对象池 (空闲链复用) | malloc | 是 |
| L4 持久化 | `lib/tcc-mmap.h` | 文件映射 + msync | OS 页 | 否 (预期) |
| L5 诊断 | bcheck.o memtrack | live/peak/total/mem_report | — | — |

现状约束（已记入项目记忆）：
- musl-nt64 **无 `ftruncate`(ENOSYS)** → L4 用 `lseek(total-1)+write(1)` 扩文件。
- L4 持久化读回须读 `tcc_mmap_data(基址)`，勿再 `tcc_mmap_alloc`。

---

## 3. 编译期判定界限

根因：内存问题的本质是**运行时动态行为**（生命周期、别名、实际大小/长度），
编译期只有**静态文本**。可判定性由「运行时值」与「别名信息」左右。

### 3.1 编译期能判

| 项 | 说明 | 判定依据 |
|---|---|---|
| 编译器静态分配、下标为编译期常量的数组越界 | `int g[10]; g[11]=1` | 常量折叠，单遍即可 |
| **分配器错配** (`free`/`_destroy` 拿到别的分配器指针) | arena 指针传 `free` | 纯调用点 + 类型/归属，无需别名分析 |
| `tcc_*_alloc` 返回值未判空直接用 | `p->f` 未先 `if(!p)` | 单遍可查（建议仅警告） |

### 3.2 编译期不能判

| # | 项 | 为何不能 | 归属工具 |
|---|---|---|---|
| 1 | 实际分配大小 | 大小来自变量/参数 `n*32` | 运行时 (`-b`) |
| 2 | 越界访问 | 下标含运行时值 `p[i*2]` | 运行时 (`-b`) |
| 3 | **跨函数别名悬垂** | 需 SSA + 过程间别名分析；TCC 单遍、无 IR、无别名图 | 运行时兜底 (见 §6) |
| 4 | 泄漏判定 | 需枚举全部控制流路径的 destroy 配对，指数级，属验证器领域 | 运行时 (memtrack) |
| 5 | 堆/别名的动态混用 | 运行时才有去向 | — |

> **结论**：能否判定取决于「变量运行时的值」；TCC 的单遍架构连数据流/别名中间表示都没有，
> 凡靠运行时值、跨函数别名、全路径配对才能定的问题，编译期一律无解。
> 这正是 bcheck (越界) 与 memtrack (归因/泄漏) 存在的根本理由。

---

## 4. `-b -bt` 捕获清单

| 问题 | `-b`+`-bt` 能否抓 | 说明 |
|---|---|---|
| 堆越界 / 用后释放 (malloc/arena/pool) | ✅ | 运行时知真实长度；移除区域后再访问报 `invalid access`，`-bt` 给调用链 |
| 非法 `free` / 堆指针错配 | 🟡 部分 | bcheck 校验指针须指向已登记块常能拦 |
| 泄漏 | ✅ (memtrack) | `__mem_report(1)` 报 `LEAK: N bytes / M blocks` 并按调用点归因 |
| **`reset` 后复用区悬垂** | ❌ | `reset` 未真释放，区域仍「有效」→ bcheck 不报 |
| **mmap 区越界** | 🟡 受限 | mmap 不经 `__bound_malloc`、无区域表 → 多数不捕获 |
| 别名动态混用 | 🟡 | 只抓「实际发生」的越界/悬垂，不抓「可能但未犯」 |

> bcheck 抓的是「真实踩到」，不静态标隐患；`-bt` 是定位器（`func@file:line`）。

---

## 5. 禁止写做法（编程规约）

这些写法进了坑，工具要么抓不到、要么只是「事后」，应在编写时避免：

1. **`reset`/`destroy` 之后仍持有并使用旧指针**
   → 在 reset/destroy 后立即置空该分配器旧指针；只允许取一次性数据。
   （工具漏：内存未释放，bcheck 当有效。）
2. **不通过 `free()`、而只通过分配器自己的 `*_destroy` 销毁**区块。
3. **`arena_alloc` 结果长期逃逸到全局/长期存储再转手多生命周期使用**。
   → 只在本地作用域、规定生命周期内使用（跨生命周期访问交给 §6 运行时核验）。
4. **在 `-b` 保护范围外裸用 mmap 做精细越界防护**
   → 用 mmap 就把映射登记进 bcheck 区域表（§6.2）。
5. **对同一个 arena/pool 不做串行/线程安全约定时并发分配**。

---

## 6. 自动化限制层（对策）

> 原则：**能自动禁的做成「编不过/运行时必抓」；抓不到的死角用「禁止写法 + 规约」约束。**

| 死角/问题 | 对策 | 层 | 成本 |
|---|---|---|---|
| 分配器错配、双重释放、释放非本系统指针 | **统一销毁入口 + 归属表** `tcc_release(void*)` | 接口/运行时 | 中 |
| `reset` 后复用区悬垂 | **epoch(纪元) + 存取核验口** | 运行时 | 中 |
| mmap 区越界 | **mmap 登记进 bcheck 区域表** (`__bound_new_region`) | 运行时/接口 | 低 |
| 未判空解引用 | 前端**警告**(非硬错) | 编译期 | 低 |
| 泄漏 | memtrack 已报 + 可选参考计数 `refcnt` 精确化 | 运行时 | 低 |

### 6.1 统一销毁入口 + 归属表（推荐；彻底解决 `void*` 通用化受限）

`void*` 灵活性不必牺牲——只需在**分配/销毁口登记「指针→分配器」归属**，销毁统一走一个口：

```c
enum tcc_owner { OWNER_HEAP, OWNER_ARENA, OWNER_POOL, OWNER_CTMEM, OWNER_MMAP };

void *m = tcc_alloc_owned(arena_new(0), 64);       /* 登记指针→OWNER_ARENA */

void tcc_release(void *p) {                        /* 取代裸 free */
    switch (tcc_owner_of(p)) {                     /* O(log n)/O(1) 查归属 */
        case OWNER_ARENA: tcc_arena_release(p); break;
        case OWNER_POOL:  tcc_pool_release(p); break;
        case OWNER_CTMEM: /* 静态区, 拒绝释放 */   break;
        case OWNER_MMAP:  tcc_mmap_release(p); break;
        case OWNER_HEAP:  free(p); break;
        default: __mem_abuse(p);                   /* 非本系统指针 */ break;
    }
}
```

- **调用方照旧写 `void*`**：通用容器、`void*` 字段不改，灵活度不变。
- 错配/双重释放/非本系统指针**当场识别并报**，可附分配点（配合 memtrack 归因）。
- 归属表可复用以优化实现：头元数据 cookie (8B) O(1) 定位，或用 bcheck 已有 splay 复用。
- 代价：必须走 `tcc_release` 而非裸 `free`；arena/pool 内部结构需加记录。

> **不建议**强类型隔离（每种分配器一个不可互转 struct）：它会与 `void*` 通用化诉求直接冲突，
> 虽零运行时成本但牺牲源码灵活性，与本方案目标相悖。

### 6.2 epoch 存取核验（补 `reset` 悬垂的洞）

给每个 arena/pool 加单调递增 `epoch`；`reset`/`destroy` 时自增。统一存取口核对：

```c
void *tcc_arena_addr(tcc_arena *a, size_t epoch_holder) { /* epoch 不符→报 */
    if (epoch_holder != a->epoch) __mem_stale(a, epoch_holder);  /* 附分配点 */
    return ...;
}
```

效果：`reset` 前取的指针在 `reset` 后再读 → 当场报错。把「reset 悬垂」从漏网变必抓。

### 6.3 mmap 登记进 bcheck 区域表

tcc_mmap.h 的 open 成功后将 `[base, base+len)` 调 `__bound_new_region` 登记为合法区域，
之后越界写可像堆那样被 bcheck 拦截。**一行登记，java 侧零改动。**

### 6.4 显式逃逸声明 + 提醒打印（唯一真空的对策）

文档 §8 指出唯一绝对禁不掉的是「手贱把指针塞进全局再从别处读」。对策不是「禁止」，
而是**把隐式危险操作变成显式声明 + 醒目打印**，让程序员明确知道自己做了什么、后果是什么：

```c
tcc_escalate(p, &g_slot);      /* 显式: 我故意让这指针活到区外、塞进全局 g_slot */
...
tcc_arena_reset(a);
```

reset/destroy 时若发现「仍有未撤销的逃逸引用」，打印提醒：

```
[memgov] ESCALATED pointer 0x... (arena slot @arena.c:27, epoch 3->4)
        still referenced by global 'g_slot' after arena reset
        ------------------------------------------------------
        WARNING: 'g_slot' now dangles until re-escalated.
        You chose this cross-scope lifetime explicitly.
```

- `tcc_escalate` 是**显式的**：做了才登记、才打印；没做不登记 → 不误报，也不强制所有跨函数传递。
- 打印附：**分配点 + 重置点 + 当前持有者符号**，一眼看清该故意动作的后果。
- 属 §6.2 epoch 的延伸：逃逸表记录「持有者 + 分配的 epoch」，reset 时持有者 epoch 过期 → 触发提醒。
- 常配套**配对撤销口** `tcc_descend(p)` 在持有者不再用时撤销登记，正常生命周期内无提醒。

---

## 7. 落地路线图

建议按「投入小 → 收益稳」的顺序：

| 优先级 | 项 | 产出 | 决定点 |
|---|---|---|---|
| P0 | 统一销毁 `tcc_release` + 归属表 (碾压错配/双释) | ✅ 已落地 `lib/tcc-own.h`, t_own.c | 已采纳「必须走 tcc_release 而非裸 free」 |
| P1 | mmap 登记进 bcheck 区域表 | ✅ 已落地 tcc-mmap.h 弱引用 `__bound_new_region`, 越界可查 | — |
| P1 | 未判空前端**警告** | ⏸ **搁置** (tcc 无 warn_unused_result, 侵入单遍编译器有回归 48 套件风险) | 是否接受高风险编译器改动 |
| P2 | epoch 存取核验 (reset 悬垂) | ✅ 已落地 tcc-arena.h epoch/outstanding, t_epoch.c | — |
| P2 | 显式逃逸 `escalate/descend` + 提醒打印 (§6.4) | ✅ 已落地 `lib/tcc-esc.h`, t_esc.c | — |
| P2 | memtrack `refcnt` 精确化 (泄漏两端定位) | ✅ 已落地 bcheck.c mem_lives 活体对象表 (ptr/size/caller), `__mem_report(1)` 逐条列出泄漏指针+尺寸+分配点; t_refcnt.c | 固定容量 8192, 溢出降级但全局计数不失真 |
| 不做 | 强类型隔离 / 全路径 destroy 配对 | — | 与 void* 通用化冲突 / 误报率高 |
| 不做 | 编译期跨函数别名分析 | — | TCC 单遍架构硬伤 |

---

## 8. 关键结论

1. **编译期能判**：常量下标数组越界、分配器错配、未判空 —— 有限且集中在「纯调用点/类型可见」。
2. **编译期不能判**：运行时时长、跨函数别名、全路径配对 —— 架构性无解；需运行时工具。
3. **`-b -bt` 能抓**：堆越界、用后释放、非法 free、泄漏；**抓不到** `reset` 悬垂、mmap 越界（分别由 §6.2 epoch / §6.3 登记区补）。
4. **`void*` 通用化可保留**：用「统一销毁口 + 归属表」，不必牺牲灵活性。
5. 「手贱把指针塞进全局再从别处读」——C 静态分析无法给保证，但**存在显式手段**：用 `tcc_escalate` 显式声明逃逸，`reset`/`destroy` 时若仍有未撤销引用则**打印醒目提醒**（§6.4），让程序员明确知道自己做了什么、后果是什么。不再仅靠规约。

---

## 9. 实测复盘：aio 线程池崩溃为何 `-b -bt` 没抓到

> 场景：`tests/t045_aio.c` 的 aio 线程池在并发越多 detached 线程退出时随机段错误 (rc=139)，
> 即便以 `-b -bt` 编译运行，bcheck 也一声不吭。

### 9.1 崩溃根因（运行时层，非应用层）

- aio 池以 **detached** 线程并发跑，多个 worker 同时走线程退出路径 `__unmapself → __psx_exit`。
- 上游 `__unmapself` 用一个**静态共享栈** `static shared_stack[256]` + 全局锁做栈切换再 `munmap`+`exit`：
  - `shared_stack[256]` 只 256B，但 `__psx_exit` 要 marshall 一条 ~200B 的 port 消息**再加调用帧**，直接撑爆共享栈 → 写穿相邻 `.bss`；
  - 锁 / `set_tid_address` 握手在**并发退出的 detached 线程间**竞争 → 踩踏。
- 修复（本次 aio 收敛）：去掉共享栈与锁，直接在当前真实 OS 栈上跑 `SYS_exit`，跳过 `munmap`（psxscl 退出路径仍读该区 TLS），并把 tid reaper 重定向到稳定 `.data` 变量。

### 9.2 为什么 bcheck 抓不到（它根本不是登记区内的「C 访问」）

| 层面 | bcheck 的前提 | 本崩溃的实际情况 |
|---|---|---|
| 检测模型 | 拦截 **C 指针/下标访问**，比对 splay 区域表 | 崩溃系 **RSP 栈指针改向 + 栈溢出写穿静态数组**，是 asm 级操作，无 C 端 checked-access 可拦 |
| 区域表 | 只登记 `malloc` 返回、`__bound_new_region` 显式登记区 | `shared_stack` 是静态 `.bss`，线程真实执行栈由 CreateThread 分配，两者都**不在区域表** |
| 插桩范围 | 只插桩**用 `-b` 编译的基因** | 崩坏发生在 **psxscl-2015 / musl 线程退出** 这些**未用 -b 编译**的运行库内部 |
| `-bt` | 定位器（把 PC→func@file:line），非探测器 | 只能把已发生的错译为调用链，前提是发生点是被 bcheck 拦下的检查点 |

### 9.3 结论（补 §4 捕获清单的一个空档）

- **`-b -bt` 的防护域 = 「用 -b 编译 且 落在区域表内」的真实越界/悬垂/泄漏**。
- 本崩溃属于**运行时自身（libc/psxscl 内部、RSP 级）**的栈溢出 + 竞态，既不在区域表、也没被插桩，bcheck **结构上无从下手**——不是配置问题，是模型边界。
- 这类「发生在运行库深处、非 C 语义访问」的缺陷 **-b -bt 抓不到**，需靠**修复运行库本身**（本次就是直接修 `__unmapself`）而非 guard。
- 映射：若把该运行库亦纳入 `-b`（区域表 + 插桩 + asm 栈更不可见）仍不能覆盖 RSP 级别，故此类问题**无法用 guard 兜底，只能运行时正确实现**。