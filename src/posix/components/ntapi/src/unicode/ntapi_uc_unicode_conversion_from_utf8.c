/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_status.h>
#include <ntapi/nt_unicode.h>
#include "ntapi_impl.h"


typedef struct ___two_bytes {
	unsigned char	low;
	unsigned char	high;
} __two_bytes;


typedef struct ___three_bytes {
	unsigned char	low;
	unsigned char	middle;
	unsigned char	high;
} __three_bytes;


static int32_t __fastcall __utf8_to_utf16_handler_1byte_or_null_termination(nt_utf8_callback_args * args)
{
	/***************************/
	/* from: 0xxxxxxx          */
	/* to:   00000000 0xxxxxxx */
	/***************************/

	wchar16_t *	dst;

	if (args->dst >= args->dst_cap)
		return NT_STATUS_BUFFER_TOO_SMALL;

	dst  = (wchar16_t *)args->dst;
	*dst = *(args->src);

	/* advance source and destination buffer */
	args->src++;
	args->dst = (void *)((uintptr_t)(args->dst) + sizeof(wchar16_t));

	/* bytes_written */
	args->bytes_written += sizeof(wchar16_t);

	return NT_STATUS_SUCCESS;
}


static int32_t __fastcall __utf8_to_utf16_handler_2bytes(nt_utf8_callback_args * args)
{
	/***************************/
	/* from: 110yyyyy 10xxxxxx */
	/* to:   00000yyy yyxxxxxx */
	/***************************/

	__two_bytes *		src; /* big endian */
	wchar16_t *		dst;

	if (args->dst >= args->dst_cap)
		return NT_STATUS_BUFFER_TOO_SMALL;

	src	= (__two_bytes *)args->src;
	dst	= (wchar16_t *)args->dst;

	/* yyyyy */
	*dst   = (src->low ^ 0xC0);
	*dst <<= 6;

	/* xxxxxx */
	*dst |= (src->high  ^ 0x80);

	/* advance source and destination buffer */
	args->src += 2;
	args->dst = (void *)((uintptr_t)(args->dst) + sizeof(wchar16_t));

	/* bytes_written */
	args->bytes_written += sizeof(wchar16_t);

	return NT_STATUS_SUCCESS;
}


static int32_t __fastcall __utf8_to_utf16_handler_3bytes(nt_utf8_callback_args * args)
{
	/************************************/
	/* from: 1110zzzz 10yyyyyy 10xxxxxx */
	/* to:   zzzzyyyy yyxxxxxx          */
	/************************************/

	__three_bytes *		src; /* big endian */
	wchar16_t *		dst;
	wchar16_t		yyyyy;

	if (args->dst >= args->dst_cap)
		return NT_STATUS_BUFFER_TOO_SMALL;

	src	= (__three_bytes *)args->src;
	dst	= (wchar16_t *)args->dst;

	/* zzzz */
	*dst   = (src->low ^ 0xE0);
	*dst <<= 12;

	/* yyyyy */
	yyyyy   = (src->middle ^ 0x80);
	yyyyy <<= 6;
	*dst |= yyyyy;

	/* xxxxxx */
	*dst |= (src->high ^ 0x80);

	/* advance source and destination buffer */
	args->src += 3;
	args->dst = (void *)((uintptr_t)(args->dst) + sizeof(wchar16_t));

	/* bytes_written */
	args->bytes_written += sizeof(wchar16_t);

	return NT_STATUS_SUCCESS;
}


static int32_t __fastcall __utf8_to_utf16_handler_4bytes(nt_utf8_callback_args * args)
{
	/*************************************************/
	/* from: 11110uuu  10uuzzzz  10yyyyyy  10xxxxxx  */
	/* to:   110110ww  wwzzzzyy  110111yy  yyxxxxxx  */
	/*************************************************/

	__two_bytes *		src_low;	/* big endian */
	__two_bytes *		src_high;	/* big endian */
	wchar16_t *		dst_lead;
	wchar16_t *		dst_trail;

	wchar16_t		u;
	unsigned char		ulow;
	unsigned char		uhigh;
	unsigned char		yyyy;

	dst_lead = dst_trail = (wchar16_t *)args->dst;
	dst_trail++;

	if ((uintptr_t)dst_trail >= (uintptr_t)args->dst_cap)
		return NT_STATUS_BUFFER_TOO_SMALL;

	src_low	= src_high = (__two_bytes *)args->src;
	src_high++;

	/* u */
	ulow	= src_low->low  ^ 0xF0;
	uhigh	= src_low->high ^ 0x80;

	ulow  <<= 2;
	uhigh >>= 4;

	u = ulow | uhigh;

	/* 110110ww wwzzzzyy */
	*dst_lead  = 0xD800;
	*dst_lead |= ((u-1) << 6);
	*dst_lead |= ((src_low->high ^ 0x80) << 2);
	*dst_lead |= ((src_high->low ^ 0x80) >> 4);

	/* 110111yy  yyxxxxxx */
	yyyy         = (src_high->low << 4);
	*dst_trail   = yyyy;
	*dst_trail <<= 2;
	*dst_trail  |= (src_high->high ^ 0x80);
	*dst_trail  |= 0xDC00;

	/* advance source and destination buffer */
	args->src += 4;
	args->dst = (void *)((uintptr_t)(args->dst) + (2 * sizeof(wchar16_t)));

	/* bytes_written */
	args->bytes_written += 2 * sizeof(wchar16_t);

	return NT_STATUS_SUCCESS;
}


static int32_t __fastcall __update_stream_leftover_info_utf8(
	__in_out	nt_unicode_conversion_params_utf8_to_utf16 *	params)
{
	int32_t		status;
	ptrdiff_t	offset;
	unsigned char *	utf8;

	offset	= (uintptr_t)params->src + (uintptr_t)params->src_size_in_bytes - (uintptr_t)params->addr_failed;
	utf8	= (unsigned char *)params->addr_failed;

	/* default status */
	status	= NT_STATUS_ILLEGAL_CHARACTER;

	if (offset == 1) {
		if ((utf8[0] >= 0xC2) && (utf8[0] <= 0xF4)) {
			/* one leftover byte */
			params->leftover_count = 1;
			params->leftover_bytes = utf8[0];
			params->leftover_bytes <<= 24;
			status = NT_STATUS_SUCCESS;
		}
	} else 	if (offset == 2) {
		if /* ------- */  (((utf8[0] == 0xE0) &&                      (utf8[1] >= 0xA0) && (utf8[1] <= 0xBF))
				|| ((utf8[0] >= 0xE1) && (utf8[0] <= 0xEC) && (utf8[1] >= 0x80) && (utf8[1] <= 0xBF))
				|| ((utf8[0] == 0xED) &&                      (utf8[1] >= 0x80) && (utf8[1] <= 0x9F))
				|| ((utf8[0] >= 0xEE) && (utf8[0] <= 0xEF) && (utf8[1] >= 0x80) && (utf8[1] <= 0xBF))
				|| ((utf8[0] == 0xF0) &&                      (utf8[1] >= 0x90) && (utf8[1] <= 0xBF))
				|| ((utf8[0] >= 0xF1) && (utf8[0] <= 0xF3) && (utf8[1] >= 0x80) && (utf8[1] <= 0xBF))
				|| ((utf8[0] == 0xF4) &&                      (utf8[1] >= 0x80) && (utf8[1] <= 0x8F))) {
			/* two leftover bytes */
			params->leftover_count = 2;
			params->leftover_bytes = utf8[0];
			params->leftover_bytes <<= 8;
			params->leftover_bytes += utf8[1];
			params->leftover_bytes <<= 16;
			status = NT_STATUS_SUCCESS;
		}
	} else if (offset == 3) {
		if /* ------- */  (((utf8[0] == 0xF0) &&                      (utf8[1] >= 0x90) && (utf8[1] <= 0xBF))
				|| ((utf8[0] >= 0xF1) && (utf8[0] <= 0xF3) && (utf8[1] >= 0x80) && (utf8[1] <= 0xBF))
				|| ((utf8[0] == 0xF4) &&                      (utf8[1] >= 0x80) && (utf8[1] <= 0x8F))) {
			/* three leftover bytes */
			params->leftover_count = 3;
			params->leftover_bytes = utf8[0];
			params->leftover_bytes <<= 8;
			params->leftover_bytes += utf8[1];
			params->leftover_bytes <<= 8;
			params->leftover_bytes += utf8[2];
			params->leftover_bytes <<= 8;
			status = NT_STATUS_SUCCESS;
		}
	}

	if (status != NT_STATUS_SUCCESS) {
		params->leftover_count = 0;
		params->leftover_bytes = 0;
	}

	return status;
}

int32_t __stdcall 	__ntapi_uc_convert_unicode_stream_utf8_to_utf16(
	__in_out	nt_unicode_conversion_params_utf8_to_utf16 *	params)
{
	int32_t 			status;
	nt_utf8_callback_args		args;
	ntapi_uc_utf8_callback_fn *	callback_fn[5];

	callback_fn[0] = (ntapi_uc_utf8_callback_fn *)__utf8_to_utf16_handler_1byte_or_null_termination;
	callback_fn[1] = (ntapi_uc_utf8_callback_fn *)__utf8_to_utf16_handler_1byte_or_null_termination;
	callback_fn[2] = (ntapi_uc_utf8_callback_fn *)__utf8_to_utf16_handler_2bytes;
	callback_fn[3] = (ntapi_uc_utf8_callback_fn *)__utf8_to_utf16_handler_3bytes;
	callback_fn[4] = (ntapi_uc_utf8_callback_fn *)__utf8_to_utf16_handler_4bytes;

	args.src		= params->src;
	args.dst		= params->dst;
	args.dst_cap		= (void *)((uintptr_t)(params->dst) + (params->dst_size_in_bytes));
	args.bytes_written	= params->bytes_written;

	status = __ntapi_uc_validate_unicode_stream_utf8(
		params->src,
		params->src_size_in_bytes,
		&params->code_points,
		&params->addr_failed,
		callback_fn,
		&args);

	params->bytes_written = args.bytes_written;

	if (status != NT_STATUS_SUCCESS)
		status = __update_stream_leftover_info_utf8(params);

	/* (optimized out on 32-bit architectures) */
	params->leftover_bytes <<= (8 * (sizeof(uintptr_t) - sizeof(uint32_t)));

	return status;
}


int32_t __stdcall 	__ntapi_uc_convert_unicode_stream_utf8_to_utf32(
	__in_out	nt_unicode_conversion_params_utf8_to_utf32 *	params)
{
	return NT_STATUS_SUCCESS;
}
