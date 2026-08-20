/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

int32_t __stdcall __ntapi_tty_client_process_register(
	__in	void *			hport,
	__in	uintptr_t		process_id,
	__in	uintptr_t		thread_id,
	__in	uintptr_t		flags,
	__in	nt_large_integer *	reserved)
{
	nt_status		status;
	nt_tty_register_msg	msg;

	__ntapi->tt_aligned_block_memset(
		&msg,0,sizeof(msg));

	msg.header.msg_type	= NT_LPC_NEW_MESSAGE;
	msg.header.data_size	= sizeof(msg.data);
	msg.header.msg_size	= sizeof(msg);
	msg.data.ttyinfo.opcode	= NT_TTY_CLIENT_PROCESS_REGISTER;

	msg.data.reginfo.process_id 	= process_id;
	msg.data.reginfo.thread_id	= thread_id;
	msg.data.reginfo.flags		= flags;

	if ((status = __ntapi->zw_request_wait_reply_port(hport,&msg,&msg)))
		return status;

	return msg.data.ttyinfo.status;
}
