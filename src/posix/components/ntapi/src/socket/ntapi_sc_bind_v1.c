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

typedef struct _nt_afd_bind_msg {
	uint32_t	domain;
	uint32_t	type;
	uint32_t	service_flags;
	char		sa_data[14];
} nt_afd_bind_msg;


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


int32_t __cdecl __ntapi_sc_bind_v1(
	__in		nt_socket *		hssocket,
	__in		const nt_sockaddr *	addr,
	__in		uintptr_t		addrlen,
	__in		uintptr_t		service_flags	__optional,
	__out		nt_sockaddr *		sockaddr	__optional,
	__out		nt_io_status_block *	iosb		__optional)
{
	nt_io_status_block	siosb;
	nt_afd_bind_msg		afd_bind_req;
	nt_afd_bind_msg		afd_bind_rep;

	_addr_memcpy *		src;
	_addr_memcpy *		dst;

	iosb = iosb ? iosb : &siosb;

	/* service_flags */
	if (!service_flags)
		service_flags = 0x2000E;

	/* afd_bind_req */
	afd_bind_req.domain = hssocket->domain;
	afd_bind_req.type   = hssocket->type;
	afd_bind_req.service_flags = (uint32_t)service_flags;

	src = (_addr_memcpy *)addr;
	dst = (_addr_memcpy *)&(afd_bind_req.sa_data);

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
			NT_AFD_IOCTL_BIND,
			&afd_bind_req,
			sizeof(afd_bind_req),
			&afd_bind_rep,
			sizeof(afd_bind_rep));

	__ntapi->sc_wait(hssocket,iosb,0);

	if (!hssocket->iostatus && sockaddr) {
		src = (_addr_memcpy *)&(afd_bind_rep.sa_data);
		dst = (_addr_memcpy *)sockaddr;

		dst->d1 = src->d0;
		dst->d2 = src->d1;
		dst->d3 = src->d2;
		dst->d4 = src->d3;
		dst->d5 = src->d4;
		dst->d6 = src->d5;
		dst->d7 = src->d6;

		sockaddr->sa_addr_in4.sa_family = hssocket->domain;
	}

	return hssocket->iostatus;
}
