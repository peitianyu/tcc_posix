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

static int32_t __fastcall __psx_iofn_devurand_open_next(
	struct __path_info *	path_info,
	int32_t			index)
{
	return NT_STATUS_NOT_FOUND;
}


static int32_t __stdcall __psx_iofn_devurand_open_parent_directory(
	void **		hparent,
	void *		hdir,
	uintptr_t *	buffer,
	uint32_t	buffer_size,
	uint32_t	desired_access,
	uint32_t	open_options,
	int32_t *	type)
{
	*hparent = 0;
	*type = PSX_FD_OS_DEVICE;
	return NT_STATUS_SUCCESS;
}


void __psx_iofn_devurand_init(struct __iovtbl * iovtbl)
{
	iovtbl->open_next	= __psx_iofn_devurand_open_next;

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
	iovtbl->open		= __psx_iofn_default_open;
	iovtbl->close		= __psx_iofn_default_close;

	iovtbl->read		= __psx_iofn_default_read;
	iovtbl->write		= __psx_iofn_default_write;

	iovtbl->fcntl		= __psx_iofn_default_fcntl;
	iovtbl->ioctl		= __psx_iofn_default_ioctl;

	iovtbl->lock		= __psx_iofn_default_lock;
	iovtbl->unlock		= __psx_iofn_default_unlock;

	iovtbl->query		= __psx_iofn_default_query;
	iovtbl->set		= __psx_iofn_default_set;

	iovtbl->cancel		= __psx_iofn_default_cancel;
	iovtbl->remove		= __psx_iofn_default_remove;

	iovtbl->getdents		= __psx_iofn_default_getdents;
	iovtbl->getvents		= __psx_iofn_default_getvents;

	iovtbl->open_logical_parent	= __psx_iofn_devurand_open_parent_directory;
	iovtbl->open_physical_parent	= __psx_iofn_devurand_open_parent_directory;
}
