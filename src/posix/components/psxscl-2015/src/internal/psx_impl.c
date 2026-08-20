/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_signal.h"
#include "psx_impl.h"


struct __psx_state	__psx = {0};
uintptr_t *		__sysvtbl[__PSX_SYSCALLS] = {0};
sigafn_t		__sigvtbl[64] = {0};



#ifdef NTAPI_STATIC
extern ntapi_vtbl ___ntapi;
#else
__attribute__((weak)) ntapi_vtbl ___ntapi = {0};
#endif



#ifdef PSXSCL_BUILD
int __stdcall __psx_entry(void * hinstance, uint32_t reason, void * reserved)
{
	return 1;
}
#endif
