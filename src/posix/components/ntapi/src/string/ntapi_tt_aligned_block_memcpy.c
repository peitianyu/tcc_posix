/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_string.h>

uintptr_t * __cdecl __ntapi_tt_aligned_block_memcpy(
	__in	uintptr_t *	dst,
	__in	uintptr_t *	src,
	__in	size_t		bytes)
{
	uintptr_t * ptr = (uintptr_t *)dst;

	for (bytes/=sizeof(uintptr_t); bytes; bytes--)
		*dst++ = *src++;

	return ptr;
}


void * __cdecl __ntapi_tt_generic_memcpy(
	__in	void *		dst,
	__in	const void *	src,
	__in	size_t		bytes)
{
	char *		ch_dst;
	const char *	ch_src;

	if (!bytes)
		return dst;

	else if (!(bytes % sizeof(size_t))
			&& (!(uintptr_t)dst % sizeof(size_t))
			&& (!(uintptr_t)src % sizeof(size_t)))
		return __ntapi_tt_aligned_block_memcpy(
			(uintptr_t *)dst,
			(uintptr_t *)src,
			bytes);

	ch_dst = (char *)dst;
	ch_src = (const char *)src;

	for (; bytes; bytes--)
		*ch_dst++ = *ch_src++;

	return dst;
}
