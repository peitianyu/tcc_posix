/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_tlca.h"
#include "psx_errno.h"
#include "psx_fcntl.h"
#include "psx_ofd.h"
#include "psx_stat.h"
#include "psx_path.h"
#include "psx_impl.h"
#include "psx.h"

static intptr_t __mkdirat(int fdidxat, const unsigned char * path, int flags, mode_t mode)
{
	int32_t			status;
	struct __psx_tlca *	tlca;
	struct __path_info	path_info;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if ((status = __psx_path_open(tlca,&path_info,path,flags,mode,0,fdidxat,PSX_PATH_OPEN_AT)))
		return __psx_sig_epilog(tlca,path_info.psxstatus,status);

	__psx_ofd_ref_dec(tlca->ctx,path_info.ofd);
	return __psx_sig_epilog(tlca,path_info.fdidx,NT_STATUS_SUCCESS);
}

__psx_api
intptr_t __sys_mkdir(const unsigned char * path, mode_t mode)
{
	return __mkdirat(AT_FDCWD,path,O_DIRECTORY|O_CREAT|O_EXCL,mode);
}

__psx_api
intptr_t __sys_mkdirat(int fdidxat, const unsigned char * path, mode_t mode)
{
	return __mkdirat(fdidxat,path,O_DIRECTORY|O_CREAT|O_EXCL,mode);
}
