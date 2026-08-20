/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx_stat.h"
#include "psx_fcntl.h"
#include "psx.h"

static intptr_t __fstatat(int fdidxat, const unsigned char * path, struct __stat * xstat, int flag)
{
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __path_info	path_info;
	int32_t			status;

	tlca = __tlca_self();
	ctx  = tlca->ctx;
	__psx_sig_prolog(tlca);

	if ((status = __psx_path_open(tlca,&path_info,path,0,0,0,fdidxat,PSX_PATH_OPEN_AT)))
		return __psx_sig_epilog(tlca,path_info.psxstatus,status);

	status = __iovtbl[path_info.ofd->info.fdtype].stat(tlca,path_info.ofd,xstat);
	__psx_ofd_ref_dec(ctx,path_info.ofd);

	if (status)
		return __psx_sig_epilog(tlca,-ENXIO,status);
	else
		return __psx_sig_epilog(tlca,0,status);
}

__psx_api
intptr_t __sys_stat(const unsigned char * path, struct __stat * xstat)
{
	return __fstatat(AT_FDCWD,path,xstat,0);
}

__psx_api
intptr_t __sys_fstatat(int fdidxat, const unsigned char * path, struct __stat * xstat, int flag)
{
	return __fstatat(fdidxat,path,xstat,flag);
}

__psx_api
intptr_t __sys_lstat(const unsigned char * path, struct __stat * xstat)
{
	return __fstatat(AT_FDCWD,path,xstat,0);
}
