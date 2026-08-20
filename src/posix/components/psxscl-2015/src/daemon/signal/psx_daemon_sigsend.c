/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_daemon.h"
#include "psx_tlca.h"
#include "psx_signal.h"
#include "psx.h"

/* WORK IN PROGRESS */
extern struct __psx_tlca * __tlca_for_signal;

int32_t __stdcall __psx_daemon_sigsend(struct __sig_msg * msg)
{
	struct __timer_msg *	tmsg;
	struct __timer *	timer;

	if (msg->sigtype == PSX_SIGALRM) {
		tmsg  = (struct __timer_msg *)msg;
		timer = &tmsg->ctx->timer[tmsg->timertype];

		if (tmsg->tinstance != timer->instance)
			return NT_STATUS_CANCELLED;
	}

	msg->tlca = __tlca_for_signal;
	msg->msginfo.key = 0;
	return __psx_daemon_sigdeliver(msg);
}
