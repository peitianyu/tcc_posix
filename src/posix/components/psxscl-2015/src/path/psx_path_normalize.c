/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_limits.h"
#include "psx_path.h"
#include "psx.h"

#define __UTF8_CHAR_IS_NON_SLASH	0x01
#define __UTF8_CHAR_IS_AGNOSTIC		0x02
#define __UTF8_CHAR_IS_FIRST		0x04

static int32_t __fastcall __normalize_validated_bytes_utf8(nt_utf8_callback_args * args);
static ntapi_uc_utf8_callback_fn * callback_fn[5] = {
							__normalize_validated_bytes_utf8,
							__normalize_validated_bytes_utf8,
							__normalize_validated_bytes_utf8,
							__normalize_validated_bytes_utf8,
							__normalize_validated_bytes_utf8};

static int32_t __fastcall __normalize_validated_bytes_utf8(nt_utf8_callback_args * args)
{
	const unsigned char *	ch_cap;
	uint32_t		copy_flags;

	if ((uintptr_t)(args->dst) + args->byte_count >= (uintptr_t)args->dst_cap)
		return NT_STATUS_BUFFER_TOO_SMALL;

	ch_cap = args->src + args->byte_count;

	while (args->src < ch_cap) {
		copy_flags = 0;

		if ((*(args->src) != '/') && (*(args->src) != '/'))
			copy_flags |= __UTF8_CHAR_IS_NON_SLASH;

		else if (!args->bytes_written)
			copy_flags |= __UTF8_CHAR_IS_FIRST;

		else if ((*(args->src - 1) != '/') && (*(args->src - 1) != '\\'))
			copy_flags |= __UTF8_CHAR_IS_AGNOSTIC;

		if (copy_flags) {
			*(unsigned char *)(args->dst) = *(args->src);
			args->dst = (unsigned char *)(args->dst) + 1;
			args->bytes_written++;
		}

		args->src++;
	}

	return NT_STATUS_SUCCESS;
}


static int32_t __fastcall __validate_and_normalize_path_utf8(
	nt_unicode_conversion_params_utf8_to_utf16 * params)
{
	int32_t 			status;
	nt_utf8_callback_args		args;

	args.src		= params->src;
	args.dst		= params->dst;
	args.dst_cap		= (void *)((uintptr_t)(params->dst) + (params->dst_size_in_bytes));
	args.bytes_written	= params->bytes_written;

	status = __ntapi->uc_validate_unicode_stream_utf8(
		params->src,
		params->src_size_in_bytes,
		&params->code_points,
		&params->addr_failed,
		callback_fn,
		&args);

	params->bytes_written = args.bytes_written;

	return status;
}


int32_t __fastcall __psx_normalize_path_utf8(
	const unsigned char *	path_arg,
	struct __path_info *	path_obj)
{
	int32_t	status;
	int two_slashes,pad;
	const unsigned char * ch;
	nt_unicode_conversion_params_utf8_to_utf16 params;

	__ntapi->tt_aligned_block_memset(
		&params,0,sizeof(params));

	params.src		 = path_arg;
	params.dst		 = (wchar16_t *)path_obj->inbuf;
	params.dst_size_in_bytes = path_obj->avail;

	/* '//' is implementation defined */
	ch = path_arg;

	if ((*ch == '/')
			&& (*(++ch) == '/')
			&& (*(++ch) != '/'))
		two_slashes = true;
	else if ((*ch == '\\')
			&& (*(++ch) == '\\')
			&& (*(++ch) != '\\'))
		two_slashes = true;
	else
		two_slashes = false;

	if (two_slashes) {
		*params.dst = 0x2f2f;
		params.dst++;
		params.bytes_written = 2;

		params.src		 += 2;
		params.dst_size_in_bytes -= 2;
	}

	if ((status = __validate_and_normalize_path_utf8(&params)))
		return status;

	path_obj->used   = params.bytes_written;
	path_obj->avail -= params.bytes_written;

	pad = path_obj->avail % sizeof(uintptr_t);
	path_obj->avail -= pad;
	path_obj->used  += pad;

	return status;
}
