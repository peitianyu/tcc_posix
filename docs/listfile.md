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
| `%dep [name=]owner/repo[#ref]` | 包管理: 拉取缓存; 无名→注入根include/lib, 有名→登记前缀别名 |

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

`%dep` 只做一件事:**把仓库内容变成一个可被 listfile 引用的路径**, 不做任何自动分派/复制/判断。
两种形态:
```
%dep <owner>/<repo>[#<ref>]            # 无名: 自动注入根 include/lib(常见简写)
%dep <name>=<owner>/<repo>[#<ref>]     # 有名: 登记前缀别名
```
- `<name>`: 你起的名字。之后 listfile 里任何以 `name/` 开头的参数, **前缀替换为缓存目录**。
- `<owner>/<repo>`: GitHub 仓库, 必填。
- `<ref>`: 版本 —— 分支 / tag / 40 位 commit sha; 空 = 默认分支 HEAD。

### 4.1 拉取与缓存
1. 缓存根 `.tcc_cache/`(或 `TCC_CACHE` 覆盖), 目录 key = `<owner__repo>[@ref]`(ref 空 → 无后缀)。
2. **不同 ref 各自一份缓存, 互不覆盖**; 用 `.dep.ok` 作"已就绪"标记, 防半截拉取误判命中。
3. 命中 → 复用; 未命中 → 拉取(§4.3)。

### 4.2 前缀引用 —— 多文件 / 层级 / 单文件 一网打尽
`name/` 前缀替换只是"路径拼接", 所以完全复用普通 listfile 语法, 无需任何专用符号:
```
%dep ff=user/ff#v1
ff/src/ff.c          # -> .tcc_cache/user__ff@v1/src/ff.c   编译源(多 .c 任意编)
-I ff/include        # -> .tcc_cache/user__ff@v1/include     头(多 .h 一起可见)
-L ff/lib            # -> .tcc_cache/user__ff@v1/lib         库
%dep s=stb/stb#master
s/stb_image.h        # 单文件: 作为源或头, 由你决定放哪
```
- **任意扩展** `.c/.h/.s/.a/.o`: 用普通参数语义表达, 编译器按类型处理。
- **任意层级** `src/ff.c`、`include/detail/x.h`: 前缀后直接跟相对路径。
- 无名(§4 第 4 行仅注): 加 `name=` 即可端起名字; 不想要自动注入就把首行写成有名。

### 4.3 拉取命令(零新增依赖)
| ref | 命令 |
|---|---|
| 分支/tag | `git clone --depth 1 --branch <ref>`; 无 git 回退 `curl -Ls codeload.../tar.gz/<ref> \| tar` |
| commit sha | 一律 `curl -Ls codeload.../tar.gz/<sha> \| tar`(sha/tag/分支通用, 免 git) |
| (空=HEAD) | `git clone --depth 1`; 回退 `tar.gz/HEAD` |
- **失败语义**: 已缓存/脱网仍能编(用现有 `.dep.ok`); 首次且无网 → 报错并提示, 不静默。
- **沙箱**: `name`/`owner`/`repo`/`ref` 全体过白名单 `[A-Za-z0-9_.-]`, 杜绝 shell 注入 / 路径穿越。

### 4.4 完整示例
```
%dep user/mathlib                          # 自动 -I/-L 根 include/lib(默认)
%dep boot=tinycc/tinycc#v0.9.28            # 起名 boot, 之后 boot/xxx 引用
boot/tcc.c  -I boot/win32                 # 用它仓库里的源件 + 子目录 include
%dep stb=stb/stb#master                    # 起名 stb
stb/stb_image.h                            # 单个头: 作源或 -I 由你定
```

> 对比旧 `:sub/{…}/staging` 设计: 该模型已删除 —— 前缀引用覆盖所有子路径/多文件/层级场景,
> 实现只含"缓存拉取 + 别名登记 + token 前缀替换"三部分, 规则和代码都显著更简。

### 4.5 顺序约定
- **先声明后使用**: `%dep name=…` 必须出现在任何 `name/…` 引用之前 —— listfile 是逐行顺序处理,
  别名只在登记后才能被前缀替换命中。用前请把 `%dep` 放在文件顶部(如同 `#include`)。
- **同名覆盖**: 重复 `%dep name=…` 后者覆盖前者; 已 push 出去的 `name/…` 引用不受影响。
- **缓存幂等**: `%dep` 对同一 `name`/`ref` 重复声明无副作用(缓存命中即复用)。

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
| P2 | `%dep` 包管理(缓存/克隆/注入 -I) | ✅ 实现: git clone(`#ref` 分支) 或无 git 回退 curl tarball 到 `.tcc_cache/<owner__repo>[@ref]`(`TCC_CACHE` 覆盖), 命中缓存复用, 注入 `-I include`/`-L lib`; 标识符白名单防注入; 仅 musl 版启用([1/3] BOOT 无 POSIX system/access), list_dep 若拉取失败 fprintf 提示不注入 |

---

## 9. 决策(已确认, 2026-08-23)

1. **`%if` 变量来源**: `@os/@arch/@tcc` 内建 + 命令行 `-D<key>`; **不含任意进程环境变量**(更可控)。
2. **`%dep` 存储**: git clone(--depth 1, `[--branch ref]`) 到 `.tcc_cache/<owner__repo>[@ref]`;
   无 git 回退 curl tarball; 支持 `TCC_CACHE` 覆盖目录; 注入 `<cache>/include`→`-I`、`<cache>/lib`→`-L`。
3. **编译选择粒度**: 只要 `%if/%else/%end` 分支(不引入 `%choose` 方案组)。
4. **`%dep` 语法(2026-08-23 重设计, 简洁版)**: `%dep [name=]owner/repo[#ref]`:
   - `%ref`: 分支 / tag / sha(sha 走 codeload tarball 免 git), 缓存按 ref 隔离。
   - 无名 → 自动注入根 `include`→`-I` 与 `lib`→`-L`(存在或回退目录本身)。
   - 有名 `name=` → 登记前缀别名; 之后 token 以 `name/` 开头**前缀替换为缓存目录**,
     从而用普通参数语法覆盖多 .c/.h、任意层级、单文件, 无需 `:sub/{…}/staging`。
   - 缓存用 `.dep.ok` 作"已就绪"标记防半截拉取误判; `%dep` 受 `%if` 条件控制;
     白名单 `[A-Za-z0-9_.-]`(name/owner/repo/ref)防注入。