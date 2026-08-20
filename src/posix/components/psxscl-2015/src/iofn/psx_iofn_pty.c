/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_path.h"
#include "psx_iovtbl.h"
#include "psx.h"

static int32_t __fastcall __psx_iofn_pty_open_next(
	struct __path_info *	path_info,
	int32_t			index)
{
	return NT_STATUS_NOT_FOUND;
}

static int32_t __stdcall __psx_iofn_pty_open_parent_directory(
	void **		hparent,
	void *		hdir,
	uintptr_t *	buffer,
	uint32_t	buffer_size,
	uint32_t	desired_access,
	uint32_t	open_options,
	int32_t *	type)
{
	*hparent = 0;
	*type = PSX_FD_DEV_PTS;
	return NT_STATUS_SUCCESS;
}

void __psx_iofn_pty_init(struct __iovtbl * iovtbl)
{
	iovtbl->open_next	= __psx_iofn_pty_open_next;

	iovtbl->prolog		= __psx_iofn_default_prolog;
	iovtbl->epilog		= __psx_iofn_default_epilog;

	iovtbl->alloc		= __psx_iofn_default_alloc;
	iovtbl->free		= __psx_iofn_default_free;

	iovtbl->stat		= __psx_iofn_default_stat;
	iovtbl->unlink		= __psx_iofn_default_unlink;

	iovtbl->poll		= __psx_iofn_default_poll;
	iovtbl->peek		= __psx_iofn_default_peek;

	iovtbl->fsync		= __psx_iofn_default_fsync;
	iovtbl->notify		= __psx_iofn_default_notify;

	iovtbl->create		= __psx_iofn_default_create;
	iovtbl->open		= (ntapi_zw_open_file *)__ntapi->pty_open;
	iovtbl->close		= (ntapi_zw_close *)__ntapi->pty_close;

	iovtbl->read		= (ntapi_zw_read_file *)__ntapi->pty_read;
	iovtbl->write		= (ntapi_zw_write_file *)__ntapi->pty_write;

	iovtbl->fcntl		= __psx_iofn_default_fcntl;
	iovtbl->ioctl		= __psx_iofn_default_ioctl;

	iovtbl->lock		= __psx_iofn_default_lock;
	iovtbl->unlock		= __psx_iofn_default_unlock;

	iovtbl->query		= __psx_iofn_default_query;
	iovtbl->set		= __psx_iofn_default_set;

	iovtbl->cancel		= (ntapi_zw_cancel_io_file *)__ntapi->pty_cancel;
	iovtbl->remove		= __psx_iofn_default_remove;

	iovtbl->getdents		= __psx_iofn_default_getdents;
	iovtbl->getvents		= __psx_iofn_default_getvents;

	iovtbl->open_logical_parent  = __psx_iofn_pty_open_parent_directory;
	iovtbl->open_physical_parent = __psx_iofn_pty_open_parent_directory;
}
