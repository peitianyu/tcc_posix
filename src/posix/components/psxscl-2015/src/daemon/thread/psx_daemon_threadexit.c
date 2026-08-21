/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_daemon.h"
#include "psx_tlca.h"
#include "psx.h"

int32_t __stdcall __psx_daemon_threadexit(struct __port_msg * msg)
{
	void *	addr;
	size_t	size;
	int	code;

	/* no reply */
	msg->msginfo.key = 0;

	/* futex wake: 唤醒 join 等待者 (musl CLONE_CHILD_CLEARTID) */
	{
		extern intptr_t __sys_futex(int *, int, int, void *, void *, int);
		if (msg->tlca->pthread_clear_child_tid)
			__sys_futex(msg->tlca->pthread_clear_child_tid,
				1 /*FUTEX_WAKE*/, 1, 0, 0, 0);
	}

	/* tlca free */
	addr = msg->tlca;
	size = msg->tlca->tlca_size;
	code = msg->tlca->exitcode;

	if (at_locked_xsub(&pthreads,1) == 1)
		return __ntapi->zw_terminate_process(
			NT_CURRENT_PROCESS_HANDLE,
			code);

	/* todo: update dtv, etc. */

	return __ntapi->zw_free_virtual_memory(
		rtdata->hprocess_self,
		&addr,&size,
		NT_MEM_RELEASE);
}
