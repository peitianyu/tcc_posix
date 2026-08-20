/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <pemagine/pemagine.h>
#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_ldso.h"

typedef int (*__iterator_fn)(void * base, struct __psx_common_descriptor * desc);

static void __do_module_global_ctors(void * base, struct __psx_common_descriptor * desc)
{
	__ctorfn_t * pfn;

	for (pfn=desc->ctors; *pfn; pfn++);
	for (pfn--; *pfn != (__ctorfn_t)-1; pfn--)
		(*pfn)();
}

static void __do_module_global_dtors(void * base, struct __psx_common_descriptor * desc)
{
	__dtorfn_t * pfn;

	for (pfn=desc->dtors+1; *pfn; pfn++)
		(*pfn)();
}

static int __do_module_global_ctors_dtors(
	struct pe_ldr_tbl_entry *	image_ldr_tbl_entry,
	enum pe_callback_reason		reason,
	void *				context)
{

	struct pe_sec_hdr *		 sec;
	struct __psx_common_descriptor * desc;
	uint32_t *			 rva;
	uintptr_t			 addr;

	switch (reason) {
		case PE_CALLBACK_REASON_INIT:
		case PE_CALLBACK_REASON_INFO:
		case PE_CALLBACK_REASON_QUERY:
			return 1;

		case PE_CALLBACK_REASON_DONE:
			return 0;

		case PE_CALLBACK_REASON_ERROR:
			return NT_STATUS_INTERNAL_ERROR;

		case PE_CALLBACK_REASON_ITEM:
			break;
	}

	/* supported image? */
	if (!image_ldr_tbl_entry->dll_base)
		return 1;
	else if (!(sec = pe_get_image_named_section_addr(
			image_ldr_tbl_entry->dll_base,
			".midipix")))
		return 1;

	rva = (uint32_t *)&sec->virtual_addr;
	addr = ((uintptr_t)image_ldr_tbl_entry->dll_base + *rva);
	desc = (struct __psx_common_descriptor *)addr;
	((__iterator_fn)context)(image_ldr_tbl_entry->dll_base,desc);

	return 1;
}

/* FIXME: remove this #ifdef before pre-pre-alpha release */
#ifdef __GNUC__
void __psx_do_global_ctors(void)
{
	pe_enum_modules_in_load_order(
		__do_module_global_ctors_dtors,
		__do_module_global_ctors);
}

void __psx_do_global_dtors(void)
{
		pe_enum_modules_in_load_order(
		__do_module_global_ctors_dtors,
		__do_module_global_dtors);
}
#else
void __psx_do_global_ctors(void)
{
}

void __psx_do_global_dtors(void)
{
}
#endif
