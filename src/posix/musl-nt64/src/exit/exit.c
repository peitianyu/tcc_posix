#include <stdlib.h>
#include <stdint.h>
#include "libc.h"

static void dummy()
{
}

/* atexit.c and __stdio_exit.c override these. the latter is linked
 * as a consequence of linking either __toread.c or __towrite.c.
 * tcc_posix: TCC weak resolution is first-definition-wins, so the
 * weak_alias here would stick to dummy() even after __stdio_exit.o
 * is pulled in.  Use strong refs so exit() always flushes stdio. */
weak_alias(dummy, __funcs_on_exit);
extern void __stdio_exit(void);

#ifndef SHARED
weak_alias(dummy, _fini);
extern void (*const __fini_array_start)() __attribute__((weak));
extern void (*const __fini_array_end)() __attribute__((weak));
#endif

_Noreturn void exit(int code)
{
	__funcs_on_exit();

#ifndef SHARED
	uintptr_t a = (uintptr_t)&__fini_array_end;
	for (; a>(uintptr_t)&__fini_array_start; a-=sizeof(void(*)()))
		(*(void (**)())(a-sizeof(void(*)())))();
	_fini();
#endif

	__stdio_exit();

	_Exit(code);
}
