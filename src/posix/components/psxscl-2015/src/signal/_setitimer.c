/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_syscalls.h"
#include "psx_helper.h"
#include "psx_errno.h"
#include "psx_daemon.h"
#include "psx_timer.h"
#include "psx.h"

__psx_api
intptr_t __sys_getitimer(enum __psx_timer_type which, struct itimerval *curr_value){return 0;}

__psx_api
intptr_t __sys_setitimer(enum __psx_timer_type which, const struct itimerval * new_value, struct itimerval * old_value)
{
	int32_t			status;
	struct __timer_msg	msg;
	struct __psx_tlca *	tlca;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if ((which < 0) || (which >= __PSX_ITIMER_CAP))
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);
	else if (!new_value)
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	__ntapi->tt_aligned_block_memset(
		&msg.header,0,sizeof(msg.header));

	msg.header.msg_type	= NT_LPC_NEW_MESSAGE;
	msg.header.data_size	= sizeof(msg) - sizeof(msg.header);
	msg.header.msg_size	= sizeof(msg);
	msg.msginfo.opcode	= PSX_DAEMON_SIGSETIMTER;

	msg.tlca		= tlca;
	msg.ucontext		= 0;
	msg.sigtype		= PSX_SIGARG;
	msg.timertype		= which;
	msg.ctx			= tlca->ctx;

	msg.itimer.it_interval.tv_sec	= new_value->it_interval.tv_sec;
	msg.itimer.it_interval.tv_usec	= new_value->it_interval.tv_usec;
	msg.itimer.it_value.tv_sec	= new_value->it_value.tv_sec;
	msg.itimer.it_value.tv_usec	= new_value->it_value.tv_usec;
	msg.ctimer.it_interval.tv_sec	= !!old_value;

	/* zw_request_wait_reply_port */
	if ((status = __ntapi->zw_request_wait_reply_port(hport_internal_client,&msg,&msg)))
		return __psx_sig_epilog(tlca,-ENOSYS,EPSXONLY);
	else if (msg.msginfo.status)
		return __psx_sig_epilog(tlca,-ENOMEM,status);

	if (old_value) {
		old_value->it_interval.tv_sec	= msg.ctimer.it_interval.tv_sec;
		old_value->it_interval.tv_usec	= msg.ctimer.it_interval.tv_usec;
		old_value->it_value.tv_sec	= msg.ctimer.it_value.tv_sec;
		old_value->it_value.tv_usec	= msg.ctimer.it_value.tv_usec;
	}

	return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
}
