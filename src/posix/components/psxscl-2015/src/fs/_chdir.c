/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx_cwd.h"
#include "psx_ofd.h"
#include "psx_tlca.h"
#include "psx_fcntl.h"
#include "psx.h"

static intptr_t __chdir(int fdidx, const unsigned char * path)
{
	int32_t			status;
	struct __psx_tlca *	tlca;
	struct __ofd *		ofd;
	struct __path_info	path_info;
	const unsigned char	cdir[] = {'.','/',0};

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if (!path && !(ofd  = __psx_ofd_ref_inc(tlca->ctx,fdidx)))
		return __psx_sig_epilog(tlca,-EBADF,EPSXONLY);
	else if (ofd && (ofd->info.fdtype < PSX_FD_OS_FS_DIR))
		return __psx_sig_epilog(tlca,-ENOTDIR,EPSXONLY);

	/* todo! allow /dev, /etc, and /proc */
	else if (ofd && (ofd->info.fdtype > PSX_FD_OS_FS_ROOT))
		return __psx_sig_epilog(tlca,-EACCES,EPSXONLY);

	if (path && (status = __psx_path_open(tlca,&path_info,path,O_DIRECTORY,0,0,0,PSX_PATH_ACCESS_CHECK|PSX_PATH_LIST_DIR)))
		return __psx_sig_epilog(tlca,path_info.psxstatus,status);

	else if ((status = __psx_path_open(tlca,&path_info,cdir,O_DIRECTORY,0,ofd,0,PSX_PATH_ACCESS_CHECK|PSX_PATH_LIST_DIR)))
		return __psx_sig_epilog(tlca,path_info.psxstatus,status);

	if ((status = __psx_setcwd(tlca->ctx,&path_info)))
		return __psx_sig_epilog(tlca,path_info.psxstatus,status);

	return __psx_sig_epilog(tlca,0,NT_STATUS_SUCCESS);
}

__psx_api
intptr_t __sys_chdir(const unsigned char * path)
{
	return __chdir(0,path);
}

__psx_api
intptr_t __sys_fchdir(int fdidx)
{
	return __chdir(fdidx,0);
}
