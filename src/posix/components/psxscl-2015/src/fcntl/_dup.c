/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_fcntl.h"
#include "psx_errno.h"
#include "psx.h"

static intptr_t __dup3(int fildes, int fildes2, int flags)
{
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __fd *		fd[2];
	struct __ofd *		ofd[2];
	intptr_t		fdidx;
	int			reused;
	int64_t			store;

	tlca = __tlca_self();
	ctx  = tlca->ctx;
	__psx_sig_prolog(tlca);

	if (!(fd[0] = __psx_fd_ref_inc(ctx,fildes)))
		return __psx_sig_epilog(tlca,-EBADF,EPSXONLY);
	else if (fildes == fildes2)
		return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);

	fdidx = fildes2;

	if ((fd[1] = __psx_fd_ref_inc(ctx,fdidx)))
		reused = 1;
	else if ((fd[1] = __psx_fd_possess(tlca->ctx,&fdidx)))
		reused = 0;
	else {
		__psx_fd_ref_dec(ctx,fd[0]);
		return __psx_sig_epilog(tlca,-EMFILE,EPSXONLY);
	}

	if (reused) {
		ofd[0] = &ctx->ofd_slots[fd[0]->ofdidx];
		ofd[1] = &ctx->ofd_slots[fd[1]->ofdidx];
		store = fd[0]->ofdidx + ((int64_t)(fd[0]->flags & ~flags) << 32);

		at_locked_inc_32(&ofd[0]->info.refcnt);
		at_store_64((int64_t *)fd[1],store);

		__psx_fd_ref_dec(ctx,fd[1]);
		__psx_ofd_ref_dec(ctx,ofd[1]);
	} else {
		fd[1]->ofdidx	= fd[0]->ofdidx;
		fd[1]->flags	= fd[0]->flags & ~flags;
		fd[1]->refcnt	= 0;
	}

	at_locked_dec_32(&fd[0]->refcnt);
	at_store_32(&fd[1]->invalid,0);

	return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
}

__psx_api
intptr_t __sys_dup(int fildes)
{
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __fd *		fd[2];
	intptr_t		fdidx;

	tlca = __tlca_self();
	ctx  = tlca->ctx;
	__psx_sig_prolog(tlca);

	if (!(fd[0] = __psx_fd_ref_inc(ctx,fildes)))
		return __psx_sig_epilog(tlca,-EBADF,EPSXONLY);

	if (!(fd[1] = __psx_fd_alloc(tlca->ctx,&fdidx))) {
		__psx_fd_ref_dec(ctx,fd[0]);
		return __psx_sig_epilog(tlca,-EMFILE,EPSXONLY);
	}

	fd[1]->ofdidx	= fd[0]->ofdidx;
	fd[1]->flags	= fd[0]->flags & ~FD_CLOEXEC;
	fd[1]->refcnt	= 0;

	__psx_ofd_ref_inc(ctx,fd[1]->ofdidx);
	at_store_32(&fd[1]->invalid,0);

	__psx_fd_ref_dec(ctx,fd[0]);
	return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
}

__psx_api
intptr_t __sys_dup2(int fildes, int fildes2)
{
	return __dup3(fildes,fildes2,0);
}

__psx_api
intptr_t __sys_dup3(int fildes, int fildes2, int flags)
{
	return __dup3(fildes,fildes2,flags);
}
