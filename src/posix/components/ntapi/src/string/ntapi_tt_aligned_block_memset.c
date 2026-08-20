/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>

void * __cdecl __ntapi_tt_aligned_block_memset(
	__in	void *		block,
	__in	uintptr_t	val,
	__in	size_t		bytes)
{
	uintptr_t * ptr = (uintptr_t *)block;

	for (bytes/=sizeof(uintptr_t); bytes; bytes--)
		*ptr++=val;

	return block;
}

void * __cdecl __ntapi_tt_generic_memset(
	__in	void *		dst,
	__in	uintptr_t	val,
	__in	size_t		bytes)
{
	char	c;
	char *	ch;
	int	i;
	size_t	abytes;

	if (!bytes)
		return dst;

	else if (!(bytes % sizeof(size_t))
			&& (!(uintptr_t)dst % sizeof(size_t)))
		return __ntapi_tt_aligned_block_memset(
			dst,val,bytes);

	c = (char)val;
	for (i=0; i<sizeof(size_t); i++, val <<= 8)
		val += c;

	for (ch=(char *)dst; (size_t)ch % sizeof(size_t); ch++, bytes--)
		*ch = c;

	abytes = bytes / sizeof(size_t) * sizeof(size_t);
	__ntapi_tt_aligned_block_memset(ch,val,abytes);

	bytes -= abytes;
	ch += abytes;

	for (; bytes; ch++, bytes--)
		*ch = c;

	return dst;
}
