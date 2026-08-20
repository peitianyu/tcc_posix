/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx_sigfn.h"
#include "psx_signal.h"
#include "psx_impl.h"
#include "psx.h"

static intptr_t __sigaction_impl(int signum, const struct __sigaction * act, struct __sigaction * oldact, size_t sigsetsize)
{
	struct __psx_tlca * tlca;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if (!(__tlca_shared_ctx(tlca)))
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);
	else if ((signum < 0) || (signum >= 64))
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	at_store((intptr_t *)&__sigvtbl[signum],(intptr_t)act->sa_handler);

	return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
}

__psx_api
intptr_t __sys_sigaction(int signum, const struct __sigaction * act, struct __sigaction * oldact)
{
	return __sigaction_impl(signum,act,oldact,sizeof(sigset_t));
}

__psx_api
intptr_t __sys_rt_sigaction(int signum, const struct __sigaction * act, struct __sigaction * oldact, size_t sigsetsize)
{
	return __sigaction_impl(signum,act,oldact,sigsetsize);
}
