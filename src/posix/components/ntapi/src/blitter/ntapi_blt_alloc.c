/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <ntapi/nt_status.h>
#include <ntapi/nt_blitter.h>
#include <ntapi/nt_sync.h>
#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "ntapi_blitter.h"
#include "ntapi_impl.h"

static int __blt_popcount(uintptr_t mask)
{
	/* todo: check cpuid, use at_popcount */
	int i,ret;

	for (i=0,ret=0; i<8*sizeof(uintptr_t); i++)
		if (mask & ((uintptr_t)1<<i))
			ret++;

	return ret;
}


int32_t __fastcall __ntapi_blt_alloc(
	__out	nt_blitter **		blitter,
	__in	nt_blitter_params *	params)
{
	int32_t		status;
	nt_blitter *	blt_ctx;
	size_t		blt_ctx_size;
	size_t		params_size;
	size_t		ptrs,i;

	/* alignment */
	if ((params->block_size % sizeof(uintptr_t)) || (params->block_count % sizeof(uintptr_t)))
		return NT_STATUS_INVALID_PARAMETER;

	/* blt control block allocation */
	ptrs		= params->block_count / (8 * sizeof(uintptr_t));
	blt_ctx		= (nt_blitter *)0;
	blt_ctx_size	= (size_t)&((nt_blitter *)0)->bits;

	/* user-provided bitmap? */
	if (!params->bitmap)
		blt_ctx_size += ptrs * sizeof(uintptr_t);

	/* alloc */
	status = __ntapi->zw_allocate_virtual_memory(
		NT_CURRENT_PROCESS_HANDLE,
		(void **)&blt_ctx,
		0,
		&blt_ctx_size,
		NT_MEM_COMMIT,
		NT_PAGE_READWRITE);

	if (status) return (status);

	/* init control block */
	__ntapi->tt_aligned_block_memset(
		blt_ctx,
		0,(size_t)&((nt_blitter *)0)->bits);

	blt_ctx->addr	= blt_ctx;
	blt_ctx->size	= blt_ctx_size;
	blt_ctx->ptrs	= ptrs;

	/* init bitmap */
	blt_ctx->bitmap = params->bitmap
		? (uintptr_t *)params->bitmap
		: blt_ctx->bits;

	if (!(params->flags & NT_BLITTER_PRESERVE_BITS))
		__ntapi->tt_aligned_block_memset(
			blt_ctx->bitmap,
			(intptr_t)0xFFFFFFFFFFFFFFFF,
			ptrs * sizeof(uintptr_t));

	/* info structure */
	blt_ctx->info.info_size   = sizeof(nt_blitter_info);
	blt_ctx->info.block_count = params->block_count;
	blt_ctx->info.block_size  = params->block_size;

	if (params->flags & NT_BLITTER_ENABLE_BLOCK_ARRAY)
		/* allocate in place */
		blt_ctx->info.region_size = params->block_count * params->block_size;
	else
		/* use pointer array */
		blt_ctx->info.region_size = params->block_count * sizeof(uintptr_t);

	/* allocate region */
	if (params->region)
		blt_ctx->info.region_addr = params->region;
	else
		status = __ntapi->zw_allocate_virtual_memory(
			NT_CURRENT_PROCESS_HANDLE,
			&blt_ctx->info.region_addr,
			0,
			&blt_ctx->info.region_size,
			NT_MEM_COMMIT,
			NT_PAGE_READWRITE);

	if (status) {
		__ntapi->blt_free(blt_ctx);
		return status;
	}

	if (params->flags & NT_BLITTER_PRESERVE_BITS)
		for (i=0,blt_ctx->info.blocks_avail=0; i<ptrs; i++)
			blt_ctx->info.blocks_avail += __blt_popcount(blt_ctx->bitmap[i]);
	else
		blt_ctx->info.blocks_avail = params->block_count;

	if (params->flags & NT_BLITTER_ENABLE_BLOCK_ARRAY)
		blt_ctx->info.blocks_cached = params->block_count;

	/* init block array */
	if (!params->region)
		__ntapi->tt_aligned_block_memset(
			blt_ctx->info.region_addr,
			0,blt_ctx->info.region_size);

	/* copy params */
	if (params->params_size < sizeof(nt_blitter_params))
		params_size = params->params_size;
	else
		params_size = sizeof(nt_blitter_params);

	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)&blt_ctx->params,
		(uintptr_t *)params,
		params_size);

	/* update params */
	blt_ctx->params.lock_tries = params->lock_tries
				? params->lock_tries
				: __NT_BLITTER_DEFAULT_LOCK_TRIES;

	blt_ctx->params.round_trips = params->round_trips
				? params->round_trips
				: __NT_BLITTER_DEFAULT_ROUND_TRIPS;

	*blitter = blt_ctx;

	return NT_STATUS_SUCCESS;
}
