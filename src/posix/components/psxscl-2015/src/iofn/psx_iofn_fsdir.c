/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_path.h"
#include "psx_iovtbl.h"
#include "psx_unlink.h"
#include "psx_dirent.h"
#include "psx.h"

static int32_t __fastcall __psx_iofn_fsdir_open_next(
	struct __path_info *	path_info,
	int32_t			index)
{
	nt_oa			oa;
	nt_iosb			iosb;
	nt_unicode_string	path;
	nt_large_integer	alloc_size;

	/* fdtype (cf. __path_update_type) */
	path_info->fdtype = PSX_FD_OS_FS_DIR;

	/* needed? */
	if (path_info->outmark[1] == path_info->outbuf)
		return NT_STATUS_MORE_ENTRIES;

	/* safe alloc_size */
	switch (path_info->ntdisposition) {
		case NT_FILE_CREATE:
		case NT_FILE_OPEN:
		case NT_FILE_OPEN_IF:
			alloc_size.quad = 0;
			break;

		default:
			return NT_STATUS_INTERNAL_ERROR;
	}


	/* path: without the leading slash */
	path.maxlen = 0;
	path.strlen = (uint16_t)(path_info->outmark[1]-path_info->outmark[0]) * sizeof(uint16_t);
	path.buffer = path_info->outmark[0];

	if ((*(path.buffer)=='/') || (*(path.buffer) == '\\')) {
		path.strlen -= sizeof(uint16_t);
		path.buffer++;
	}

	/* oa */
	oa.len = sizeof(nt_oa);
	oa.root_dir = path_info->hat;
	oa.obj_name = &path;
	oa.obj_attr = path_info->ntobjattr;
	oa.sec_desc = 0;
	oa.sec_qos  = 0;

	/* open/create file/folder */
	return __ntapi->zw_create_file(
		&path_info->hfile,
		NT_SEC_SYNCHRONIZE | NT_FILE_READ_ATTRIBUTES | path_info->ntaccess,
		&oa,&iosb,
		&alloc_size,
		path_info->ntattr,
		path_info->ntshare,
		path_info->ntdisposition,
		path_info->ntoptions | path_info->dirflags | NT_FILE_SYNCHRONOUS_IO_ALERT,
		0,0);
}

int32_t	__stdcall __psx_iofn_fsdir_getvents(
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
	iosb->info	= 0;
	iosb->pointer	= 0;
	return iosb->status;
}

void __psx_iofn_fsdir_init(struct __iovtbl * iovtbl)
{
	iovtbl->open_next	= __psx_iofn_fsdir_open_next;

	iovtbl->prolog		= __psx_iofn_default_prolog;
	iovtbl->epilog		= __psx_iofn_default_epilog;

	iovtbl->alloc		= __psx_iofn_default_alloc;
	iovtbl->free		= __psx_dirent_free;

	iovtbl->stat		= __psx_stat;
	iovtbl->unlink		= __psx_unlink;

	iovtbl->poll		= __psx_iofn_default_poll;
	iovtbl->peek		= __psx_iofn_default_peek;

	iovtbl->fsync		= __psx_iofn_default_fsync;
	iovtbl->notify		= __psx_iofn_default_notify;

	iovtbl->create		= __ntapi->zw_create_file;
	iovtbl->open		= __ntapi->zw_open_file;
	iovtbl->close		= __ntapi->zw_close;

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
	iovtbl->getvents		= __psx_iofn_fsdir_getvents;

	iovtbl->open_logical_parent  = __ntapi->tt_open_logical_parent_directory;
	iovtbl->open_physical_parent = __ntapi->tt_open_physical_parent_directory;
}
