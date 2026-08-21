/* 测试: R6 termios console 映射.
 * 1) 回归: 普通文件 fd 非 tty → isatty()=0, tcgetattr() 返回 ENOTTY (不崩溃).
 * 2) console std fd (0/1/2): 若 isatty() 为真 (真 console 附着), 则
 *    tcgetattr()/TIOCGWINSZ 必须成功 — 直接验证 __sys_ioctl console 分支.
 *    注: test.sh 捕获 stdout 时 1/2 为管道, 该分支仅在有真 console 时触发. */
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>

int main(void) {
    int fail = 0;

    /* 1) 普通文件非 tty (回归) */
    int fd = open("ioctl_probe.tmp", O_CREAT | O_WRONLY, 0600);
    if (fd < 0) { printf("FAIL: open errno=%d\n", errno); return 1; }
    struct termios t;
    errno = 0;
    if (isatty(fd) != 0) { printf("FAIL: isatty(file)=%d\n", isatty(fd)); fail++; }
    else                 { printf("  isatty(file)=0 ok\n"); }
    errno = 0;
    if (tcgetattr(fd, &t) == 0) { printf("FAIL: tcgetattr(file) succeeded\n"); fail++; }
    else if (errno != ENOTTY)   { printf("FAIL: tcgetattr(file) errno=%d want ENOTTY\n", errno); fail++; }
    else                        { printf("  tcgetattr(file)->ENOTTY ok\n"); }
    close(fd);
    unlink("ioctl_probe.tmp");

    /* 2) console std fd (自洽): isatty 真 则 tcgetattr/TIOCGWINSZ 必须通 */
    for (int i = 0; i < 3; i++) {
        if (isatty(i)) {
            printf("  fd%d isatty=1 (console)\n", i);
            errno = 0;
            if (tcgetattr(i, &t) != 0) { printf("FAIL: tcgetattr(fd%d) errno=%d\n", i, errno); fail++; }
            else printf("  fd%d tcgetattr ok c_lflag=%#x\n", i, (unsigned)t.c_lflag);
            struct winsize ws;
            memset(&ws, 0, sizeof(ws));
            errno = 0;
            if (ioctl(i, TIOCGWINSZ, &ws) != 0) { printf("FAIL: TIOCGWINSZ fd%d errno=%d\n", i, errno); fail++; }
            else printf("  fd%d winsize=%ux%u\n", i, ws.ws_row, ws.ws_col);
        } else {
            printf("  fd%d isatty=0 (pipe/redirected)\n", i);
        }
    }

    if (fail) { printf("FAIL %d\n", fail); return 1; }
    printf("termios console mapping ok\n");
    return 0;
}