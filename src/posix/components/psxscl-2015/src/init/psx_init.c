/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include <psxscl/psxscl.h>
#include "psx_systypes.h"
#include "psx_init.h"
#include "psx_ldso.h"
#include "psx_tlca.h"
#include "psx_daemon.h"
#include "psx_impl.h"
#include "psx_debug.h"
#include "psx.h"

/* pre-alpha */
#ifndef PSX_INTERNAL_STRACE
#define PSX_INTERNAL_STRACE 1
#endif

static __psx_init_routine	__psx_init_impl;
static __psx_init_routine	__psx_terminate_malformed_executable;
static void			__psx_init_abort(int32_t);
static int			__psx_init_vms(void);
static int			__psx_init_iovtbl(void);
static int			__psx_init_wintls_accessors(void);
static int			__psx_tls_index_alloc(void);

static intptr_t			__init_idx = 0;
static __psx_init_routine *	__init_routine[2] = {
					__psx_init_impl,
					__psx_terminate_malformed_executable
				};


static int __init_dbg_dummy = 0;

static int __init_dbg_helper(void)
{
	return __init_dbg_dummy;
}


__psx_api
int __psx_init(
	int *			argc,
	char ***		argv,
	char ***		envp,
	struct __psx_context *	ctx)
{
	intptr_t idx;

	idx = at_locked_cas(&__init_idx,0,1);
	return __init_routine[idx](argc,argv,envp,ctx);
}


static int __psx_terminate_malformed_executable(
	int *			argc,
	char ***		argv,
	char ***		envp,
	struct __psx_context *	ctx)
{
	return __ntapi->zw_terminate_process(
			NT_CURRENT_PROCESS_HANDLE,
			NT_STATUS_ALREADY_REGISTERED);
}

static int __psx_init_impl(
	int *			argc,
	char ***		argv,
	char ***		envp,
	struct __psx_context *	ctx)
{
	int32_t				status;
	ntapi_vtbl *			_ntapi;
	struct __psx_tlca *		tlca;
	struct pe_stack_heap_info	stkinfo;

	if (ctx->size < sizeof(*ctx))
		return NT_STATUS_INVALID_PARAMETER_4;

	if (ctx->options & __PSXOPT_TTYDBG)
		ctx->options |= __PSXOPT_POSIX;

	if ((status = ntapi_init(&_ntapi)))
		return status;

	#ifndef NTAPI_STATIC
	_ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)__ntapi,
		(uintptr_t *)_ntapi,
		sizeof(ntapi_vtbl));
	#endif

	/* winapi accessors */
	if ((status = __psx_init_wintls_accessors()))
		__psx_init_abort(status);

	/* tls indexes */
	if ((status = __psx_tls_index_alloc()))
		__psx_init_abort(status);

	/* argv, envp (peb) */
	if ((status = __ntapi->tt_get_argv_envp_utf8(
			&rtctx.argc,
			&rtctx.argv_utf8,
			&rtctx.envp_utf8,
			0,0,0)))
		__psx_init_abort(status);

	/* internal process information */
	if ((status = __ntapi->tt_get_runtime_data(&rtdata,0)))
		__psx_init_abort(status);

	/* argv, envp (integral) */
	if (rtdata->argv) {
		rtctx.envc	= rtdata->envc;
		rtctx.argc	= rtdata->argc;
		rtctx.argv_utf8 = rtdata->argv;
		rtctx.envp_utf8 = rtdata->envp;
	}

	/* rtdata */
	/* (todo: use _start as a tip for finding base) */
	rtdata->himage = pe_get_first_module_handle();

	if ((status = pe_get_image_stack_heap_info(rtdata->himage,&stkinfo)))
		__psx_init_abort(status);

	rtdata->stack_reserve	= stkinfo.size_of_stack_reserve;
	rtdata->stack_commit	= stkinfo.size_of_stack_commit;
	rtdata->heap_reserve	= stkinfo.size_of_heap_reserve;
	rtdata->heap_commit	= stkinfo.size_of_heap_commit;

	rtdata->ctx_options |= ctx->options;
	rtdata->ctx_counter++;

	/* tlca init */
	if ((status = __psx_tlca_init(&tlca)))
		__psx_init_abort(status);


	/* debug */
	if ((status = __psx_init_dbg()))
		__psx_init_abort(status);

	/* c-o-w context */
	if ((status = __psx_init_ctx()))
		__psx_init_abort(status);


	/* daemon thread */
	if ((status = __psx_daemon_init(0)))
		__psx_init_abort(status);


	/* create or join sub-system session (optional) */
	if ((status = __psx_init_tty()))
		__psx_init_abort(status);

	/* create or join a process group */
	if ((status = __psx_init_pgid()))
		__psx_init_abort(status);

	/* current working directory */
	if ((status = __psx_init_cwd()))
		__psx_init_abort(status);

	/* brk */
	if ((status = __psx_init_brk()))
		__psx_init_abort(status);

	/* mman */
	if ((status = __psx_init_mman()))
		__psx_init_abort(status);

	/* iovtbl */
	if ((status = __psx_init_iovtbl()))
		__psx_init_abort(status);

	/* ofd tables */
	if ((status = __psx_init_ofd(&rtctx)))
		__psx_init_abort(status);

	/* signal */
	if ((status = __psx_init_signal(&rtctx)))
		__psx_init_abort(status);

	while (__init_dbg_helper());

	/* env */
	if ((status = __psx_init_env()))
		__psx_init_abort(status);

	/* session */
	if ((status = __psx_init_session(&rtctx)))
		__psx_init_abort(status);

	/* syscall_vtbl (wip) */
	__psx_populate_syscall_vtbl(0);
	ctx->sys_vtbl = (void ***)__sysvtbl;

	/* token (stub) */
	__psx.__uid  = 1000;
	__psx.__euid = 1000;

	/* context */
	ctx->teb_sys_idx  = wintls_sys_idx;
	ctx->teb_libc_idx = wintls_libc_idx;
	ctx->do_global_ctors_fn = __psx_do_global_ctors;
	ctx->do_global_dtors_fn = __psx_do_global_dtors;

	/* virtual mount system */
	if (rtctx.root.hat && (ctx->options & __PSXOPT_POSIX))
		if ((status = __psx_init_vms()))
			__psx_init_abort(status);

	/* execve */
	if (rtdata->hready) {
		__ntapi->zw_set_event(rtdata->hready,0);
		__ntapi->zw_close(rtdata->hready);
	}

	/* ready set go */
	*argc = rtctx.argc;
	*argv = rtctx.argv_utf8;
	*envp = rtctx.envp_utf8;

	return NT_STATUS_SUCCESS;
}


int __psx_init_ctx(void)
{
	struct __psx_ctx * pctx;

	if (rtdata->ctx_hsection) {
		pctx = (struct __psx_ctx *)rtdata->ctx_addr;
		__ntapi->tt_aligned_block_memcpy(
			(uintptr_t *)&rtctx,
			(uintptr_t *)pctx,
			sizeof(rtctx));
	}

	return NT_STATUS_SUCCESS;
}


static int __psx_init_vms(void)
{
	int32_t			status;
	nt_tty_vms_info		vmsinfo;
	nt_cid			cid;
	nt_oa			oa;
	nt_stat			stat;
	struct __psx_tlca *	tlca;

	/* root directory native stat */
	tlca = __tlca_self();
	status = __ntapi->tt_stat(
		rtctx.root.hat,
		(void *)0,
		(nt_unicode_string *)0,
		&stat,
		tlca->buffer,
		(uint32_t)tlca->buflen,
		0,
		0);

	__psx_init_abort(status);

	/* vms query */
	status = __ntapi->tty_vms_query(0, &vmsinfo);
	__psx_init_abort(status);

	/* handle to sub-system process */
	oa.len		= sizeof(oa);
	oa.root_dir	= (void *)0;
	oa.obj_name	= (nt_unicode_string *)0;
	oa.obj_attr	= 0;
	oa.sec_desc	= (nt_sd *)0;
	oa.sec_qos	= (nt_sqos *)0;

	cid.process_id	= vmsinfo.vms_keys.key[0];
	cid.thread_id	= 0;

	status = __ntapi->zw_open_process(
		&hterminal,
		NT_PROCESS_DUP_HANDLE | NT_PROCESS_SYNCHRONIZE,
		&oa,
		&cid);

	__psx_init_abort(status);

	/* pass handle to sub-system */
	status = __ntapi->zw_duplicate_object(
		NT_CURRENT_PROCESS_HANDLE,
		stat.hfile,
		hterminal,
		&vmsinfo.hroot,
		0,
		0,
		NT_DUPLICATE_SAME_ATTRIBUTES | NT_DUPLICATE_SAME_ACCESS);

	__psx_init_abort(status);

	/* request vmount daemon */
	vmsinfo.hash	= stat.dev_name_hash;
	vmsinfo.key	= stat.fii.index_number.quad;
	vmsinfo.flags	= 0;

	status = __ntapi->tty_vms_request(0, &vmsinfo);
	__psx_init_abort(status);

	/* connect to vmount daemon */
	status = __ntapi->vms_client_connect(&hport_vms,&vmsinfo);
	__psx_init_abort(status);

	status = __ntapi->vms_table_query(hport_vms,(nt_vms_daemon_info *)tlca->buffer);

	/* vms section mapping */
	vms_section_addr	= (void *)0;
	vms_section_size	= ((nt_vms_daemon_info *)tlca->buffer)->section_size;
	vms_section_handle	= ((nt_vms_daemon_info *)tlca->buffer)->section_handle;

	status = __ntapi->zw_map_view_of_section(
		vms_section_handle,
		NT_CURRENT_PROCESS_HANDLE,
		&vms_section_addr,
		0,
		vms_section_size,
		(nt_large_integer *)0,
		&vms_section_size,
		NT_VIEW_UNMAP,
		0,
		NT_PAGE_READONLY);

	__psx_init_abort(status);

	/* all done */
	return NT_STATUS_SUCCESS;
}



static void __psx_init_abort(int32_t status)
{
	if (status)
		__ntapi->zw_terminate_process(
			NT_CURRENT_PROCESS_HANDLE,
			status);
	return;
}


static int __psx_init_wintls_accessors(void)
{
	void * kernel32;

	kernel32 = pe_get_kernel32_module_handle();

	tls_alloc = (winapi_tls_alloc *)pe_get_procedure_address(
		kernel32,
		"TlsAlloc");

	tls_free = (winapi_tls_free *)pe_get_procedure_address(
		kernel32,
		"TlsFree");

	tls_get_value = (winapi_tls_get_value *)pe_get_procedure_address(
		kernel32,
		"TlsGetValue");

	tls_set_value = (winapi_tls_set_value *)pe_get_procedure_address(
		kernel32,
		"TlsSetValue");

	if (tls_alloc && tls_free && tls_get_value && tls_set_value)
		return NT_STATUS_SUCCESS;
	else
		return NT_STATUS_DLL_INIT_FAILED;
}


static int __psx_tls_index_alloc(void)
{
	wintls_sys_idx = tls_alloc();

	if (wintls_sys_idx == WINAPI_TLS_OUT_OF_INDEXES)
		return NT_STATUS_DLL_INIT_FAILED;

	wintls_libc_idx = tls_alloc();

	if (wintls_libc_idx == WINAPI_TLS_OUT_OF_INDEXES)
		return NT_STATUS_DLL_INIT_FAILED;

	return NT_STATUS_SUCCESS;
}


static int __psx_init_iovtbl(void)
{
	int i;

	for (i=1; i<PSX_FD_TYPE_CAP; i++)
		if (__iovtbl[i].init)
			__iovtbl[i].init(&__iovtbl[i]);

	return 0;
}
