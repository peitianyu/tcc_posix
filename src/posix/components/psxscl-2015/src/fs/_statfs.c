/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/*                                                      */
/*  statfs/fstatfs: 卷信息 (映射 NTFS 卷属性)           */
/*  (2015 pre-alpha 未注册 → __sysvtbl[137/138]=NULL;    */
/*   R1 保护前直接段错误。ntapi_tt_statfs 底层完备,     */
/*   此处接口层补齐, 字段映射 musl x86_64 struct statfs)*/
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx_fcntl.h"
#include "psx_ofd.h"
#include "psx_tlca.h"
#include "psx_impl.h"
#include "psx.h"

/* struct __statfs 布局见 psx_stat.h (对齐 musl x86_64 struct statfs) */
static intptr_t __statfs_map(struct __psx_tlca * tlca, void * hfile,
	struct __statfs * xstatfs)
{
	int32_t		status;
	nt_statfs	nstatfs;

	status = __ntapi->tt_statfs(
			hfile,0,0,
			&nstatfs,
			tlca->buffer,(uint32_t)tlca->buflen,
			NT_STATFS_DEFAULT);

	if (status)
		return status;

	xstatfs->f_type		= nstatfs.f_type;
	xstatfs->f_bsize	= nstatfs.f_bsize;
	xstatfs->f_blocks	= nstatfs.f_blocks;
	xstatfs->f_bfree	= nstatfs.f_bfree;
	xstatfs->f_bavail	= nstatfs.f_bavail;
	xstatfs->f_files	= nstatfs.f_files;
	xstatfs->f_ffree	= nstatfs.f_ffree;
	xstatfs->f_fsid[0]	= (int32_t)nstatfs.dev_name_hash;
	xstatfs->f_fsid[1]	= 0;
	xstatfs->f_namelen	= nstatfs.f_namelen;
	xstatfs->f_frsize	= nstatfs.f_frsize;
	xstatfs->f_flags	= nstatfs.f_flags;
	xstatfs->f_spare[0]	= 0;
	xstatfs->f_spare[1]	= 0;
	xstatfs->f_spare[2]	= 0;
	xstatfs->f_spare[3]	= 0;

	return NT_STATUS_SUCCESS;
}

__psx_api
intptr_t __sys_statfs(const unsigned char * path, struct __statfs * xstatfs)
{
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __path_info	path_info;
	int32_t			status;

	tlca = __tlca_self();
	ctx  = tlca->ctx;
	__psx_sig_prolog(tlca);

	if ((status = __psx_path_open(
			tlca,&path_info,path,0,0,0,AT_FDCWD,
			PSX_PATH_OPEN_AT)))
		return __psx_sig_epilog(tlca,path_info.psxstatus,status);

	status = __statfs_map(tlca,path_info.ofd->info.hfile,xstatfs);
	__psx_ofd_ref_dec(ctx,path_info.ofd);

	if (status)
		return __psx_sig_epilog(tlca,-ENXIO,status);

	return __psx_sig_epilog(tlca,0,status);
}

__psx_api
intptr_t __sys_fstatfs(int fdidx, struct __statfs * xstatfs)
{
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __ofd *		ofd;
	int32_t			status;

	tlca = __tlca_self();
	ctx  = tlca->ctx;
	__psx_sig_prolog(tlca);

	if (!(ofd = __psx_ofd_ref_inc(ctx,fdidx)))
		return __psx_sig_epilog(tlca,-EBADF,EPSXONLY);

	status = __statfs_map(tlca,ofd->info.hfile,xstatfs);
	__psx_ofd_ref_dec(ctx,ofd);

	if (status)
		return __psx_sig_epilog(tlca,-ENXIO,status);

	return __psx_sig_epilog(tlca,0,status);
}