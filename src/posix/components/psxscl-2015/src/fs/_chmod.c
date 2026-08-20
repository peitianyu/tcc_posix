/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx_access.h"
#include "psx_fcntl.h"
#include "psx.h"

static intptr_t __fchmodat(int fdidxat, const unsigned char * path, mode_t mode, int flag)
{
	struct __psx_tlca *	tlca;
	struct __path_info	path_info;
	int32_t			status;

	/* prolog */
	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	/* ofd */
	if ((status = __psx_path_open(tlca,&path_info,path,0,0,0,fdidxat,PSX_PATH_ACCESS_CHECK)))
		return __psx_sig_epilog(tlca,path_info.psxstatus,status);

	/* epilog */
	__psx_ofd_ref_dec(tlca->ctx,path_info.ofd);
	return __psx_sig_epilog(tlca,0,status);
}

__psx_api
intptr_t __sys_chmod(const unsigned char * path, mode_t mode)
{
	return __fchmodat(AT_FDCWD,path,mode,0);
}

__psx_api
intptr_t __sys_fchmodat(int fdidxat, const unsigned char * path, mode_t mode, int flag)
{
	return __fchmodat(fdidxat,path,mode,flag);
}
