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
#include "psx_unicode.h"
#include "psx_unlink.h"
#include "psx.h"

#define __PSX_FSTX_TAG		".fstx.41a5f1e5-b109-4dff-948b-edff3b00e1ec"
#define __PSX_TRANSLATE_RET	128
#define __PSX_ELEMENT_EXFLAGS	PSX_PATH_OPEN_AT		\
				| PSX_PATH_EXPLICIT_LAST	\
				| PSX_PATH_ACCESS_DELETE

enum __rename_ofd_type {
	__SRC_FD,
	__SRC_DIR,
	__DST_FD,
	__DST_DIR,
	__OFD_ENUM_CAP
};

struct __rename_info {
	uintptr_t	freplace;
	void *		hroot;
	uint32_t	strlen;
	wchar16_t	buffer[512+48];
};

struct __rollback_info {
	struct __ofd *		ofdat;
	struct __ofd *		ofd;
	struct __rename_info *	frni;
};

#ifdef __midipix__
static
#endif
void ___chkstk_ms(void)
{
}

static intptr_t __rename_epilog(
	struct __psx_tlca *	tlca,
	struct __path_info	path_info[],
	int32_t			ret,
	int32_t			status)
{
	if (path_info[__SRC_FD].ofd)
		__psx_ofd_free(tlca->ctx,path_info[__SRC_FD].ofd);

	if (path_info[__SRC_DIR].ofd)
		__psx_ofd_free(tlca->ctx,path_info[__SRC_DIR].ofd);

	if (path_info[__DST_FD].ofd)
		__psx_ofd_free(tlca->ctx,path_info[__DST_FD].ofd);

	if (path_info[__DST_DIR].ofd)
		__psx_ofd_free(tlca->ctx,path_info[__DST_DIR].ofd);

	if (ret == __PSX_TRANSLATE_RET) {
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
			ret = -EIO;
	}

	return __psx_sig_epilog(tlca,ret,status);
}

static int32_t __ofd_delattr_set(struct __ofd * ofd)
{
	nt_iosb  iosb;
	nt_fdi   fdi = {1};

	return __ntapi->zw_set_information_file(
		ofd->info.hfile,
		&iosb,&fdi,sizeof(fdi),
		NT_FILE_DISPOSITION_INFORMATION);
}

static int32_t __ofd_delattr_clear(struct __ofd * ofd)
{
	nt_iosb  iosb;
	nt_fdi   fdi = {0};

	return __ntapi->zw_set_information_file(
		ofd->info.hfile,
		&iosb,&fdi,sizeof(fdi),
		NT_FILE_DISPOSITION_INFORMATION);
}

static int32_t __ofd_rename_file(struct __ofd * ofdat, struct __ofd * ofd, struct __ofd * dst, struct __rename_info * frni)
{
	int32_t			status;
	nt_iosb			iosb;
	nt_ftagi		ftagi;

	frni->freplace = 1;
	frni->hroot    = ofdat->info.hfile;

	/* clear read-only attribute */
	if (dst && (status = __psx_roattr_clear(dst,&ftagi)))
		return status;

	/* rename */
	status = __ntapi->zw_set_information_file(
			ofd->info.hfile,
			&iosb,frni,sizeof(*frni),
			NT_FILE_RENAME_INFORMATION);

	/* restore attr as needed */
	if (dst && status)
		__psx_roattr_restore(dst,&ftagi);

	return status;
}


static int32_t __ofd_rename_dir(struct __ofd * ofdat, struct __ofd * ofd, struct __rename_info * frni, struct __rollback_info * rbi)
{
	int32_t			status;
	nt_iosb			iosb;
	nt_ftagi		ftagi;

	frni->freplace = 0;
	frni->hroot    = ofdat->info.hfile;

	/* clear read-only attribute */
	if ((status = __psx_roattr_clear(ofd,&ftagi)))
		return status;

	/* non-empty transacted directory? */
	if (rbi && (rbi->ofd == ofd) && (status = __ofd_delattr_set(ofd)))
		return status;

	/* (clear disposition for rename operation) */
	if ((status = __ofd_delattr_clear(ofd)))
		return status;

	/* rename */
	status = __ntapi->zw_set_information_file(
		ofd->info.hfile,
		&iosb,frni,sizeof(*frni),
		NT_FILE_RENAME_INFORMATION);

	/* non-empty transacted directory? */
	if (rbi && rbi && (rbi->ofd == ofd) && (status == NT_STATUS_SUCCESS)) {
		status = __ofd_delattr_set(ofd);
		__ofd_delattr_clear(ofd);
	}

	/* rollback needed? */
	if (rbi && status) {
		rbi->frni->freplace = 0;
		rbi->frni->hroot    = rbi->ofdat->info.hfile;

		if (__ntapi->zw_set_information_file(
				rbi->ofd->info.hfile,
				&iosb,rbi->frni,sizeof(*rbi->frni),
				NT_FILE_RENAME_INFORMATION))
			status = NT_STATUS_TRANSACTION_INTEGRITY_VIOLATED;
	}

	/* (success / normal failure / bummer) */
	__psx_roattr_restore(ofd,&ftagi);
	return status;
}


static int32_t __ofd_check_fstx_state(struct __ofd * ofd, struct __rename_info * frni)
{
	int32_t			status;
	void *			hfile;
	nt_unicode_string	name;
	nt_oa			oa;
	nt_iosb			iosb;

	name.strlen = frni->strlen;
	name.maxlen = 0;
	name.buffer = frni->buffer;

	oa.len      = sizeof(oa);
	oa.root_dir = ofd->info.hfile;
	oa.obj_name = &name;
	oa.obj_attr = 0;
	oa.sec_desc = 0;
	oa.sec_qos  = 0;

	/* transaction directory already exists? */
	status = __ntapi->zw_open_file(
		&hfile,
		NT_FILE_READ_ATTRIBUTES,
		&oa,&iosb,
		NT_FILE_SHARE_READ|NT_FILE_SHARE_WRITE|NT_FILE_SHARE_DELETE,
		0);

	switch (status) {
		case NT_STATUS_OBJECT_NAME_NOT_FOUND:
			return NT_STATUS_SUCCESS;

		case NT_STATUS_SUCCESS:
			__ntapi->zw_close(hfile);
			return NT_STATUS_TRANSACTIONAL_CONFLICT;

		default:
			return NT_STATUS_UNEXPECTED_IO_ERROR;
	}
}


static void __init_name_from_path_info(struct __path_info * path_info, struct __rename_info * rni)
{
	wchar16_t * wch = path_info->lastmark[0];
	rni->strlen = sizeof(wchar16_t)*(uint16_t)(path_info->lastmark[1]-path_info->lastmark[0]);

	if ((path_info->lastmark[0][0] == '/') || (path_info->lastmark[0][0] == '\\')) {
		wch++;
		rni->strlen -= sizeof(wchar16_t);
	};

	__ntapi->tt_memcpy_utf16(
		rni->buffer,
		wch,rni->strlen);
}

static intptr_t __renameat(int srcfdidx, const unsigned char * src, int dstfdidx, const unsigned char * dst)
{
	int32_t			status;
	struct __psx_tlca *	tlca;
	struct __ofd *		ofd[__OFD_ENUM_CAP] = {0};
	struct __path_info	path_info[__OFD_ENUM_CAP];
	struct __stat		xstat[__OFD_ENUM_CAP];
	struct __rename_info	srcrni;
	struct __rename_info	dstrni;
	struct __rename_info	fstxrni;
	struct __rollback_info	rbi;
	wchar16_t *		wch;
	wchar16_t *		wchnext;
	const unsigned char	fstxtag[] = __PSX_FSTX_TAG;
	const unsigned char	parent[3] = {'.','.',0};

	/* prolog */
	tlca = __tlca_self();
	__psx_sig_prolog(tlca);
	___chkstk_ms();

	path_info[__SRC_FD].ofd	 = 0;
	path_info[__SRC_DIR].ofd = 0;
	path_info[__DST_FD].ofd  = 0;
	path_info[__DST_DIR].ofd = 0;

	/* src fd */
	if ((status = __psx_path_open(tlca,&path_info[__SRC_FD],src,O_RDWR,0,0,srcfdidx,__PSX_ELEMENT_EXFLAGS)))
		return __rename_epilog(tlca,path_info,path_info[__SRC_FD].psxstatus,status);
	else if (path_info[__SRC_FD].lastmark[1]-path_info[__SRC_FD].lastmark[0] > 512)
		return __rename_epilog(tlca,path_info,-ENAMETOOLONG,status);

	__init_name_from_path_info(&path_info[__SRC_FD],&srcrni);

	/* src dir */
	path_info[__SRC_FD].ofd->info.refcnt++;

	if ((status = __psx_path_open(tlca,&path_info[__SRC_DIR],parent,O_RDWR,0,path_info[__SRC_FD].ofd,0,PSX_PATH_OPEN_AT|PSX_PATH_INTERNAL_CALL)))
		return __rename_epilog(tlca,path_info,path_info[__SRC_DIR].psxstatus,status);

	/* dst fd */
	status = __psx_path_open(tlca,&path_info[__DST_FD],dst,O_RDWR,0,0,dstfdidx,__PSX_ELEMENT_EXFLAGS);

	if (path_info[__DST_FD].lastmark[1]-path_info[__DST_FD].lastmark[0] > 512)
		return __rename_epilog(tlca,path_info,-ENAMETOOLONG,status);

	__init_name_from_path_info(&path_info[__DST_FD],&dstrni);

	/* dst dir */
	if (path_info[__DST_FD].ofd) {
		path_info[__DST_FD].ofd->info.refcnt++;

		if ((status = __psx_path_open(tlca,&path_info[__DST_DIR],parent,O_RDWR,0,path_info[__DST_FD].ofd,0,PSX_PATH_OPEN_AT|PSX_PATH_INTERNAL_CALL)))
			return __rename_epilog(tlca,path_info,path_info[__DST_DIR].psxstatus,status);
	} else {
		if ((status = __psx_path_open(tlca,&path_info[__DST_DIR],dst,O_RDWR,0,0,dstfdidx,PSX_PATH_OPEN_AT|PSX_PATH_SKIP_LAST)))
			return __rename_epilog(tlca,path_info,path_info[__DST_DIR].psxstatus,status);
	}

	/* convenience */
	ofd[__SRC_FD]  = path_info[__SRC_FD].ofd;
	ofd[__SRC_DIR] = path_info[__SRC_DIR].ofd;
	ofd[__DST_FD]  = path_info[__DST_FD].ofd;
	ofd[__DST_DIR] = path_info[__DST_DIR].ofd;

	/* type validation */
	if ((ofd[__SRC_DIR]->info.fdtype > PSX_FD_OS_FS_DIR)
			|| (ofd[__SRC_FD]->info.fdtype > PSX_FD_OS_FS_DIR)
			|| (ofd[__DST_FD] && (ofd[__DST_FD]->info.fdtype > PSX_FD_OS_FS_DIR)))
		return __rename_epilog(tlca,path_info,-ENOTDIR,status);
	else if (ofd[__DST_DIR]->info.fdtype > PSX_FD_OS_FS_ROOT)
		return __rename_epilog(tlca,path_info,-EPERM,status);

	/* different types? */
	if (ofd[__DST_FD] && (ofd[__DST_FD]->info.fdtype != ofd[__SRC_FD]->info.fdtype))
		return __rename_epilog(tlca,path_info,-EISDIR,status);

	/* stat */
	if ((status = __iovtbl[ofd[__SRC_DIR]->info.fdtype].stat(tlca,ofd[__SRC_DIR],&xstat[__SRC_DIR])))
		return __rename_epilog(tlca,path_info,-EIO,status);

	if ((status = __iovtbl[ofd[__SRC_FD]->info.fdtype].stat(tlca,ofd[__SRC_FD],&xstat[__SRC_FD])))
		return __rename_epilog(tlca,path_info,-EIO,status);

	if ((status = __iovtbl[ofd[__DST_DIR]->info.fdtype].stat(tlca,ofd[__DST_DIR],&xstat[__DST_DIR])))
		return __rename_epilog(tlca,path_info,-EIO,status);

	if (ofd[__DST_FD] && (status = __iovtbl[ofd[__DST_FD]->info.fdtype].stat(tlca,ofd[__DST_FD],&xstat[__DST_FD])))
		return __rename_epilog(tlca,path_info,-EIO,status);

	/* different devices? */
	if (xstat[__SRC_DIR].st_dev != xstat[__DST_DIR].st_dev)
		return __rename_epilog(tlca,path_info,-EXDEV,status);

	/* same node? */
	if (ofd[__DST_FD] && (xstat[__SRC_FD].st_ino == xstat[__DST_FD].st_ino))
		return __rename_epilog(tlca,path_info,0,NT_STATUS_SUCCESS);

	/* rename file (atomic fs tree) */
	if (ofd[__SRC_FD]->info.fdtype == PSX_FD_OS_FS_FILE) {
		status = __ofd_rename_file(ofd[__DST_DIR],ofd[__SRC_FD],ofd[__DST_FD],&dstrni);
		return __rename_epilog(tlca,path_info,__PSX_TRANSLATE_RET,status);
	}

	/* rename folder (atomic fs tree, dst dir does not exist) */
	if (!ofd[__DST_FD]) {
		status = __ofd_rename_dir(ofd[__DST_DIR],ofd[__SRC_FD],&dstrni,0);
		return __rename_epilog(tlca,path_info,__PSX_TRANSLATE_RET,status);
	}

	/* rename folder (transacted) */
	__ntapi->tt_generic_memcpy(
		(char *)fstxrni.buffer,
		(char *)dstrni.buffer,
		dstrni.strlen);

	wch = fstxrni.buffer + dstrni.strlen/sizeof(wchar16_t);
	__psx_strconv_utf8_to_utf16(tlca,(const char *)fstxtag,wch,sizeof(fstxtag)-1,48*sizeof(wchar16_t),&wchnext);
	fstxrni.strlen = (uint16_t)(wchnext-fstxrni.buffer) * sizeof(wchar16_t);

	rbi.ofdat = ofd[__DST_DIR];
	rbi.ofd   = ofd[__DST_FD];
	rbi.frni  = &dstrni;

	/* directory rename already in progress? */
	if ((status = __ofd_check_fstx_state(ofd[__DST_DIR],&fstxrni)))
		return __rename_epilog(tlca,path_info,-EBUSY,status);

	/* dstdir/dstfd --> dstdir->dstfd.fstx.guid */
	if ((status = __ofd_rename_dir(ofd[__DST_DIR],ofd[__DST_FD],&fstxrni,&rbi)))
		return __rename_epilog(tlca,path_info,__PSX_TRANSLATE_RET,status);

	/* srcdir/srcfd --> dstdir->dstfd */
	if ((status = __ofd_rename_dir(ofd[__DST_DIR],ofd[__SRC_FD],&dstrni,&rbi)))
		return __rename_epilog(tlca,path_info,__PSX_TRANSLATE_RET,status);

	/* mark dstdir->dstfd.fstx.guid for removal */
	if (ofd[__DST_FD])
		__ofd_delattr_set(ofd[__DST_FD]);

	/* yay */
	return __rename_epilog(tlca,path_info,0,NT_STATUS_SUCCESS);
}

__psx_api
intptr_t __sys_rename(const unsigned char * src, const unsigned char * dst)
{
	return __renameat(AT_FDCWD,src,AT_FDCWD,dst);
}

__psx_api
intptr_t __sys_renameat(int srcfd, const unsigned char * src, int dstfd, const unsigned char * dst)
{
	return __renameat(AT_FDCWD,src,AT_FDCWD,dst);
}
