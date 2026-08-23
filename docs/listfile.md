# `@listfile.txt` 编译描述 —— 编译选择 + 包管理 设计方案

> 状态: 2026-08-23 设计
> 定位: TCC 已内置 `@file` 响应文件(TCC 参数按空白拆词展开, libtcc.c `insert_args` +
> tcc_parse_args 的 `@` 分支)。本方案把 `@listfile.txt` 升级为**声明式编译描述**:
> 支持 注释 / 嵌套 / 条件选择(编译选择) / `%dep` 依赖(包管理), 零额外运行时依赖。

---

## 1. 动机与现状

**现状**: `tcc @build.txt` — `insert_args` 把文件内容按空白拆成参数, 逐参交给 tcc_parse_args。
无: 注释、条件、依赖。短板: 跨平台/多配置要维护多个 `.bat`/`Makefile`, 无法一行声明依赖。

**目标**: 让用户写一个 `@build.txt`, tcc 单命令完成"选配置 → 编译 → (可选)取依赖":
```c
@build.txt
  src/main.c  -O2  -Iinclude          // 源 + 选项
  %dep user/mathlib                     // 拉依赖 → 自动 -I
  %if @os == win                        // 编译选择: 平台分支
    -D_WIN   win_impl.c
  %else
    -D_POSIX posix_impl.c
  %end
```

## 2. 文件语法

| 行 | 含义 |
|---|---|
| 普通 token(源 / `-I` / `-D` / `-o`…) | 作为命令参数, 原样交给编译器 |
| 含 `*` `?` `[..]` 的路径 token | **终端式 glob 展开**为匹配文件列表(`src/*.c`); 无匹配则原样保留(让 tcc 报 not found) |
| `#` 开头 / 空白行 | 注释, 跳过 |
| `@other.txt` | 嵌套展开(现有 `@` 机制, 复用) |
| `%if <cond>` `%else` `%end` | **编译选择**: 条件启用一段参数 |
| `%dep owner/repo[#ref]` | 包管理: 拉取依赖并注入 include |

- **token 拆分**: 与现有一致, 空白拆分; 支持引号组(`"a b"` 一个参数)。
- **相对路径**: 源文件/`-I`/嵌套 `@` 的相对路径, 基于**本 listfile 所在目录**(便于移动)。

## 3. 编译选择 (`%if`)

条件在**预处理阶段**求值, 决定 `%if..%end` 段是否展开进参数。条件预定义:

| 变量 | 来源 |
|---|---|
| `@os` | `win` / `linux` (编译 tcc 时的 target) |
| `@arch` | `x86_64` / … |
| `@tcc` | `tcc-版本`
| 环境变量 | 读进程环境 (如 `@DEBUG`) |
| `-D<key>` | tcc 命令行 `-D` 传入 (如 `-DPLATFORM=vulkan`) |

比较: `== !=` ; 与/或: `!` 仅在简单字面量上。支持 `%else`。给"一份脚本编多平台/多配置"。

> 设计取舍: 不做完整表达式/宏语言, 只做"朴素字面量 == 判断 + 环境/命令行注入", 保持 ~150 行内。

## 4. 包管理 (`%dep`)

`%dep owner/repo[#ref]` → 预处理阶段同步拉取并缓存:
1. 缓存目录 `.<outdir>/.tcc_cache/`(或 `TCC_CACHE` 覆盖), 子目录 `<owner__repo>[@ref]`。
2. 若缓存存在 → 复用; 否则 `git clone --depth 1 [--branch ref] https://github.com/owner/repo <cache>`;
   无 git 时回退 `curl -LO` tarball (`https://codeload.github.com/owner/repo/tar.gz/ref`)。
3. 拉取后向本 listfile **追加注入**:
   - `<cache>/include` → `-I`
   - `<cache>/lib` → `-L`
   - 其余参数不再自动加(库需用户 `-l` 或用 repo 的 `build.txt`)。
- **零新增依赖**: 只用系统 `git`/`curl`/`tar`, 走 `system()`; Windows 依赖 Git Bash 的 `git` 在 PATH。
- **失败语义**: 已缓存/脱网仍能编(用现有缓存); 首次且无网则报错并出现编译, 不静默。

## 5. 实现钩子 (复用现有, 改动局部)

- **新增 `src/tcc-args.c`**(或并入 libtcc.c): 一个**前置解析** `tcc_expand_listfile(TCCState*, char ***pargv, int *pargc)`。
  在 `tcc_parse_args` 的 `@` 分支调用或 main 之前处理 listfile 文件本身:
  1. 逐行读; 按 §2 语法拆 token;
  2. `%if/%else/%end` 经 §3 求值决定展开与否;
  3. `%dep` 经 §4 拉取依赖, 把 `-I/-L` 作为**该文件展开参数的一部分**压入;
  4. 产出展开后的 argv, 交给现有 tcc_parse_args 循环(现有 `@`/`-` 分支原样复用)。
- **相对路径解析**: 记录 listfile 目录, 对文件/`-I`/`@` 相对化(参考 `tcc 找 include` 现有逻辑)。
- 环境/`-D` 读取: 预处理阶段读 `getenv` 与扫描已有 `-D` 参数建立变量表。

## 6. 边界与安全

- 只在 `-run`/编译目标为"本地构建"时启用依赖拉取(不默认在裸 `-c` 后端触发)。
- `%dep` 目录名 sanitize(去 `/`、`..`、空白) 防路径穿越。
- `git clone` 通过 `system`, 命令字符串须拼好并避免注入仓库名含 shell 元字符(校验 `[A-Za-z0-9_.-]`)。

## 7. 测试计划

- `tests/t052_listfile.c` 不方便(依赖文件 IO/命令行), 改: 一组 `build/*.list` 样例 +
  一个 `t052_list.sh`(或现有 test.sh 增 `listfile` 模式)断言:
  - 简单参数展开 + 源文件编译运行;
  - `%if @os == win` 分支选中正确文件;
  - 嵌套 `@sub.list`;
  - (可选, 联网/离线均可) `%dep` 到一个本地 git 或已缓存依赖被复用。
- 离线用例优先(避免 CI 依赖网络)。

## 8. 分阶段

| 阶段 | 产出 | 验收 |
|---|---|---|
| P0 | `@listfile` 增强: 注释/引号/嵌套 + **glob 通配符**(`src/*.c`) | ✅ 实现: 简单 list + glob(`*.c`) + %if 编出 exe |
| P1 | `%if/%else/%end` 编译选择(@os/@arch/@tcc + `-D`) | 平台分支选对 |
| P2 | `%dep` 包管理(缓存/克隆/注入 -I) | 拉依赖并 include 通过 |

---

## 9. 决策(已确认, 2026-08-23)

1. **`%if` 变量来源**: `@os/@arch/@tcc` 内建 + 命令行 `-D<key>`; **不含任意进程环境变量**(更可控)。
2. **`%dep` 存储**: git clone(--depth 1, `[--branch ref]`) 到 `.tcc_cache/<owner__repo>[@ref]`;
   无 git 回退 curl tarball; 支持 `TCC_CACHE` 覆盖目录; 注入 `<cache>/include`→`-I`、`<cache>/lib`→`-L`。
3. **编译选择粒度**: 只要 `%if/%else/%end` 分支(不引入 `%choose` 方案组)。