/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include <ntapi/nt_argv.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

/**
 * scenario: program -e app [arg1 arg2 ... argn]
 * input:    a utf-16 argument vector
 * output:   a utf-16 cmd_line string
 * example:  tty_pipe_create_child_process
**/

int32_t __stdcall __ntapi_tt_array_copy_utf16(
	__out	int *			argc,
	__in	const wchar16_t **	wargv,
	__in	const wchar16_t **	wenvp,
	__in	const wchar16_t *	image_name	__optional,
	__in	const wchar16_t *	interpreter	__optional,
	__in	const wchar16_t *	optarg		__optional,
	__in	void *			base,
	__out	void *			buffer,
	__in	size_t			buflen,
	__out	size_t *		blklen)
{
	const wchar16_t **	parg;
	const wchar16_t *	warg;
	const wchar16_t *	dummy;
	wchar16_t *		wch;
	ptrdiff_t		diff;
	ptrdiff_t		ptrs;
	size_t			needed;

	/* fallback */
	dummy = 0;
	wargv = wargv ? wargv : &dummy;
	wenvp = wenvp ? wenvp : &dummy;

	/* ptrs, needed */
	ptrs   = 0;
	needed = 0;

	if (image_name) {
		ptrs++;
		needed += sizeof(wchar16_t *)
			+ __ntapi->tt_string_null_offset_short((const int16_t *)image_name)
			+ sizeof(wchar16_t);
	}

	for (parg=wargv; *parg; parg++)
		needed += sizeof(wchar16_t *)
			+ __ntapi->tt_string_null_offset_short((const int16_t *)*parg)
			+ sizeof(wchar16_t);

	ptrs += (parg - wargv);
	*argc = (int)ptrs;

	for (parg=wenvp; *parg; parg++)
		needed += sizeof(wchar16_t *)
			+ __ntapi->tt_string_null_offset_short((const int16_t *)*parg)
			+ sizeof(wchar16_t);

	ptrs += (parg - wenvp);

	ptrs    += 2;
	needed  += 2*sizeof(wchar16_t *);
	blklen  = blklen ? blklen : &needed;
	*blklen = needed;

	if (buflen < needed)
		return NT_STATUS_BUFFER_TOO_SMALL;

	/* init */
	parg = (const wchar16_t **)buffer;
	wch  = (wchar16_t *)(parg+ptrs);
	diff = (uintptr_t)base / sizeof(wchar16_t);

	/* image_name */
	if (image_name) {
		*parg++ = wch-diff;
		for (warg=image_name; *warg; warg++,wch++)
			*wch = *warg;
		*wch++ = '\0';
	}

	/* argv */
	for (; *wargv; wargv++) {
		*parg++=wch-diff;
		for (warg=*wargv; *warg; warg++,wch++)
			*wch = *warg;
		*wch++ = '\0';
	}

	*parg++ = 0;

	/* envp */
	for (; *wenvp; wenvp++) {
		*parg++=wch-diff;
		for (warg=*wenvp; *warg; warg++,wch++)
			*wch = *warg;
		*wch++ = '\0';
	}

	*parg++ = 0;

	return NT_STATUS_SUCCESS;
}

int32_t __stdcall __ntapi_tt_array_convert_utf16_to_utf8(
	__in		wchar16_t **			warrv,
	__in		char **				arrv,
	__in		void *				base,
	__in		char *				buffer,
	__in		size_t				buffer_len,
	__out		size_t *			bytes_written)
{
	uint8_t *	ubound;
	uint8_t *	ch;
	wchar16_t *	wch;
	wchar16_t	wx;
	wchar16_t	wy;
	wchar16_t	wz;
	wchar16_t	wy_low;
	wchar16_t	wy_high;
	wchar16_t	ww;
	wchar16_t	uuuuu;
	wchar16_t	u_low;
	wchar16_t	u_high;
	ptrdiff_t	diff;

	#define __UTF8_MAX_CODE_POINT_BYTES	(4)

	ch	= (uint8_t *)buffer;
	ubound	= (uint8_t *)buffer + buffer_len - __UTF8_MAX_CODE_POINT_BYTES;
	diff	= (uintptr_t)base / sizeof(wchar16_t);

	while (warrv && *warrv) {
		*arrv	= (char *)(ch-(uintptr_t)base);
		wch	= *warrv + diff;

		/* all utf-16 streams at stake have been validated */
		while (*wch && (ch < ubound)) {
			if (*wch <= 0x7F) {
				/* from: 00000000 0xxxxxxx (little endian) */
				/* to:   0xxxxxxx          (utf-8)    */
				*ch = (char)(*wch);
			} else if (*wch <= 0x7FF) {
				/* from: 00000yyy yyxxxxxx (little endian) */
				/* to:   110yyyyy 10xxxxxx (utf-8)    */
				wy  = *wch;
				wy >>= 6;

				wx  = *wch;
				wx <<= 10;
				wx >>= 10;

				/* write the y part */
				*ch = (char)(0xC0 | wy);
				ch++;

				/* write the x part */
				*ch = (char)(0x80 | wx);
			} else if ((*wch < 0xD800) || (*wch >= 0xE000)) {
				/* from: zzzzyyyy yyxxxxxx (little endian)  */
				/* to:   1110zzzz 10yyyyyy 10xxxxxx (utf-8) */
				wz  = *wch;
				wz >>= 12;

				wy  = *wch;
				wy <<= 4;
				wy >>= 10;

				wx  = *wch;
				wx <<= 10;
				wx >>= 10;

				/* write the z part */
				*ch = (char)(0xE0 | wz);
				ch++;

				/* write the y part */
				*ch = (char)(0x80 | wy);
				ch++;

				/* write the x part */
				*ch = (char)(0x80 | wx);
			} else {
				/* from: 110110ww  wwzzzzyy  110111yy  yyxxxxxx (little endian) */
				/* to:   11110uuu  10uuzzzz  10yyyyyy  10xxxxxx (utf-8)         */

				/* low two bytes */
				wx  = *wch;
				wx <<= 10;
				wx >>= 10;

				wy_low  = *wch;
				wy_low <<= 6;
				wy_low >>= 12;

				/* (surrogate pair) */
				wch++;

				/* high two bytes */
				wy_high  = *wch;
				wy_high <<= 14;
				wy_high >>= 10;

				wz  = *wch;
				wz <<= 10;
				wz >>= 12;
				wz <<= 2;

				ww  = *wch;
				ww <<= 6;
				ww >>= 12;

				uuuuu  = ww + 1;
				u_high  = uuuuu >> 2;
				u_low = ((uuuuu << 14) >> 10);

				/* 1st byte: 11110uuu */
				*ch = (char)(0xF0 | u_high);
				ch++;

				/* 2nd byte: 10uuzzzz */
				*ch = (char)(0x80 | u_low | wz);
				ch++;

				/* 3rd byte: 10yyyyyy */
				*ch = (char)(0x80 | wy_low | wy_high);
				ch++;

				/* 4th byte: 10xxxxxx */
				*ch = (char)(0x80 | wx);
			}

			ch++;
			wch++;
		}

		if (*wch)
			return NT_STATUS_BUFFER_TOO_SMALL;

		ch++;
		arrv++;
		warrv++;
	}

	*bytes_written = (size_t)(ch - (uint8_t *)buffer);

	return NT_STATUS_SUCCESS;
}
