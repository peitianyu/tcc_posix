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

static int32_t __fastcall __psx_iofn_device_open_next(
	struct __path_info *	path_info,
	int32_t			index)
{
	nt_unicode_string	path;

	/* path: without the leading slash */
	path.strlen = (uint16_t)(path_info->outmark[1]-path_info->outmark[0]) * sizeof(uint16_t);
	path.buffer = (uint16_t *)path_info->outmark[0];

	if ((*(path.buffer)=='/') || (*(path.buffer) == '\\')) {
		path.strlen -= sizeof(uint16_t);
		path.buffer++;
	}

	/* /dev/null? */
	if ((path.strlen == 4*sizeof(wchar16_t))
			&& (*path.buffer++ == 'n')
			&& (*path.buffer++ == 'u')
			&& (*path.buffer++ == 'l')
			&& (*path.buffer++ == 'l')) {
		path_info->fdtype = PSX_FD_DEV_NULL;
		return NT_STATUS_SUCCESS;

	/* /dev/zero? */
	} else if ((path.strlen == 4*sizeof(wchar16_t))
			&& (*path.buffer++ == 'z')
			&& (*path.buffer++ == 'e')
			&& (*path.buffer++ == 'r')
			&& (*path.buffer++ == 'o')) {
		path_info->fdtype = PSX_FD_DEV_ZERO;
		return NT_STATUS_SUCCESS;

	/* /dev/ptmx? */
	} else if ((path.strlen == 4*sizeof(wchar16_t))
			&& (*path.buffer++ == 'p')
			&& (*path.buffer++ == 't')
			&& (*path.buffer++ == 'm')
			&& (*path.buffer++ == 'x')) {
		path_info->fdtype = PSX_FD_DEV_PTMX;
		return NT_STATUS_NOT_FOUND;

	/* /dev/pts? */
	} else if ((path.strlen == 3*sizeof(wchar16_t))
			&& (*path.buffer++ == 'p')
			&& (*path.buffer++ == 't')
			&& (*path.buffer++ == 's')) {
		path_info->fdtype = PSX_FD_DEV_PTMX;
		return NT_STATUS_NOT_FOUND;

	/* /dev/random? */
	} else if ((path.strlen == 6*sizeof(wchar16_t))
			&& (*path.buffer++ == 'r')
			&& (*path.buffer++ == 'a')
			&& (*path.buffer++ == 'n')
			&& (*path.buffer++ == 'd')
			&& (*path.buffer++ == 'o')
			&& (*path.buffer++ == 'm')) {
		path_info->fdtype = PSX_FD_DEV_RANDOM;
		return NT_STATUS_SUCCESS;

	/* /dev/urandom? */
	} else if ((path.strlen == 7*sizeof(wchar16_t))
			&& (*path.buffer++ == 'u')
			&& (*path.buffer++ == 'r')
			&& (*path.buffer++ == 'a')
			&& (*path.buffer++ == 'n')
			&& (*path.buffer++ == 'd')
			&& (*path.buffer++ == 'o')
			&& (*path.buffer++ == 'm')) {
		path_info->fdtype = PSX_FD_DEV_URANDOM;
		return NT_STATUS_SUCCESS;

	/* /dev/mailslot? */
	} else if ((path.strlen == 8*sizeof(wchar16_t))
			&& (*path.buffer++ == 'm')
			&& (*path.buffer++ == 'a')
			&& (*path.buffer++ == 'i')
			&& (*path.buffer++ == 'l')
			&& (*path.buffer++ == 's')
			&& (*path.buffer++ == 'l')
			&& (*path.buffer++ == 'o')
			&& (*path.buffer++ == 't')) {
		path_info->fdtype = PSX_FD_OS_MAILSLOT;
		return NT_STATUS_SUCCESS;

	/* /dev/mntmgr? */
	} else if ((path.strlen == 6*sizeof(wchar16_t))
			&& (*path.buffer++ == 'm')
			&& (*path.buffer++ == 'n')
			&& (*path.buffer++ == 't')
			&& (*path.buffer++ == 'm')
			&& (*path.buffer++ == 'g')
			&& (*path.buffer++ == 'r')) {
		path_info->fdtype = PSX_FD_DEV_MNTMGR;
		return NT_STATUS_SUCCESS;

	/* /dev/tty? */
	} else if ((path.strlen == 3*sizeof(wchar16_t))
			&& (*path.buffer++ == 't')
			&& (*path.buffer++ == 't')
			&& (*path.buffer++ == 'y')) {
		if (rtctx.ctty) {
			path_info->fdtype = PSX_FD_DEV_TTY;
			path_info->hfile  = rtctx.ctty->info.hpty;
			return NT_STATUS_SUCCESS;
		} else
			return NT_STATUS_ACCESS_DENIED;
	} else
		return NT_STATUS_NOT_FOUND;
}

static int32_t __stdcall __psx_iofn_device_open_parent_directory(
	void **		hparent,
	void *		hdir,
	uintptr_t *	buffer,
	uint32_t	buffer_size,
	uint32_t	desired_access,
	uint32_t	open_options,
	int32_t *	type)
{
	*hparent = (__tlca_self())->ctx->root.hfile;
	*type = PSX_FD_OS_FS_ROOT;
	return NT_STATUS_SUCCESS;
}


void __psx_iofn_device_init(struct __iovtbl * iovtbl)
{
	iovtbl->open_next	= __psx_iofn_device_open_next;

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

	iovtbl->open_logical_parent	= __psx_iofn_device_open_parent_directory;
	iovtbl->open_physical_parent	= __psx_iofn_device_open_parent_directory;
}
