# tcc_posix — 纯 POSIX 双平台编译链

同一份 POSIX C 源码,编译出 Windows 原生 exe 与 Linux 静态 ELF,两边行为一致。

基于 TCC 0.9.28rc + musl libc + midipix psxscl(musl-libc.org / midipix 2015)。

## 快速开始

```bash
./install.sh                          # 生成自足目录 bin/ (tcc.exe + libc.a + musl 头)
bin/tcc.exe hello.c -o hello.exe      # 一条命令, 默认链 musl (零 Windows API 依赖)
```

```bash
./build.sh                            # 从源码完整重建 (自举 TCC + 后端 + musl libc)
build/tcc-win.exe examples/hello.c              # Windows exe (默认平台)
build/tcc-win.exe -platform=linux examples/hello.c  # Linux ELF
```

## 测试

```bash
./test.sh              # Windows 编译+运行 (28 测试)
./test.sh -run         # 追加 tcc -run 模式
./test.sh -linux       # 追加 Linux (WSL) 测试
```

当前 **84/84 通过** (Win 28 + -run 28 + Linux 28)。覆盖 stdio/malloc/mmap/文件/目录/时间/
宽字符/信号/tmp 映射、pthread 全套 (create/join/mutex/cond/barrier/sem/递归锁)、
线程压力、main 提前退出、tcc -run (含 futex 真阻塞),以及 ctype/setjmp/regex/search/
fenv/multibyte/crypt/prng 模块 (t022-t028)。

## 已知限制

- **`__thread` / `thread_local` 不可用** — TCC x86-64 只生成 `%fs:TPOFF32`
  (initial-exec, fs 基址 = TLS 块末尾),而 Windows fs 固定指向 TEB,无法指向
  musl TLS 块;修需重写 TCC TLS 代码生成 (TEB 槽/dtv),不支持 TLSGD 动态模型。
- **pty / termios 链路不可用** — psxscl-2015 的 /dev 分派器对 ptmx/pts 显式
  返回 NOT_FOUND(存根),/dev/tty 需控制终端;tcgetattr 等走 ioctl 的链路
  无法在 Windows 端验证 (termios 纯函数 cfgetospeed/cfmakeraw 等可用)。
- **socket / network 不可用** — socket() 返回 EACCES。根因线索:反汇编显示
  TCC 对 8+ 参数的 stdcall 调用只传 4 寄存器 + 3 栈 = 7 个参数,NtCreateFile
  (11 参)的后 4 个 (disposition/options/EA) 被静默丢弃 → ntapi v2 AFD 路径
  zw_create_file 失败;psx 文件 IO 同走 zw_create_file 却正常,待专项验证
  (疑似 TCC x86-64 stdcall 代码生成缺陷, 修复在 TCC 侧)。
- **fenv 为 dummy 实现** — nt64 的 fenv.c 注释 "Dummy functions for archs
  lacking fenv implementation",feclearexcept/fetestexcept 等返回 0,真实
  舍入/异常语义不生效 (t026 只测 API 契约)。
- **`-run` 模式** 走完整 musl 链 (非 msvcrt):printf/malloc/time/opendir/pthread
  全套可用,futex 基于 ntdll RtlWaitOnAddress/RtlWakeAddressAll 真阻塞。
