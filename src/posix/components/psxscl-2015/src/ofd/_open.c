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

static intptr_t __open_cancel(
	struct __psx_tlca *	tlca,
	struct __path_info *	path_info,
	int32_t			ret,
	int32_t			status)
{
	if (path_info->fd)
		__ntapi->blt_release(
			tlca->ctx->fd_blt_ctx[path_info->fdidx / __PSX_BITS_PER_PAGE],
			path_info->fdidx % __PSX_BITS_PER_PAGE);

	if (path_info->ofd)
		__psx_ofd_free(tlca->ctx,path_info->ofd);
	else
		__iovtbl[path_info->fdtype].close(path_info->hfile);

	if (path_info->hat && (path_info->pathflags & PSX_PATH_CLOSE_AT))
		__iovtbl[path_info->fdtypeat].close(path_info->hat);

	return __psx_sig_epilog(tlca,ret,status);
}


static intptr_t __openat(int fdidxat, const unsigned char * path, int flags, mode_t mode)
{
	int32_t			status;
	struct __psx_tlca *	tlca;
	struct __path_info	path_info;
	nt_iosb			iosb;
	nt_eof			eof;

	/* prolog */
	tlca = __tlca_self();

	if (__psx_sig_prolog(tlca)) 
		return __psx_sig_epilog(tlca,-EINTR,EPSXONLY);

	/* ofd */
	if ((status = __psx_path_open(tlca,&path_info,path,flags,mode,0,fdidxat,PSX_PATH_OPEN_AT)))
		return __psx_sig_epilog(tlca,path_info.psxstatus,status);

	/* fd */
	if (!(path_info.fd = __psx_fd_alloc(tlca->ctx,&path_info.fdidx)))
		return __open_cancel(tlca,&path_info,-ENOMEM,EPSXONLY);

	/* truncate as needed */
	if ((flags & O_TRUNC) && (path_info.fdtype == PSX_FD_OS_FS_FILE)) {
		eof.end_of_file.quad = 0;
		if ((status = __ntapi->zw_set_information_file(
				path_info.hfile,&iosb,
				&eof,sizeof(eof),
				NT_FILE_END_OF_FILE_INFORMATION)))
			return __open_cancel(tlca,&path_info,-ENXIO,status);
	}

	/* finalize */
	path_info.fd->ofdidx  = (int32_t)path_info.ofdidx;
	path_info.fd->flags   = flags;
	path_info.fd->refcnt  = 0;
	at_store_32(&path_info.fd->invalid,0);

	return __psx_sig_epilog(tlca,path_info.fdidx,NT_STATUS_SUCCESS);
}

__psx_api
intptr_t __sys_creat(const unsigned char * path, mode_t mode)
{
	return __openat(AT_FDCWD,path, O_CREAT|O_TRUNC|O_WRONLY,mode);
}

__psx_api
intptr_t __sys_open(const unsigned char * path, int flags, mode_t mode)
{
	return __openat(AT_FDCWD,path,flags,mode);
}

__psx_api
intptr_t __sys_openat(int fdidxat, const unsigned char * path, int flags, mode_t mode)
{
	return __openat(fdidxat,path,flags,mode);
}
