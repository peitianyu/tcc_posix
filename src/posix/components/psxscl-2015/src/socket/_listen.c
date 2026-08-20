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

__psx_api
intptr_t __sys_listen(int socket, int backlog)
{
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __ofd *		ctxofd;
	struct __ofd *		ofd;
	nt_iosb			iosb;

	tlca = __tlca_self();
	ctx  = tlca->ctx;
	__psx_io_prolog(tlca);

	if (!(ctxofd  = __psx_ofd_ref_inc(ctx,socket)))
		return __psx_io_epilog(tlca,-EBADF,EPSXONLY);

	else if (ctxofd->info.fdtype != PSX_FD_OS_SOCKET)
		return __psx_sig_epilog(tlca,-ENOTSOCK,EPSXONLY);

	else if ((iosb.status = __iovtbl[ctxofd->info.fdtype].prolog(tlca,ctxofd,&ofd)))
		return __psx_io_epilog(tlca,-ENOMEM,iosb.status);

	__ntapi->sc_listen(
		&ofd->info,
		backlog,
		&iosb);

	__psx_io_set_status(tlca,ofd,&iosb);
	__psx_ofd_ref_dec(tlca->ctx,ofd);

	return __psx_io_epilog(tlca,iosb.info,tlca->ntstatus);
}
