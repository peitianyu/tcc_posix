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

static ssize_t __readlinkat(int fdidxat, const unsigned char * path, const unsigned char * buf, size_t buflen)
{
	int32_t			status;
	struct __psx_tlca *	tlca;
	struct __path_info	path_info;
	struct __ofd *		ofd;

	/* prolog */
	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	/* ofd */
	if ((status = __psx_path_open(tlca,&path_info,path,0,0,0,fdidxat,PSX_PATH_OPEN_AT|PSX_PATH_ACCESS_CHECK|PSX_PATH_ATTR_READ)))
		return __psx_sig_epilog(tlca,path_info.psxstatus,status);
	else
		ofd = path_info.ofd;

	if (ofd->info.fdtype > PSX_FD_OS_FS_ROOT)
		return __psx_sig_epilog(tlca,path_info.psxstatus,status);

	/* todo */
	return __psx_sig_epilog(tlca,path_info.psxstatus,status);
}

ssize_t __sys_readlink(const unsigned char * path, const unsigned char * buf, size_t buflen)
{
	return __readlinkat(AT_FDCWD,path,buf,buflen);
}

ssize_t __sys_readlinkat(int fdidxat, const unsigned char * path, const unsigned char * buf, size_t buflen)
{
	return __readlinkat(fdidxat,path,buf,buflen);
}
