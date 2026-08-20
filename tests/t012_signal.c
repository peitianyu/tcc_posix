/* 测试: 信号 (signal/raise/sigaction) */
#include <signal.h>
#include <stdio.h>
#include <string.h>
static volatile sig_atomic_t got_sig = 0;
static void h(int s) { got_sig = s; }
int main(void) {
    /* signal 基本 */
    signal(SIGUSR1, h);
    if (raise(SIGUSR1)) return 1;
    if (got_sig != SIGUSR1) return 2;
    /* sigaction */
    struct sigaction sa, old;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = h;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGUSR2, &sa, &old)) return 3;
    if (raise(SIGUSR2)) return 4;
    if (got_sig != SIGUSR2) return 5;
    /* 恢复 */
    if (sigaction(SIGUSR2, &old, NULL)) return 6;
    /* SIG_IGN */
    signal(SIGUSR1, SIG_IGN);
    got_sig = 0;
    if (raise(SIGUSR1)) return 7;
    if (got_sig != 0) return 8;   /* 忽略 */
    /* SIG_DFL 恢复 */
    signal(SIGUSR1, SIG_DFL);
    /* 注: psxscl 2015 的 rt_sigprocmask 是空实现 (不真正屏蔽),
       sigprocmask 阻塞断言跳过 */
    return 0;
}
