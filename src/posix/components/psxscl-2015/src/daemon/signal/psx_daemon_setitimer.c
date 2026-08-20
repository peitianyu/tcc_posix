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
#include "psx_time.h"
#include "psx_timer.h"
#include "psx.h"

int32_t __stdcall __psx_daemon_setitimer(struct __timer_msg * msg)
{
	struct __timer *		timer;
	struct __timer_ctx		ctx;
	nt_timer_basic_information	ti;
	size_t				len;

	if ((msg->timertype < 0) || (msg->timertype >= __PSX_ITIMER_CAP))
		return NT_STATUS_INVALID_PARAMETER_1;

	timer = &msg->ctx->timer[msg->timertype];
	at_locked_inc(&timer->instance);

	if (msg->ctimer.it_interval.tv_sec) {
		if (timer->hthread) {
			__ntapi->zw_query_timer(
				timer->htimer,
				NT_TIMER_BASIC_INFORMATION,
				&ti,sizeof(ti),&len);

			__psx_time_convert_native_to_timeval(
				ti.timer_remaining,
				&msg->ctimer.it_value);
		} else {
			msg->ctimer.it_value.tv_sec	= 0;
			msg->ctimer.it_value.tv_usec	= 0;
		}

		msg->ctimer.it_interval.tv_sec	= timer->itimer.it_interval.tv_sec;
		msg->ctimer.it_interval.tv_usec	= timer->itimer.it_interval.tv_usec;
	}

	if (timer->hthread) {
		__ntapi->zw_queue_apc_thread(
			timer->hthread,
			__psx_terminate_internal_thread,
			0,(void *)NT_STATUS_CANCELLED,0);

		__ntapi->zw_cancel_timer(
			timer->htimer,
			&ti.signal_state);

		__ntapi->zw_close(timer->hthread);
	}

	ctx.tinstance	= timer->instance;
	ctx.dinstance	= &timer->instance;
	ctx.htimer	= timer->htimer;
	ctx.type	= msg->timertype;
	ctx.ctx		= msg->ctx;

	ctx.itimer.it_interval.tv_sec	= msg->itimer.it_interval.tv_sec;
	ctx.itimer.it_interval.tv_usec	= msg->itimer.it_interval.tv_usec;
	ctx.itimer.it_value.tv_sec	= msg->itimer.it_value.tv_sec;
	ctx.itimer.it_value.tv_usec	= msg->itimer.it_value.tv_usec;

	return __psx_create_internal_thread(
		&timer->hthread,
		__psx_timer_routine,
		&ctx,sizeof(ctx));
}
