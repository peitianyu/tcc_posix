/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <dalist/dalist.h>
#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_init.h"
#include "psx_session.h"
#include "psx_helper.h"
#include "psx.h"

int __psx_init_session(struct __psx_ctx * ctx)
{
	int32_t status;
	size_t	size;
	void *	addr;

	if ((status = dalist_init_ex(
			&rtctx.peers,
			sizeof(struct __process_record),
			__PSX_VIRTUAL_PAGE_SIZE,
			__ntapi->zw_allocate_virtual_memory,
			DALIST_MEMFN_NT_ALLOCATE_VIRTUAL_MEMORY)))
		return status;

	if ((status = dalist_init_ex(
			&rtctx.offsprings,
			sizeof(struct __process_record),
			__PSX_VIRTUAL_PAGE_SIZE,
			__ntapi->zw_allocate_virtual_memory,
			DALIST_MEMFN_NT_ALLOCATE_VIRTUAL_MEMORY)))
		return status;

	size = __PSX_PAGE_SIZE;
	addr = 0;

	if ((status = __psx_create_primary_section(&ctx->hrecsec,size)))
		return status;

	if ((status = __psx_map_primary_section(ctx->hrecsec,&addr,&size)))
		return status;

	if ((status = dalist_deposit_memory_block(
			&rtctx.peers,
			addr,
			size/2)))
		return status;

	if ((status = dalist_deposit_memory_block(
			&rtctx.peers,
			(char *)addr + (size/2),
			size/2)))
		return status;

	return NT_STATUS_SUCCESS;
}
