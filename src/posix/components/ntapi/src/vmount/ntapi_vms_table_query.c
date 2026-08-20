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


int32_t __stdcall __ntapi_vms_table_query(
	__in	void *			hvms,
	__in	nt_vms_daemon_info *	vms_info)
{
	int32_t			status;
	nt_vms_daemon_msg	msg;

	/* msg */
	__ntapi->tt_aligned_block_memset(&msg,0,sizeof(msg));

	msg.header.msg_type	= NT_LPC_NEW_MESSAGE;
	msg.header.data_size	= sizeof(msg.data);
	msg.header.msg_size	= sizeof(msg);
	msg.data.msginfo.opcode	= NT_VMS_TABLE_QUERY;

	/* zw_request_wait_reply_port */
	status = __ntapi->zw_request_wait_reply_port(
		hvms,
		&msg,
		&msg);

	if (status) return status;

	/* return info */
	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)vms_info,
		(uintptr_t *)&(msg.data.vmsinfo),
		sizeof(*vms_info));

	/* return vms status */
	return status ? status : msg.data.msginfo.status;
}
