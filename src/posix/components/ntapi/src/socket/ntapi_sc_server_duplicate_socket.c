/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_object.h>
#include <ntapi/nt_file.h>
#include <ntapi/nt_socket.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

int32_t __cdecl __ntapi_sc_server_duplicate_socket(
	__in	nt_socket *		hssock_listen,
	__in	nt_socket *		hssock_dedicated,
	__in	nt_afd_accept_info *	accept_info,
	__out	nt_io_status_block *	iosb		__optional)
{
	nt_afd_duplicate_info	duplicate_info;
	nt_io_status_block	siosb;

	iosb = iosb ? iosb : &siosb;

	/* duplicate_info */
	duplicate_info.unknown  = 0;
	duplicate_info.sequence = accept_info->sequence;
	duplicate_info.hsocket_dedicated = hssock_dedicated->hsocket;

	hssock_dedicated->iostatus = __ntapi->zw_device_io_control_file(
			hssock_listen->hsocket,
			hssock_dedicated->hevent,
			0,
			0,
			iosb,
			NT_AFD_IOCTL_DUPLICATE,
			&duplicate_info,
			sizeof(duplicate_info),
			0,
			0);

	return hssock_dedicated->iostatus
		? __ntapi->sc_wait(hssock_dedicated,iosb,0)
		: NT_STATUS_SUCCESS;
}
