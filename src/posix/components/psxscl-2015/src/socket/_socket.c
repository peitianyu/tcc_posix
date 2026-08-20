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
#include "psx_acl.h"
#include "psx.h"

static int __domain[PSX_SOCKET_CAP] = {
	NT_AF_UNIX,
	NT_AF_INET,
	NT_AF_INET6
};


static int32_t __socket_cancel(
	struct __psx_tlca *	tlca,
	struct __ofd *		ofd,
	int32_t			ret,
	int32_t			status)
{
	if (ofd)
		__psx_ofd_free(tlca->ctx,ofd);

	tlca->ntstatus = status;
	return ret;
}

/* todo!!! static __socket function should return struct __ofd (threads)  */
/* fd allocation should take place in __socketpair after ultimate success */
static intptr_t __socket(struct __psx_tlca * tlca, int domain, int type, int protocol)
{
	int32_t			status;
	struct __psx_ctx *	ctx;
	struct __fd *		fd;
	struct __ofd *		ofd;
	intptr_t		fdidx;
	intptr_t		ofdidx;
	nt_iosb			iosb;
	int			sctype;

	/* prolog */
	ctx = tlca->ctx;

	/* sctype */
	if (domain == PSX_AF_UNIX)
		sctype = PSX_SOCKET_UNIX;
	else if (domain == PSX_AF_INET)
		sctype = PSX_SOCKET_INET4;
	else if (domain == PSX_AF_INET6)
		sctype = PSX_SOCKET_INET6;
	else
		return -EINVAL;

	/* type */
	if (!type && (protocol == NT_IPPROTO_TCP))
		type = NT_SOCK_STREAM;

	/* protocol */
	if (!protocol && (type == NT_SOCK_DGRAM))
		protocol = NT_IPPROTO_UDP;

	/* ofd */
	if (!(ofd = __psx_ofd_alloc(ctx,&ofdidx)))
		return -ENOMEM;

	/* socket */
	if ((status = __ntapi->sc_socket(
			&ofd->info,
			__domain[sctype],
			type,
			protocol,
			NT_FILE_ALL_ACCESS,
			__PSX_DEF_SEC_QOS,
			&iosb)))
		return __socket_cancel(tlca,ofd,-EACCES,status);

	ofd->info.refcnt  = 1;
	ofd->info.sctype  = sctype;
	ofd->info.fdtype  = PSX_FD_OS_SOCKET;
	ofd->info.ntflags = NT_FILE_SYNCHRONOUS_IO_ALERT;

	/* fd */
	if (!(fd = __psx_fd_alloc(ctx,&fdidx)))
		return __socket_cancel(tlca,ofd,-EACCES,status);

	/* finalize */
	fd->ofdidx  = (int32_t)ofdidx;
	fd->flags   = 0;
	fd->refcnt  = 0;
	at_store_32(&fd->invalid,0);

	return fdidx;
}

static intptr_t __socketpair(int domain, int type, int protocol, int flags, int32_t sockfd[2])
{
	struct __psx_tlca *	tlca;
	intptr_t		ret;
	int32_t			fdidx[2];

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	tlca->ntstatus = EPSXONLY;

	if ((ret = __socket(tlca,domain,type,protocol)) < 0)
		return __psx_sig_epilog(tlca,ret,tlca->ntstatus);
	else if (flags)
		return __psx_sig_epilog(tlca,ret,NT_STATUS_SUCCESS);

	fdidx[0] = (int32_t)ret;
	fdidx[1] = (int32_t)__socket(tlca,domain,type,protocol);
	ret = fdidx[1];

	if (ret < 0) {
		__psx_fd_free(tlca->ctx,&tlca->ctx->fd_slots[fdidx[0]]);
		return __psx_sig_epilog(tlca,ret,tlca->ntstatus);
	}

	sockfd[0] = fdidx[0];
	sockfd[1] = fdidx[1];

	return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
}

__psx_api
intptr_t __sys_socket(int domain, int type, int protocol)
{
	return __socketpair(domain,type,protocol,-1,0);
}

__psx_api
intptr_t __sys_socketpair(int domain, int type, int protocol, int32_t sockfd[2])
{
	return __socketpair(domain,type,protocol,0,sockfd);
}
