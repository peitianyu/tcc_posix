# 系统型模块可用性矩阵与实现路线图

> 状态: 2026-08-21 审计完成, 待逐一实现
> 依据: 实测探针 (build/tests/probe_*.c) + psx syscall 注册清单
> (`src/posix/components/psxscl-2015/src/init/psx_dev_wip.c`, 标题即 "WORK IN PROGRESS")

## 背景

测试套件已覆盖 28 个纯计算/基础模块 (t001-t028, 84/84 通过), 但审计时列出的
**系统型模块** (依赖 psx 后端/syscall 的模块) 全部没有正式测试。本矩阵基于
探针实测与 `psx_dev_wip.c` 的 syscall 注册清单, 给出每个模块的可用性、根因与
实现动作, 作为后续逐一实现的清单。

## 关键机制发现: `__syscall_vtbl` NULL 槽即段错误

musl 的 syscall 走 `__syscallN(n, ...)` → `__syscall_vtbl[n](...)`
(`musl-nt64/arch/nt64/syscall_arch.h` 内联)。`__sysvtbl[]` 在 psx 初始化时
全 0 填充 (`psx_impl.c`), 仅 `psx_dev_wip.c` 注册过的槽有 handler。

**未注册的 syscall 槽 = NULL → musl 调用直接段错误, 而不是返回 ENOSYS。**
这是 psx 2015 pre-alpha 的系统性缺口, 影响所有未实现模块 (sched_yield/timer/
mq/aio/ipc/statvfs/utimensat/poll 实测全部崩溃)。

**通用修复 (最高优先级)**: musl 侧 `__syscall` 包装检查 `__syscall_vtbl[n]==NULL`
→ 返回 `-ENOSYS`。一次把"崩溃"变成"可预期错误", 之后测试才能写。

## 可用性矩阵

| # | 模块 | psx syscall 注册 | 实测 | 根因 | 需要的动作 |
|---|---|---|---|---|---|
| 1 | network/socket (66) | ✅ 15 个全注册 (socket/bind/connect/accept/…) | ⚠️ socket() EACCES | TCC 8+ 参数 stdcall 缺陷 (反汇编证实 NtCreateFile 11 参只传 7 参) | 修 TCC x86-64 调用生成, 或 psx 侧包装 |
| 2 | process (31) | ✅ execve/fork/wait4/waitid/exit | ❓ 未测 (探针被 sched_yield 卡住) | — | 温和探针 (getpid/system/fork) |
| 3 | sched (9) | ⚠️ 仅 sched_setscheduler | ❌ sched_yield 段错误 | `__sysvtbl[24]` = NULL | 补 `__sys_sched_yield` (SwitchToThread) |
| 4 | timer | ❌ 全未注册 (222-226) | ❌ timer_create 段错误 | `__sysvtbl[222]` = NULL | 补 handler (psx_timer.c 已有底层) |
| 5 | select/poll (3) | ❌ 未注册 | ❌ 段错误 | vtbl NULL | 补 handler |
| 6 | mq (10) | ❌ 未注册 (240-244) | ❌ | vtbl NULL | 补 handler 或标记不支持 |
| 7 | aio (3) | ❌ 未注册 | ❌ | vtbl NULL | 同上 |
| 8 | ipc (13) | ❌ 未注册 (msg/sem/shm 29-71) | ❌ | vtbl NULL | 同上 |
| 9 | statvfs/utimensat | ⚠️ mprotect ✅, 其余 ❌ | ❓ | vtbl NULL | 补 statvfs/utimensat handler |
| 10 | pwd/grp (20) | n/a (库函数, 读 /etc/passwd) | ❓ 未测 | musl 依赖 /etc 映射 | 探针验证 |
| 11 | termios (10) | ioctl ✅ (TTY 分支需 hpty) | ❌ isatty=0, pty 存根 | device.c 显式 NOT_FOUND + 无 console 支持 | console termios 映射方案 |
| 12 | dlfcn | ❌ 链接层缺符号 | ❌ 链接失败 | musl-nt64 无 dlfcn 实现 | 补实现或标记不支持 |

## 实现路线图 (按优先级)

- [ ] **R1. 通用 ENOSYS 保护** — musl `syscall.c`/`syscall_arch.h` 的
      `__syscall` 加 `__syscall_vtbl[n]==NULL → -ENOSYS` 检查。所有未实现
      syscall 从"段错误"变"可预期错误"。影响面: 全部 12 个模块的测试可写性。
- [ ] **R2. socket / TCC 缺陷** — 修 TCC x86-64 stdcall 8+ 参数调用生成
      (NtCreateFile 11 参只传 7 参, 反汇编证据), 或 psx 侧参数包装。
      socket 可能直接复活。
- [ ] **R3. sched_yield** — psx 补 `__sys_sched_yield` (SwitchToThread 让出),
      `psx_dev_wip.c` 注册。t016 等线程测试可顺带验证。
- [ ] **R4. timer** — psx 补 `__sys_timer_create/settime/gettime/delete`
      (psx_timer.c 已有 APC 底层), 注册 222-226。
- [ ] **R5. select/poll** — psx 补 `__sys_poll` / select (ofd poll 回调已存在)。
- [ ] **R6. termios console 映射** — psx ioctl 对 console fd 加 TCGETS/TCSETS/
      TIOCGWINSZ 分支 (GetConsoleMode/SetConsoleMode, ≤7 参数不触发 TCC bug),
      isatty 对 console 返回真。补 t029。
- [ ] **R7. statvfs/utimensat** — psx 补 handler (statvfs 可映射 NTFS 卷信息)。
- [ ] **R8. process 探针** — 温和验证 getpid/system/fork (tt_fork 已编入)。
- [ ] **R9. pwd/grp 探针** — 验证 /etc/passwd 映射 (musl 库函数路径)。
- [ ] **R10. mq/aio/ipc/dlfcn** — 逐个评估"补实现"还是"明确 ENOSYS/不支持"。

## 参考文件

- `src/posix/components/psxscl-2015/src/init/psx_dev_wip.c` — syscall 注册清单
- `src/posix/components/psxscl-2015/src/internal/psx_impl.c` — `__sysvtbl[]` 初始化
- `src/posix/musl-nt64/arch/nt64/syscall_arch.h` — `__syscallN` 内联 (vtbl 调用)
- `src/posix/components/psxscl-2015/src/iofn/psx_iofn_device.c` — /dev 分派 (pty 存根)
- `src/posix/components/ntapi/src/socket/ntapi_sc_socket_v2.c` — AFD 创建 (TCC 缺陷现场)
- `src/posix/components/psxscl-2015/src/internal/psx_timer.c` — timer 底层 (APC)
