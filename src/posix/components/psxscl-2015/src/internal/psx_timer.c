/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx_time.h"
#include "psx_timer.h"
#include "psx_impl.h"
#include "psx.h"

static void __stdcall __psx_timer_apc(
	void *		actx,
	uint32_t	low,
	uint32_t	high)
{
	struct __timer_msg	msg;
	struct __timer_ctx *	ctx;

	ctx = (struct __timer_ctx *)actx;

	if (ctx->tinstance != *ctx->dinstance)
		return;

	__ntapi->tt_aligned_block_memset(
		&msg.header,0,sizeof(msg.header));

	msg.header.msg_type	= NT_LPC_NEW_MESSAGE;
	msg.header.data_size	= sizeof(msg) - sizeof(msg.header);
	msg.header.msg_size	= sizeof(msg);
	msg.msginfo.opcode	= PSX_DAEMON_SIGSEND;

	msg.tlca		= 0;
	msg.ucontext		= 0;
	msg.sigtype		= PSX_SIGALRM;

	msg.timertype		= ctx->type;
	msg.htimer		= ctx->htimer;
	msg.tinstance		= ctx->tinstance;
	msg.ctx			= ctx->ctx;

	__ntapi->zw_request_port(
		hport_internal_client,&msg);
}

int32_t __stdcall __psx_timer_routine(struct __timer_ctx * ctx)
{
	nt_timeout	timeout;
	nt_itimerval	nttval;
	int32_t		state;
	int32_t		alert = 1;

	if (!(__psx_is_timer_active(&ctx->itimer)))
		return NT_STATUS_CANCELLED;

	__psx_time_convert_itimerval_to_native(
		ctx->itimer,
		&nttval);

	timeout.quad = nttval.value.quad
		? nttval.value.quad
		: nttval.interval.quad;

	while (timeout.quad) {
		if (ctx->tinstance == at_locked_cas(ctx->dinstance,ctx->tinstance,ctx->tinstance-1)) {
			__ntapi->zw_set_timer(
				ctx->htimer,&timeout,__psx_timer_apc,
				ctx,0,0,&state);
			at_locked_inc(ctx->dinstance);
		} else
			return NT_STATUS_SUCCESS;

		__ntapi->zw_wait_for_single_object(
			ctx->htimer,
			NT_SYNC_ALERTABLE,0);

		__ntapi->zw_delay_execution(
			&alert,&timeout);

		nttval.value.quad = 0;

		timeout.quad = nttval.value.quad
			? nttval.value.quad
			: nttval.interval.quad;
	}

	return NT_STATUS_SUCCESS;
}
