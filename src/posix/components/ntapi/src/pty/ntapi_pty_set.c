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

int32_t	__stdcall __ntapi_pty_set(
	nt_pty *		pty,
	nt_io_status_block *	iosb,
	void *			pty_info,
	uint32_t		pty_info_length,
	nt_pty_info_class	pty_info_class)
{
	int32_t			status;
	nt_pty_sigctl_msg	msg;
	uintptr_t *		info;

	if ((pty_info_class<NT_PTY_BASIC_INFORMATION) || (pty_info_class>=NT_PTY_INFORMATION_CAP))
		return NT_STATUS_INVALID_INFO_CLASS;
	else if (pty_info_class == NT_PTY_BASIC_INFORMATION)
		return NT_STATUS_NOT_IMPLEMENTED;
	else if ((pty_info_class == NT_PTY_CLIENT_INFORMATION) && (pty_info_length != sizeof(nt_pty_client_info)))
		return NT_STATUS_INVALID_PARAMETER;

	__ntapi->tt_aligned_block_memset(
		&msg,0,sizeof(msg));

	msg.header.msg_type		= NT_LPC_NEW_MESSAGE;
	msg.header.data_size		= sizeof(msg.data);
	msg.header.msg_size		= sizeof(msg);
	msg.data.ttyinfo.opcode		= NT_TTY_PTY_SET;

	msg.data.ctlinfo.hpty		= pty->hpty;
	msg.data.ctlinfo.luid.high	= pty->luid.high;
	msg.data.ctlinfo.luid.low	= pty->luid.low;
	msg.data.ctlinfo.ctlcode	= pty_info_class;

	__ntapi->tt_guid_copy(
		&msg.data.ctlinfo.guid,
		&pty->guid);

	info = (uintptr_t *)pty_info;
	msg.data.ctlinfo.ctxarg[0] = info[0];
	msg.data.ctlinfo.ctxarg[1] = info[1];
	msg.data.ctlinfo.ctxarg[2] = info[2];
	msg.data.ctlinfo.ctxarg[3] = info[3];

	if ((status = __ntapi->zw_request_wait_reply_port(pty->hport,&msg,&msg)))
		return status;
	else if (msg.data.ttyinfo.status)
		return msg.data.ttyinfo.status;

	iosb->info   = msg.data.ctlinfo.iosb.info;
	iosb->status = msg.data.ctlinfo.iosb.status;

	return NT_STATUS_SUCCESS;
}
