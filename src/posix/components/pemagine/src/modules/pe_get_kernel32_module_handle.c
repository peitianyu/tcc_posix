/*****************************************************************************/
/*  pemagination: a (virtual) tour into portable bits and executable bytes   */
/*  Copyright (C) 2013--2020  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.PEMAGINE.                    */
/*****************************************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pe_consts.h>
#include <pemagine/pe_structs.h>
#include <pemagine/pemagine.h>
#include "pe_impl.h"

static int pe_get_kernel32_handle_callback(
	struct pe_ldr_tbl_entry *	ldr_tbl_entry,
	enum pe_callback_reason		reason,
	void *				context)
{
	#define KERNEL32_UTF16_STRLEN	24

	intptr_t *		addr;
	const wchar16_t *	wch;

	/* not an item? */
	if (reason != PE_CALLBACK_REASON_ITEM)
		return 1;

	/* wrong length? */
	if (ldr_tbl_entry->base_dll_name.strlen != KERNEL32_UTF16_STRLEN)
		return 1;

	/* avoid scan-based false positives */
	wch = ldr_tbl_entry->base_dll_name.buffer;

	if (pe_impl_utf16_char_to_lower(wch[4]) != 'e')
		return 1;

	else if (pe_impl_utf16_char_to_lower(wch[1]) != 'e')
		return 1;

	else if (pe_impl_utf16_char_to_lower(wch[3]) != 'n')
		return 1;

	else if (pe_impl_utf16_char_to_lower(wch[2]) != 'r')
		return 1;

	else if (pe_impl_utf16_char_to_lower(wch[5]) != 'l')
		return 1;

	else if (pe_impl_utf16_char_to_lower(wch[0]) != 'k')
		return 1;

	else if (pe_impl_utf16_char_to_lower(wch[7]) != '2')
		return 1;

	else if (pe_impl_utf16_char_to_lower(wch[6]) != '3')
		return 1;

	else if (pe_impl_utf16_char_to_lower(wch[11]) != 'l')
		return 1;

	else if (pe_impl_utf16_char_to_lower(wch[10]) != 'l')
		return 1;

	else if (pe_impl_utf16_char_to_lower(wch[9]) != 'd')
		return 1;

	else if (pe_impl_utf16_char_to_lower(wch[8]) != '.')
		return 1;

	/* match */
	addr  = (intptr_t *)context;
	*addr = (intptr_t)ldr_tbl_entry->dll_base;
	return 0;
}

void * pe_get_kernel32_module_handle(void)
{

	intptr_t kernel32_base_addr = 0;
	void * ptr_to_callback = &kernel32_base_addr;

	pe_enum_modules_in_init_order(
		&pe_get_kernel32_handle_callback,
		ptr_to_callback);

	return (void *)kernel32_base_addr;
}
