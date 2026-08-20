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


static int32_t __stdcall __ntapi_vms_point_attach_detach(
	__in	void *			hvms,
	__in	nt_vms_point_info *	point_info,
	__in	int32_t			vms_opcode)
{
	int32_t			status;
	nt_vms_daemon_msg	msg;

	/* msg */
	__ntapi->tt_aligned_block_memset(&msg,0,sizeof(msg));

	msg.header.msg_type	= NT_LPC_NEW_MESSAGE;
	msg.header.data_size	= sizeof(msg.data);
	msg.header.msg_size	= sizeof(msg);
	msg.data.msginfo.opcode	= vms_opcode;

	/* copy point to msg */
	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)&(msg.data.pointinfo),
		(uintptr_t *)point_info,
		sizeof(*point_info));

	/* zw_request_wait_reply_port */
	status = __ntapi->zw_request_wait_reply_port(hvms,&msg,&msg);

	/* return vms status */
	return status ? status : msg.data.msginfo.status;
}


int32_t __stdcall __ntapi_vms_point_attach(
	__in	void *			hvms,
	__in	nt_vms_point_info *	point_info)
{
	return __ntapi_vms_point_attach_detach(
		hvms,
		point_info,
		NT_VMS_POINT_ATTACH);
}
