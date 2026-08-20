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

/* weed in Redmond during the 1990's anyone? */
typedef struct _nt_afd_connect_request {
	uintptr_t	unknown;
	void *		paddr;
	void *		hasync;
	uint32_t	type;
	uint32_t	service_flags;
	char		sa_data[14];
	uint16_t	hangover;
	uint32_t	unused;
} nt_afd_connect_request;

typedef struct __addr_memcpy {
	uint16_t	d0;
	uint16_t	d1;
	uint16_t	d2;
	uint16_t	d3;
	uint16_t	d4;
	uint16_t	d5;
	uint16_t	d6;
	uint16_t	d7;
} _addr_memcpy;

int32_t __cdecl __ntapi_sc_connect_v1(
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

	/* service_flags */
	if (!service_flags)
		service_flags = 0x2000E;

	/* afd_connect_req */
	afd_connect_req.type          = hssocket->type;
	afd_connect_req.service_flags = (uint32_t)service_flags;

	afd_connect_req.paddr    = (void *)0;
	afd_connect_req.hasync   = (void *)0;

	afd_connect_req.unknown  = 0;
	afd_connect_req.unused   = 0;
	afd_connect_req.hangover = 0;

	src = (_addr_memcpy *)addr;
	dst = (_addr_memcpy *)&(afd_connect_req.sa_data);

	dst->d0 = src->d1;
	dst->d1 = src->d2;
	dst->d2 = src->d3;
	dst->d3 = src->d4;
	dst->d4 = src->d5;
	dst->d5 = src->d6;
	dst->d6 = src->d7;

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
