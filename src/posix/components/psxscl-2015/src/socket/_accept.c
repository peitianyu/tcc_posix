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

__psx_api
intptr_t __sys_accept(int socket, struct __sockaddr * addr, socklen_t * addrlen)
{
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __ofd *		ctxofd;
	struct __ofd *		sofd;
	struct __saddr		saddr;
	struct __ofd *		ofd;
	struct __fd *		fd;
	intptr_t		ofdidx;
	intptr_t		fdidx;
	uint16_t		len;
	nt_iosb			iosb;

	tlca = __tlca_self();
	ctx  = tlca->ctx;
	__psx_io_prolog(tlca);

	/* sofd */
	if (!(ctxofd  = __psx_ofd_ref_inc(ctx,socket)))
		return __psx_io_epilog(tlca,-EBADF,EPSXONLY);

	else if (ctxofd->info.fdtype != PSX_FD_OS_SOCKET)
		return __psx_sig_epilog(tlca,-ENOTSOCK,EPSXONLY);

	else if ((iosb.status = __iovtbl[ctxofd->info.fdtype].prolog(tlca,ctxofd,&sofd)))
		return __psx_io_epilog(tlca,-ENOMEM,iosb.status);

	/* ofd */
	if (!(ofd = __psx_ofd_alloc(ctx,&ofdidx)))
		return __psx_io_epilog(tlca,-ENFILE,EPSXONLY);

	ofd->info.fdtype = PSX_FD_OS_SOCKET;
	ofd->info.sctype = sofd->info.sctype;
	len = sizeof(saddr.addr);

	/* fd */
	if (!(fd = __psx_fd_alloc(ctx,&fdidx)))
		return __psx_io_epilog(tlca,-ENFILE,EPSXONLY);

	/* accept */
	__ntapi->sc_accept(
		&sofd->info,
		&saddr.addr,
		&len,
		&ofd->info,
		0,0,&iosb);

	__psx_io_set_status(tlca,sofd,&iosb);
	__psx_ofd_ref_dec(tlca->ctx,sofd);

	/* (fail) */
	if (tlca->ntstatus) {
		__ntapi->blt_release(
			ctx->fd_blt_ctx[fdidx / __PSX_BITS_PER_PAGE],
			fdidx % __PSX_BITS_PER_PAGE);

		ofd->info.fdtype = PSX_FD_OS_DEFAULT;
		__psx_ofd_free(ctx,ofd);

		return __psx_io_epilog(tlca,iosb.info,tlca->ntstatus);
	}

	/* addr */
	else if (addr && addrlen) {
		__ntapi->tt_generic_memcpy(
			addr,&saddr.addr,*addrlen);
		*addrlen = (*addrlen>=len) ? len : *addrlen;
	}

	/* finalize */
	fd->ofdidx  = (int32_t)ofdidx;
	fd->flags   = 0;
	fd->refcnt  = 0;
	at_store_32(&fd->invalid,0);

	return __psx_io_epilog(tlca,fdidx,NT_STATUS_SUCCESS);
}
