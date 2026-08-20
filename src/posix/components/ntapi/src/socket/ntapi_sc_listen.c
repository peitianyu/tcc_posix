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

int32_t __cdecl __ntapi_sc_listen(
	__in	nt_socket *		hssocket,
	__in	uintptr_t		backlog,
	__out	nt_io_status_block *	iosb		__optional)
{
	nt_afd_listen_info	afd_listen;
	nt_io_status_block	siosb;

	iosb = iosb ? iosb : &siosb;

	/* afd_listen */
	afd_listen.unknown_1st = 0;
	afd_listen.unknown_2nd = 0;
	afd_listen.backlog     = (uint32_t)backlog;

	hssocket->iostatus = __ntapi->zw_device_io_control_file(
			hssocket->hsocket,
			hssocket->hevent,
			0,
			0,
			iosb,
			NT_AFD_IOCTL_LISTEN,
			&afd_listen,
			sizeof(afd_listen),
			0,
			0);

	return hssocket->iostatus
		? __ntapi->sc_wait(hssocket,iosb,0)
		: NT_STATUS_SUCCESS;
}
