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
| pthread create/join/返回值 | ✅ (真 futex 阻塞) | ✅ |
| pthread mutex/cond/barrier/sem/detach | ✅ | ✅ |
| 线程压力 (100 顺序 + 80 并发) | ✅ | ✅ |
| main 提前退出 (不等子线程) | ✅ | ✅ |
| 静态链接 (无动态依赖) | ✅ (PE) | ✅ (ELF) |
| tcc -run (内存执行) | ✅ printf/malloc/time/opendir | — |

## 测试套件 (tests/ + test.sh)

```bash
./test.sh              # Windows 编译+运行 (19 测试)
./test.sh -run         # 追加 tcc -run 模式
./test.sh -linux       # 追加 Linux (WSL) 测试
./test.sh -clean       # 清理测试产物
```

覆盖: stdio 格式化 / 字符串 (mem*/strtok_r/strcasestr) / 内存压力 /
文件 IO (lseek/pread/rename/access) / 目录 (opendir/mkdir/rmdir) /
时间 (time/strftime/strptime) / mmap (匿名+文件映射) / 环境 (env/cwd) /
数值转换+数学 (strtol/floor/sqrt/fmod) / 宽字符函数 / 编译正确性
(递归/对齐/64位) / 信号 (raise/sigaction) / /tmp 映射 (t013)。

线程系列 (t014-t019):
- t014_thread: 8 线程 create/join/返回值校验
- t015_stress: 100 顺序 + 80 并发 create/join (ctx 槽复用 + 计数平衡)
- t016_mutex: 8 线程 × 20 万锁竞争精确计数
- t017_cond: condvar + timedwait (CLOCK_REALTIME 超时)
- t018_mainexit: main 提前退出进程立即结束
- t019_sync: barrier + semaphore + detach

`/tmp` 映射: psxscl 兼容层把 `/tmp` 重写到环境变量 TMP 指向的
用户临时目录 (字节拷贝 tt_generic_memcpy, tt_aligned_block_memcpy
按 uintptr_t 块会截断路径).

当前: **Windows 19 + Linux 19 = 38/38 通过**。

## 已知限制

1. **`__thread` / `thread_local` 不可用** — TCC PE 目标生成 `%fs:TPOFF32`
   (负偏移, fs:0 为 TLS 基址), 而 musl x86-64 的 TLS 变量在 fs 正偏移
   (TP_ADJ = pthread+sizeof(pthread)-16, 16 字节 TIB 头) → worker 访问
   __thread 段错误。修需改 TCC 的 TLS 代码生成 + PE TLS 重定位。
2. **宽字符字面量 (L"...") 不可用** — TCC 的 PE 目标 wchar_t 字面量是
   2 字节, musl 的 wchar_t 是 4 字节 → 宽字符串/宽格式串错乱;
   宽字符函数 (isw*/tow*/wcslen 等) 正常 (t010 用非字面量测试)
3. **musl-nt64 头缺 `PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP`** —
   递归锁需用 pthread_mutexattr_settype(PTHREAD_MUTEX_RECURSIVE);
   NORMAL 锁同一线程递归锁死锁是 musl 标准语义
4. **`-run` 模式** — runmain.o 的 `_runmain` 转发到
   `__libc_entry_routine` (crt_glue): psx_init 初始化后端 + `__libc_start_main`
   (stdio/TLS) + exit(main)。printf/malloc/time/opendir/getcwd/write 全可用

## Windows 线程实现 (psxscl 后端)

- 线程 = kernel32 CreateThread; join/互斥等待 = **ntdll RtlWaitOnAddress**
  真阻塞 (WaitOnAddress 在 KernelBase 不在 kernel32, 用 ntdll Rtl 版更稳),
  futex WAIT/WAKE 语义与 Linux 一致, 等待零 CPU
- daemon 线程通过 LPC 端口 (ZwRequestPort) 收线程退出消息, 维护全局
  pthreads 计数 — 最后一个线程退出时终止进程 (main 提前返回即进程结束)
- worker 的 tlca 由 __clone_tlca_init 独立初始化 (zw_allocate_virtual_memory),
  简化版不调完整 __psx_tlca_init, 需手动 at_locked_inc(&pthreads) 补计数
- ntapi 哈希装载: midipix 用自定义 CRC32 多项式 0xd35a6b40 (非标准), 已验证
  与哈希表一致, 250 个 ntdll API 全部装载成功

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
