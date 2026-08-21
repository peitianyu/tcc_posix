/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_path.h"
#include "psx_iovtbl.h"
#include "psx_dirent.h"
#include "psx.h"

static int32_t __fastcall __psx_iofn_fsroot_open_next(
	struct __path_info *	path_info,
	int32_t			index)
{
	int32_t			status;
	nt_unicode_string	path;

	/* file system entry? */
	if (!(status = __iovtbl[PSX_FD_OS_FS_DIR].open_next(path_info,index)))
		return status;
	else
		path_info->hfile = 0;

	/* path: without the leading slash */
	path.strlen = (uint16_t)((uintptr_t)path_info->outmark[1] - (uintptr_t)path_info->outmark[0]);
	path.buffer = (uint16_t *)path_info->outmark[0];

	if ((*(path.buffer)=='/') || (*(path.buffer) == '\\')) {
		path.strlen -= sizeof(uint16_t);
		path.buffer++;
	}

	/* virtual folder? */
	/* 修复: 原实现用 *buf++ 跨分支共享递增同一指针, 3 字符目录 "etc" 会在
	 * "dev" 分支的 strlen==3 判断通过后被 +1 跳过 'e', 导致 /etc 永远失配.
	 * 改为按索引比较, 不破坏 path.buffer. */
	{
		const wchar16_t *	cand;
		uint32_t		n;

		cand = path.buffer;
		n    = path.strlen / sizeof(wchar16_t);

		if ((n == 3) && (cand[0]=='d') && (cand[1]=='e') && (cand[2]=='v')) {
			path_info->fdtype = PSX_FD_OS_DEVICE;
			return NT_STATUS_SUCCESS;
		}

		if ((n == 4) && (cand[0]=='p') && (cand[1]=='r')
				&& (cand[2]=='o') && (cand[3]=='c')) {
			path_info->fdtype = PSX_FD_OS_PROCFS;
			return NT_STATUS_SUCCESS;
		}

		if ((n == 3) && (cand[0]=='e') && (cand[1]=='t') && (cand[2]=='c')) {
			path_info->fdtype = PSX_FD_OS_CONFIG;
			return NT_STATUS_SUCCESS;
		}
	}

	return status;
}


static int32_t __stdcall __psx_iofn_fsroot_open_parent_directory(
	void **		hparent,
	void *		hdir,
	uintptr_t *	buffer,
	uint32_t	buffer_size,
	uint32_t	desired_access,
	uint32_t	open_options,
	int32_t *	type)
{
	*hparent = hdir;
	return NT_STATUS_SUCCESS;
}

int32_t	__stdcall __psx_iofn_fsroot_getvents(
	void *			hfile,
	void *			hevent,
	nt_io_apc_routine *	apc_routine,
	void *			apc_context,
	nt_io_status_block *	iosb,
	void *			file_info,
	uint32_t		file_info_length,
	nt_file_info_class	file_info_class,
	unsigned char		return_single_entry,
	nt_unicode_string *	file_name,
	unsigned char		restart_scan)
{
	/* todo */
	iosb->pointer	= 0;
	iosb->status	= 0;
	return iosb->status;
}

void __psx_iofn_fsroot_init(struct __iovtbl * iovtbl)
{
	iovtbl->open_next	= __psx_iofn_fsroot_open_next;

	iovtbl->prolog		= __psx_iofn_default_prolog;
	iovtbl->epilog		= __psx_iofn_default_epilog;

	iovtbl->alloc		= __psx_iofn_default_alloc;
	iovtbl->free		= __psx_dirent_free;

	iovtbl->stat		= __psx_iofn_default_stat;
	iovtbl->unlink		= __psx_iofn_default_unlink;

	iovtbl->poll		= __psx_iofn_default_poll;
	iovtbl->peek		= __psx_iofn_default_peek;

	iovtbl->fsync		= __psx_iofn_default_fsync;
	iovtbl->notify		= __psx_iofn_default_notify;

	iovtbl->create		= __ntapi->zw_create_file;
	iovtbl->open		= __ntapi->zw_open_file;
	iovtbl->close		= __psx_iofn_default_close;

	iovtbl->read		= __ntapi->zw_read_file;
	iovtbl->write		= __ntapi->zw_write_file;

	iovtbl->fcntl		= __ntapi->zw_fs_control_file;
	iovtbl->ioctl		= __ntapi->zw_device_io_control_file;

	iovtbl->lock		= __ntapi->zw_lock_file;
	iovtbl->unlock		= __ntapi->zw_unlock_file;

	iovtbl->query		= __ntapi->zw_query_information_file;
	iovtbl->set		= __ntapi->zw_set_information_file;

	iovtbl->cancel		= __ntapi->zw_cancel_io_file;
	iovtbl->remove		= __ntapi->zw_delete_file;

	iovtbl->getdents		= __ntapi->zw_query_directory_file;
	iovtbl->getvents		= __psx_iofn_fsroot_getvents;

	iovtbl->open_logical_parent  = __psx_iofn_fsroot_open_parent_directory;
	iovtbl->open_physical_parent = __psx_iofn_fsroot_open_parent_directory;
}
