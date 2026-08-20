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
#include "psx_io.h"
#include "psx.h"

struct __saddr {
	uintptr_t	__align;
	nt_sockaddr	addr;
	uintptr_t	__pad;
};

static intptr_t __connect(struct __psx_tlca * tlca, struct __ofd * ofd, const struct __sockaddr * addr, socklen_t addrlen)
{
	struct __saddr		raddr;
	nt_iosb			iosb;
	int			ret;

	switch (ofd->info.sctype) {
		case PSX_SOCKET_UNIX:
			/* todo: dsr-based, high priority */
			ret = -ENOSYS;
			ofd->info.iostatus = NT_STATUS_NOT_IMPLEMENTED;
			break;

		case PSX_SOCKET_INET4:
		case PSX_SOCKET_INET6:
			if (addrlen > sizeof(nt_sockaddr)) {
				ret = -EINVAL;
				ofd->info.iostatus = NT_STATUS_INVALID_PARAMETER;
				break;
			}

			__ntapi->tt_aligned_block_memset(&raddr,0,sizeof(raddr));
			__ntapi->tt_generic_memcpy((char *)&raddr.addr,(char *)addr,addrlen);

			__ntapi->sc_connect(
				&ofd->info,
				&raddr.addr,
				addrlen,0,
				&iosb);
			ret = ofd->info.iostatus ? -ENXIO : 0;
			break;

		default:
			ret = -ENOSYS;
			ofd->info.iostatus = NT_STATUS_NOT_IMPLEMENTED;
			break;
	}

	return ret;
}


__psx_api
intptr_t __sys_connect(int socket, const struct __sockaddr * addr, socklen_t addrlen)
{
	struct __psx_tlca *	tlca;
	struct __ofd *		ofd;
	intptr_t		ret;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if (!(ofd  = __psx_ofd_ref_inc(tlca->ctx,socket)))
		return __psx_sig_epilog(tlca,-EBADF,EPSXONLY);
	else if (ofd->info.fdtype != PSX_FD_OS_SOCKET)
		return __psx_sig_epilog(tlca,-ENOTSOCK,EPSXONLY);

	ret = __connect(tlca,ofd,addr,addrlen);
	__psx_ofd_ref_dec(tlca->ctx,ofd);

	return __psx_sig_epilog(tlca,ret,tlca->ntstatus);
}


__psx_api
ssize_t __sys_sendto(int socket, const void * msg, size_t len, int flags, const struct __sockaddr * addr, socklen_t addrlen)
{
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __ofd *		ctxofd;
	struct __ofd *		ofd;
	nt_iosb			iosb;

	tlca = __tlca_self();
	ctx  = tlca->ctx;

	if (__psx_io_prolog(tlca))
		return __psx_io_epilog(tlca,-EINTR,EPSXONLY);

	if (!(ctxofd  = __psx_ofd_ref_inc(ctx,socket)))
		return __psx_io_epilog(tlca,-EBADF,EPSXONLY);

	else if (ctxofd->info.fdtype != PSX_FD_OS_SOCKET)
		return __psx_io_epilog(tlca,-ENOTSOCK,EPSXONLY);

	else if ((iosb.status = __iovtbl[ctxofd->info.fdtype].prolog(tlca,ctxofd,&ofd)))
		return __psx_io_epilog(tlca,-ENOMEM,iosb.status);

	if (addr && (ofd->info.type != NT_SOCK_STREAM))
		(void)0;
	else __ntapi->sc_send(
		&ofd->info,
		msg,len,0,
		0,0,&iosb);

	__psx_io_set_status(tlca,ofd,&iosb);
	__iovtbl[ctxofd->info.fdtype].epilog(ctxofd,ofd);
	__psx_ofd_ref_dec(tlca->ctx,ofd);

	return __psx_io_epilog(tlca,iosb.info,tlca->ntstatus);
}


__psx_api
ssize_t __sys_recvfrom(int socket, void * msg, size_t len, int flags, struct __sockaddr * addr, socklen_t * addrlen)
{
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __ofd *		ctxofd;
	struct __ofd *		ofd;
	nt_iosb			iosb;

	tlca = __tlca_self();
	ctx  = tlca->ctx;

	if (__psx_io_prolog(tlca))
		return __psx_io_epilog(tlca,-EINTR,EPSXONLY);

	if (!(ctxofd  = __psx_ofd_ref_inc(ctx,socket)))
		return __psx_io_epilog(tlca,-EBADF,EPSXONLY);

	else if (ctxofd->info.fdtype != PSX_FD_OS_SOCKET)
		return __psx_io_epilog(tlca,-ENOTSOCK,EPSXONLY);

	else if ((iosb.status = __iovtbl[ctxofd->info.fdtype].prolog(tlca,ctxofd,&ofd)))
		return __psx_io_epilog(tlca,-ENOMEM,iosb.status);

	if (addr && (ofd->info.type != NT_SOCK_STREAM))
		(void)0;
	else __ntapi->sc_recv(
		&ofd->info,
		msg,len,0,
		0,0,&iosb);

	__psx_io_set_status(tlca,ofd,&iosb);
	__iovtbl[ctxofd->info.fdtype].epilog(ctxofd,ofd);
	__psx_ofd_ref_dec(tlca->ctx,ofd);

	return __psx_io_epilog(tlca,iosb.info,tlca->ntstatus);
}
