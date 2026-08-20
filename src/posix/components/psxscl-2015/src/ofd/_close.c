/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include "psx_systypes.h"
#include "psx_tlca.h"
#include "psx_errno.h"
#include "psx_ofd.h"
#include "psx_signal.h"
#include "psx_impl.h"
#include "psx.h"

__psx_api
intptr_t __sys_close(int fdidx)
{
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;

	tlca = __tlca_self();
	ctx  = tlca->ctx;
	__psx_sig_prolog(tlca);

	if (fdidx > ctx->fd_cap)
		return __psx_sig_epilog(tlca,-EBADF,EPSXONLY);

	if (__psx_fd_free(ctx,&ctx->fd_slots[fdidx]))
		return __psx_sig_epilog(tlca,-EBADF,EPSXONLY);

	return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
}
