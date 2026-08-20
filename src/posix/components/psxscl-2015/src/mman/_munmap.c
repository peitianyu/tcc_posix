/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_tlca.h"
#include "psx_errno.h"
#include "psx_flags.h"
#include "psx_mman.h"
#include "psx_signal.h"
#include "psx.h"

__psx_api
intptr_t __sys_munmap(void * addr, size_t length)
{
	struct __psx_tlca *	tlca;
	intptr_t		ret;
	void *			faddr;
	size_t			flen;

	(void)length;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	/* 2015 pre-alpha bug: 匿名 mmap 走 VirtualAlloc, 但 munmap 用
	 * UnmapViewOfSection (只适用文件 section) -> 改 FreeVirtualMemory。 */
	faddr = addr;
	flen  = 0;
	tlca->ntstatus = __ntapi->zw_free_virtual_memory(
		NT_CURRENT_PROCESS_HANDLE,
		&faddr,
		&flen,
		NT_MEM_RELEASE);

	ret = tlca->ntstatus ? -EINVAL : 0;

	return __psx_sig_epilog(tlca,ret,tlca->ntstatus);
}
