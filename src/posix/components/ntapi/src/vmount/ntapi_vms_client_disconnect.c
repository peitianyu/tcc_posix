/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_port.h>
#include <ntapi/nt_vmount.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"


int32_t __stdcall __ntapi_vms_client_disconnect(
	__in	void *	hvms)
{
	nt_vms_daemon_msg msg;

	if (!hvms) return NT_STATUS_INVALID_HANDLE;

	/* msg */
	__ntapi->tt_aligned_block_memset(&msg,0,sizeof(msg));

	msg.header.msg_type	= NT_LPC_NEW_MESSAGE;
	msg.header.data_size	= sizeof(msg.data);
	msg.header.msg_size	= sizeof(msg);
	msg.data.msginfo.opcode	= NT_VMS_CLIENT_DISCONNECT;

	/* zw_request_wait_reply_port */
	__ntapi->zw_request_wait_reply_port(
		hvms,
		&msg,
		&msg);

	/* close client handle */
	return __ntapi->zw_close(hvms);
}
