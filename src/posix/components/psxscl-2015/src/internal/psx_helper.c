/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_helper.h"
#include "psx_session.h"
#include "psx_impl.h"
#include "psx_debug.h"

static nt_sqos sqos = {
	sizeof(sqos),
	NT_SECURITY_IMPERSONATION,
	NT_SECURITY_TRACKING_DYNAMIC,
	1};

static int32_t __psx_create_section(
	void **		hsection,
	size_t		size,
	uint32_t	attr)
{
	nt_sec_size sec_size;

	nt_oa oa = {sizeof(oa),
		    0,0,NT_OBJ_INHERIT,0,&sqos};

	sec_size.quad = size;

	return __ntapi->zw_create_section(
		hsection,
		NT_SECTION_ALL_ACCESS,
		&oa,
		&sec_size,
		NT_PAGE_READWRITE,
		attr,
		(void *)0);
}

static int32_t __psx_map_section(
	void *			hsection,
	void **			addr,
	size_t *		size,
	uint32_t		protect,
	nt_section_inherit	inheritance)
{
	int32_t status;
	size_t  asize = *size;

	if ((status = __ntapi->zw_map_view_of_section(
			hsection,
			NT_CURRENT_PROCESS_HANDLE,
			addr,0,*size,0,size,
			inheritance,0,
			protect)))
		return status;
	else if (asize != *size)
		return NT_STATUS_INTERNAL_ERROR;
	else
		return status;
}

int32_t __psx_create_cow_section(
	void **		hsection,
	size_t		size)
{
	return __psx_create_section(hsection,size,NT_SEC_COMMIT);
}

int32_t __psx_create_primary_section(
	void **		hsection,
	size_t		size)
{
	return __psx_create_section(hsection,size,NT_SEC_RESERVE);
}

int32_t __psx_map_cow_section(
	void *		hsection,
	void **		addr,
	size_t *	size)
{
	return __psx_map_section(hsection,addr,size,NT_PAGE_WRITECOPY,NT_VIEW_SHARE);
}

int32_t __psx_map_primary_section(
	void *		hsection,
	void **		addr,
	size_t *	size)
{
	return __psx_map_section(hsection,addr,size,NT_PAGE_READWRITE,NT_VIEW_UNMAP);
}

int32_t __psx_clone_primary_section(
	void **		hlocalsec,
	void *		srcaddr,
	size_t		secsize,
	void **		mapaddr,
	size_t *	mapsize)
{
	int32_t		status;
	void *		addr = 0;

	mapaddr = mapaddr ? mapaddr : &addr;

	if ((status = __psx_create_primary_section(hlocalsec,secsize)))
		return status;

	if ((status = __psx_map_primary_section(*hlocalsec,mapaddr,mapsize)))
		return status;

	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)*mapaddr,
		(uintptr_t *)srcaddr,
		*mapsize);

	return NT_STATUS_SUCCESS;
}

int32_t __psx_swap_primary_section(
	void *		hsecold,
	void *		srcaddr,
	void **		hsecnew,
	void **		mapaddr,
	size_t		secsize,
	size_t		mapsize)
{
	int32_t status;

	*mapaddr = 0;

	if ((status = __psx_create_primary_section(hsecnew,secsize)))
		return status;

	if ((status = __psx_map_cow_section(*hsecnew,mapaddr,&mapsize)))
		return status;

	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)*mapaddr,
		(uintptr_t *)srcaddr,
		mapsize);

	__ntapi->zw_close(hsecold);

	return __ntapi->zw_unmap_view_of_section(
		NT_CURRENT_PROCESS_HANDLE,
		srcaddr);
}

int32_t __psx_blt_alloc(
	nt_blitter **	blt,
	void *		bitmap,
	void *		region,
	size_t		block_size,
	uint32_t	flags)
{
	nt_blitter_params params;

	__ntapi->tt_aligned_block_memset(&params,0,sizeof(params));
	params.params_size	= sizeof(params);
	params.block_size	= block_size;
	params.block_count	= __PSX_BITS_PER_PAGE;
	params.srvtid		= (int32_t)rtdata->cid_self.thread_id;
	params.flags		= flags;
	params.bitmap		= bitmap;
	params.region		= region;

	return __ntapi->blt_alloc(blt,&params);
}

static void __psx_thread_entry_routine(struct __psx_internal_thread_context * ctx)
{
	nt_thread_start_routine * entry;

	entry = (nt_thread_start_routine *)ctx->entry;

	__ntapi->zw_terminate_thread(
		NT_CURRENT_THREAD_HANDLE,
		entry(ctx->data));
}

int32_t __psx_create_internal_thread(void ** hthread, void * entry, void * ctx, size_t size)
{
	struct __psx_internal_thread_context {
		void *		entry;
		uintptr_t	data[256];}	xctx;
	nt_thread_params			params;
	int32_t					status;

	xctx.entry = entry;

	__ntapi->tt_aligned_block_memcpy(xctx.data,(uintptr_t *)ctx,size);
	__ntapi->tt_aligned_block_memset(&params,0,sizeof(params));

	params.hprocess		 = rtdata->hprocess_self;
	params.start		 = (nt_thread_start_routine *)__psx_thread_entry_routine;
	params.ext_ctx		 = &xctx;
	params.ext_ctx_size	 = size + sizeof(xctx.entry);
	params.stack_size_commit = 4 * __PSX_VIRTUAL_PAGE_SIZE;
	params.stack_size_reserve= 4 * __PSX_VIRTUAL_PAGE_SIZE;
	params.creation_flags	 = NT_CREATE_LOCAL_THREAD;

	if ((status = __ntapi->tt_create_thread(&params)))
		return status;

	if (hthread)
		*hthread = params.hthread;
	else
		__ntapi->zw_close(params.hthread);

	return NT_STATUS_SUCCESS;
}

void __stdcall __psx_terminate_internal_thread(void * ctx, void * code, void * unused)
{
	__ntapi->zw_terminate_thread(
		NT_CURRENT_THREAD_HANDLE,
		(int32_t)(intptr_t)code);
}
