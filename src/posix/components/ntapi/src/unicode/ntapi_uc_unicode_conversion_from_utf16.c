/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_status.h>
#include <ntapi/nt_unicode.h>
#include "ntapi_impl.h"


static int32_t __fastcall __utf16_to_utf8_handler_1byte_or_null_termination(nt_utf16_callback_args * args)
{
	/*******************************************/
	/* from: 00000000 0xxxxxxx (little endian) */
	/* to:   0xxxxxxx          (utf-8)         */
	/*******************************************/

	uint8_t * dst;

	if (args->dst >= args->dst_cap)
		return NT_STATUS_BUFFER_TOO_SMALL;

	dst  = (uint8_t *)args->dst;
	*dst = *(uint8_t *)(args->src);

	/* advance source and destination buffer */
	args->src++;
	args->dst = (void *)((uintptr_t)(args->dst) + 1);

	/* bytes_written */
	args->bytes_written++;

	return NT_STATUS_SUCCESS;
}


static int32_t __fastcall __utf16_to_utf8_handler_2bytes(nt_utf16_callback_args * args)
{
	/*******************************************/
	/* from: 00000yyy yyxxxxxx (little endian) */
	/* to:   110yyyyy 10xxxxxx (utf-8)         */
	/*******************************************/

	const wchar16_t * src;
	uint8_t *	  dst;

	wchar16_t	wx;
	wchar16_t	wy;

	if ((uintptr_t)(args->dst) + 1 >= (uintptr_t)(args->dst_cap))
		return NT_STATUS_BUFFER_TOO_SMALL;

	src = args->src;
	dst = (uint8_t *)args->dst;

	wy  = *src;
	wy >>= 6;

	wx  = *src;
	wx <<= 10;
	wx >>= 10;

	/* write the y part */
	*dst = (char)(0xC0 | wy);
	dst++;

	/* write the x part */
	*dst = (char)(0x80 | wx);

	/* advance source and destination buffer */
	args->src++;
	args->dst = (void *)((uintptr_t)(args->dst) + 2);

	/* bytes_written */
	args->bytes_written += 2;

	return NT_STATUS_SUCCESS;
}


static int32_t __fastcall __utf16_to_utf8_handler_3bytes(nt_utf16_callback_args * args)
{
	/********************************************/
	/* from: zzzzyyyy yyxxxxxx (little endian)  */
	/* to:   1110zzzz 10yyyyyy 10xxxxxx (utf-8) */
	/********************************************/

	const wchar16_t * src;
	uint8_t *	  dst;

	wchar16_t	wx;
	wchar16_t	wy;
	wchar16_t	wz;

	if ((uintptr_t)(args->dst) + 2 >= (uintptr_t)(args->dst_cap))
		return NT_STATUS_BUFFER_TOO_SMALL;

	src = args->src;
	dst = (uint8_t *)args->dst;

	wz  = *src;
	wz >>= 12;

	wy  = *src;
	wy <<= 4;
	wy >>= 10;

	wx  = *src;
	wx <<= 10;
	wx >>= 10;

	/* write the z part */
	*dst = (char)(0xE0 | wz);
	dst++;

	/* write the y part */
	*dst = (char)(0x80 | wy);
	dst++;

	/* write the x part */
	*dst = (char)(0x80 | wx);

	/* advance source and destination buffer */
	args->src++;
	args->dst = (void *)((uintptr_t)(args->dst) + 3);

	/* bytes_written */
	args->bytes_written += 3;

	return NT_STATUS_SUCCESS;
}


static int32_t __fastcall __utf16_to_utf8_handler_4bytes(nt_utf16_callback_args * args)
{
	/****************************************************************/
	/* from: 110110ww  wwzzzzyy  110111yy  yyxxxxxx (little endian) */
	/* to:   11110uuu  10uuzzzz  10yyyyyy  10xxxxxx (utf-8)         */
	/****************************************************************/

	const wchar16_t * src;
	uint8_t *	  dst;

	wchar16_t	wx;
	wchar16_t	wz;

	wchar16_t	wy_low;
	wchar16_t	wy_high;
	wchar16_t	ww;
	wchar16_t	uuuuu;
	wchar16_t	u_low;
	wchar16_t	u_high;

	if ((uintptr_t)(args->dst) + 3 >= (uintptr_t)(args->dst_cap))
		return NT_STATUS_BUFFER_TOO_SMALL;

	src = args->src;
	dst = (uint8_t *)args->dst;

	/* low two bytes */
	wx  = *src;
	wx <<= 10;
	wx >>= 10;

	wy_low  = *src;
	wy_low <<= 6;
	wy_low >>= 12;

	/* (surrogate pair) */
	src++;

	/* high two bytes */
	wy_high  = *src;
	wy_high <<= 14;
	wy_high >>= 10;

	wz  = *src;
	wz <<= 10;
	wz >>= 12;
	wz <<= 2;

	ww  = *src;
	ww <<= 6;
	ww >>= 12;

	uuuuu  = ww + 1;
	u_high  = uuuuu >> 2;
	u_low = ((uuuuu << 14) >> 10);

	/* 1st byte: 11110uuu */
	*dst = (char)(0xF0 | u_high);
	dst++;

	/* 2nd byte: 10uuzzzz */
	*dst = (char)(0x80 | u_low | wz);
	dst++;

	/* 3rd byte: 10yyyyyy */
	*dst = (char)(0x80 | wy_low | wy_high);
	dst++;

	/* 4th byte: 10xxxxxx */
	*dst = (char)(0x80 | wx);

	/* advance source and destination buffer */
	args->src += 2;
	args->dst = (void *)((uintptr_t)(args->dst) + 4);

	/* bytes_written */
	args->bytes_written += 4;

	return NT_STATUS_SUCCESS;
}


static int32_t __fastcall __update_stream_leftover_info_utf16(
	__in_out	nt_unicode_conversion_params_utf16_to_utf8 *	params)
{
	int32_t		status;
	ptrdiff_t	offset;
	wchar16_t *	wlead;

	offset	= (uintptr_t)params->src + (uintptr_t)params->src_size_in_bytes - (uintptr_t)params->addr_failed;
	wlead	= (wchar16_t *)params->addr_failed;


	if ((offset == 2) && (*wlead >= 0xD800) && (*wlead < 0xDC00)) {
			/* possibly the lead of a surrogate pair lead */
			params->leftover_count = 2;
			params->leftover_bytes = *wlead;
			params->leftover_bytes <<= 16;
			status = NT_STATUS_SUCCESS;
	} else {
		params->leftover_count = 0;
		params->leftover_bytes = 0;
		status	= NT_STATUS_ILLEGAL_CHARACTER;
	}

	return status;
}


int32_t __stdcall 	__ntapi_uc_convert_unicode_stream_utf16_to_utf8(
	__in_out	nt_unicode_conversion_params_utf16_to_utf8 *	params)
{
	int32_t 			status;
	nt_utf16_callback_args		args;
	ntapi_uc_utf16_callback_fn *	callback_fn[5];

	callback_fn[0] = (ntapi_uc_utf16_callback_fn *)__utf16_to_utf8_handler_1byte_or_null_termination;
	callback_fn[1] = (ntapi_uc_utf16_callback_fn *)__utf16_to_utf8_handler_1byte_or_null_termination;
	callback_fn[2] = (ntapi_uc_utf16_callback_fn *)__utf16_to_utf8_handler_2bytes;
	callback_fn[3] = (ntapi_uc_utf16_callback_fn *)__utf16_to_utf8_handler_3bytes;
	callback_fn[4] = (ntapi_uc_utf16_callback_fn *)__utf16_to_utf8_handler_4bytes;

	args.src		= params->src;
	args.dst		= params->dst;
	args.dst_cap		= (void *)((uintptr_t)(params->dst) + (params->dst_size_in_bytes));
	args.bytes_written	= params->bytes_written;

	status = __ntapi_uc_validate_unicode_stream_utf16(
		params->src,
		params->src_size_in_bytes,
		&params->code_points,
		&params->addr_failed,
		callback_fn,
		&args);

	params->bytes_written = args.bytes_written;

	if (status)
		status = __update_stream_leftover_info_utf16(params);

	/* the following bit shift will be optimized out on 32-bit architectures */
	params->leftover_bytes <<= (8 * (sizeof(uintptr_t) - sizeof(uint32_t)));

	return status;
}


int32_t __stdcall 	__ntapi_uc_convert_unicode_stream_utf16_to_utf32(
	__in_out	nt_unicode_conversion_params_utf16_to_utf32 *	params)
{
	return NT_STATUS_SUCCESS;
}
