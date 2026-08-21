# tcc_posix — 纯 POSIX 双平台编译链

一套**只使用 POSIX 语法**的完整编译链:同一份 C 源码,编译出 Windows 原生可执行文件
和 Linux 静态 ELF,两边行为一致。

```
源码 (POSIX C) ──→ tcc -platform=win/linux ──→ foo.exe   (Windows 原生)
                                             └── foo_linux (Linux 静态 ELF)
```

基于 TCC 0.9.28rc + musl libc + midipix psxscl 组件(musl-libc.org / midipix 2015,Wayback 恢复)。

## 快速开始

编译器内联 `-platform=win|linux` 选项(**默认 win**),两个编译器可互相转发:

```bash
build/tcc-win.exe examples/hello.c               # Windows exe (默认平台)
build/tcc-win.exe -platform=linux examples/hello.c  # Linux ELF
build/tcc-linux.exe -platform=win examples/hello.c  # 反向转发
```

### 安装 (Windows, 开箱即用)

```bash
./install.sh
# 产物: bin/tcc.exe + bin/lib/libc.a (唯一运行时文件) + bin/include/ (musl 头)

bin/tcc.exe hello.c -o hello.exe   # 一条命令编译链接, 默认链 musl (无 msvcrt)
```

`bin/` 是自足目录:tcc.exe 按自身所在目录找 `lib/` 与 `include/`,零 Windows API 依赖。

### 完整构建 (从源码重建)

```bash
./build.sh
# [1/4] 自举 TCC (tcc-win.exe + tcc-linux.exe)
# [2/4] Windows 后端 (psxscl/ntapi/pemagine/dalist)
# [3/4] musl libc (Windows + Linux)
# [4/4] 固化 include/ 与 lib/*.a
```

## 已验证功能 (同一份 POSIX 源码, 双平台)

| 功能 | Windows | Linux |
|---|---|---|
| stdio / malloc / mmap / 文件 IO / dirent / time | ✅ | ✅ |
| 字符串 / 数学 / 宽字符函数 / 宽字符字面量 L"..." / 信号 | ✅ | ✅ |
| pthread create/join/mutex/cond/barrier/sem/detach/递归锁 | ✅ | ✅ |
| 线程压力 (100 顺序 + 80 并发) | ✅ | ✅ |
| main 提前退出 (不等子线程) | ✅ | ✅ |
| 静态链接 (无动态依赖) | ✅ (PE) | ✅ (ELF) |
| tcc -run (内存执行, 含线程/futex) | ✅ | — |

## 测试套件

```bash
./test.sh              # Windows 编译+运行 (21 测试)
./test.sh -run         # 追加 tcc -run 模式
./test.sh -linux       # 追加 Linux (WSL) 测试
./test.sh -clean       # 清理测试产物
```

覆盖 stdio/字符串/内存/文件/目录/时间/mmap/环境/数学/宽字符/信号/tmp 映射,
以及线程系列 t014-t021(create/join、压力、mutex 竞争、condvar、递归锁、宽字符字面量、main 提前退出、barrier/sem/detach)。

当前: **Windows 21 + -run 21 + Linux 21 = 63/63 通过** (`./test.sh -run -linux`)。

## 已知限制

1. **`__thread` / `thread_local` 不可用** — TCC 的 x86-64 TLS 代码生成只用
   `%fs:TPOFF32`(initial-exec, fs 基址 = TLS 块末尾), 而 Windows 的 fs
   寄存器固定指向 TEB, 无法指向 musl 的 TLS 块 (musl-nt64 用 TEB 系统槽存
   pthread 指针 + dtv 访问); 修需重写 TCC 的 TLS 代码生成 (TEB 槽/dtv 路径),
   不支持 TLSGD 动态模型。
2. **`-run` 模式** 完整 musl 链(非 msvcrt): printf/malloc/time/opendir/
   getcwd/write 及 pthread 全套 (create/join/mutex/cond/barrier/sem) —
   futex 基于 ntdll RtlWaitOnAddress/RtlWakeAddressAll 真阻塞。
