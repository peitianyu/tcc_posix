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

typedef struct __addr_memcpy {
	uint64_t	d0;
	uint64_t	d1;
} _addr_memcpy;


int32_t __cdecl __ntapi_sc_accept(
	__in	nt_socket *		hssock_listen,
	__out	nt_sockaddr *		addr,
	__out	uint16_t *		addrlen,
	__out	nt_socket *		hssock_dedicated,
	__in	uintptr_t		afdflags	__optional,
	__in	uintptr_t		tdiflags	__optional,
	__out	nt_io_status_block *	iosb		__optional)
{
	int32_t			status;

	nt_afd_accept_info	accept_info;
	nt_io_status_block	siosb;

	_addr_memcpy *		src;
	_addr_memcpy *		dst;

	iosb = iosb ? iosb : &siosb;

	/* establish kernel connection */
	if ((status = __ntapi->sc_server_accept_connection(
			hssock_listen,
			&accept_info,
			iosb)))
		return status;

	/* create connection-dedicated socket handle */
	if ((status = __ntapi->sc_socket(
			hssock_dedicated,
			hssock_listen->domain,
			hssock_listen->type,
			hssock_listen->protocol,
			0,
			0,
			0)))
		return status;

	/* associate the dedicated handle with the connection */
	if ((status = __ntapi->sc_server_duplicate_socket(
			hssock_listen,
			hssock_dedicated,
			&accept_info,
			0)))
		return status;

	/* return address information */
	if (addr) {
		src = (_addr_memcpy *)&(accept_info.addr);
		dst = (_addr_memcpy *)addr;

		dst->d0 = src->d0;
		dst->d1 = src->d1;
	}

	/* return address length information */
	if (addrlen)
		*addrlen = sizeof(nt_sockaddr);

	return status;
}
