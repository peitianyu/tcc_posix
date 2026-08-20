/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_tlca.h"
#include "psx_dirent.h"
#include "psx_errno.h"
#include "psx.h"

__psx_api
intptr_t __sys_getdents(int fdidx, struct __dirent * dirent, unsigned int count)
{
	int32_t			status;
	struct __psx_tlca *	tlca;
	struct __ofd *		ofd;
	nt_iosb			iosb;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if (!(ofd  = __psx_ofd_ref_inc(tlca->ctx,fdidx)))
		return __psx_sig_epilog(tlca,-EBADF,EPSXONLY);

	status = __psx_dirent_query(tlca,ofd,dirent,count,&iosb);
	__psx_ofd_ref_dec(tlca->ctx,ofd);

	if (status)
		return __psx_sig_epilog(tlca,-ENXIO,status);
	else
		return __psx_sig_epilog(tlca,iosb.info,status);
}
