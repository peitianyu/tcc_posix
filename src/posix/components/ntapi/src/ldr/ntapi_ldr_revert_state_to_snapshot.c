/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include <dalist/dalist.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

struct callback_ctx {
	struct dalist_ex *		ldr_state_snapshot;
	struct pe_ldr_tbl_entry *	ldr_tbl_entry;
	void *				image_base;
	uint32_t			load_count;
	int32_t				status;
};

static int __cdecl __find_next_module_to_unload(
	struct pe_ldr_tbl_entry *	ldr_tbl_entry,
	enum pe_callback_reason		int_callback_reason,
	void *				context)
{
	struct dalist_node *	node;
	struct callback_ctx *	ctx;

	ctx = (struct callback_ctx *)context;

	if (int_callback_reason == PE_CALLBACK_REASON_ERROR) {
		ctx->status = NT_STATUS_UNSUCCESSFUL;
		return ctx->status;
	} else if (int_callback_reason != PE_CALLBACK_REASON_ITEM) {
		ctx->status = NT_STATUS_SUCCESS;
		return 1;
	} else if (!ldr_tbl_entry->dll_base) {
		ctx->status = NT_STATUS_SUCCESS;
		return 1;
	}


	ctx->status = dalist_get_node_by_key(
		ctx->ldr_state_snapshot,
		(struct dalist_node_ex **)&node,
		(uintptr_t)ldr_tbl_entry->dll_base,
		DALIST_NODE_TYPE_EXISTING,
		0);

	if (ctx->status != DALIST_OK)
		return -1;
	else if (node)
		return 1;
	else if (!ctx->image_base || (ldr_tbl_entry->load_count < ctx->load_count)) {
			ctx->image_base = ldr_tbl_entry->dll_base;
			ctx->load_count = ldr_tbl_entry->load_count;
			ctx->ldr_tbl_entry = ldr_tbl_entry;
	}

	return 1;
}


int __cdecl __ntapi_ldr_revert_state_to_snapshot(
	__in	struct dalist_ex * ldr_state_snapshot)
{
	struct callback_ctx	ctx;
	uint32_t		i;

	if (!ldr_state_snapshot->free && !ldr_state_snapshot->memfn_ptr)
		return NT_STATUS_BUFFER_TOO_SMALL;

	ctx.ldr_state_snapshot = ldr_state_snapshot;
	ctx.image_base = (void *)0;
	ctx.load_count = 0;

	pe_enum_modules_in_load_order(
		__find_next_module_to_unload,
		&ctx);

	while ((ctx.image_base) && (ctx.status == NT_STATUS_SUCCESS)) {
			if (ctx.load_count == 0xffff) {
				ctx.load_count = 1;
				ctx.ldr_tbl_entry->load_count = 1;
				ctx.ldr_tbl_entry->entry_point = (void *)0;
				ctx.ldr_tbl_entry->flags = 0;
			}

			for (i=0; i<ctx.load_count; i++)
				__ntapi->ldr_unload_dll(ctx.image_base);

			__ntapi->zw_unmap_view_of_section(
				NT_CURRENT_PROCESS_HANDLE,
				ctx.image_base);
		ctx.image_base = (void *)0;
		ctx.load_count = 0;

		pe_enum_modules_in_load_order(
			__find_next_module_to_unload,
			&ctx);
	}

	return ctx.status;
}
