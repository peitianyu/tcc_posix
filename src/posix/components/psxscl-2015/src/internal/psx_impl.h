/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#ifndef _PSX_IMPL_H_
#define _PSX_IMPL_H_

#include <ntapi/ntapi.h>
#include <ntapi/nt_auxv.h>
#include "psx_systypes.h"
#include "psx_limits.h"
#include "psx_daemon.h"
#include "psx_device.h"
#include "psx_path.h"
#include "psx_wintls.h"
#include "psx_ofd.h"
#include "psx_sigfn.h"
#include "psx_ctx.h"
#include "psx_iovtbl.h"

/* state */
extern struct __psx_state	__psx;

/* handlers */
extern uintptr_t *		__sysvtbl[__PSX_SYSCALLS];
extern struct __iovtbl		__iovtbl[PSX_FD_TYPE_CAP];
extern sigafn_t			__sigvtbl[64];

/* native api vtable */
extern	ntapi_vtbl		___ntapi;
#define	__ntapi			(&___ntapi)


/*+-+-+-+-+-+-+-+-+-+-*/
/* convenience macros */
/*-+-+-+-+-+-+-+-+-+-+*/

/* internal synchronization */
#define ofd_lock		__psx.__ofd_lock
#define heap_lock		__psx.__heap_lock
#define mman_lock		__psx.__mman_lock
#define pid_lock		__psx.__pid_lock
#define sigfn_lock		__psx.__sigfn_lock

/* runtime data */
#define rtdata			__psx.__rtdata

/* ctx */
#define rtctx			__psx.__rtctx

/* tls indexes */
#define wintls_sys_idx		__psx.__wintls_sys_idx
#define	wintls_libc_idx		__psx.__wintls_libc_idx

/* daemon */
#define	daemon_keys		__psx.__daemon_keys
#define	daemon_attr		__psx.__daemon_attr
#define	daemon_name		__psx.__daemon_name

#define hport_daemon		__psx.__hport_daemon
#define hport_internal_client	__psx.__hport_internal_client

/* tty session */
#define	hterminal		__psx.__hterminal
#define	hport_tty		__psx.__hport_tty
#define hport_vms		__psx.__hport_vms

/* user environment */
#define dos_drives		__psx.__dos_drives
#define pthreads		__psx.__pthreads

/* virtual mount system */
#define vms_section_handle	__psx.__vms_section_handle
#define	vms_section_addr	__psx.__vms_section_addr
#define vms_section_size	__psx.__vms_section_size

/* brk */
#define brk_base		__psx.__brk_base
#define	brk_size		__psx.__brk_size
#define	brk_cap			__psx.__brk_cap

/* tls function pointers */
#define tls_alloc		__psx.__pfn_winapi_tls_alloc
#define tls_free		__psx.__pfn_winapi_tls_free
#define tls_get_value		__psx.__pfn_winapi_tls_get_value
#define tls_set_value		__psx.__pfn_winapi_tls_set_value

#endif
