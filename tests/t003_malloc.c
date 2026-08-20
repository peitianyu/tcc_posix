/* 测试: 内存管理 (malloc/realloc/calloc/free 压力) */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    /* 基本分配 */
    char *p = malloc(100);
    if (!p) return 1;
    memset(p, 0xAB, 100);
    if ((unsigned char)p[0] != 0xAB) return 2;
    /* realloc 扩大保持内容 */
    char *q = malloc(10);
    strcpy(q, "0123456789");
    q = realloc(q, 1000);
    if (!q) return 3;
    if (strcmp(q, "0123456789")) return 4;
    /* calloc 清零 */
    int *z = calloc(100, sizeof(int));
    if (!z) return 5;
    for (int i = 0; i < 100; i++) if (z[i]) return 6;
    /* 大量小块 (堆压力) */
    void *ptrs[1000];
    for (int i = 0; i < 1000; i++) {
        ptrs[i] = malloc((i % 64) + 1);
        if (!ptrs[i]) return 7;
        memset(ptrs[i], i & 0xFF, (i % 64) + 1);
    }
    for (int i = 999; i >= 0; i--) free(ptrs[i]);
    /* 大块 */
    void *big = malloc(10 * 1024 * 1024);
    if (!big) return 8;
    memset(big, 0, 10 * 1024 * 1024);
    free(big);
    /* 释放后重用 */
    free(p);
    p = malloc(100);
    if (!p) return 9;
    free(p); free(q); free(z);
    return 0;
}
