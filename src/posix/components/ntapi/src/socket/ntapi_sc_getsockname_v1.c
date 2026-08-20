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

typedef struct _nt_afd_server_socket_name_info {
	uint32_t	unknown;
	uint32_t	type;
	uint32_t	service_flags;
	char		sa_data[14];
} nt_afd_server_socket_name_info;


struct __addr_memcpy {
	uint16_t	d0;
	uint16_t	d1;
	uint16_t	d2;
	uint16_t	d3;
	uint16_t	d4;
	uint16_t	d5;
	uint16_t	d6;
	uint16_t	d7;
};


int32_t __cdecl __ntapi_sc_getsockname_v1(
	__in	nt_socket *		hssocket,
	__in	nt_sockaddr *		addr,
	__in	uint16_t *		addrlen,
	__out	nt_io_status_block *	iosb	__optional)
{
	nt_io_status_block		siosb;
	nt_afd_server_socket_name_info	sock_name_info;

	struct __addr_memcpy *		asrc;
	struct __addr_memcpy *		adst;

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
			&sock_name_info,
			sizeof(sock_name_info));

	__ntapi->sc_wait(hssocket,iosb,0);

	if (!hssocket->iostatus) {
		addr->sa_addr_in4.sa_family = hssocket->domain;

		asrc = (struct __addr_memcpy *)&(sock_name_info.sa_data);
		adst = (struct __addr_memcpy *)addr;

		adst->d1 = asrc->d0;
		adst->d2 = asrc->d1;
		adst->d3 = asrc->d2;
		adst->d4 = asrc->d3;
		adst->d5 = asrc->d4;
		adst->d6 = asrc->d5;
		adst->d7 = asrc->d6;

		*addrlen = (uint16_t)iosb->info;
	};

	return hssocket->iostatus;
}
