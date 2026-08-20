/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#ifndef _PSX_IOVTBL_H_
#define _PSX_IOVTBL_H_

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_path.h"
#include "psx_stat.h"
#include "psx_ofd.h"

struct __iovtbl;

typedef void	__fastcall __psx_iofn_init	(struct __iovtbl *);
typedef int32_t	__fastcall __psx_iofn_open_next	(struct __path_info *, int32_t index);
typedef int32_t	__fastcall __psx_iofn_prolog	(struct __psx_tlca *, struct __ofd *, struct __ofd **);
typedef void	__fastcall __psx_iofn_epilog	(struct __ofd *, struct __ofd *);
typedef int32_t	__fastcall __psx_iofn_alloc	(struct __path_info *);
typedef int32_t __fastcall __psx_iofn_free	(struct __ofd *);
typedef int32_t __fastcall __psx_iofn_stat	(struct __psx_tlca *, struct __ofd *, struct __stat *);
typedef int32_t __fastcall __psx_iofn_unlink	(struct __ofd *, uint32_t flags);
typedef int32_t __fastcall __psx_iofn_poll	(struct __pollofd *, uint32_t flags);
typedef int32_t __fastcall __psx_iofn_peek	(struct __ofd *);
typedef int32_t __fastcall __psx_iofn_fsync	(struct __ofd *, nt_iosb *);
typedef int32_t __fastcall __psx_iofn_notify	(struct __ofd *, struct __ofd *, int action, uint32_t mask);

struct __iovtbl {
	__psx_iofn_init *			init;
	__psx_iofn_open_next *			open_next;

	__psx_iofn_prolog *			prolog;
	__psx_iofn_epilog *			epilog;

	__psx_iofn_alloc *			alloc;
	__psx_iofn_free *			free;

	__psx_iofn_stat *			stat;
	__psx_iofn_unlink *			unlink;

	__psx_iofn_poll *			poll;
	__psx_iofn_peek *			peek;

	__psx_iofn_fsync *			fsync;
	__psx_iofn_notify *			notify;

	ntapi_zw_create_file *			create;
	ntapi_zw_open_file *			open;
	ntapi_zw_close *			close;

	ntapi_zw_read_file *			read;
	ntapi_zw_write_file *			write;

	ntapi_zw_fs_control_file *		fcntl;
	ntapi_zw_device_io_control_file *	ioctl;

	ntapi_zw_lock_file *			lock;
	ntapi_zw_unlock_file *			unlock;

	ntapi_zw_query_information_file *	query;
	ntapi_zw_set_information_file *		set;

	ntapi_zw_cancel_io_file *		cancel;
	ntapi_zw_delete_file *			remove;

	ntapi_zw_query_directory_file *		getdents;
	ntapi_zw_query_directory_file *		getvents;

	ntapi_tt_open_logical_parent_directory *open_logical_parent;
	ntapi_tt_open_physical_parent_directory*open_physical_parent;
};

extern	__psx_iofn_open_next			__psx_iofn_default_open_next;
extern	__psx_iofn_prolog			__psx_iofn_default_prolog;
extern	__psx_iofn_epilog			__psx_iofn_default_epilog;
extern	__psx_iofn_alloc			__psx_iofn_default_alloc;
extern	__psx_iofn_free				__psx_iofn_default_free;
extern	__psx_iofn_stat				__psx_iofn_default_stat;
extern	__psx_iofn_unlink			__psx_iofn_default_unlink;
extern	__psx_iofn_poll				__psx_iofn_default_poll;
extern	__psx_iofn_peek				__psx_iofn_default_peek;
extern	__psx_iofn_fsync			__psx_iofn_default_fsync;
extern	__psx_iofn_notify			__psx_iofn_default_notify;
extern	ntapi_zw_create_file 			__psx_iofn_default_create;
extern	ntapi_zw_open_file 			__psx_iofn_default_open;
extern	ntapi_zw_close				__psx_iofn_default_close;
extern	ntapi_zw_read_file 			__psx_iofn_default_read;
extern	ntapi_zw_write_file 			__psx_iofn_default_write;
extern	ntapi_zw_fs_control_file		__psx_iofn_default_fcntl;
extern	ntapi_zw_device_io_control_file 	__psx_iofn_default_ioctl;
extern	ntapi_zw_lock_file 			__psx_iofn_default_lock;
extern	ntapi_zw_unlock_file 			__psx_iofn_default_unlock;
extern	ntapi_zw_query_information_file 	__psx_iofn_default_query;
extern	ntapi_zw_set_information_file 		__psx_iofn_default_set;
extern	ntapi_zw_cancel_io_file 		__psx_iofn_default_cancel;
extern	ntapi_zw_delete_file 			__psx_iofn_default_remove;
extern	ntapi_zw_query_directory_file		__psx_iofn_default_getdents;
extern	ntapi_zw_query_directory_file		__psx_iofn_default_getvents;
extern	ntapi_tt_open_logical_parent_directory	__psx_iofn_default_open_logical_parent;
extern	ntapi_tt_open_physical_parent_directory	__psx_iofn_default_open_physical_parent;

extern	__psx_iofn_init				__psx_iofn_fsfile_init;
extern	__psx_iofn_init				__psx_iofn_fsdir_init;
extern	__psx_iofn_init				__psx_iofn_fsroot_init;

extern	__psx_iofn_init				__psx_iofn_pipe_init;
extern	__psx_iofn_init				__psx_iofn_socket_init;
extern	__psx_iofn_init				__psx_iofn_mailslot_init;
extern	__psx_iofn_init				__psx_iofn_config_init;
extern	__psx_iofn_init				__psx_iofn_device_init;
extern	__psx_iofn_init				__psx_iofn_procfs_init;
extern	__psx_iofn_init				__psx_iofn_mount_init;
extern	__psx_iofn_init				__psx_iofn_udp_init;
extern	__psx_iofn_init				__psx_iofn_pty_init;
extern	__psx_iofn_init				__psx_iofn_vfd_init;

extern	__psx_iofn_init				__psx_iofn_devnull_init;
extern	__psx_iofn_init				__psx_iofn_devzero_init;
extern	__psx_iofn_init				__psx_iofn_devptmx_init;
extern	__psx_iofn_init				__psx_iofn_devpts_init;
extern	__psx_iofn_init				__psx_iofn_devtty_init;
extern	__psx_iofn_init				__psx_iofn_devrand_init;
extern	__psx_iofn_init				__psx_iofn_devurand_init;
extern	__psx_iofn_init				__psx_iofn_devmnt_init;

extern	__psx_iofn_init				__psx_iofn_procself_init;
extern	__psx_iofn_init				__psx_iofn_procnet_init;
extern	__psx_iofn_init				__psx_iofn_procsys_init;
extern	__psx_iofn_init				__psx_iofn_procipc_init;
extern	__psx_iofn_init				__psx_iofn_procreg_init;

#endif
