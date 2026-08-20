/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_crc32.h>

static const uint32_t crc32_table[256] = NTAPI_CRC32_TABLE;

uint32_t __ntapi_tt_buffer_crc32(
	uint32_t	prev_hash,
	const void *	buffer,
	size_t		size)
{
	unsigned char *	ch;
	uint32_t	crc32;

	crc32	= prev_hash ^ 0xFFFFFFFF;
	ch	= (unsigned char *)buffer;

	for (; size; size--,ch++)
		crc32 = (crc32 >> 8) ^ crc32_table[(crc32 ^ *ch) & 0xFF];

	return (crc32 ^ 0xFFFFFFFF);
}


uint32_t __cdecl __ntapi_tt_mbstr_crc32(const void * str)
{
	uint32_t	crc32;
	unsigned char *	ch;

	crc32	= 0 ^ 0xFFFFFFFF;
	ch	= (unsigned char *)str;

	while (*ch) {
		crc32 = (crc32 >> 8) ^ crc32_table[(crc32 ^ *ch) & 0xFF];
		ch++;
	}

	return (crc32 ^ 0xFFFFFFFF);
}


const uint32_t * __cdecl __ntapi_tt_crc32_table(void)
{
	return crc32_table;
}
