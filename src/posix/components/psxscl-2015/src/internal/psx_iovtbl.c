/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_iovtbl.h"
#include "psx_ofd.h"

struct __iovtbl __iovtbl[PSX_FD_TYPE_CAP] = {
	{
		0,
		__psx_iofn_default_open_next,
		__psx_iofn_default_prolog,
		__psx_iofn_default_epilog,
		__psx_iofn_default_alloc,
		__psx_iofn_default_free,
		__psx_iofn_default_stat,
		__psx_iofn_default_unlink,
		__psx_iofn_default_poll,
		__psx_iofn_default_peek,
		__psx_iofn_default_fsync,
		__psx_iofn_default_notify,
		__psx_iofn_default_create,
		__psx_iofn_default_open,
		__psx_iofn_default_close,
		__psx_iofn_default_read,
		__psx_iofn_default_write,
		__psx_iofn_default_fcntl,
		__psx_iofn_default_ioctl,
		__psx_iofn_default_lock,
		__psx_iofn_default_unlock,
		__psx_iofn_default_query,
		__psx_iofn_default_set,
		__psx_iofn_default_cancel,
		__psx_iofn_default_remove,
		__psx_iofn_default_getdents,
		__psx_iofn_default_getvents,
		__psx_iofn_default_open_logical_parent,
		__psx_iofn_default_open_physical_parent
	},

	/* file system */
	{__psx_iofn_fsfile_init},
	{__psx_iofn_fsdir_init},
	{__psx_iofn_fsroot_init},

	/* base named objects */
	{0},
	{0},
	{0},

	/* registry */
	{0},
	{0},
	{0},

	/* misc. */
	{__psx_iofn_pipe_init},
	{__psx_iofn_socket_init},
	{__psx_iofn_mailslot_init},
	{__psx_iofn_config_init},
	{__psx_iofn_device_init},
	{__psx_iofn_procfs_init},
	{__psx_iofn_mount_init},

	/* subsystem */
	{__psx_iofn_udp_init},
	{__psx_iofn_pty_init},
	{__psx_iofn_vfd_init},

	{__psx_iofn_devnull_init},
	{__psx_iofn_devzero_init},
	{__psx_iofn_devptmx_init},
	{__psx_iofn_devpts_init},
	{__psx_iofn_devtty_init},
	{__psx_iofn_devrand_init},
	{__psx_iofn_devurand_init},
	{__psx_iofn_devmnt_init},

	{__psx_iofn_procself_init},
	{__psx_iofn_procnet_init},
	{__psx_iofn_procsys_init},
	{__psx_iofn_procipc_init},
	{__psx_iofn_procreg_init}
};
