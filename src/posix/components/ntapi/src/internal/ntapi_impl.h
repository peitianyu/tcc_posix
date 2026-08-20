/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#ifndef ___NTAPI_IMPL_H_
#define ___NTAPI_IMPL_H_

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_object.h>
#include <ntapi/nt_sysinfo.h>
#include <ntapi/nt_thread.h>
#include <ntapi/nt_process.h>
#include <ntapi/ntapi.h>
#include "ntapi_hash_table.h"
#include "ntapi_context.h"
#include "ntapi_fnapi.h"

#define __NT_BASED_NAMED_OBJECTS	{'\\','B','a','s','e', \
					'N','a','m','e','d', \
					'O','b','j','e','c','t','s'}

/* helper macros */
#define __NT_ROUND_UP_TO_POWER_OF_2(x,y)(x + (y-1)) & ~(y-1)
#define __NT_IS_MISALIGNED_BUFFER(x)	((!(uintptr_t)x) || ((uintptr_t)x % sizeof(size_t)))
#define __NT_IS_MISALIGNED_LENGTH(x)	(x % sizeof(size_t))
#define __NT_FILE_SYNC_IO		(NT_FILE_SYNCHRONOUS_IO_ALERT|NT_FILE_SYNCHRONOUS_IO_NONALERT)

/* user-defined options: head */
#ifndef __NT_TTY_MONITORS
#define __NT_TTY_MONITORS		0x10
#endif

#ifndef __NT_FORK_CHILD_WAIT_MILLISEC
#define __NT_FORK_CHILD_WAIT_MILLISEC	60000
#endif

#ifndef __NT_SYNC_BLOCK_LOCK_TRIES
#define __NT_SYNC_BLOCK_LOCK_TRIES	1024
#endif
/* user-defined options: tail */

/* internal page size */
#ifndef __NT_INTERNAL_PAGE_SIZE
#define __NT_INTERNAL_PAGE_SIZE		4096
#endif

/* .bss section */
#ifndef __NT_BSS_RESERVED_PAGES
#define __NT_BSS_RESERVED_PAGES		8
#endif

/* runtime buffers */
#define __NT_BSS_ARGV_BUFFER_SIZE	__NT_INTERNAL_PAGE_SIZE * 2

#define __NT_BSS_ARGV_MAX_IDX		__NT_BSS_ARGV_BUFFER_SIZE \
					/ sizeof(uintptr_t)

#define __NT_BSS_ARGS_BUFFER_SIZE	__NT_INTERNAL_PAGE_SIZE \
					* __NT_BSS_RESERVED_PAGES \
					- __NT_BSS_ARGV_BUFFER_SIZE

/* ntapi .bss section structure */
typedef struct ___ntapi_img_sec_bss {
	wchar16_t *	argv_envp_array[__NT_BSS_ARGV_MAX_IDX];
	char		args_envs_buffer[__NT_BSS_ARGS_BUFFER_SIZE];
} __ntapi_img_sec_bss;


/* ntapi library internals */
typedef struct __attr_ptr_size_aligned__ _ntapi_internals {
	nt_runtime_data *				rtdata;
	nt_port_name *					subsystem;
	void *						hport_tty_session;
	void *						hport_tty_daemon;
	void *						hport_tty_debug;
	void *						hport_tty_monitor[__NT_TTY_MONITORS];
	size_t						nt_mem_page_size;
	size_t						nt_mem_allocation_granularity;
	size_t						ntapi_internals_alloc_size;
	void **						csr_port_handle_addr;
	void *						hdev_mount_point_mgr;
	void *						hany[8];
	intptr_t					hlock;
	uintptr_t					v1_pipe_counter;
	ntapi_tt_get_csr_port_handle_addr_by_logic *	tt_get_csr_port_handle_addr_by_logic;
	__ntapi_img_sec_bss *				ntapi_img_sec_bss;
} ntapi_internals;


/* __ntapi_img_sec_data */
typedef struct __attr_ptr_size_aligned__ ___ntapi_img_sec_rdata {
	ntapi_hashed_symbol	__ntapi_import_table[__NT_IMPORTED_SYMBOLS_ARRAY_SIZE];
	ntapi_vtbl * 		__ntapi;
	nt_port_name		__session_name;
	ntapi_internals *	__internals;
} __ntapi_img_sec_rdata;

union __ntapi_img_rdata {
	__ntapi_img_sec_rdata	img_sec_data;
	char			buffer[__NT_INTERNAL_PAGE_SIZE];
};


/* accessor table */
extern	ntapi_vtbl	___ntapi;
extern	ntapi_vtbl	___ntapi_shadow;
#define	__ntapi		(&___ntapi)


/* access to library internals */
ntapi_internals * __cdecl __ntapi_internals(void);


/* debug */
#define __ntidx(x) (&(((ntapi_vtbl *)0)->x)) / sizeof(size_t)


#endif
