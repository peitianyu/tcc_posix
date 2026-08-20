/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/*                                                      */
/*  __sys_mprotect: VirtualProtect (2015 pre-alpha had  */
/*  no mprotect; musl pthread guard pages need it)      */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include <ntapi/ntapi.h>
#include "psx_tlca.h"
#include "psx.h"

__psx_api
intptr_t __sys_mprotect(void * addr, size_t length, int prot)
{
	uint32_t	nprot;
	uint32_t	old;
	int32_t		status;

	nprot = 0;
	if (prot & PROT_READ)
		nprot = NT_PAGE_READONLY;
	if (prot & PROT_WRITE)
		nprot = NT_PAGE_READWRITE;
	if (prot & PROT_EXEC)
		nprot |= (nprot ? NT_PAGE_EXECUTE_READ : NT_PAGE_EXECUTE);
	if (!nprot)
		nprot = NT_PAGE_NOACCESS;

	status = __ntapi->zw_protect_virtual_memory(
		NT_CURRENT_PROCESS_HANDLE,
		&addr,
		&length,
		nprot,
		&old);

	return status ? -EINVAL : 0;
}
