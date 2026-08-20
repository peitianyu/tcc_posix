/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_tty.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

int32_t __stdcall __ntapi_tty_request_peer(
	__in	void *		hport,
	__in	int32_t		opcode,
	__in	uint32_t	flags,
	__in	const nt_guid *	service,
	__in	nt_port_attr *	peer)
{
	int32_t		status;
	nt_tty_peer_msg	msg;

	__ntapi->tt_aligned_block_memset(
		&msg,0,sizeof(msg));

	msg.header.msg_type	= NT_LPC_NEW_MESSAGE;
	msg.header.data_size	= sizeof(msg.data);
	msg.header.msg_size	= sizeof(msg);
	msg.data.ttyinfo.opcode	= NT_TTY_REQUEST_PEER;

	msg.data.peerinfo.opcode= opcode;
	msg.data.peerinfo.flags = flags;

	if (service) __ntapi->tt_guid_copy(
		&msg.data.peerinfo.service,
		service);

	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)&msg.data.peerinfo.peer,
		(uintptr_t *)peer,
		sizeof(*peer));

	if ((status = __ntapi->zw_request_wait_reply_port(hport,&msg,&msg)))
		return status;

	return msg.data.ttyinfo.status;
}
