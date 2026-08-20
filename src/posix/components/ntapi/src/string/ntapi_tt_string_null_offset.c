/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_string.h>
#include "ntapi_impl.h"

size_t __cdecl __ntapi_tt_string_null_offset_multibyte(
	__in	const char *	str)
{
	const char *		cap;
	const uintptr_t *	ptr;

	#define HIGH_BIT_TEST	(uintptr_t)0x0101010101010101
	#define	AND_BITS	(uintptr_t)0x8080808080808080

	cap = str;
	while ((uintptr_t)cap % sizeof(uintptr_t)) {
		if (!(*cap))
			return cap - str;
		cap++;
	}

	ptr = (uintptr_t *)cap;
	while (!((*ptr - HIGH_BIT_TEST) & ~(*ptr) & AND_BITS))
		ptr++;

	cap = (const char *)ptr;
	while (*cap)
		cap++;

	return cap - str;
}


size_t __cdecl __ntapi_tt_string_null_offset_short(
	__in	const int16_t *	str)
{
	const int16_t *	cap;

	cap = str;
	while (*cap)
		cap++;

	return (size_t)cap - (size_t)str;
}


size_t __cdecl __ntapi_tt_string_null_offset_dword(
	__in	const int32_t *	str)
{
	const int32_t *	cap;

	cap = str;
	while (*cap)
		cap++;

	return (size_t)cap - (size_t)str;
}

size_t __cdecl __ntapi_tt_string_null_offset_qword(
	__in	const int64_t *	str)
{
	const int64_t *	cap;

	cap = str;
	while (*cap)
		cap++;

	return (size_t)cap - (size_t)str;
}

size_t __cdecl __ntapi_tt_string_null_offset_ptrsize(
	__in	const intptr_t *str)
{
	const intptr_t * cap;

	cap = str;
	while (*cap)
		cap++;

	return (size_t)cap - (size_t)str;
}

size_t __cdecl __ntapi_wcslen(const wchar16_t * str)
{
	size_t len;
	len = __ntapi_tt_string_null_offset_short((const int16_t *)str);
	return len / 2;
}
