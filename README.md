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
# 产物: bin/tcc.exe + bin/lib/libc.a (唯一运行时文件) + bin/include/ (musl 头)
#
#   bin/lib/libc.a  3.2M, 1283 成员: musl + chkstk + init_array + mem 4件套
#                  + psxscl/ntapi/pemagine/dalist 后端 + libtcc1 (编译器辅助)
#                  + crt_crt1 (入口 _start) + runmain (-run)
#                  → 零 Windows API 依赖, 无裸 .o (同原版 TCC lib 只有 .a)

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
| tcc -run (内存执行) | ✅ printf/malloc/time/opendir | — |

## 测试套件 (tests/ + test.sh)

```bash
./test.sh              # Windows 编译+运行 (12 测试)
./test.sh -run         # 追加 tcc -run 模式 (12 测试)
./test.sh -clean       # 清理测试产物
```

覆盖: stdio 格式化 / 字符串 (mem*/strtok_r/strcasestr) / 内存压力 /
文件 IO (lseek/pread/rename/access) / 目录 (opendir/mkdir/rmdir) /
时间 (time/strftime/strptime) / mmap (匿名+文件映射) / 环境 (env/cwd) /
数值转换+数学 (strtol/floor/sqrt/fmod) / 宽字符函数 / 编译正确性
(递归/对齐/64位) / 信号 (raise/sigaction) / /tmp 映射 (t013)。

`/tmp` 映射: psxscl 兼容层把 `/tmp` 重写到环境变量 TMP 指向的
用户临时目录 (字节拷贝 tt_generic_memcpy, tt_aligned_block_memcpy
按 uintptr_t 块会截断路径).

当前: **Windows 13 + -run 13 + Linux 13 = 39/39 通过**。

## 已知限制

1. **Windows 端 pthread join 卡死** — psxscl 2015 的 futex/ctid 机制在
   TCC 编译下不完整 (ctid 清除不可靠); 单线程程序无影响, Linux 端线程正常
2. **宽字符字面量 (L"...") 不可用** — TCC 的 PE 目标 wchar_t 字面量是
   2 字节, musl 的 wchar_t 是 4 字节 → 宽字符串/宽格式串错乱;
   宽字符函数 (isw*/tow*/wcslen 等) 正常 (t010 用非字面量测试)
3. **`-run` 模式完整支持 musl** — runmain.o 的 `_runmain` 转发到
   `__libc_entry_routine` (crt_glue): psx_init 初始化后端 + `__libc_start_main`
   (stdio/TLS) + exit(main)。printf/malloc/time/opendir/getcwd/write 全可用。
   已知: Windows 端 pthread join 卡死 (同限制 1)

## 链接细节 (TCC 特性 workaround)

- **TCC 归档提取闭包有效**: tccelf.c `tcc_load_alacarte` 的 do-while 循环
  会自动补提被新加载成员引用的符号 → 链接只需单遍列库
- **入口符号经 set_global_sym 提前注册**: tccpe.c 的 posix 分支先
  `set_global_sym(_start)` 再加载 libc.a → alacarte 索引能查到 _start →
  crt_crt1 成员被提取 (无需显式裸 .o)
- **chkstk / init_array / mem 4件套 / 后端 4 库 / libtcc1 / crt_crt1 /
  runmain 全部并入 libc.a** (1283 成员): 闭包提取自动解析全部交叉引用
- **剔除 msvcrt 时代 CRT**: libtcc1 里的 crt1/crt1w/wincrt1/wincrt1w/tcov/
  dllcrt1/dllmain/winex 引用 kernel32 API, 而 musl/psxscl 零 Windows 依赖 →
  从 libc.a 剔除, 无需 kernel32.def (libc.a 完全自足)
- **-run 的 runmain 提取**: tccrun.c 找不到 runmain.o 文件时 fallback -
  set_global_sym(_runmain) + 重载 libc.a, alacarte 提取 _runmain 成员
- **后端合并依赖重名修复**: psxscl/ntapi 构建时 C 版与汇编版同名对象
  (`__psx_init_tlca.o` / `tt_fork_v1.o`) 会互相覆盖, 丢失
  `__psx_tlca_prolog` / `__tt_fork_v1` 等关键符号 → 构建脚本已把汇编版改名
  (psx_init_tlca_nt64.o / tt_fork_v1_nt64.o)
- **Windows 默认链接** (tccpe.c CONFIG_TCC_POSIX): `crt_crt1.o libc.a`

## 目录映射

- Windows 头: `src/posix/musl-nt64/include` + `src/posix/musl-nt64/arch/nt64`
- Linux 头: `include/` (固化) 或 `build/linux-musl-inc` + `src/posix/musl-1.1.11/include` + `src/posix/musl-1.1.11/arch/x86_64`
