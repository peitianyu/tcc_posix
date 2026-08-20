/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include <ntapi/nt_object.h>
#include <ntapi/nt_crc32.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

struct callback_ctx {
	void *			import_table;
	ntapi_hashed_symbol *	hash_table;
	uint32_t		hash_table_array_size;
};


static int __process_exported_symbol(
	const void *				base,
	struct pe_export_hdr *			exp_hdr,
	struct pe_export_sym *			exp_item,
	enum pe_callback_reason			reason,
	void *					context)
{
	uint32_t		hash_value;
	struct callback_ctx *	ctx;
	ntapi_hashed_symbol *	hashed_symbol;
	uintptr_t *		fnptr;

	/* binary search variables */
	uint32_t	lower;
	uint32_t	upper;
	uint32_t	idx;

	if (reason != PE_CALLBACK_REASON_ITEM)
		return 1;

	ctx = (struct callback_ctx *)context;
	hash_value = __ntapi_tt_mbstr_crc32(exp_item->name);

	/* zero-based array, binary search, idx < upper is guaranteed */
	lower  = 0;
	upper  = ctx->hash_table_array_size;

	/* binary search */
	while (lower < upper) {
		idx		= (lower + upper) / 2;
		hashed_symbol	= (ntapi_hashed_symbol *)
					((uintptr_t)ctx->hash_table
					+ idx * sizeof(ntapi_hashed_symbol));

		if (hash_value == hashed_symbol->crc32_hash) {
			fnptr = (uintptr_t *)(
				(uintptr_t)ctx->import_table
				+ (sizeof(uintptr_t)
					* hashed_symbol->ordinal));
			*fnptr = (uintptr_t)exp_item->addr;
			return 1;
		}

		else {
			if (hash_value > hashed_symbol->crc32_hash)
				lower = idx + 1;
			else
				upper = idx;
		}
	}

	return 1;
}

int32_t __cdecl __ntapi_tt_populate_hashed_import_table(
    __in 	void *			image_base,
    __in	void *			import_table,
    __in	ntapi_hashed_symbol *	hash_table,
    __in	uint32_t		hash_table_array_size)
{
	struct pe_export_sym	exp_item;
	struct callback_ctx	ctx;

	ctx.import_table		= import_table;
	ctx.hash_table			= hash_table;
	ctx.hash_table_array_size	= hash_table_array_size;

	pe_enum_image_exports(
		image_base,
		&__process_exported_symbol,
		&exp_item,
		&ctx);

	return 0;
}
