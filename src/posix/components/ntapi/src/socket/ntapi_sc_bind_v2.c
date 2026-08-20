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

typedef struct _nt_afd_bind_request {
	uint32_t	unknown;
	nt_sockaddr	addr;
} nt_afd_bind_request;

typedef struct _nt_afd_bind_reply {
	nt_sockaddr	addr;
} nt_afd_bind_reply;

typedef struct __addr_memcpy {
	uint32_t	d0;
	uint32_t	d1;
	uint32_t	d2;
	uint32_t	d3;
} _addr_memcpy;


int32_t __cdecl __ntapi_sc_bind_v2(
	__in		nt_socket *		hssocket,
	__in		const nt_sockaddr *	addr,
	__in		uintptr_t		addrlen,
	__in		uintptr_t		service_flags	__optional,
	__out		nt_sockaddr *		sockaddr	__optional,
	__out		nt_io_status_block *	iosb		__optional)
{
	nt_io_status_block	siosb;
	nt_afd_bind_request	afd_bind_req;
	nt_afd_bind_reply	afd_bind_rep;

	_addr_memcpy *		src;
	_addr_memcpy *		dst;

	iosb = iosb ? iosb : &siosb;

	/* request */
	afd_bind_req.unknown = hssocket->domain;

	src = (_addr_memcpy *)addr;
	dst = (_addr_memcpy *)&(afd_bind_req.addr);

	dst->d0 = src->d0;
	dst->d1 = src->d1;
	dst->d2 = src->d2;
	dst->d3 = src->d3;

	hssocket->iostatus = __ntapi->zw_device_io_control_file(
			hssocket->hsocket,
			hssocket->hevent,
			0,
			0,
			iosb,
			NT_AFD_IOCTL_BIND,
			&afd_bind_req,
			sizeof(afd_bind_req),
			&afd_bind_rep,
			sizeof(afd_bind_rep));

	__ntapi->sc_wait(hssocket,iosb,0);

	if (!hssocket->iostatus && sockaddr) {
		/* return updated address information */
		src = (_addr_memcpy *)&(afd_bind_rep);
		dst = (_addr_memcpy *)sockaddr;

		dst->d0 = src->d0;
		dst->d1 = src->d1;
		dst->d2 = src->d2;
		dst->d3 = src->d3;
	}

	return hssocket->iostatus;
}
