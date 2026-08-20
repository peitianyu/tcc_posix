/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx_tlca.h"
#include "psx_impl.h"
#include "psx.h"

static intptr_t __pipe_cancel(
	struct __psx_tlca *	tlca,
	struct __ofd *		ofd[2],
	int32_t			ret,
	int32_t			status)
{
	if (ofd[0])
		__psx_ofd_free(tlca->ctx,ofd[0]);

	if (ofd[1])
		__psx_ofd_free(tlca->ctx,ofd[1]);

	return __psx_sig_epilog(tlca,ret,status);
}

__psx_api
intptr_t __sys_pipe(int fildes[2])
{
	int32_t			status;
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __fd *		fd[2];
	struct __ofd *		ofd[2];
	intptr_t		fdidx[2];
	intptr_t		ofdidx[2];

	/* prolog */
	tlca = __tlca_self();
	ctx  = tlca->ctx;
	__psx_sig_prolog(tlca);

	/* ofd */
	if (!(ofd[0] = __psx_ofd_alloc(ctx,&ofdidx[0])))
		return __psx_sig_epilog(tlca,-ENOMEM,EPSXONLY);

	if (!(ofd[1] = __psx_ofd_alloc(ctx,&ofdidx[1])))
		return __psx_sig_epilog(tlca,-ENOMEM,EPSXONLY);

	/* pipe */
	if ((status = __ntapi->ipc_create_pipe(
			&ofd[0]->info.hpipe,
			&ofd[1]->info.hpipe,
			2*__PSX_VIRTUAL_PAGE_SIZE)))
		return __pipe_cancel(tlca,ofd,-ENFILE,status);

	ofd[0]->info.refcnt = 1;
	ofd[0]->info.fdtype = PSX_FD_OS_PIPE;

	ofd[1]->info.refcnt = 1;
	ofd[1]->info.fdtype = PSX_FD_OS_PIPE;

	/* fd */
	if (!(fd[0] = __psx_fd_alloc(ctx,&fdidx[0])))
		return __pipe_cancel(tlca,ofd,-EMFILE,status);

	if (!(fd[1] = __psx_fd_alloc(ctx,&fdidx[1]))) {
		__psx_fd_free(ctx,fd[0]);
		return __pipe_cancel(tlca,ofd,-EMFILE,status);
	}

	/* finalize */
	fd[0]->ofdidx  = (int32_t)ofdidx[0];
	fd[0]->flags   = 0;
	fd[0]->refcnt  = 0;
	at_store_32(&fd[0]->invalid,0);

	fd[1]->ofdidx  = (int32_t)ofdidx[1];
	fd[1]->flags   = 0;
	fd[1]->refcnt  = 0;
	at_store_32(&fd[1]->invalid,0);

	fildes[0] = (int)fdidx[0];
	fildes[1] = (int)fdidx[1];

	return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
}
