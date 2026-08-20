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

typedef struct _nt_afd_connect_request {
	uintptr_t	unknown[2];
	void *		paddr;
	nt_sockaddr	addr;
} nt_afd_connect_request;

typedef struct __addr_memcpy {
	uint64_t	d0;
	uint64_t	d1;
} _addr_memcpy;


int32_t __cdecl __ntapi_sc_connect_v2(
	__in		nt_socket *		hssocket,
	__in		nt_sockaddr *		addr,
	__in		uintptr_t		addrlen,
	__in		uintptr_t		service_flags	__optional,
	__out		nt_io_status_block *	iosb		__optional)
{
	nt_io_status_block	siosb;
	nt_afd_connect_request	afd_connect_req;

	_addr_memcpy *		src;
	_addr_memcpy *		dst;

	iosb = iosb ? iosb : &siosb;

	/* afd_connect_req */
	afd_connect_req.unknown[0] = 0;
	afd_connect_req.unknown[1] = 0;

	src = (_addr_memcpy *)addr;
	dst = (_addr_memcpy *)&(afd_connect_req.addr);

	dst->d0 = src->d0;
	dst->d1 = src->d1;

	afd_connect_req.paddr = &(afd_connect_req.addr);
	afd_connect_req.addr.sa_addr_in4.sa_family = hssocket->domain;

	hssocket->iostatus = __ntapi->zw_device_io_control_file(
			hssocket->hsocket,
			hssocket->hevent,
			0,
			0,
			iosb,
			NT_AFD_IOCTL_CONNECT,
			&afd_connect_req,
			sizeof(afd_connect_req),
			(void *)0,
			0);

	return hssocket->iostatus
		? __ntapi->sc_wait(hssocket,iosb,0)
		: NT_STATUS_SUCCESS;
}
