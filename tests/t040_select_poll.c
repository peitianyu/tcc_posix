/* 测试: R5 select/poll 已实现. 合法已打开 fd → 就绪, 非法 fd 不置位, 不崩溃.
 * 覆盖: poll(POLLOUT 立即就绪) / select(w++可写) / select(非法 fd 超时0 不挂) /
 *       poll(events==0 返回 0) */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <sys/select.h>

int main(void) {
    int fail = 0;
    int r;
    struct pollfd pfd;
    struct timeval tv;
    fd_set wf, rf;

    /* poll: stdout (fd=1, 打开) 请求可写 → 立即就绪 */
    pfd.fd = 1; pfd.events = POLLOUT; pfd.revents = 0;
    errno = 0;
    r = poll(&pfd, 1, 0);
    if (r < 0) { printf("poll errno=%d %s\n", errno, strerror(errno)); return 1; }
    if (!(pfd.revents & POLLOUT)) { printf("poll stdout not writable revents=%#x\n", pfd.revents); fail++; }
    printf("  poll r=%d revents=%#x\n", r, pfd.revents);

    /* poll: events==0 → 返回 0 (revents 本实现不触碰) */
    pfd.fd = 1; pfd.events = 0; pfd.revents = 0;
    r = poll(&pfd, 1, 0);
    if (r != 0) { printf("poll(events=0) r=%d (want 0)\n", r); fail++; }
    printf("  poll events=0 r=%d\n", r);

    /* select: fd 1 (打开) 请求可写 → >0 且置位 */
    FD_ZERO(&wf); FD_SET(1, &wf);
    tv.tv_sec = 0; tv.tv_usec = 0;
    errno = 0;
    r = select(2, NULL, &wf, NULL, &tv);
    if (r < 0) { printf("select errno=%d %s\n", errno, strerror(errno)); return 1; }
    if (!FD_ISSET(1, &wf)) { printf("select fd1 not set\n"); fail++; }
    printf("  select r=%d\n", r);

    /* select: 仅非法高位 fd (未打开) + 超时 0 → 不置位, 不阻塞.
     * 语义差异: psxscl 返回 0 (不检查 fd); 真实 Linux 立即 -1 EBADF. */
    FD_ZERO(&rf); FD_SET(200, &rf);
    tv.tv_sec = 0; tv.tv_usec = 0;
    errno = 0;
    r = select(201, &rf, NULL, NULL, &tv);
    printf("  select invalid-fd r=%d errno=%d (%s)\n", r, errno, strerror(errno));
#ifdef __linux__
    if (r != -1 || errno != EBADF) { printf("select invalid fd r=%d errno=%d (want -1 EBADF)\n", r, errno); fail++; }
    /* linux: select 出错 (EBADF) 时 fd_set 内容未定义 (保留原值), 不断言 */
#else
    if (r < 0) { printf("select invalid fd errored (want 0, no block)\n"); fail++; }
    if (r != 0) { printf("select invalid fd r=%d (want 0)\n", r); fail++; }
    if (FD_ISSET(200, &rf)) { printf("select set closed fd200\n"); fail++; }
#endif

    if (fail) { printf("FAIL %d\n", fail); return 1; }
    printf("select/poll ok\n");
    return 0;
}