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

static int32_t __stdcall __ntapi_pty_read_write(
	nt_pty *		pty,
	void *			hevent,
	nt_io_apc_routine *	apc_routine,
	void *			apc_context,
	nt_iosb *		iosb,
	void *			buffer,
	size_t			nbytes,
	nt_large_integer *	offset,
	uint32_t *		key,
	int32_t			opcode)
{
	int32_t		status;
	nt_pty_io_msg	msg;
	off_t		soffset;
	int		mode;

	mode = opcode - NT_TTY_PTY_READ;
	soffset = mode * pty->section_size / 2;

	if (offset && offset->quad)
		return NT_STATUS_INVALID_PARAMETER;

	else if (__ntapi->tt_sync_block_lock(&pty->sync[mode],1,0,0))
		return NT_STATUS_RESOURCE_NOT_OWNED;

	nbytes = nbytes <= pty->section_size / 2
		? nbytes
		: pty->section_size / 2;

	__ntapi->tt_aligned_block_memset(
		&msg,0,sizeof(msg));

	msg.header.msg_type		= NT_LPC_NEW_MESSAGE;
	msg.header.data_size		= sizeof(msg.data);
	msg.header.msg_size		= sizeof(msg);
	msg.data.ttyinfo.opcode		= opcode;

	msg.data.ioinfo.hpty		= pty->hpty;
	msg.data.ioinfo.hevent		= hevent;
	msg.data.ioinfo.apc_routine	= apc_routine;
	msg.data.ioinfo.apc_context	= apc_context;
	msg.data.ioinfo.key		= key ? *key : 0;

	msg.data.ioinfo.luid.high	= pty->luid.high;
	msg.data.ioinfo.luid.low	= pty->luid.low;

	msg.data.ioinfo.riosb		= iosb;
	msg.data.ioinfo.raddr		= buffer;

	__ntapi->tt_guid_copy(
		&msg.data.ioinfo.guid,
		&pty->guid);

	msg.data.ioinfo.nbytes		= nbytes;
	msg.data.ioinfo.offset		= soffset;

	if (mode == __PTY_WRITE)
		__ntapi->tt_generic_memcpy(
			(char *)pty->section_addr + soffset,
			(char *)buffer,
			nbytes);

	if ((status = __ntapi->zw_request_wait_reply_port(pty->hport,&msg,&msg)))
		return status;
	else if (msg.data.ttyinfo.status)
		return msg.data.ttyinfo.status;

	if (mode == __PTY_READ)
		__ntapi->tt_generic_memcpy(
			(char *)buffer,
			(char *)pty->section_addr + soffset,
			msg.data.ioinfo.iosb.info);

	iosb->info   = msg.data.ioinfo.iosb.info;
	iosb->status = msg.data.ioinfo.iosb.status;

	return NT_STATUS_SUCCESS;
}


int32_t	__stdcall __ntapi_pty_read(
	__in	nt_pty *		pty,
	__in	void *			hevent		__optional,
	__in	nt_io_apc_routine *	apc_routine	__optional,
	__in	void *			apc_context	__optional,
	__out	nt_iosb *		iosb,
	__out	void *			buffer,
	__in	uint32_t		nbytes,
	__in	nt_large_integer *	offset		__optional,
	__in	uint32_t *		key		__optional)
{
	return __ntapi_pty_read_write(
		pty,
		hevent,apc_routine,apc_context,
		iosb,buffer,nbytes,offset,key,
		NT_TTY_PTY_READ);
}


int32_t __stdcall __ntapi_pty_write(
	__in	nt_pty *		pty,
	__in 	void *			hevent		__optional,
	__in	nt_io_apc_routine *	apc_routine	__optional,
	__in	void * 			apc_context	__optional,
	__out	nt_iosb *		iosb,
	__in	void * 			buffer,
	__in	uint32_t		nbytes,
	__in	nt_large_integer *	offset		__optional,
	__in	uint32_t *		key		__optional)
{
	return __ntapi_pty_read_write(
		pty,
		hevent,apc_routine,apc_context,
		iosb,buffer,nbytes,offset,key,
		NT_TTY_PTY_WRITE);
}
