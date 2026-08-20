#include "psxglue.h"

unsigned long **	__syscall_vtbl	= 0;
struct __ldso_vtbl *	__ldso_vtbl	= 0;
struct __psx_vtbl *	__psx_vtbl	= 0;
unsigned long		__teb_sys_idx	= 0;
unsigned long		__teb_libc_idx	= 0;

/* __chkstk / ___chkstk_ms 由 chkstk.s 提供真实现 (tcc win64 大栈帧探测) */

