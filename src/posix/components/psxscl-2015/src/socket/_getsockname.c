/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx_socket.h"
#include "psx_tlca.h"
#include "psx_impl.h"
#include "psx.h"

struct __saddr {
	uintptr_t	__align;
	nt_sockaddr	addr;
	uintptr_t	__pad;
};

__psx_api
intptr_t __sys_getsockname(int socket, struct __sockaddr * addr, socklen_t * addrlen)
{
	int32_t			status;
	struct __psx_tlca *	tlca;
	struct __ofd *		ofd;
	struct __saddr		raddr;
	uint16_t		raddrlen;
	nt_iosb			iosb;
	int			ret;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if (!(ofd  = __psx_ofd_ref_inc(tlca->ctx,socket)))
		return __psx_sig_epilog(tlca,-EBADF,EPSXONLY);

	switch (ofd->info.sctype) {
		case PSX_SOCKET_UNIX:
			/* todo: dsr-based, high priority */
			ret	= -ENOSYS;
			status	= NT_STATUS_NOT_IMPLEMENTED;
			break;

		case PSX_SOCKET_INET4:
		case PSX_SOCKET_INET6:
			raddrlen = sizeof(raddr);

			status = __ntapi->sc_getsockname(
				&ofd->info,
				&raddr.addr,
				&raddrlen,
				&iosb);

			if (status) {
				ret = -ENXIO;
				break;
			} else {
				*addrlen = (*addrlen >= raddrlen) ? raddrlen : *addrlen;
				__ntapi->tt_generic_memcpy((char *)addr,(char *)&raddr.addr,*addrlen);
				ret = 0;
				break;
			}

		default:
			ret	= -ENOSYS;
			status	= NT_STATUS_NOT_IMPLEMENTED;
			break;
	}

	__psx_ofd_ref_dec(tlca->ctx,ofd);
	return __psx_sig_epilog(tlca,ret,status);
}
