/*****************************************************************************/
/*  pemagination: a (virtual) tour into portable bits and executable bytes   */
/*  Copyright (C) 2013--2020  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.PEMAGINE.                    */
/*****************************************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include "pe_os.h"

struct ldr_entry_addr_ctx {
	uintptr_t			addr;
	struct pe_ldr_tbl_entry *	entry;
};

static int ldr_entry_addr_compare(
	struct pe_ldr_tbl_entry *	image_ldr_tbl_entry,
	enum pe_callback_reason		reason,
	void *				context)
{
	uintptr_t			base;
	uintptr_t			cap;
	struct ldr_entry_addr_ctx *	ctx;

	switch (reason) {
		case PE_CALLBACK_REASON_INIT:
		case PE_CALLBACK_REASON_INFO:
		case PE_CALLBACK_REASON_QUERY:
			return 1;

		case PE_CALLBACK_REASON_DONE:
			return 0;

		case PE_CALLBACK_REASON_ERROR:
			return OS_STATUS_INTERNAL_ERROR;

		case PE_CALLBACK_REASON_ITEM:
			break;
	}

	ctx  = (struct ldr_entry_addr_ctx *)context;
	base = (uintptr_t)image_ldr_tbl_entry->dll_base;
	cap  = base + image_ldr_tbl_entry->size_of_image;

	if ((ctx->addr >= base) && (ctx->addr < cap)) {
		ctx->entry =  image_ldr_tbl_entry;
		return 0;
	}

	return 1;
}

struct pe_ldr_tbl_entry * pe_get_ldr_entry_from_addr(const void * addr)
{
	struct ldr_entry_addr_ctx ctx = {(uintptr_t)addr,0};

	if (pe_enum_modules_in_load_order(
			ldr_entry_addr_compare,
			&ctx))
		return 0;

	return ctx.entry;
}
