/* 测试: 时间 (time/gettimeofday/clock/localtime/strftime) */
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    /* time 单调前进 */
    time_t t0 = time(NULL);
    if (t0 < 1500000000) return 1;   /* 2017+ */
    for (volatile int i = 0; i < 1000000; i++);
    time_t t1 = time(NULL);
    if (t1 < t0) return 2;
    /* gettimeofday */
    struct timeval tv;
    if (gettimeofday(&tv, NULL)) return 3;
    if (tv.tv_sec < 1500000000) return 4;
    if (tv.tv_usec < 0 || tv.tv_usec >= 1000000) return 5;
    /* clock 可用 */
    clock_t c = clock();
    if (c == (clock_t)-1) return 6;
    /* localtime/gmtime 往返 */
    struct tm tmv;
    time_t tt = 1700000000;
    if (!localtime_r(&tt, &tmv)) return 7;
    time_t back = mktime(&tmv);
    if (back != tt) return 8;
    if (!gmtime_r(&tt, &tmv)) return 9;
    /* strftime */
    char buf[128];
    if (!strftime(buf, sizeof buf, "%Y-%m-%d", &tmv)) return 10;
    if (strlen(buf) != 10 || buf[4] != '-') return 11;
    /* strptime */
    struct tm pt;
    memset(&pt, 0, sizeof pt);
    if (!strptime("2023-11-15", "%Y-%m-%d", &pt)) return 12;
    if (pt.tm_year != 123 || pt.tm_mon != 10 || pt.tm_mday != 15) return 13;
    /* difftime */
    if (difftime(t1, t0) < 0) return 14;
    return 0;
}
