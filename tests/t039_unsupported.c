/* 测试: R1 ENOSYS 保护跨模块生效. 未注册 syscall 槽 → -ENOSYS (不崩溃).
 * 覆盖: 未注册 syscall (getrandom=318, membarrier=324) → 期望 ENOSYS.
 * 注: ipc(msg/sem/shm) R10b 已实现 (见 t044_ipc.c), timer R4 已实现 (t042),
 *     select/poll R5 已实现 (t040), mq R10a 已实现 (t043), 均不再断言 ENOSYS.
 * 注: aio 不含在内 — musl 用户态线程池在 nt64 移植上后台完成线程会段错误,
 *      属已知缺陷 (见 docs/system-modules.md R10), 不在本 ENOSYS 测试断言. */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

static int chk(const char *name, long r, int want) {
    if (r != -1) { printf("  %-22s -> r=%ld (expected -1)\n", name, r); return 1; }
    if (errno != want) { printf("  %-22s errno=%d (%s) want %d (%s)\n",
        name, errno, strerror(errno), want, strerror(want)); return 1; }
    printf("  %-22s OK (%s)\n", name, strerror(errno));
    return 0;
}

int main(void) {
    int fail = 0;

    /* R1: 未注册 syscall 槽 → -ENOSYS (不崩溃) */
    printf("[R1] unregistered syscall\n");
    errno = 0; fail += chk("getrandom(318)", syscall(SYS_getrandom, 0, 0, 0), ENOSYS);
    errno = 0; fail += chk("fanotify_init(300)", syscall(SYS_fanotify_init, 0, 0), ENOSYS);
    errno = 0; fail += chk("bpf(321)", syscall(SYS_bpf, 0, 0, 0), ENOSYS);

    if (fail) { printf("FAIL %d\n", fail); return 1; }
    printf("all unsupported -> ENOSYS ok\n");
    return 0;
}