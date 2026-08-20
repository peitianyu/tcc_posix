/* ------------------------------------------------------------- */
/* tcc_posix -run support: full musl startup                        */
/*                                                               */
/* tcc_run() enters at _runmain.  The normal PE path goes         */
/* crt_crt1.o:_start -> __libc_entry_routine (psx_init + vtbl +   */
/* __libc_start_main).  We forward to that same routine so -run   */
/* gets the complete musl libc (malloc/stdio/threads/write...).   */
/*                                                               */
/* No exit/atexit/etc. here: musl provides them.  Defining them   */
/* here would clash with musl's own (__stdio_exit pulls in exit). */
/* ------------------------------------------------------------- */

int main(int, char**, char**);

extern void __libc_entry_routine(int (*main)(int,char**,char**),
                                 void *psx_init, int options);

/* Force __stdio_exit.o to be pulled from libc.a: it defines the
   strong __stdio_exit that musl's exit() calls to flush stdio. */
extern void __stdio_exit_needed(void);
static void __force_stdio_exit(void) { __stdio_exit_needed(); }

int _runmain(int argc, char **argv, char **envp)
{
    (void)argc; (void)argv; (void)envp;
    extern void *__psx_init;
    __force_stdio_exit();
    /* __libc_entry_routine never returns (exit chain). */
    __libc_entry_routine(main, &__psx_init, 0);
    return 0; /* not reached */
}
