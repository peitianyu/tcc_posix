/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>

static void __fastcall __ntapi_tt_uint_to_hex_utf8(
	__in	uint64_t	key,
	__out	unsigned char *	buffer,
	__in	unsigned	bits)
{
	unsigned	i;
	uint32_t	hex_buf[4];
	unsigned char *	hex_chars;
	unsigned char *	uch;
	unsigned	offset;
	unsigned	bytes;

	/* avoid using .rdata for that one */
	hex_buf[0] = ('3' << 24) | ('2' << 16) | ('1' << 8) | '0';
	hex_buf[1] = ('7' << 24) | ('6' << 16) | ('5' << 8) | '4';
	hex_buf[2] = ('B' << 24) | ('A' << 16) | ('9' << 8) | '8';
	hex_buf[3] = ('F' << 24) | ('E' << 16) | ('D' << 8) | 'C';

	uch = (unsigned char *)&key;
	hex_chars = (unsigned char *)&hex_buf;

	bytes  = bits / 8;
	offset = bits / 4;

	for (i = 0; i < bytes; i++) {
		buffer[offset - 1 - (i*2)] =	hex_chars[uch[i] % 16];
		buffer[offset - 2 - (i*2)] =	hex_chars[uch[i] / 16];
	}
}


void __fastcall __ntapi_tt_uint16_to_hex_utf8(
	__in	uint32_t	key,
	__out	unsigned char *	buffer)
{
	__ntapi_tt_uint_to_hex_utf8(key,buffer,16);
}


void __fastcall __ntapi_tt_uint32_to_hex_utf8(
	__in	uint32_t	key,
	__out	unsigned char *	buffer)
{
	__ntapi_tt_uint_to_hex_utf8(key,buffer,32);
}


void __fastcall __ntapi_tt_uint64_to_hex_utf8(
	__in	uint64_t	key,
	__out	unsigned char *	buffer)
{
	__ntapi_tt_uint_to_hex_utf8(key,buffer,64);
}


void __fastcall __ntapi_tt_uintptr_to_hex_utf8(
	__in	uintptr_t	key,
	__out	unsigned char *	buffer)
{
	#if defined (__NT32)
		__ntapi_tt_uint_to_hex_utf8(key,buffer,32);
	#elif defined (__NT64)
		__ntapi_tt_uint_to_hex_utf8(key,buffer,64);
	#endif
}
