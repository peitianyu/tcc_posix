/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_limits.h"
#include "psx_daemon.h"
#include "psx.h"

extern psx_daemon_routine * psx_daemon_vtbl[PSX_DAEMON_OPCODE_CAP - PSX_DAEMON_OPCODE_BASE];

int32_t __stdcall __psx_daemon_loop(void * context)
{
	struct __port_msg	inbuf;
	struct __port_msg	outbuf;

	struct __port_msg *	request;
	struct __port_msg *	reply;

	intptr_t		port_id;
	int32_t			opcode;

	/* init */
	request = &inbuf;
	__ntapi->tt_aligned_block_memset(
		request,0,sizeof(*request));

	/* get first message */
	__ntapi->zw_reply_wait_receive_port(
		hport_daemon,
		&port_id,
		(nt_port_message *)0,
		(nt_port_message *)request);

	/* message loop */
	do {
		switch (request->header.msg_type) {
			case NT_LPC_REQUEST:
			case NT_LPC_DATAGRAM:
				opcode = request->msginfo.opcode;
				break;

			case NT_LPC_CONNECTION_REQUEST:
				opcode = PSX_DAEMON_CONNECT;
				break;

			default:
				opcode = -1;
				break;
		}

		/* dispatch */
		reply = &outbuf;

		__ntapi->tt_aligned_block_memcpy(
			(uintptr_t *)reply,
			(uintptr_t *)request,
			sizeof(*reply));

		reply->header.msg_type = NT_LPC_REPLY;
		reply->msginfo.key = reply->header.msg_id;

		if ((opcode >= PSX_DAEMON_OPCODE_BASE) && (opcode < PSX_DAEMON_OPCODE_CAP)) {
			opcode -= PSX_DAEMON_OPCODE_BASE;
			reply->msginfo.status = psx_daemon_vtbl[opcode](reply);
		} else {
			reply->msginfo.key = -1;
			reply->msginfo.status = NT_STATUS_LPC_INVALID_CONNECTION_USAGE;
		}

		__ntapi->tt_aligned_block_memset(request,0,sizeof(*request));
		reply = reply->msginfo.key ? &outbuf : 0;

		if (!reply)
			__ntapi->zw_reply_wait_receive_port(
				hport_daemon,
				&port_id,0,
				&request->header);
		else if (reply->header.client_id.process_id == rtdata->cid_self.process_id)
			__ntapi->zw_reply_wait_receive_port(
				hport_daemon,
				&port_id,
				&reply->header,
				&request->header);
		else {
			__ntapi->zw_reply_port(
				hport_daemon,
				&reply->header);

			__ntapi->zw_reply_wait_receive_port(
				hport_daemon,
				&port_id,0,
				&request->header);
		}
	} while (request->header.msg_id);

	{
		nt_iosb iosb;

		__ntapi->zw_write_file(
			rtdata->hstderr,
			0,0,0,
			&iosb,"PSXSCL: DAEMON: INTERNAL ERROR",
			30,0,0);
	}

	return NT_STATUS_INTERNAL_ERROR;
}
