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

int32_t __cdecl __ntapi_sc_getsockname_v2(
	__in	nt_socket *		hssocket,
	__in	nt_sockaddr *		addr,
	__in	uint16_t *		addrlen,
	__out	nt_io_status_block *	iosb	__optional)
{
	nt_iosb siosb;

	iosb = iosb ? iosb : &siosb;

	hssocket->iostatus = __ntapi->zw_device_io_control_file(
			hssocket->hsocket,
			hssocket->hevent,
			0,
			0,
			iosb,
			NT_AFD_IOCTL_GET_SOCK_NAME,
			0,
			0,
			addr,
			sizeof(*addr));

	__ntapi->sc_wait(hssocket,iosb,0);

	if (!hssocket->iostatus)
		*addrlen = (uint16_t)iosb->info;

	return hssocket->iostatus;
}
