/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_port.h>
#include <ntapi/nt_tty.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"
#include "ntapi_pty.h"

int32_t	__stdcall __ntapi_pty_cancel(
	nt_pty *	pty,
	nt_iosb *	iosb)
{
	int32_t		status;
	nt_pty_io_msg	msg;

	__ntapi->tt_aligned_block_memset(
		&msg,0,sizeof(msg));

	msg.header.msg_type		= NT_LPC_NEW_MESSAGE;
	msg.header.data_size		= sizeof(msg.data);
	msg.header.msg_size		= sizeof(msg);
	msg.data.ttyinfo.opcode		= NT_TTY_PTY_CANCEL;

	msg.data.ioinfo.hpty		= pty->hpty;
	msg.data.ioinfo.luid.high	= pty->luid.high;
	msg.data.ioinfo.luid.low	= pty->luid.low;

	__ntapi->tt_guid_copy(
		&msg.data.ioinfo.guid,
		&pty->guid);

	if ((status = __ntapi->zw_request_wait_reply_port(pty->hport,&msg,&msg)))
		return status;
	else if (msg.data.ttyinfo.status)
		return msg.data.ttyinfo.status;

	iosb->info   = msg.data.ioinfo.iosb.info;
	iosb->status = msg.data.ioinfo.iosb.status;

	return NT_STATUS_SUCCESS;
}
