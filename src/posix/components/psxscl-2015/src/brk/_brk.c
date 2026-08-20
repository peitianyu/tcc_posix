/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx_impl.h"
#include "psx_tlca.h"
#include "psx_signal.h"
#include "psx.h"

__psx_api
intptr_t __sys_brk(uintptr_t brk)
{
	struct __psx_tlca *	tlca;
	uintptr_t		nbase;
	size_t			size;
	int32_t			status;

	/* first */
	if (!brk)
		return brk_base;
	else if (brk < brk_base)
		return -EINVAL;
	else if (brk >= (brk_base+brk_cap))
		return -ENOMEM;

	/* prolog */
	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	/* heap lock */
	if (__ntapi->tt_sync_block_lock(&heap_lock,1,0,0))
		return __psx_sig_epilog(tlca,-ENOMEM,EPSXONLY);

	nbase = brk_base + at_locked_xadd(&brk_size,0);

	/* fake */
	if (brk <= nbase) {
		__ntapi->tt_sync_block_unlock(&heap_lock);
		return __psx_sig_epilog(tlca,brk,NT_STATUS_SUCCESS);
	}

	/* alloc */
	size =  brk - nbase;

	if ((status = __ntapi->zw_allocate_virtual_memory(
			NT_CURRENT_PROCESS_HANDLE,
			(void **)&nbase,0,
			&size,
			NT_MEM_COMMIT,
			NT_PAGE_READWRITE))) {
		__ntapi->tt_sync_block_unlock(&heap_lock);
		return __psx_sig_epilog(tlca,-ENOMEM,status);
	}

	/* book-keeping */
	at_locked_add(&brk_size,size);
	__ntapi->tt_sync_block_unlock(&heap_lock);
	return __psx_sig_epilog(tlca,brk,status);
}

__psx_api
 intptr_t __sys_madvise(void * addr, size_t length, int advice)
{
	/* todo */
	__tlca_self()->ntstatus = NT_STATUS_SUCCESS;
	return 0;
}
