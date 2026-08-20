/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_init.h"

int __psx_init_brk(void)
{
	int32_t status;

	brk_size = __PSX_PAGE_SIZE;
	brk_cap  = __PSX_PAGE_SIZE*1024*(sizeof(size_t)/4)*(sizeof(size_t)/4);

	if ((status = __ntapi->zw_allocate_virtual_memory(
			NT_CURRENT_PROCESS_HANDLE,
			(void **)&brk_base,0,
			&brk_cap,
			NT_MEM_RESERVE,
			NT_PAGE_READWRITE)))
		return status;

	return 	__ntapi->zw_allocate_virtual_memory(
			NT_CURRENT_PROCESS_HANDLE,
			(void **)&brk_base,0,
			(size_t *)&brk_size,
			NT_MEM_COMMIT,
			NT_PAGE_READWRITE);
}
