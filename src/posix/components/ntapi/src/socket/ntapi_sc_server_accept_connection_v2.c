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

typedef struct _nt_afd_server_accept_info {
	uint32_t	sequence;
	nt_sockaddr	addr;
} nt_afd_server_accept_info;

int32_t __cdecl __ntapi_sc_server_accept_connection_v2(
	__in	nt_socket *		hssocket,
	__out	nt_afd_accept_info *	accept_info,
	__out	nt_io_status_block *	iosb	__optional)
{
	nt_io_status_block siosb;

	iosb = iosb ? iosb : &siosb;

	hssocket->iostatus = __ntapi->zw_device_io_control_file(
			hssocket->hsocket,
			hssocket->hevent,
			0,
			0,
			iosb,
			NT_AFD_IOCTL_ACCEPT,
			0,
			0,
			accept_info,
			sizeof(nt_afd_server_accept_info));

	if (hssocket->iostatus && (hssocket->ntflags & __NT_FILE_SYNC_IO))
		__ntapi->sc_wait(hssocket,iosb,&hssocket->timeout);

	return hssocket->iostatus;
}
