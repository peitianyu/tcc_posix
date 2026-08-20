/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_tlca.h"
#include "psx_errno.h"
#include "psx_stat.h"
#include "psx_fcntl.h"
#include "psx.h"

__psx_api
intptr_t __sys_fstat(int fdidx, struct __stat * xstat)
{
	struct __psx_tlca *	tlca;
	struct __ofd *		ofd;
	int32_t			status;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if (!(ofd  = __psx_ofd_ref_inc(tlca->ctx,fdidx)))
		return __psx_sig_epilog(tlca,-EBADF,EPSXONLY);

	status = __iovtbl[ofd->info.fdtype].stat(tlca,ofd,xstat);
	__psx_ofd_ref_dec(tlca->ctx,ofd);

	if (status)
		return __psx_sig_epilog(tlca,-ENXIO,status);
	else
		return __psx_sig_epilog(tlca,0,status);
}
