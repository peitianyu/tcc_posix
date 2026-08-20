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

static intptr_t __faccessat(int fdidxat, const unsigned char * path, int amode, int flag)
{
	struct __psx_tlca *	tlca;
	struct __path_info	path_info;
	int			exflags;
	int32_t			status;

	/* prolog */
	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	/* amode bits */
	exflags = PSX_PATH_OPEN_AT | PSX_PATH_ACCESS_CHECK;

	if (amode & X_OK)
		exflags |= PSX_PATH_ACCESS_EXEC;

	if (amode & W_OK)
		exflags |= PSX_PATH_ACCESS_WRITE;

	if (amode & R_OK)
		exflags |= PSX_PATH_ACCESS_READ;

	/* ofd */
	if ((status = __psx_path_open(tlca,&path_info,path,0,0,0,fdidxat,exflags)))
		return __psx_sig_epilog(tlca,path_info.psxstatus,status);

	/* epilog */
	__psx_ofd_ref_dec(tlca->ctx,path_info.ofd);
	return __psx_sig_epilog(tlca,0,status);
}

__psx_api
intptr_t __sys_access(const unsigned char * path, int amode)
{
	return __faccessat(AT_FDCWD,path,amode,0);
}

__psx_api
intptr_t __sys_faccessat(int fdidxat, const unsigned char * path, int amode, int flag)
{
	return __faccessat(fdidxat,path,amode,flag);
}
