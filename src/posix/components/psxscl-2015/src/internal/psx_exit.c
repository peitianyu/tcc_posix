/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_tlca.h"
#include "psx_debug.h"

int __psx_exit_count = 0;

/* tcc_posix: 清 ctid 后唤醒 join 等待者 (futex WAKE) */
extern intptr_t __sys_futex(int *, int, int, void *, void *, int);
#define __PSX_FUTEX_WAKE	1

int32_t __psx_exit(int status)
{
	__psx_exit_count++;
	struct __psx_tlca *	tlca;
	struct __port_msg	msg;
	nt_tib *		tib;

	/* block signals */
	tlca = __tlca_self();
	at_store_32(&tlca->sig_tlock,-1);

	/* pthread join: clear child_tid (musl CLONE_CHILD_CLEARTID)
	   + futex wake (WaitOnAddress 阻塞的 join 必须显式唤醒) */
	if (tlca->pthread_clear_child_tid) {
		at_store_32(tlca->pthread_clear_child_tid,0);
		__sys_futex(tlca->pthread_clear_child_tid,
			__PSX_FUTEX_WAKE, 1, 0, 0, 0);
	}

	while (tlca->wait_count);

	/* restore system stack info */
	tib = (nt_tib *)pe_get_teb_address();
	tib->stack_base  = tlca->tib_system.stack_base;
	tib->stack_limit = tlca->tib_system.stack_limit;

	/* exit status */
	tlca->ntstatus = NT_STATUS_SUCCESS;
	tlca->exitcode = status;
	*(int32_t *)tib->stack_limit = status;

	/* notify daemon */
	__ntapi->tt_aligned_block_memset(
		&msg.header,0,sizeof(msg.header));

	msg.header.msg_type	= NT_LPC_NEW_MESSAGE;
	msg.header.data_size	= sizeof(msg) - sizeof(msg.header);
	msg.header.msg_size	= sizeof(msg);
	msg.msginfo.opcode	= PSX_DAEMON_THREADEXIT;
	msg.tlca		= tlca;

	__ntapi->zw_request_port(
		hport_internal_client,&msg);

	/* exit */
	__psx_tlca_epilog(
		tib->stack_limit,
		__ntapi->zw_terminate_thread);

	return NT_STATUS_INTERNAL_ERROR;
}
