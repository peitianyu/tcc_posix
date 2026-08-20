/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include <dalist/dalist.h>
#include <ntapi/ntapi.h>

struct callback_ctx {
	struct dalist_ex *	ldr_state_snapshot;
	int32_t			status;
};

static int __cdecl __add_module_base_address_to_list(
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
		DALIST_NODE_TYPE_NEW,
		0);

	if (ctx->status != DALIST_OK)
		return -1;
	else
		return 1;
}


int __cdecl __ntapi_ldr_create_state_snapshot(
	__out	struct dalist_ex * ldr_state_snapshot)
{
	struct callback_ctx ctx;

	if (!ldr_state_snapshot->free && !ldr_state_snapshot->memfn_ptr)
		return NT_STATUS_BUFFER_TOO_SMALL;
	else if (ldr_state_snapshot->info.list_nodes)
		return NT_STATUS_INVALID_USER_BUFFER;

	ctx.ldr_state_snapshot = ldr_state_snapshot;

	pe_enum_modules_in_load_order(
		__add_module_base_address_to_list,
		&ctx);

	return ctx.status;
}
