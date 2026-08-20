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
intptr_t __sys_bind(int socket, const struct __sockaddr * addr, socklen_t addrlen)
{
	int32_t			status;
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __ofd *		ctxofd;
	struct __ofd *		ofd;
	struct __saddr		laddr;
	nt_sockaddr		raddr;
	nt_iosb			iosb;
	int			ret;

	tlca = __tlca_self();
	ctx  = tlca->ctx;
	__psx_sig_prolog(tlca);

	if (!(ctxofd  = __psx_ofd_ref_inc(ctx,socket)))
		return __psx_io_epilog(tlca,-EBADF,EPSXONLY);

	else if (ctxofd->info.fdtype != PSX_FD_OS_SOCKET)
		return __psx_sig_epilog(tlca,-ENOTSOCK,EPSXONLY);

	else if ((iosb.status = __iovtbl[ctxofd->info.fdtype].prolog(tlca,ctxofd,&ofd)))
		return __psx_io_epilog(tlca,-ENOMEM,iosb.status);

	switch (ofd->info.sctype) {
		case PSX_SOCKET_UNIX:
			/* todo: dsr-based, high priority */
			ret	= -ENOSYS;
			status	= NT_STATUS_NOT_IMPLEMENTED;
			break;

		case PSX_SOCKET_INET4:
		case PSX_SOCKET_INET6:
			if (addrlen > sizeof(nt_sockaddr)) {
				ret	= -EINVAL;
				status	= NT_STATUS_INVALID_PARAMETER;
				break;
			}

			__ntapi->tt_aligned_block_memset(&laddr,0,sizeof(laddr));
			__ntapi->tt_generic_memcpy((char *)&laddr.addr,(char *)addr,addrlen);

			status = __ntapi->sc_bind(
				&ofd->info,
				&laddr.addr,
				addrlen,0,
				&raddr,
				&iosb);
			ret = status ? -ENXIO : 0;
			break;

		default:
			ret	= -ENOSYS;
			status	= NT_STATUS_NOT_IMPLEMENTED;
			break;
	}

	__psx_ofd_ref_dec(tlca->ctx,ofd);
	return __psx_sig_epilog(tlca,ret,status);
}
