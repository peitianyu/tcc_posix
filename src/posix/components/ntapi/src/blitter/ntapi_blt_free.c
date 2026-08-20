/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <ntapi/nt_status.h>
#include <ntapi/nt_blitter.h>
#include <ntapi/nt_sync.h>
#include <ntapi/ntapi.h>
#include "ntapi_blitter.h"
#include "ntapi_impl.h"

int32_t __fastcall __ntapi_blt_free(nt_blitter * blt_ctx)
{
	int32_t	status;
	void *	region_addr;
	size_t	region_size;

	/* validation */
	if (!blt_ctx) return NT_STATUS_INVALID_PARAMETER;

	/* free blt block */
	region_addr = blt_ctx->info.region_addr;
	region_size = blt_ctx->info.region_size;

	if (region_size && !blt_ctx->params.region) {
		status = __ntapi->zw_free_virtual_memory(
			NT_CURRENT_PROCESS_HANDLE,
			&region_addr,
			&region_size,
			NT_MEM_RELEASE);

		if (status) return status;
	}

	/* free blt control block */
	region_addr = blt_ctx->addr;
	region_size = blt_ctx->size;

	status = __ntapi->zw_free_virtual_memory(
		NT_CURRENT_PROCESS_HANDLE,
		&region_addr,
		&region_size,
		NT_MEM_RELEASE);

	return status;
}
