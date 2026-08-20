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

static intptr_t __unlinkat(int fdidxat, const unsigned char * path, int flag)
{
	int32_t			status;
	struct __psx_tlca *	tlca;
	struct __path_info	path_info;
	struct __ofd *		ofd;
	intptr_t		ret;

	/* prolog */
	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	/* flag */
	if (flag & ~AT_REMOVEDIR)
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	/* ofd */
	if ((status = __psx_path_open(tlca,&path_info,path,0,0,0,fdidxat,PSX_PATH_OPEN_AT|PSX_PATH_ACCESS_DELETE)))
		return __psx_sig_epilog(tlca,path_info.psxstatus,status);
	else
		ofd = path_info.ofd;

	/* semantics */
	if (ofd->info.fdtype >= PSX_FD_OS_FS_ROOT)
		return __psx_sig_epilog(tlca,-EROFS,status);
	else if ((ofd->info.fdtype == PSX_FD_OS_FS_FILE) && (flag & AT_REMOVEDIR))
		return __psx_sig_epilog(tlca,-ENOTDIR,status);
	else if ((ofd->info.fdtype == PSX_FD_OS_FS_DIR) && !(flag & AT_REMOVEDIR))
		return __psx_sig_epilog(tlca,-EPERM,status);

	/* mark for deletion */
	status = __iovtbl[path_info.ofd->info.fdtype].unlink(path_info.ofd,flag);
	__psx_ofd_ref_dec(tlca->ctx,path_info.ofd);

	/* epilog */
	if (status == NT_STATUS_SUCCESS)
		ret = 0;
	else if (status == NT_STATUS_DIRECTORY_NOT_EMPTY)
		ret = -ENOTEMPTY;
	else if (status == NT_STATUS_ACCESS_DENIED)
		ret = -EPERM;
	else if (status == NT_STATUS_SHARING_VIOLATION)
		ret = -EBUSY;
	else if (status == NT_STATUS_OBJECT_NAME_INVALID)
		ret = -ENAMETOOLONG;
	else
		ret = -EACCES;

	return __psx_sig_epilog(tlca,ret,status);
}

__psx_api
intptr_t __sys_unlink(const unsigned char * path)
{
	return __unlinkat(AT_FDCWD,path,0);
}

__psx_api
intptr_t __sys_unlinkat(int fdidxat, const unsigned char * path, int flag)
{
	return __unlinkat(fdidxat,path,flag);
}

__psx_api
intptr_t __sys_rmdir(const unsigned char * path)
{
	return __unlinkat(AT_FDCWD,path,AT_REMOVEDIR);
}
