/* 测试: stdio 格式化输出 */
#include <stdio.h>
#include <string.h>
int main(void) {
    char buf[256];
    int n = snprintf(buf, sizeof buf, "%d %s %c %x %o %ld %f",
        42, "str", 'A', 255, 8, 123456789L, 3.5);
    if (n != (int)strlen(buf)) return 1;
    if (strcmp(buf, "42 str A ff 10 123456789 3.500000")) return 2;
    /* 宽度/精度 */
    snprintf(buf, sizeof buf, "%5d|%-5d|%05d|%.2f", 7, 7, 7, 3.14159);
    if (strcmp(buf, "    7|7    |00007|3.14")) return 3;
    /* 大数 */
    snprintf(buf, sizeof buf, "%lld %llu", -9000000000LL, 18000000000ULL);
    if (strcmp(buf, "-9000000000 18000000000")) return 4;
    /* %s 空指针处理 */
    fputs("ok\n", stdout);
    return 0;
}
