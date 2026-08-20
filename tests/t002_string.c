/* 测试: 字符串函数 */
#include <string.h>
#include <stdio.h>
int main(void) {
    char a[64], b[64];
    /* memcpy/memmove 重叠 */
    memset(b, 'A', 64);
    memcpy(a, b, 64);
    if (memcmp(a, b, 64)) return 1;
    strcpy(a, "0123456789");
    memmove(a + 3, a, 6);          /* 重叠前移: 0120123459 */
    if (strcmp(a, "0120123459")) return 2;
    memmove(a, a + 3, 6);          /* 重叠后移: 0123453459 */
    if (strcmp(a, "0123453459")) return 3;
    /* strtok_r 线程安全 (musl: 连续分隔符跳过, 无空 token) */
    char s[] = "a,b,c,,d";
    char *save = 0, *tok;
    int n = 0;
    for (tok = strtok_r(s, ",", &save); tok; tok = strtok_r(0, ",", &save))
        n++;
    if (n != 4) return 4;
    /* strcasestr / strerror_r */
    if (!strcasestr("Hello World", "WORLD")) return 5;
    char errbuf[64];
    if (strerror_r(2, errbuf, sizeof errbuf)) return 6;
    if (!errbuf[0]) return 7;
    /* strdup */
    char *dup = strdup("copy me");
    if (!dup || strcmp(dup, "copy me")) return 8;
    free(dup);
    /* 边界: 空串 */
    if (strlen("")) return 9;
    return 0;
}
