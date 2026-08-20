/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#ifndef _PSX_CTX_H_
#define _PSX_CTX_H_

#include <dalist/dalist.h>
#include <ntapi/ntapi.h>
#include <ntapi/nt_auxv.h>
#include "psx_systypes.h"
#include "psx_limits.h"
#include "psx_daemon.h"
#include "psx_device.h"
#include "psx_path.h"
#include "psx_wintls.h"
#include "psx_ofd.h"
#include "psx_timer.h"

#define PSX_CTX_TOP_LEVEL	0x00000000
#define PSX_CTX_FORK_CHILD	0x00000001
#define PSX_CTX_EXEC_CHILD	0x00000002

struct __psx_ctx {
	void *			hsection;
	void *			addr;
	size_t			size;
	size_t			commit;
	int32_t			envc;
	int32_t			argc;
	char **			argv_utf8;
	char **			envp_utf8;
	nt_auxv_t *		auxv;
	wchar16_t **		argv_utf16;
	wchar16_t **		envp_utf16;

	void *			fd_sec;
	struct __fd *		fd_slots;
	void *			fd_bitmap_sec;
	void *			fd_bitmap_addr;
	nt_blitter **		fd_blt_ctx;
	nt_blitter *		fd_blt_ctx_array[__PSX_OFD_CAP / __PSX_BITS_PER_PAGE];

	void *			ofd_sec;
	struct __ofd *		ofd_slots;
	void *			ofd_bitmap_sec;
	void *			ofd_bitmap_addr;
	nt_blitter **		ofd_blt_ctx;
	nt_blitter *		ofd_blt_ctx_array[__PSX_OFD_CAP / __PSX_BITS_PER_PAGE];

	int32_t			fd_cap;
	int32_t			ofd_cap;

	struct __path_info	cwd;
	struct __path_info	root;
	struct __ofd *		ctty;

	struct __timer		timer[__PSX_ITIMER_CAP];

	nt_guid			guid_self;
	nt_guid			guid_parent;
	nt_guid			guid_session;
	nt_guid			guid_reserved;

	void *			hrecsec;
	struct dalist_ex	peers;
	struct dalist_ex	offsprings;
	struct dalist_ex	sections;
};

struct __psx_state {
	/* runtime data */
	nt_rtdata *		__rtdata;

	/* ctx */
	struct __psx_ctx	__rtctx;

	/* internal synchronization */
	nt_sync_block		__ofd_lock;
	nt_sync_block		__heap_lock;
	nt_sync_block		__mman_lock;
	nt_sync_block		__pid_lock;
	nt_sync_block		__sigfn_lock;

	/* system tls indexes */
	uint32_t		__wintls_sys_idx;
	uint32_t		__wintls_libc_idx;

	/* daemon */
	nt_port_keys		__daemon_keys;
	nt_port_attr		__daemon_attr;
	nt_port_name		__daemon_name;

	void *			__hport_daemon;
	void *			__hevent_daemon_ready;

	void *			__hport_internal_client;
	void *			__hevent_internal_client_ready;

	/* tty session */
	void *			__hterminal;
	void *			__hport_tty;
	void *			__hport_vms;

	/* user environment */
	struct __dos_drive	__dos_drives[26];
	intptr_t		__pthreads;

	/* brk */
	uintptr_t		__brk_base;
	ssize_t			__brk_size;
	size_t			__brk_cap;

	/* tls function pointers */
	winapi_tls_alloc *	__pfn_winapi_tls_alloc;
	winapi_tls_free *	__pfn_winapi_tls_free;
	winapi_tls_get_value *	__pfn_winapi_tls_get_value;
	winapi_tls_set_value *	__pfn_winapi_tls_set_value;

	/* token */
	uid_t			__uid;
	uid_t			__euid;
	gid_t			__gid;
	gid_t			__egid;

	/* virtual mount system */
	void *			__vms_section_handle;
	void *			__vms_section_addr;
	size_t			__vms_section_size;

	/* convenience */
	uint32_t		__options;
	uint32_t		__flags;
};

#endif
