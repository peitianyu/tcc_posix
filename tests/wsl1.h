/* tests/wsl1.h — WSL1 (内核 4.4.x-Microsoft) 检测.
 *
 * WSL1 是 syscall 翻译层, 部分 POSIX 能力缺失/异常 (与真实 Linux/WSL2 不同):
 *   - timer_create(SIGEV_NONE) 失败 (EINVAL; glibc 版直接 core dump)
 *   - POSIX mq_* 全部 ENOSYS (38)
 *   - System V IPC msgget 等 syscall 挂起不返回
 * 这些测试在 WSL1 上无条件跳过 (SKIP 退出码 0), 真实 Linux/WSL2 仍全量断言.
 * 用法: #ifdef __linux__  if (tcc_is_wsl1()) { printf("SKIP (WSL1)\n"); return 0; } #endif
 */
#ifndef TCC_WSL1_H
#define TCC_WSL1_H

#ifdef __linux__
#include <string.h>
#include <sys/utsname.h>

static int tcc_is_wsl1(void) {
    struct utsname u;
    if (uname(&u) != 0)
        return 0;
    return strstr(u.release, "Microsoft") != 0;
}
#else
static int tcc_is_wsl1(void) { return 0; }
#endif

#endif /* TCC_WSL1_H */
