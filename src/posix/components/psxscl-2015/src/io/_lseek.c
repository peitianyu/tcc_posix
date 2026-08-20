/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx_fcntl.h"
#include "psx_ofd.h"
#include "psx.h"

__psx_api
off_t __sys_lseek(int fdidx, off_t offset, int whence)
{
	struct __psx_tlca *	tlca;
	struct __ofd *		ofd;
	nt_iosb			iosb;
	nt_fpi			fpi;
	nt_eof			eof;
	int32_t			status;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	ofd  = __psx_ofd_ref_inc(tlca->ctx,fdidx);
	if (!ofd) return -EBADF;

	if (ofd->info.fdtype == PSX_FD_OS_PIPE)
		return __psx_sig_epilog(tlca,-EPIPE,EPSXONLY);
	else if (ofd->info.fdtype != PSX_FD_OS_FS_FILE)
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	if ((ofd->info.fdtype == PSX_FD_OS_FS_ROOT) || (ofd->info.fdtype == PSX_FD_OS_FS_DIR)) {
		if (offset && !ofd->info.dirctx)
			return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

		else if ((tlca->ntstatus = __psx_dirent_seek(ofd,&offset,whence)))
			return __psx_sig_epilog(tlca,offset,tlca->ntstatus);

		else
			return __psx_sig_epilog(tlca,-EINVAL,tlca->ntstatus);
	}

	if (whence == SEEK_SET)
		fpi.current_byte_offset.quad = offset;

	else if (whence == SEEK_CUR) {
		if ((status = __iovtbl[ofd->info.fdtype].query(
				ofd->info.hfile,&iosb,
				&fpi,sizeof(fpi),
				NT_FILE_POSITION_INFORMATION)))
			return __psx_sig_epilog(tlca,-ENXIO,status);

		fpi.current_byte_offset.quad += offset;

	} else if (whence == SEEK_END) {
		/* tcc_posix: NT_FILE_END_OF_FILE_INFORMATION 查询在 2015
		   实现下失败 (ENXIO); 改用 tt_stat (fstat 同路径) 拿文件大小 */
		nt_stat nstat;
		struct __psx_tlca * tl;
		tl = __tlca_self();
		if ((status = __ntapi->tt_stat(
				ofd->info.hfile,0,0,
				&nstat,tl->buffer,(uint32_t)tl->buflen,
				0,0)))
			return __psx_sig_epilog(tlca,-ENXIO,status);
		if (offset > 0)
			return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);
		fpi.current_byte_offset.quad = nstat.fsi.end_of_file.quad + offset;
	}

	if ((status = __iovtbl[ofd->info.fdtype].set(
			ofd->info.hfile,&iosb,
			&fpi,sizeof(fpi),
			NT_FILE_POSITION_INFORMATION)))
		return __psx_sig_epilog(tlca,-ENXIO,status);
	else
		return __psx_sig_epilog(tlca,fpi.current_byte_offset.quad,status);
}
