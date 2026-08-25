/* 测试: R4 timer 库层恢复. musl timer_create/settime/gettime/getoverrun/delete
 * 不再 ENOSYS, 在 PSX 接口层维护 timer 槽表.
 * 覆盖:
 *  - timer_create(CLOCK_REALTIME, SIGEV_NONE, &tid) -> 0
 *  - timer_settime 存储 it_interval / it_value -> 0
 *  - timer_gettime 读回 -> 0, 数值一致
 *  - timer_getoverrun -> 0
 *  - timer_delete -> 0 (之后 gettime 应 -EINVAL)
 *  - 无效 timer id 操作 -> -1 errno=EINVAL
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <signal.h>
#include "wsl1.h"

static int is_eq(const struct itimerspec *a, const struct itimerspec *b)
{
    return a->it_interval.tv_sec == b->it_interval.tv_sec &&
           a->it_interval.tv_nsec == b->it_interval.tv_nsec &&
           a->it_value.tv_sec == b->it_value.tv_sec &&
           a->it_value.tv_nsec == b->it_value.tv_nsec;
}

int main(void) {
#ifdef __linux__
    if (tcc_is_wsl1()) { printf("SKIP (WSL1: timer_create 不可用)\n"); return 0; }
#endif
    int fail = 0;
    timer_t tid = (timer_t)(intptr_t)-1;
    struct sigevent sev;
    struct itimerspec its, back;

    /* timer_create: SIGEV_NONE */
    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_NONE;
    errno = 0;
    if (timer_create(CLOCK_REALTIME, &sev, &tid) != 0) {
        printf("  timer_create(SIGEV_NONE) failed errno=%d %s\n", errno, strerror(errno));
        fail++;
    } else printf("  timer_create ok tid=%p\n", (void *)tid);

    if (fail == 0) {
        /* timer_settime: interval 1.5s / value 2s */
        its.it_interval.tv_sec = 1;  its.it_interval.tv_nsec = 500000000;
        its.it_value.tv_sec   = 2;   its.it_value.tv_nsec   = 0;
        errno = 0;
        if (timer_settime(tid, 0, &its, 0) != 0) {
            printf("  timer_settime failed errno=%d\n", errno); fail++;
        } else printf("  timer_settime ok\n");

        /* timer_gettime: 读回一致 */
        memset(&back, 0xAA, sizeof(back));
        errno = 0;
        if (timer_gettime(tid, &back) != 0) {
            printf("  timer_gettime failed errno=%d\n", errno); fail++;
        } else {
            if (!is_eq(&its, &back)) {
                printf("  timer_gettime mismatch (iv=%ld.%ld val=%ld.%ld)\n",
                    (long)back.it_interval.tv_sec, (long)back.it_interval.tv_nsec,
                    (long)back.it_value.tv_sec, (long)back.it_value.tv_nsec);
                fail++;
            } else printf("  timer_gettime ok (matches settime)\n");
        }

        /* timer_getoverrun */
        errno = 0;
        if (timer_getoverrun(tid) != 0) {
            printf("  timer_getoverrun !=0 (errno=%d)\n", errno); fail++;
        } else printf("  timer_getoverrun ok\n");

        /* timer_delete */
        errno = 0;
        if (timer_delete(tid) != 0) {
            printf("  timer_delete failed errno=%d\n", errno); fail++;
        } else printf("  timer_delete ok\n");
    }

    /* 已删除 timer: gettime -> -1 errno=EINVAL */
    errno = 0;
    if (timer_gettime(tid, &back) != -1 || errno != EINVAL) {
        printf("  gettime on deleted timer rc=%d errno=%d (want -1/EINVAL)\n",
            errno, errno); fail++;
    } else printf("  gettime on deleted -> -1/EINVAL ok\n");

    /* 无效 timer id: getoverrun -> -1 errno=EINVAL */
    errno = 0;
    if (timer_getoverrun((timer_t)(intptr_t)9999) != -1 || errno != EINVAL) {
        printf("  getoverrun invalid id rc/errno=%d (want -1/EINVAL)\n", errno); fail++;
    } else printf("  getoverrun invalid id -> -1/EINVAL ok\n");

    if (fail) { printf("FAIL %d\n", fail); return 1; }
    printf("timer ok\n");
    return 0;
}