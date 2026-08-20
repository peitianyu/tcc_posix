/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_tty.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

int32_t __stdcall __ntapi_tty_client_session_set(
	__in	void *			hport,
	__in	nt_tty_session_info *	sessioninfo)
{
	int32_t			status;
	nt_tty_session_msg	msg;

	hport = hport ? hport :  __ntapi_internals()->hport_tty_session;

	__ntapi->tt_aligned_block_memset(
		&msg,0,sizeof(msg));

	msg.header.msg_type	= NT_LPC_NEW_MESSAGE;
	msg.header.data_size	= sizeof(msg.data);
	msg.header.msg_size	= sizeof(msg);
	msg.data.ttyinfo.opcode	= NT_TTY_CLIENT_SESSION_SET;

	msg.data.sessioninfo.pid	= sessioninfo->pid;
	msg.data.sessioninfo.pgid	= sessioninfo->pgid;
	msg.data.sessioninfo.sid	= sessioninfo->sid;
	msg.data.sessioninfo.reserved	= sessioninfo->reserved;

	if ((status = __ntapi->zw_request_wait_reply_port(hport,&msg,&msg)))
		return status;

	return msg.data.ttyinfo.status;
}
