# 系统型模块可用性矩阵与实现路线图

> 状态: 2026-08-22 更新 (夜班自主处理收尾 + R10b System V IPC 落地
>             + R10a mq + R10c dlfcn + R12 aio 修复 + R4 timer 恢复 + R5 select/poll)
> 依据: 实测探针 (build/tests/probe_*.c) + psx syscall 注册清单
> (`src/posix/components/psxscl-2015/src/init/psx_dev_wip.c`, 标题即 "WORK IN PROGRESS")

## 背景

测试套件已覆盖 28 个纯计算/基础模块 (t001-t028) + 系统型回归模块
(t029-t045, 含 t040_select_poll / t040_termios / t041_dlfcn / t042_timer /
t043_mq / t044_ipc),
当前 **46/46 通过** (test.sh, Windows tcc 自编译运行)。

## R11: env 路径变量大小写规范化 (t008)

t008_env 曾 rc=8: `getenv("PATH")` 返回 NULL。根因: Windows 真实环境变量名是
**`Path`** (首字母大写), 而 musl `getenv` 大小写敏感 → 找不到 `"PATH"`; 原
`__is_path` 也是精确大写匹配, 既不识别 `Path` 也不做转换。

修复 (psx 接口层 `psx_init_env.c`, musl 不动):
- `__is_path` 改大小写不敏感 (字符转大写后比对 PATH/PATH_/TEMP/TMP)。
- 拷贝环: 命中路径变量时, 把新缓冲里的名字规范化为规范大写 (`Path`→`PATH`),
  再走 `__copy_path_value` 转 unix 风格路径。

t008 通过; 全量 43/43 无回归。(PSX_CTX_FORK/EXEC_CHILD 分支仍原样保留)

## 关键机制发现: `__syscall_vtbl` NULL 槽即段错误

musl 的 syscall 走 `__syscallN(n, ...)` → `__syscall_vtbl[n](...)`
(`musl-nt64/arch/nt64/syscall_arch.h` 内联)。`__sysvtbl[]` 在 psx 初始化时
全 0 填充 (`psx_impl.c`), 仅 `psx_dev_wip.c` 注册过的槽有 handler。

**未注册的 syscall 槽 = NULL → musl 调用直接段错误。**
psx 2015 pre-alpha 的系统性缺口, 影响所有未实现模块。

**通用修复 (R1, 已合入)**: musl 侧 `__syscall` 包装检查 `__syscall_vtbl[n]==NULL`
→ 返回 `-ENOSYS`。一次把"崩溃"变成"可预期错误"。测试见 t034/t039。

## 可用性矩阵 (2026-08-22)

| # | 模块 | 注册 | 实测/结论 | 测试 |
|---|---|---|---|---|
| 1 | network/socket (66) | ✅ 15 全注册 | ✅ **已解决**: socket() 对 AF_INET/6 (stream/dgram/raw) 全部创建成功; 文档旧根因 "TCC 11参只传7参" 已被裸探针证伪 | t035 |
| 2 | process | ✅ | ✅ getpid/getppid/getpgid/getpgrp/getuid/geteuid/getgid/getegid 全正常 | t036 |
| 3 | sched (9) | ✅ sched_yield/setscheduler | ✅ sched_yield (SwitchToThread) 已实现 | t033 |
| 4 | timer | ✅ 注册 (222-226) | ✅ **已实现**: psx 接口层槽表 (timer_create/settime/gettime/getoverrun/delete), musl timer 库层恢复 (保留原版不动), t042 通过 | t042 |
| 5 | select/poll | ✅ 注册 (7/23/270/271) | ✅ **已实现**: psx 接口层 __sys_poll/ppoll/select/pselect6, t040_select_poll 通过 | t040 |
| 6 | mq (10) | ✅ 注册 (240-245) | ✅ **已实现**: psx 接口层静态槽表 (mq_open/unlink/timedsend/timedreceive/notify/getsetattr), musl mq 库层走 syscall, t043 通过 | t043 |
| 7 | aio (3) | n/a (用户态) | ✅ **已修复 (R12)**: musl 用户态线程池后台完成线程退出段错误,
   根因 musl-nt64 `crt_glue.c` 未初始化全局 `__psx_vtbl` → 补 psxscl 接口层
   `psx_vtbl.c` (`__psx_vtbl`→unmapself 静态共享栈清理) + `__psx_init` 填充
   ctx.psx_vtbl; bash 环境 10/10 稳定, t045 通过 | t045 |
| 8 | ipc (13) | ✅ 注册 (msg/sem/shm) | ✅ **已实现**: psx 接口层静态槽表 (msgget/msgsnd/msgrcv/msgctl/semget/semop/semtimedop/semctl/shmget/shmat/shmdt/shmctl), musl ipc 库层走 syscall, t044 通过 | t044 |
| 9 | statvfs/utimensat | ✅ statfs/fstatfs 注册 | ✅ **statfs/fstatfs 已实现**; utimensat 未注册 → ENOSYS | t037 |
| 10 | pwd/grp (20) | n/a (库函数) | ✅ **虚拟 /etc 映射已实现**: config iofn 挂只读虚拟 passwd/group 文件; getpwuid/getpwnam/getgrgid 均正确解析到 root (uid/gid=1000) | t038 |
| 11 | termios (10) | ioctl ✅ (console 分支已加) | ✅ **console 映射已实现**: __sys_ioctl 对非 PTY fd 先 GetConsoleMode 探 console,
    是则处理 TCGETS/TCSETS(等)/TIOCGWINSZ (GetConsoleMode/SetConsoleMode/
    GetConsoleScreenBufferInfo); 非 console (pipe/file) 维持 ENOTTY → isatty 语义正确 | t040 |
| 12 | dlfcn | ✅ 链接层已补 | ✅ **已实现**: psx 接口层 _dlfcn (dlopen/dlsym/dlclose/dlerror/dladdr), t041 通过 | t041 |
| 13 | aio/utimensat 等杂项 | — | 见上 | — |

## 实现路线图 (2026-08-22 快照)

- [x] **R1. 通用 ENOSYS 保护** — musl `__syscall` 加
      `__syscall_vtbl[n]==NULL → -ENOSYS`。t034/t039 验证。
- [x] **R2. socket** — socket() 协议推导修复, 创建全通过。t035。
- [x] **R3. sched_yield** — psx `__sys_sched_yield` (SwitchToThread), 注册。
      t033。
- [x] **R7. statvfs/statfs** — psx `__sys_statfs/__sys_fstatfs` 映射 NTFS 卷信息,
      注册 SYS_statfs(137)/SYS_fstatfs(138)。t037。
- [x] **R8. process 探针** — getpid/getppid/getuid 等全正常。t036。
- [x] **R6. termios console 映射** — psxscl `__sys_ioctl` 非 PTY 分支: 对 fd 句柄
      `GetConsoleMode` 探 console, 命中则处理 TCGETS/TCSETS/TCSETSW/TCSETSF/
      TCGETA/TCSETA/TCSETAW/TCSETAF (GetConsoleMode/SetConsoleMode 映射
      ECHO↔ENABLE_ECHO_INPUT, ICANON↔ENABLE_LINE_INPUT, ISIG↔
      ENABLE_PROCESSED_INPUT) 与 TIOCGWINSZ (GetConsoleScreenBufferInfo);
      非 console (pipe/file/socket) 维持 ENOTTY → isatty() 对真 console=1、
      对 pipe=0 语义正确。t040 通过。**注**: 本自动化环境无任何 console
      (GetStdHandle(STDIN)=INVALID), 需真交互 console 做 E2E 验证。
- [x] **R4. timer** — psx 接口层定时器槽表 (timer_create/settime/gettime/
      getoverrun/delete, 见 `src/psxscl-2015/src/timer/_timer.c`), 注册
      SYS_timer_create(222)..timer_delete(226); musl timer 库层保留原版不动
      (恢复 musl `src/time/timer_*.c` 编译, 不改语义)。t042 通过。
- [x] **R5. select/poll** — psx 接口层补 `__sys_poll/__sys_ppoll/__sys_select/
      __sys_pselect6` (非阻塞探测 + 事件等待, 见 `src/psxscl-2015/src/select/`),
      注册 SYS_poll(7)/SYS_select(23)/SYS_pselect6(270)/SYS_ppoll(271)。
      t040_select_poll 通过。
- [x] **R6. termios console 映射** — 见上方 ✅ (已实现, t040 通过; E2E 留真 console)。
- [x] **R9. pwd/grp /etc 虚拟映射** — config iofn 挂只读虚拟 passwd/group 文件;
      getpwuid/getpwnam/getgrgid 正确解析到 root (uid/gid=1000)。
      t038 通过。修复两处 iofn 根因:
      (1) `psx_iofn_fsroot.c` 原 `*buf++` 跨分支共享递增指针, 3 字符 /etc 在
      "dev" 分支 strlen==3 判断通过后被 +1 跳过 'e' 恒失配 → 改索引比较;
      (2) config open_next 长度计算: 原按 wchar16 指针差再除 sizeof 得 bytes/4,
      "passwd" 得 3 恒失配 → 改与 fsroot 一致的 (uintptr 字节差)/sizeof。
- [x] **R10. mq/ipc/aio/dlfcn 补全** — 四路全落地:
      - R10a mq: psx 接口层静态槽表 (mq_open/unlink/timedsend/timedreceive/
        notify/getsetattr, 见 `src/psxscl-2015/src/mq/_mq.c`), 注册
        SYS_mq_open(240)..mq_getsetattr(245); musl mq 库层走 syscall。t043 通过。
      - R10b ipc: psx 接口层静态槽表 (msgget/msgsnd/msgrcv/msgctl/semget/
        semop/semtimedop/semctl/shmget/shmat/shmdt/shmctl, 见
        `src/psxscl-2015/src/ipc/_ipc.c`), 注册 SYS_msgget(68)..SYS_shmctl(31);
        musl ipc 库层走 syscall。t044 通过。
      - R10c dlfcn: psx 接口层补 dlopen/dlsym/dlclose/dlerror/dladdr
        (见 `src/psxscl-2015/src/ldso/_dlfcn.c`), 链接层符号补齐。t041 通过。
      - R12 aio: musl 用户态线程池在 nt64 移植上后台完成线程段错误, **已修复**:
        根因 musl-nt64 `crt_glue.c` 未初始化全局 `__psx_vtbl`, 线程退出时
        `__unmapself → __psx_vtbl->unmapself` 调用垃圾地址段错误。psx 接口层
        补 `src/thread/psx_vtbl.c` (定义 `__psx_vtbl_impl`, unmapself 切静态共享栈
        执行 TLCA 释放/栈区解除映射/线程终止) + `__psx_init` 填充 ctx.psx_vtbl。
        t045 通过, bash 环境 10/10 稳定。

## R12: aio 后台完成线程段错误修复

**现象**: musl aio 是用户态线程池 (src/aio/aio.c), 每笔 aio 创建一个 detached
后台完成线程, 完成后 `pthread_exit → __unmapself` 释放 mmap 线程栈并终止线程。
在 nt64 移植上该退出路径段错误 (bash 环境稳定复现 rc=139, PowerShell 环境偶发通过)。

**根因**: musl-nt64 `arch/nt64/src/crt_glue.c` 只初始化了 `__syscall_vtbl` /
`__ldso_vtbl`, 遗漏 `__psx_vtbl` → 全局指针指向栈垃圾 (0x11120000),
`__unmapself` 调用 `__psx_vtbl->unmapself` 时访问无效地址段错误。

**修复 (psx 接口层, musl 不动)**:
1. 新增 `src/thread/psx_vtbl.c`: 定义 `__psx_vtbl_impl` (convert_thread /
   unmapself / get_osfhandle) 与 `__psx_get_psx_vtbl()`。
   `unmapself` 先把 base/size 存入全局静态变量, 再 `__psx_tlca_prolog` 切到静态
   共享栈执行清理 (线程计数递减、释放 TLCA、解除线程栈映射), 最后
   `__psx_tlca_epilog` (jmp) 调 ZwTerminateThread — 全程不读写已释放的线程栈。
2. `src/init/psx_init.c` 的 `__psx_init_impl` 增加: `ctx->psx_vtbl =
   __psx_get_psx_vtbl();` → musl `crt_glue.c` 据此初始化全局 `__psx_vtbl`。

**验证**: t045_aio (aio_read/aio_write/aio_suspend + 32 并发) 全过;
bash (MSYS2) 环境连续 10/10 通过, 全量回归 46/46 无回归。

**备注**: `__psx_convert_thread_sys` (第三方线程转换) 与 `get_osfhandle` 当前
无调用方, 保持占位。

## 需用户明日拍板的决策 (本次自主处理未拿定主意项)

1. ~~**select/poll 是否实现**~~ — 已实现 (R5), t040_select_poll 通过。
2. ~~**pwd/grp /etc 映射**~~ — 已拍板落地 (虚拟 /etc 映射), 见"已拍板决策"。
3. ~~**mq/ipc dlfcn**~~ — 已全部补实现 (R10a/b/c), 见路线图。
4. ~~**aio nt64 配送崩溃**~~ — 已修复 (R12): psx 接口层补 `__psx_vtbl`
   (psx_vtbl.c) + `__psx_init` 填充 ctx.psx_vtbl, t045 通过, 见 R12 节。
5. ~~**timer**~~ — 已恢复 (R4): psx 接口层槽表实现, musl timer 库层保留原版
   不动 (无需改动 musl 语义)。
6. **termios console E2E** — 映射已实现 (R6), 但本自动化环境无 console,
   需在真交互终端跑一次 t040 确认 tcgetattr/TIOCGWINSZ/TCSETS 实际数值。

## 已拍板决策 (夜班)

- **[R9] pwd/grp 虚拟 /etc 映射已落地** — 选用"文件层虚拟 /etc 映射"(config iofn
  挂只读 passwd/group), 保留 musl 原版库函数不动。per-fd 读位置用静态槽池承载
  (系统调用层避免递归进 musl malloc)。t038 通过。
- **[R4] timer 恢复已落地** — psx 接口层定时器槽表实现, 不恢复 musl 用户态
  timer 线程 (避免 nt64 移植线程问题); musl timer 库层保留原版。t042 通过。
- **[R5] select/poll 已实现** — psx 接口层非阻塞探测 + 事件等待, 跨 fd 类型
  (socket/pipe/文件) 就绪判断。t040_select_poll 通过。
- **[R10] mq/ipc/dlfcn 补全已落地** — 全部接口层实现, musl 库层保留原版走
  syscall。t041/t043/t044 通过。
- **[R12] aio 崩溃已修复** — psx 接口层补 `__psx_vtbl` (psx_vtbl.c, unmapself
  静态共享栈清理) + `__psx_init` 填充 ctx.psx_vtbl; 保留 musl aio 用户态线程池
  原版不动。t045 通过, bash 10/10。

- **归档工具: 保留 mingw `ar`**, 不用 `tcc -ar`。原因: `tcc -ar` 对长文件名
  15 字符截断, 会造成静态库成员名冲突 (libtcc1.a 内 chkstk.o 等), 无法自足。
  构建脚本 script/build_musl.sh 继续用 mingw ar 打包 libc.a。
- **R1/R2/R3/R6/R7/R8 全部按接口层/编译器层合入**, 未改动 musl 语义 (仅 syscall
  层兜底 + psxscl handler + socket 协议推导 + console termios 分支)。
- **aio 不在 ENOSYS 测试中断言**: 它是用户态实现, 且当前在 nt64 崩, 与 ENOSYS
  主张无关。
- **t039 断言对象更新**: R10b ipc 已实现后, t039 的 ENOSYS 断言从
  msgget/semget/shmget 换成未注册 syscall (getrandom=318 / fanotify_init=300 /
  bpf=321), 验证 vtbl 兜底仍生效。

## 参考文件

- `src/posix/components/psxscl-2015/src/init/psx_dev_wip.c` — syscall 注册清单
- `src/posix/components/psxscl-2015/src/internal/psx_impl.c` — `__sysvtbl[]` 初始化
- `src/posix/musl-nt64/arch/nt64/syscall_arch.h` — `__syscallN` 内联 (vtbl 调用)
- `src/posix/components/psxscl-2015/src/iofn/psx_iofn_device.c` — /dev 分派 (pty 存根)
- `src/posix/components/ntapi/src/socket/ntapi_sc_socket_v2.c` — AFD 创建
- `src/posix/components/psxscl-2015/src/internal/psx_timer.c` — timer 底层 (APC)
- `src/posix/components/psxscl-2015/src/stat/_statfs.c` — statfs/fstatfs 实现
- `src/posix/components/psxscl-2015/src/ioctl/_ioctl.c` — ioctl: pty 分支 + console termios 映射
- `src/posix/components/psxscl-2015/src/thread/psx_vtbl.c` — `__psx_vtbl` 实现 (unmapself 静态共享栈清理)
- `src/posix/components/psxscl-2015/src/init/psx_init.c` — 填充 ctx.psx_vtbl
- `src/posix/musl-nt64/arch/nt64/src/crt_glue.c` — 初始化全局 `__syscall_vtbl/__ldso_vtbl/__psx_vtbl`
- `src/posix/musl-nt64/src/aio/aio.c` — aio 用户态线程池 (nt64 崩溃现场)