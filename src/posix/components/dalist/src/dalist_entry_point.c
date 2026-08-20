/*****************************************************************************/
/*  dalist: a zero-dependency book-keeping library                           */
/*  Copyright (C) 2013--2021  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.DALIST.                      */
/*****************************************************************************/

#ifdef _MIDIPIX_FREESTANDING

#include <dalist/dalist.h>
#include <psxtypes/section/freestd.h>

__attr_section_decl__(".freestd")
static const void * const dalist_affiliation
	__attr_section__(".freestd")
	= 0;

int __stdcall __attr_protected__ dalist_entry_point(
	void *		hinstance,
	uint32_t	reason,
	void *		reserved)
{
	(void)dalist_affiliation;
	(void)hinstance;
	(void)reason;
	(void)reserved;

	return 1;
}

#endif
