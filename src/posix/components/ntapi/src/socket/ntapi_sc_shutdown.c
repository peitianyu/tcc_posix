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

int32_t __cdecl __ntapi_sc_shutdown(
	__in	nt_socket *		hssocket,
	__in	uintptr_t		psxhow,
	__in	uintptr_t		afdhow,
	__out	nt_io_status_block *	iosb	__optional)
{
	nt_afd_disconnect_info	afd_disconnect;
	nt_io_status_block	siosb;

	iosb = iosb ? iosb : &siosb;

	if (afdhow == 0) {
		switch (psxhow) {
			case NT_SHUT_RD:
				afdhow = NT_AFD_DISCONNECT_RD;
				break;

			case NT_SHUT_WR:
				afdhow = NT_AFD_DISCONNECT_WR;
				break;

			case NT_SHUT_RDWR:
				afdhow = NT_AFD_DISCONNECT_RD | NT_AFD_DISCONNECT_WR;
				break;

			default:
				return NT_STATUS_INVALID_PARAMETER_2;
				break;
		}
	}

	afd_disconnect.shutdown_flags = (uint32_t)afdhow;
	afd_disconnect.unknown[0] = 0xff;
	afd_disconnect.unknown[1] = 0xff;
	afd_disconnect.unknown[2] = 0xff;

	hssocket->iostatus = __ntapi->zw_device_io_control_file(
			hssocket->hsocket,
			hssocket->hevent,
			0,
			0,
			iosb,
			NT_AFD_IOCTL_DISCONNECT,
			&afd_disconnect,
			sizeof(afd_disconnect),
			0,
			0);

	return hssocket->iostatus
		? __ntapi->sc_wait(hssocket,iosb,0)
		: NT_STATUS_SUCCESS;
}
