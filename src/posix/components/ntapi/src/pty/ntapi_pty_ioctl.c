/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_port.h>
#include <ntapi/nt_tty.h>
#include <ntapi/nt_termios.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"
#include "ntapi_pty.h"

int32_t	__stdcall __ntapi_pty_ioctl(
	nt_pty *		pty,
	void *			hevent			__optional,
	nt_io_apc_routine *	apc_routine		__optional,
	void *			apc_context		__optional,
	nt_iosb *		iosb,
	uint32_t		ctlcode,
	void *			input_buffer		__optional,
	uint32_t		input_buffer_length,
	void *			output_buffer		__optional,
	uint32_t		output_buffer_length)
{
	int32_t			status;
	nt_pty_sigctl_msg	msg;
	nt_tty_sigctl_info *	input;
	nt_tty_sigctl_info *	output;

	if ((uintptr_t)input_buffer % sizeof(uintptr_t))
		return NT_STATUS_DATATYPE_MISALIGNMENT_ERROR;
	else if (input_buffer_length != sizeof(nt_tty_sigctl_info))
		return NT_STATUS_INVALID_BUFFER_SIZE;
	else if (!output_buffer)
		return NT_STATUS_ACCESS_DENIED;
	else if ((uintptr_t)output_buffer % sizeof(uintptr_t))
		return NT_STATUS_DATATYPE_MISALIGNMENT_ERROR;
	else if (output_buffer_length < sizeof(nt_tty_sigctl_info))
		return NT_STATUS_BUFFER_TOO_SMALL;

	input  = (nt_tty_sigctl_info *)input_buffer;
	output = (nt_tty_sigctl_info *)output_buffer;

	__ntapi->tt_aligned_block_memset(
		&msg,0,sizeof(msg));

	msg.header.msg_type		= NT_LPC_NEW_MESSAGE;
	msg.header.data_size		= sizeof(msg.data);
	msg.header.msg_size		= sizeof(msg);
	msg.data.ttyinfo.opcode		= NT_TTY_PTY_IOCTL;

	msg.data.ctlinfo.hpty		= pty->hpty;
	msg.data.ctlinfo.luid.high	= pty->luid.high;
	msg.data.ctlinfo.luid.low	= pty->luid.low;
	msg.data.ctlinfo.ctlcode	= ctlcode;

	__ntapi->tt_guid_copy(
		&msg.data.ctlinfo.guid,
		&pty->guid);

	msg.data.ctlinfo.ctxarg[0]	= input->ctxarg[0];
	msg.data.ctlinfo.ctxarg[1]	= input->ctxarg[1];
	msg.data.ctlinfo.ctxarg[2]	= input->ctxarg[2];
	msg.data.ctlinfo.ctxarg[3]	= input->ctxarg[3];

	__ntapi->tt_generic_memcpy(
		(char *)&input->terminfo,
		(char *)&msg.data.ctlinfo.terminfo,
		sizeof(input->terminfo));

	__ntapi->tt_generic_memcpy(
		(char *)&input->winsize,
		(char *)&msg.data.ctlinfo.winsize,
		sizeof(input->winsize));

	if ((status = __ntapi->zw_request_wait_reply_port(pty->hport,&msg,&msg)))
		return status;
	else if (msg.data.ttyinfo.status)
		return msg.data.ttyinfo.status;

	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)output,
		(uintptr_t *)&msg.data.ctlinfo,
		sizeof(*output));

	iosb->info   = msg.data.ctlinfo.iosb.info;
	iosb->status = msg.data.ctlinfo.iosb.status;

	return NT_STATUS_SUCCESS;
}
