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

void __psx_iofn_pipe_init(struct __iovtbl * iovtbl)
{
	iovtbl->open_next	= __psx_iofn_default_open_next;

	iovtbl->prolog		= __psx_iofn_default_prolog;
	iovtbl->epilog		= __psx_iofn_default_epilog;

	iovtbl->alloc		= __psx_iofn_default_alloc;
	iovtbl->free		= __psx_iofn_default_free;

	iovtbl->stat		= __psx_stat;
	iovtbl->unlink		= __psx_iofn_default_unlink;

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

	iovtbl->getdents		= __psx_iofn_default_getdents;
	iovtbl->getvents		= __psx_iofn_default_getvents;

	iovtbl->open_logical_parent  = __ntapi->tt_open_logical_parent_directory;
	iovtbl->open_physical_parent = __ntapi->tt_open_physical_parent_directory;
}
