/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_tlca.h"
#include "psx_dirent.h"
#include "psx_limits.h"
#include "psx_errno.h"
#include "psx_debug.h"
#include "psx_fcntl.h"
#include "psx_ofd.h"
#include "psx_path.h"
#include "psx_unicode.h"
#include "psx.h"

static int32_t __dirent_init_cancel(struct __ofd * ofd, size_t * size, int32_t status)
{
	__ntapi->zw_free_virtual_memory(
		NT_CURRENT_PROCESS_HANDLE,
		(void **)&ofd->info.dirctx,size,NT_MEM_RELEASE);

	ofd->info.dirctx = 0;

	return status;
}

uintptr_t buffer[1024];

static unsigned char __dirent_inode_type(uint32_t fattr)
{
	if (fattr & NT_FILE_ATTRIBUTE_DIRECTORY)
		return DT_DIR;
	else if (fattr & NT_FILE_ATTRIBUTE_NORMAL)
		return DT_REG;
	else if (fattr & NT_FILE_ATTRIBUTE_ARCHIVE)
		return DT_REG;
	else if (fattr & NT_FILE_ATTRIBUTE_TEMPORARY)
		return DT_REG;
	else if (fattr & NT_FILE_ATTRIBUTE_SYSTEM)
		return DT_REG;
	else if (fattr & NT_FILE_ATTRIBUTE_COMPRESSED)
		return DT_REG;
	else
		return DT_UNKNOWN;
}

static int32_t __dirent_init(struct __psx_tlca * tlca,struct __ofd * ofd)
{
	int32_t			status;
	size_t			reserve;
	size_t			commit;
	struct __dirctx *	dirctx;
	nt_iosb			iosb;
	struct __path_info	path_info;
	const unsigned char	path[] = {'.','/',0};

	reserve = __PSX_PAGE_SIZE * 1024;
	commit  = __PSX_PAGE_SIZE;

	if (!ofd->info.dirctx) {
		if ((status = __ntapi->zw_allocate_virtual_memory(
				NT_CURRENT_PROCESS_HANDLE,
				&ofd->info.dirctx,0,
				&reserve,
				NT_MEM_RESERVE,
				NT_PAGE_READWRITE)))
			return status;

		if ((status = __ntapi->zw_allocate_virtual_memory(
				NT_CURRENT_PROCESS_HANDLE,
				&ofd->info.dirctx,0,
				&commit,
				NT_MEM_COMMIT,
				NT_PAGE_READWRITE)))
			return __dirent_init_cancel(ofd,&reserve,status);

		dirctx		= (struct __dirctx *)ofd->info.dirctx;
		dirctx->reserve = reserve;
		dirctx->commit	= commit;
		dirctx->free	= (uint32_t)dirctx->commit - (size_t)&(((struct __dirctx *)0)->fsdirents);
		dirctx->used	= 0;
		dirctx->dir	= 0;
	}

	/* 2015 pre-alpha bug: path_open("./") 覆盖 ofd->info.hfile,
	 * 导致 readdir 总是读当前目录。直接使用 opendir 的目录句柄。 */
	if (!dirctx->dir)
		dirctx->dir = ofd;

	if ((status = __iovtbl[ofd->info.fdtype].getvents(
				ofd->info.hfile,
				0,0,0,
				&iosb,
				&dirctx->fsdirents,
				dirctx->free,
				NT_FILE_ID_BOTH_DIRECTORY_INFORMATION,
				0,0,0)))
		return status;

	dirctx->used += (uint32_t)iosb.info;
	dirctx->free -= (uint32_t)iosb.info;
	dirctx->next = (nt_fsdirent *)((uintptr_t)&dirctx->fsdirents + iosb.info);

	if ((status = __iovtbl[ofd->info.fdtype].getdents(
				dirctx->dir->info.hfile,
				0,0,0,
				&iosb,
				dirctx->next,
				dirctx->free,
				NT_FILE_ID_BOTH_DIRECTORY_INFORMATION,
				0,0,0)))
		return status;

	dirctx->next = &dirctx->fsdirents;

	/* todo: loop, add commit as needed */

	return NT_STATUS_SUCCESS;
}

static int32_t __dirent_translate(
	struct __psx_tlca *	tlca,
	nt_fsdirent *		nentry,
	struct __dirent *	dirent)
{
	int32_t				status;
	char *				nullterm;

	if ((status = __psx_strconv_utf16_to_utf8(
			tlca,
			nentry->file_name,
			dirent->d_name,
			nentry->file_name_length,
			sizeof(dirent->d_name),
			&nullterm)))
		return status;

	dirent->d_type = __dirent_inode_type(nentry->file_attributes);
	dirent->d_ino  = (ino_t)nentry->file_id.quad;
	dirent->d_reclen = (uint16_t)(size_t)&((struct __dirent *)0)->d_name;
	dirent->d_reclen += (uint16_t)(nullterm - dirent->d_name) + 1;
	dirent->d_reclen += sizeof(ino_t)-1;
	dirent->d_reclen |= sizeof(ino_t)-1;
	dirent->d_reclen ^= sizeof(ino_t)-1;

	return NT_STATUS_SUCCESS;
}

int32_t __psx_dirent_query(
	struct __psx_tlca *	tlca,
	struct __ofd *		ofd,
	struct __dirent *	dirent,
	unsigned int		count,
	nt_iosb *		iosb)
{
	int32_t			status;
	struct __dirctx *	dirctx;
	nt_fsdirent *		next;

	if (!ofd->info.dirctx && (status = __dirent_init(tlca,ofd)))
		return status;

	dirctx		= (struct __dirctx *)ofd->info.dirctx;
	iosb->pointer	= 0;
	iosb->info	= 0;

	__ntapi->tt_generic_memset(dirent,0,count);

	while ((count>=sizeof(struct __dirent)) && dirctx->next) {
		__dirent_translate(tlca,dirctx->next,dirent);

		count -= dirent->d_reclen;
		iosb->info += dirent->d_reclen;

		next = (dirctx->next->next_entry_offset)
			? (nt_fsdirent *)((uintptr_t)dirctx->next + dirctx->next->next_entry_offset)
			: 0;

		dirent->d_off = next ? (off_t)next - (off_t)&dirctx->fsdirents : -1;
		dirent = (struct __dirent *)((uintptr_t)dirent + dirent->d_reclen);

		dirctx->next = next;
	}

	return NT_STATUS_SUCCESS;
}

int32_t __psx_dirent_seek(struct __ofd * ofd, off_t * offset, int whence)
{
	struct __dirctx *	dirctx;
	nt_fsdirent *		fsdirent;

	dirctx   = (struct __dirctx *)ofd->info.dirctx;
	fsdirent = &dirctx->fsdirents;

	if (whence == SEEK_CUR)
		*offset += (off_t)dirctx->next;

	if (*offset == 0) {
		ofd->info.psxflags |= O_RESCAN;
		return NT_STATUS_SUCCESS;
	}

	while (fsdirent->next_entry_offset)
		if (((off_t)fsdirent - (off_t)&dirctx->fsdirents) == *offset)
			return NT_STATUS_SUCCESS;
		else
			fsdirent = (nt_fsdirent *)((uintptr_t)fsdirent + fsdirent->next_entry_offset);

	return NT_STATUS_INVALID_PARAMETER;
}

int32_t __psx_dirent_free(struct __ofd * ofd)
{
	void *	addr;
	size_t	size;

	if (!ofd->info.dirctx)
		return NT_STATUS_SUCCESS;

	addr = ofd->info.dirctx;
	size = ((struct __dirctx *)ofd->info.dirctx)->reserve;

	return __ntapi->zw_free_virtual_memory(
		NT_CURRENT_PROCESS_HANDLE,
		&addr,&size,
		NT_MEM_RELEASE);
}
