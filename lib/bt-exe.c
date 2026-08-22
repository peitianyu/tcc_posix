/* ------------------------------------------------------------- */
/* for linking rt_printline and the signal/exception handler
   from tccrun.c into executables. */

#define CONFIG_TCC_BACKTRACE_ONLY
#define ONE_SOURCE 1
#define pstrcpy tcc_pstrcpy
#include "../src/tccrun.c"

#ifndef _WIN32
# define __declspec(n)
#endif

#ifdef _WIN64
static void bt_init_pe_prog_base(rt_context *p)
{
#if defined(CONFIG_TCC_MUSL)
    /* pure musl: no winapi VirtualQuery. dlopen(NULL) returns the main module
       handle == its imagebase (psxscl: pe_get_first_module_handle). */
    void *base;

    if (!p->prog_base)
        return;
    base = dlopen(NULL, RTLD_LAZY);
    if (!base)
        return;
    {
        addr_t imagebase = (addr_t)base - p->prog_base;
        p->prog_base = (addr_t)base - (imagebase & 0xffffffffu);
    }
#else
    MEMORY_BASIC_INFORMATION mbi;
    addr_t imagebase;

    if (!p->prog_base)
        return;
    if (!VirtualQuery(p, &mbi, sizeof(mbi)) || !mbi.AllocationBase)
        return;
    imagebase = (addr_t)mbi.AllocationBase - p->prog_base;
    p->prog_base = (addr_t)mbi.AllocationBase - (imagebase & 0xffffffffu);
#endif
}
#endif

__declspec(dllexport)
void __bt_init(rt_context *p, int is_exe)
{
    __attribute__((weak)) int main();
    __attribute__((weak)) void __bound_init(void*, int);

    //fprintf(stderr, "__bt_init %d %p %p %p\n", is_exe, p, p->stab_sym, p->bounds_start), fflush(stderr);

    /* call __bound_init here due to redirection of sigaction */
    /* needed to add global symbols */
    if (p->bounds_start)
	__bound_init(p->bounds_start, -1);

#ifdef _WIN64
    bt_init_pe_prog_base(p);
#endif

    /* add to chain */
    rt_wait_sem();
    p->next = g_rc, g_rc = p;
    rt_post_sem();
    if (is_exe) {
        /* we are the executable (not a dll) */
        p->top_func = main;
        set_exception_handler();
    }
}

__declspec(dllexport)
void __bt_exit(rt_context *p)
{
    struct rt_context *rc, **pp;
    __attribute__((weak)) void __bound_exit_dll(void*);

    //fprintf(stderr, "__bt_exit %d %p\n", !!p->top_func, p);

    if (p->bounds_start)
	__bound_exit_dll(p->bounds_start);

    /* remove from chain */
    rt_wait_sem();
    for (pp = &g_rc; rc = *pp, rc; pp = &rc->next)
        if (rc == p) {
            *pp = rc->next;
            break;
        }
    rt_post_sem();
}

/* copy a string and truncate it. */
ST_FUNC char *pstrcpy(char *buf, size_t buf_size, const char *s)
{
    int l = strlen(s);
    if (l >= buf_size)
        l = buf_size - 1;
    memcpy(buf, s, l);
    buf[l] = 0;
    return buf;
}

/* Resolve an absolute code address to "func@file:line" for the bcheck
   memtrack heap reports (weak-linked from bcheck.o).  Scans the registered
   rt_context chain (tccrun.c is compiled into this translation unit, so its
   static g_rc/rt_printline are visible).  Returns 0 on success, -1 otherwise
   (the caller then falls back to printing the raw address). */
__declspec(dllexport)
int __bt_resolve_addr(unsigned long long pc, char *buf, unsigned long len)
{
    rt_context *rc;
    bt_info bi;

    rt_wait_sem();
    for (rc = g_rc; rc; rc = rc->next) {
        bi.file[0] = bi.func[0] = '\0';
        bi.line = 0;
        bi.func_pc = 0;
        if (rt_printline(rc, (addr_t) pc, &bi) && bi.func[0]) {
            snprintf(buf, len, "%s@%s:%d", bi.func, bi.file, bi.line);
            rt_post_sem();
            return 0;
        }
    }
    rt_post_sem();
    snprintf(buf, len, "0x%llx", pc);
    return -1;
}
