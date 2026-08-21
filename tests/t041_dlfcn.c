/* 测试: R10c dlfcn 补全. dlopen/dlsym/dlclose/dlerror/dladdr 全链路.
 * 覆盖:
 *  - dlopen(NULL)  -> 主程序模块句柄 (非 NULL)
 *  - dlopen("ws2_32.dll") -> 真加载已存在于进程的模块 (非 NULL)
 *  - dlsym(h,"socket")   -> 从导出表按名解析 (非 NULL)
 *  - dlclose(h)          -> 0
 *  - dlopen(不存在.dll)   -> NULL, 且 dlerror() 非 NULL
 *  - dlerror() 无错时     -> NULL
 *  - dladdr(&main)        -> 命中所属模块 (dli_fbase 非 NULL)
 */
#define _GNU_SOURCE 1  /* musl dlfcn.h 仅在 _GNU_SOURCE/_BSD_SOURCE 下暴露 Dl_info */
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

int main(void) {
    int fail = 0;
    void *h, *sym, *m;

    /* dlopen(NULL): 主程序 */
    m = dlopen(0, RTLD_LAZY);
    if (!m) { printf("  dlopen(0) -> NULL\n"); fail++; }
    else printf("  dlopen(0) ok\n");

    /* dlopen 一个真实已加载模块 */
    h = dlopen("ws2_32.dll", RTLD_LAZY);
    if (!h) { printf("  dlopen(ws2_32) -> NULL\n"); fail++; }
    else printf("  dlopen(ws2_32) ok\n");

    if (h) {
        /* dlsym: 导出表按名解析 */
        sym = dlsym(h, "socket");
        if (!sym) { printf("  dlsym(h,socket) -> NULL\n"); fail++; }
        else printf("  dlsym(h,socket) ok\n");

        /* dlerror: 无错时应为 NULL */
        if (dlerror() != NULL) { printf("  dlerror no-error -> non-NULL\n"); fail++; }
        else printf("  dlerror no-error ok\n");

        /* dlclose */
        if (dlclose(h) != 0) { printf("  dlclose -> !=0\n"); fail++; }
        else printf("  dlclose ok\n");
    }

    /* 不存在的模块 -> NULL + dlerror 非空 */
    h = dlopen("NoSuchDll_12345.dll", RTLD_LAZY);
    if (h) { printf("  dlopen(notexist) -> non-NULL (want NULL)\n"); fail++; }
    else {
        char *e = dlerror();
        if (!e) { printf("  dlerror after fail -> NULL (want msg)\n"); fail++; }
        else printf("  dlopen(notexist)->NULL, dlerror=\"%s\"\n", e);
    }

    /* dladdr: 命中本模块 */
    {
        Dl_info di;
        if (dladdr((void *)&main, &di)) {
            if (!di.dli_fbase) { printf("  dladdr dli_fbase NULL\n"); fail++; }
            else printf("  dladdr &main dli_fbase=%p ok\n", di.dli_fbase);
        } else { printf("  dladdr(&main) -> 0\n"); fail++; }
    }

    if (fail) { printf("FAIL %d\n", fail); return 1; }
    printf("dlfcn ok\n");
    return 0;
}