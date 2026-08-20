# tcc_posix — 纯 POSIX 双平台编译链

一套**只使用 POSIX 语法**的完整编译链:同一份 C 源码,编译出 Windows 原生可执行文件
和 Linux 静态 ELF,两边行为一致。

```
源码 (POSIX C) ──→ tcc -platform=win/linux ──→ foo.exe   (Windows 原生, psxscl 后端)
                                             └── foo_linux (Linux 静态, musl 原生)
```

## 组成

| 组件 | 说明 | 来源 |
|---|---|---|
| `src/` | TCC 0.9.28rc 编译器源码 (ONE_SOURCE 自举, 含 -platform 选项) | 本地魔改 |
| `src/posix/musl-1.1.11/` | musl libc 源码 (Linux 目标) | musl-libc.org |
| `src/posix/musl-nt64/` | musl + mmglue 架构适配 (Windows 目标, nt64) | midipix 2015 |
| `src/posix/components/` | psxscl-2015 / ntapi / pemagine / dalist / psxtypes | midipix 2015 (Wayback 恢复) |
| `include/` | **固化** musl 头 (Linux) | 构建生成 |
| `lib/` | **固化** 库: libc-win.a (含全部运行时: musl+后端+libtcc1, 零 Windows API 依赖) / libc-linux.a | 构建生成 |
| `build/` | 构建产物: tcc-win.exe / tcc-linux.exe + 各目标对象 | 构建生成 |
| `examples/` | 示例程序 (hello.c 等) | — |

## 快速开始

编译器本体已内联 `-platform=win|linux` 选项 (**默认 win**), 直接使用:

```bash
# 编译 Windows exe (默认平台, 可省略 -platform=win)
build/tcc-win.exe -platform=win examples/hello.c

# 编译 Linux ELF (自动转发到 tcc-linux.exe)
build/tcc-win.exe -platform=linux examples/hello.c

# 两个编译器可互相转发, 无需关心从哪个入口调用
build/tcc-linux.exe -platform=win examples/hello.c
```

`-platform` 选项实现在 `src/tcc.c` (help) + `src/libtcc.c` (解析, 处理
`-platform=win` 带等号形式) + `src/tcctools.c` (`tcc_tool_platform`, 同目录
exec 转发), 两个单目标 tcc (tcc-win.exe / tcc-linux.exe) 通过它互相转发;
请求平台与自身一致时直接编译, 不一致时 exec 同目录的另一个 tcc。

## 安装 (Windows, 开箱即用)

```bash
./install.sh
# 产物: bin/tcc.exe + bin/lib/ (2 个文件) + bin/include/ (musl 头)
#
#   bin/lib/crt_crt1.o   入口对象 (唯一需显式的运行时对象, 定义 _start)
#   bin/lib/libc.a       3.2M, 1281 成员: musl + chkstk + init_array
#                        + mem 4件套 + psxscl/ntapi/pemagine/dalist 后端
#                        + libtcc1 (编译器辅助), 剔除 msvcrt 时代 CRT
#                        → 零 Windows API 依赖, 无需 kernel32.def

# 用法: 一条命令编译链接, 默认链 musl (无 msvcrt)
bin/tcc.exe hello.c -o hello.exe
```

`bin/` 是自足安装: tcc.exe 按自身所在目录找 `lib/` 与 `include/`。
默认链接已内联 (CONFIG_TCC_POSIX), 无需手动列库。

## 完整构建 (从源码重建编译链)

```bash
./build.sh
# [1/4] 自举 TCC:        build/tcc-win.exe (PE) + build/tcc-linux.exe (ELF)
# [2/4] Windows 后端:    psxscl/ntapi/pemagine/dalist → .a (含重名修复)
# [3/4] musl libc:       Windows (1281 成员, 含后端+libtcc1, 无 CRT) + Linux (1007 成员)
# [4/4] 固化:            include/ + lib/*.a
```

注: `build.sh` 第一步自举需要宿主 TCC (`/d/work/tinycc/win32/tcc.exe`),
之后的编译链完全自足, 不依赖任何外部工具链。

> 编译链**不包含原版 libc (msvcrt)**: `src/tccpe.c` 的默认库列表已改为
> musl libc (CONFIG_TCC_POSIX), 链接时不再链 msvcrt/kernel32 之外的
> Windows CRT。

## 已验证功能 (同一份 POSIX 源码, 双平台)

| 功能 | Windows | Linux |
|---|---|---|
| printf / stdio | ✅ | ✅ |
| malloc / free | ✅ | ✅ |
| mmap / munmap (匿名) | ✅ | ✅ |
| open / read / write / close | ✅ | ✅ |
| opendir / readdir / closedir | ✅ | ✅ |
| time / getcwd | ✅ | ✅ |
| memcpy/memmove/memset/memcmp | ✅ | ✅ |
| 大栈帧 (16KB, __chkstk) | ✅ | ✅ |
| pthread create/join (4 线程) | ⚠️ join 卡死 | ✅ |
| 静态链接 (无动态依赖) | ✅ (PE) | ✅ (ELF) |

## 已知限制

1. **Windows 端 pthread 多线程 join 卡死** — psxscl 2015 的已知问题
   (符号重叠/布局相关, 单线程程序无影响; Linux 端线程正常)
2. **Windows 端 `/tmp` 等绝对 POSIX 路径** — psxscl 的路径映射与
   Linux 不完全一致, 建议用相对路径或当前目录
3. **数学库 (complex/浮点主块) 未编译** — musl 构建时排除 complex/math 主块,
   仅含 `__signbitl/__fpclassifyl/frexpl` 辅助 (printf %Lf 需要)
4. **`-run` 模式仅支持纯计算** — tcc_run 内存执行不走 crt_crt1.o 的 `_start`
   (psx_init 完整初始化: daemon 线程/brk 等依赖正常 PE 启动环境), 故
   malloc/write/printf 等依赖后端的调用不可用; 纯函数 (strlen/memcpy) 与
   无 libc 依赖的程序正常。完整程序用 `tcc hello.c -o hello.exe && ./hello.exe`

## 链接细节 (TCC 特性 workaround)

- **TCC 归档提取闭包有效**: tccelf.c `tcc_load_alacarte` 的 do-while 循环
  会自动补提被新加载成员引用的符号 → 链接只需单遍列库
- **crt_crt1.o 必须显式且最先**: 它定义入口 `_start`, 而入口符号是链接器
  隐式引用 (不在任何 .o 的未定义符号表里), alacarte 索引查不到 → 必须显式
  加载 (libc.a 内的 crt1.o 已剔除, 无冲突风险)
- **chkstk / init_array / mem 4件套 / 后端 4 库 / libtcc1 全部并入 libc.a**
  (1281 成员): 闭包提取自动解析 musl 内部及 musl→后端的交叉引用
- **剔除 msvcrt 时代 CRT**: libtcc1 里的 crt1/crt1w/wincrt1/wincrt1w/tcov/
  dllcrt1/dllmain/winex 引用 kernel32 API, 而 musl/psxscl 零 Windows 依赖 →
  从 libc.a 剔除, 无需 kernel32.def (libc.a 完全自足)
- **后端合并依赖重名修复**: psxscl/ntapi 构建时 C 版与汇编版同名对象
  (`__psx_init_tlca.o` / `tt_fork_v1.o`) 会互相覆盖, 丢失
  `__psx_tlca_prolog` / `__tt_fork_v1` 等关键符号 → 构建脚本已把汇编版改名
  (psx_init_tlca_nt64.o / tt_fork_v1_nt64.o)
- **Windows 默认链接** (tccpe.c CONFIG_TCC_POSIX): `crt_crt1.o libc.a`

## 目录映射

- Windows 头: `src/posix/musl-nt64/include` + `src/posix/musl-nt64/arch/nt64`
- Linux 头: `include/` (固化) 或 `build/linux-musl-inc` + `src/posix/musl-1.1.11/include` + `src/posix/musl-1.1.11/arch/x86_64`
