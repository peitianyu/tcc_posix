/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_init.h"
#include "psx_mman.h"

int __psx_init_mman(void)
{
	return dalist_init_ex(
		&rtctx.sections,
		sizeof(struct __mmap_ctx),
		__PSX_VIRTUAL_PAGE_SIZE,
		__ntapi->zw_allocate_virtual_memory,
		DALIST_MEMFN_NT_ALLOCATE_VIRTUAL_MEMORY);
}
