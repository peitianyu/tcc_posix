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
	struct __mmap_ctx *	mapinfo;
	intptr_t		ret;
	void *			faddr;
	size_t			flen;

	(void)length;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	/* tcc_posix: 区分匿名 (VirtualAlloc) 与文件映射 (MapViewOfSection).
	   文件映射必须用 UnmapViewOfSection, 匿名用 FreeVirtualMemory. */
	mapinfo = __psx_section_get(tlca->ctx, addr, 0);

	if (mapinfo && mapinfo->hsection) {
		/* 文件映射: UnmapViewOfSection */
		tlca->ntstatus = __ntapi->zw_unmap_view_of_section(
			NT_CURRENT_PROCESS_HANDLE,
			addr);
		ret = tlca->ntstatus ? -EINVAL : 0;
	} else {
		/* 匿名: FreeVirtualMemory */
		faddr = addr;
		flen  = 0;
		tlca->ntstatus = __ntapi->zw_free_virtual_memory(
			NT_CURRENT_PROCESS_HANDLE,
			&faddr,
			&flen,
			NT_MEM_RELEASE);
		ret = tlca->ntstatus ? -EINVAL : 0;
	}

	return __psx_sig_epilog(tlca,ret,tlca->ntstatus);
}
