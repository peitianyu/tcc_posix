/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_status.h>
#include <ntapi/nt_unicode.h>

/**
 *  unofficial bit distribution table for comprehension purposes only
 *
 *  scalar	nickname	utf-16		utf-8[0]  utf-8[1]  utf-8[2]  utf-8[3]
 *  ------	--------	--------	--------  --------  --------  --------
 *  00000000	7x		00000000	0xxxxxxx
 *  0xxxxxxx			0xxxxxxx
 *
 *  00000yyy	5y6x		00000yyy	110yyyyy  10xxxxxx
 *  yyxxxxxx			yyxxxxxx
 *
 *  zzzzyyyy	4z6y6x		zzzzyyyy	1110zzzz  10yyyyyy  10xxxxxx
 *  yyxxxxxx			yyxxxxxx
 *
 *  000uuuuu	5u4z6y6x	110110ww	11110uuu  10uuzzzz  10yyyyyy  10xxxxxx
 *  zzzzyyyy			wwzzzzyy
 *  yyxxxxxx			110111yy
 *				yyxxxxxx        (where wwww = uuuuu - 1)
 *
 *
 *  validation of utf-8
 *
 *  from        to          utf-8[0]      utf-8[1]      utf-8[2]      utf-8[3]
 *  ------      ------      --------      --------      --------      --------
 *  0x0000      0x007F      00..7F
 *  0x0080      0x07FF      C2..DF        80..BF
 *  0x0800      0x0FFF      E0            A0..BF        80..BF
 *  0x1000      0xCFFF      E1..EC        80..BF        80..BF
 *  0xD000      0xD7FF      ED            80..9F        80..BF
 *  0xE000      0xFFFF      EE..EF        80..BF        80..BF
 *  0x10000     0x3FFFF     F0            90..BF        80..BF        80..BF
 *  0x40000     0xFFFFF     F1..F3        80..BF        80..BF        80..BF
 *  0x100000    0x10FFFF    F4            80..8F        80..BF        80..BF
 *
**/


#define __AVAILABLE_CODE_POINTS	0x110000

int __stdcall __ntapi_uc_get_code_point_byte_count_utf8(uint32_t code_point)
{
	/* try clearing 7x bits */
	if ((code_point >> 7) == 0)
		return 1;

	/* try clearing 5y + 6x bits */
	else if ((code_point >> 11) == 0)
		return 2;

	/* try clearing 4z +6y + 6x bits */
	else if ((code_point >> 16) == 0)
		return 3;

	/* try clearing 5u + 4z + 6y + 6x bits */
	else if ((code_point >> 21) == 0)
		return 4;

	/* __AVAILABLE_CODE_POINTS exceeded */
	else
		return 0;
}


int __stdcall __ntapi_uc_get_code_point_byte_count_utf16(uint32_t code_point)
{
	/* try clearing 4z +6y + 6x bits */
	if ((code_point >> 16) == 0)
		return 2;

	/* try clearing 5u + 4z + 6y + 6x bits */
	else if ((code_point >> 21) == 0)
		return 4;

	/* __AVAILABLE_CODE_POINTS exceeded */
	else
		return 0;
}


/**
 *  following is a straight-forward implementation
 *  of unicode conversion and validation (see also:
 *  Table 3-7 of the Unicode Standard, version 6.2).
 *
 *  the use of callbacks allows the validation
 *  functions to be the basis of our utf-8 conversion
 *  functions on the one hand, and the posix path arg
 *  normalization routine on the other.
**/

static int32_t __fastcall __default_callback_fn_utf8(nt_utf8_callback_args * args)
{
	args->src += args->byte_count;
	return NT_STATUS_SUCCESS;
}

int32_t __stdcall __ntapi_uc_validate_unicode_stream_utf8(
	__in	const unsigned char *		ch,
	__in	size_t				size_in_bytes	__optional,
	__out	size_t *			code_points	__optional,
	__out	void **				addr_failed	__optional,
	__in	ntapi_uc_utf8_callback_fn **	callback_fn	__optional,
	__in	nt_utf8_callback_args *		callback_args	__optional)
{
	const unsigned char *	utf8;
	unsigned char *		ch_boundary;
	unsigned char		byte_count;
	size_t			_code_points;

	ntapi_uc_utf8_callback_fn *	_callback_fn[5];
	nt_utf8_callback_args		_callback_args;

	if (!callback_fn) {
		_callback_fn[0] = __default_callback_fn_utf8;
		_callback_fn[1] = __default_callback_fn_utf8;
		_callback_fn[2] = __default_callback_fn_utf8;
		_callback_fn[3] = __default_callback_fn_utf8;
		_callback_fn[4] = __default_callback_fn_utf8;
		callback_fn = (ntapi_uc_utf8_callback_fn **)&_callback_fn;
	}

	if (!callback_args) {
		callback_args = &_callback_args;
		callback_args->src = (unsigned char *)0;
	}

	if (callback_args->src)
		ch = callback_args->src;
	else
		callback_args->src = ch;

	if (size_in_bytes)
		ch_boundary = (unsigned char *)((uintptr_t)ch + size_in_bytes);
	else
		ch_boundary = (unsigned char *)(~0);

	if (!code_points)
		code_points = &_code_points;

	while ((ch < ch_boundary) && (*ch)) {
		utf8 		= ch;
		byte_count	= 0;

		/* try one byte */
		if (utf8[0] <= 0x7F)
			byte_count = 1;

		/* try two bytes */
		else if ((++ch < ch_boundary)
				&& (utf8[0] >= 0xC2) && (utf8[0] <= 0xDF)
				&& (utf8[1] >= 0x80) && (utf8[1] <= 0xBF))
			byte_count = 2;

		/* try three bytes */
		else if ((++ch < ch_boundary)
				&& (utf8[0] == 0xE0)
				&& (utf8[1] >= 0xA0) && (utf8[1] <= 0xBF)
				&& (utf8[2] >= 0x80) && (utf8[2] <= 0xBF))
			byte_count = 3;

		else if (
				(utf8[0] >= 0xE1) && (utf8[0] <= 0xEC)
				&& (utf8[1] >= 0x80) && (utf8[1] <= 0xBF)
				&& (utf8[2] >= 0x80) && (utf8[2] <= 0xBF))
			byte_count = 3;

		else if (
				(utf8[0] == 0xED)
				&& (utf8[1] >= 0x80) && (utf8[1] <= 0x9F)
				&& (utf8[2] >= 0x80) && (utf8[2] <= 0xBF))
			byte_count = 3;

		else if (
				(utf8[0] >= 0xEE) && (utf8[0] <= 0xEF)
				&& (utf8[1] >= 0x80) && (utf8[1] <= 0xBF)
				&& (utf8[2] >= 0x80) && (utf8[2] <= 0xBF))
			byte_count = 3;

		/* try four bytes */
		else if ((++ch < ch_boundary)
				&& (utf8[0] == 0xF0)
				&& (utf8[1] >= 0x90) && (utf8[1] <= 0xBF)
				&& (utf8[2] >= 0x80) && (utf8[2] <= 0xBF)
				&& (utf8[3] >= 0x80) && (utf8[3] <= 0xBF))
			byte_count = 4;

		else if (
				(utf8[0] >= 0xF1) && (utf8[0] <= 0xF3)
				&& (utf8[1] >= 0x80) && (utf8[1] <= 0xBF)
				&& (utf8[2] >= 0x80) && (utf8[2] <= 0xBF)
				&& (utf8[3] >= 0x80) && (utf8[3] <= 0xBF))
			byte_count = 4;

		else if (
				(utf8[0] == 0xF4)
				&& (utf8[1] >= 0x80) && (utf8[1] <= 0x8F)
				&& (utf8[2] >= 0x80) && (utf8[2] <= 0xBF)
				&& (utf8[3] >= 0x80) && (utf8[3] <= 0xBF))
			byte_count = 4;

		if (byte_count) {
			(*code_points)++;
			callback_args->byte_count = byte_count;
			callback_fn[byte_count](callback_args);
		} else {
			if (addr_failed)
				*addr_failed = (void *)utf8;
			return NT_STATUS_ILLEGAL_CHARACTER;
		}

		/* advance, transcode if needed */
		ch = callback_args->src;
	}

	if ((ch < ch_boundary) && (*ch == 0))
		callback_fn[0](callback_args);

	return NT_STATUS_SUCCESS;
}


static int32_t __fastcall __default_callback_fn_utf16(nt_utf16_callback_args * args)
{
	if (args->byte_count == 4)
		args->src += 2;
	else
		args->src++;

	return NT_STATUS_SUCCESS;
}


int32_t __stdcall __ntapi_uc_validate_unicode_stream_utf16(
	__in	const wchar16_t *		wch,
	__in	size_t				size_in_bytes	__optional,
	__out	size_t *			code_points	__optional,
	__out	void **				addr_failed	__optional,
	__in	ntapi_uc_utf16_callback_fn **	callback_fn	__optional,
	__in	nt_utf16_callback_args *	callback_args	__optional)
{
	const wchar16_t * wch_trail;
	wchar16_t *	  wch_boundary;
	unsigned char	  byte_count;
	size_t		  _code_points;

	ntapi_uc_utf16_callback_fn *	_callback_fn[5];
	nt_utf16_callback_args		_callback_args;

	if (!callback_fn) {
		_callback_fn[0] = __default_callback_fn_utf16;
		_callback_fn[1] = __default_callback_fn_utf16;
		_callback_fn[2] = __default_callback_fn_utf16;
		_callback_fn[3] = __default_callback_fn_utf16;
		_callback_fn[4] = __default_callback_fn_utf16;
		callback_fn = (ntapi_uc_utf16_callback_fn **)&_callback_fn;
	}

	if (!callback_args) {
		callback_args = &_callback_args;
		callback_args->src = (wchar16_t *)0;
	}

	if (callback_args->src)
		wch = callback_args->src;
	else
		callback_args->src = wch;

	if (size_in_bytes)
		wch_boundary = (wchar16_t *)((uintptr_t)wch + size_in_bytes);
	else
		wch_boundary = (wchar16_t *)(~0);

	if (!code_points)
		code_points = &_code_points;

	while ((wch < wch_boundary) && (*wch)) {
		byte_count	= 0;

		/* try one byte */
		if (*wch <= 0x7F)
			byte_count = 1;

		/* try two bytes */
		else if (*wch <= 0x7FF)
			byte_count = 2;

		/* try three bytes */
		else if ((*wch < 0xD800) || (*wch >= 0xE000))
			byte_count = 3;

		/* try four bytes */
		else if ((*wch >= 0xD800) && (*wch < 0xDC00)) {
			wch_trail = wch + 1;

			if ((wch_trail < wch_boundary)
					&& (*wch_trail >= 0xDC00)
					&& (*wch_trail < 0xE000))
				byte_count = 4;
		}

		if (byte_count) {
			(*code_points)++;
			callback_args->byte_count = byte_count;
			callback_fn[byte_count](callback_args);
		} else {
			if (addr_failed)
				*addr_failed = (void *)wch;
			return NT_STATUS_ILLEGAL_CHARACTER;
		}

		/* advance, transcode as needed */
		wch = callback_args->src;
	}

	if ((wch < wch_boundary) && (*wch == 0))
		callback_fn[0](callback_args);

	return NT_STATUS_SUCCESS;
}
