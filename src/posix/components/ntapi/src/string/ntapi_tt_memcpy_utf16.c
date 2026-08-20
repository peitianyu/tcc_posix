/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>

wchar16_t * __cdecl __ntapi_tt_memcpy_utf16(
	__in	wchar16_t *	dst,
	__in	wchar16_t *	src,
	__in	size_t		bytes)
{
	wchar16_t *	wch_cap;
	wchar16_t *	wch_ret;

	wch_cap = (wchar16_t *)((uintptr_t)src + bytes);
	wch_ret = dst;

	while (src < wch_cap) {
		*dst = *src;
		src++;
		dst++;
	}

	return wch_ret;
}
