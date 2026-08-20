/*****************************************************************************/
/*  pemagination: a (virtual) tour into portable bits and executable bytes   */
/*  Copyright (C) 2013--2020  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.PEMAGINE.                    */
/*****************************************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>

#define PE_STR_MAX_SYMBOL_LEN_ALLOWED			(uint32_t)0x10000

#define IN_LOAD_ORDER_MODULE_LIST_OFFSET		(intptr_t)0x00
#define IN_MEMORY_ORDER_MODULE_LIST_OFFSET		(intptr_t)0x01 * sizeof(struct pe_list_entry)
#define IN_INITIALIZATION_ORDER_MODULE_LIST_OFFSET	(intptr_t)0x02 * sizeof(struct pe_list_entry)

struct pe_block {
	uint32_t	rva;
	uint32_t	size;
};


static __inline__ intptr_t pe_impl_strlen_ansi(const char * str)
{
	const char * ch;
	const char * upper_bound;

	upper_bound = str + PE_STR_MAX_SYMBOL_LEN_ALLOWED;

	for (ch=str; *ch && ch<upper_bound; )
		ch++;

	return (ch < upper_bound)
		? ch - str
		: -1;
}


static __inline__ intptr_t pe_impl_strlen_utf16(const wchar16_t * str)
{
	const wchar16_t * wch;
	const wchar16_t * upper_bound;

	upper_bound = str + PE_STR_MAX_SYMBOL_LEN_ALLOWED;

	for (wch=str; *wch && wch<upper_bound; )
		wch++;

	if (wch < upper_bound)
		return (wch - str) * sizeof(wchar16_t);
	else
		return -1;
}


static __inline__ wchar16_t pe_impl_utf16_char_to_lower(const wchar16_t c)
{
	return ((c >= 'A') && (c <= 'Z'))
		? c + 'a' - 'A'
		: c;
}
