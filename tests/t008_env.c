/* 测试: 进程环境 (getpid/getuid/getcwd/getenv/putenv) */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
int main(void) {
    /* pid */
    pid_t pid = getpid();
    if (pid <= 0) return 1;
    /* uid/gid */
    if (getuid() < 0) return 2;
    /* getcwd */
    char cwd[512];
    if (!getcwd(cwd, sizeof cwd)) return 3;
    if (cwd[0] != '/' && !(cwd[0] >= 'A' && cwd[0] <= 'Z')) return 4;
    /* chdir */
    char cwd2[512];
    if (chdir(".")) return 5;
    if (!getcwd(cwd2, sizeof cwd2)) return 6;
    if (strcmp(cwd, cwd2)) return 7;
    /* getenv */
    if (!getenv("PATH")) return 8;
    /* putenv + getenv */
    if (putenv("TCC_TEST_VAR=hello")) return 9;
    const char *v = getenv("TCC_TEST_VAR");
    if (!v || strcmp(v, "hello")) return 10;
    /* setenv 覆盖/追加 */
    if (setenv("TCC_TEST_VAR", "world", 0)) return 11;
    if (strcmp(getenv("TCC_TEST_VAR"), "hello")) return 12;  /* 不覆盖 */
    if (setenv("TCC_TEST_VAR", "world", 1)) return 13;
    if (strcmp(getenv("TCC_TEST_VAR"), "world")) return 14;
    if (unsetenv("TCC_TEST_VAR")) return 15;
    if (getenv("TCC_TEST_VAR")) return 16;
    /* getppid */
    if (getppid() < 0) return 17;
    /* sleep 精度 */
    struct timespec ts = {0, 50000000};  /* 50ms */
    if (nanosleep(&ts, NULL)) return 18;
    return 0;
}
