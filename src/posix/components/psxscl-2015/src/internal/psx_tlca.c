/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include <psxscl/psxscl.h>
#include "psx_systypes.h"
#include "psx_init.h"
#include "psx_ldso.h"
#include "psx_tlca.h"
#include "psx_daemon.h"
#include "psx_impl.h"
#include "psx.h"

/* WORK IN PROGRESS */
struct __psx_tlca * __tlca_for_signal;

static int __psx_tlca_alloc(struct __psx_tlca ** ptlca)
{
	int32_t	status;
	size_t	tlca_size;

	*ptlca	  = (struct __psx_tlca *)0;
	tlca_size = __PSX_PAGE_SIZE;

	if ((status = __ntapi->zw_allocate_virtual_memory(
			NT_CURRENT_PROCESS_HANDLE,
			(void **)ptlca,
			0,&tlca_size,
			NT_MEM_COMMIT,
			NT_PAGE_READWRITE)))
		return status;

	__ntapi->tt_aligned_block_memset(
		*ptlca,0,tlca_size);

	(*ptlca)->tlca_addr = *ptlca;
	(*ptlca)->tlca_size = tlca_size;
	(*ptlca)->buflen    = tlca_size - (size_t)&((struct __psx_tlca *)0)->buffer;

	return NT_STATUS_SUCCESS;
}


static int __psx_tlca_stack_alloc(struct __psx_tlca * tlca)
{
	int32_t	status;
	size_t	stack_size;
	char *	stack_addr;

	stack_addr = 0;
	stack_size = (rtdata->stack_commit > 0x20000)
		? rtdata->stack_commit
		: 0x20000;

	if ((status = __ntapi->zw_allocate_virtual_memory(
			NT_CURRENT_PROCESS_HANDLE,
			(void **)&stack_addr,
			0,&stack_size,
			NT_MEM_COMMIT,
			NT_PAGE_READWRITE)))
		return status;

	__ntapi->tt_aligned_block_memset(
		stack_addr,0,stack_size);

	tlca->tib_posix.stack_limit = stack_addr;
	tlca->tib_posix.stack_base  = stack_addr + stack_size;

	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)tlca->tib_posix.stack_limit,
		(uintptr_t *)tlca->tib_system.stack_limit,
		(size_t)tlca->tib_system.stack_base - (size_t)tlca->tib_system.stack_limit);

	return NT_STATUS_SUCCESS;
}


int32_t __fastcall __attr_hidden__ __psx_tlca_init(struct __psx_tlca ** ptlca)
{
	int32_t			status;
	void *			addr;
	size_t			size;
	nt_tib *		tib;
	nt_cid			cid;
	nt_oa			oa = {sizeof(oa)};
	struct __psx_tlca *	tlca;

        /* tlca alloc */
        if ((status = __psx_tlca_alloc(ptlca)))
		return status;

	tlca = *ptlca;

        /* tlca stack alloc */
        if ((status = __psx_tlca_stack_alloc(tlca)))
		return status;

	/* user thread count */
	at_locked_inc(&pthreads);

	/* store tlca address */
	*(__tls_slot_addr()) = tlca;

	/* save system stack info */
	tib = (nt_tib *)pe_get_teb_address();
	tlca->tib_system.stack_base  = tib->stack_base;
	tlca->tib_system.stack_limit = tib->stack_limit;

	/* ntapi: cached flags and args */
	tlca->cfalert 		= NT_SYNC_ALERTABLE;
	tlca->cfnonalert		= NT_SYNC_NON_ALERTABLE;
	tlca->cfzerowait		= &tlca->zerowait;
	tlca->cfinfinity		= 0;

	/* context */
	tlca->ctx = &rtctx;
	cid.process_id = pe_get_current_process_id();
	cid.thread_id  = pe_get_current_thread_id();

	__ntapi->zw_open_thread(
		&tlca->hthread,
		NT_THREAD_ALL_ACCESS,
		&oa,&cid);



	/* WORK IN PROGRESS */
	__tlca_for_signal = tlca;

	/* first thread? */
	if (!tlca->entry_routine) {
		addr = (void *)((uintptr_t)tlca->tib_system.stack_base - rtdata->stack_reserve);
		size = (size_t)tlca->tib_system.stack_limit - (size_t)addr;

		return __ntapi->zw_allocate_virtual_memory(
				NT_CURRENT_PROCESS_HANDLE,
				&addr,0,&size,
				NT_MEM_COMMIT,
				NT_PAGE_READWRITE);
	}

	/* switch stacks and call entry routine */
	tib->stack_base  = tlca->tib_posix.stack_base;
	tib->stack_limit = tlca->tib_posix.stack_limit;
	__psx_tlca_prolog(tlca->entry_routine,tib->stack_limit);

	return NT_STATUS_SUCCESS;
}
