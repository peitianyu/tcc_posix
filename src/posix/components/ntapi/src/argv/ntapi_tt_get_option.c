/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"


/**
 *  a simple facility for minimal programs or system libraries
 *  with no libc available at the time of invocation, as well
 *  as applications using the midipix free-standing development
 *  environment.
 *
 *  the approach taken by this module to the support of short
 *  and long options reflects the above constraint, namely
 *  the absence of a callable libc at the time of invocation;
 *  there is no intent for interfaces in this module to
 *  be POSIXLY correct or otherwise portable.  the sole
 *  purpose of all functions in this module is to serve
 *  internal or otherwise free-standing midipix applications,
 *  and their relevance otherwise is accordingly non-existent.
 *
 *  all options are encoded in utf-16; note, however, that
 *  short options may only use code points that are located
 *  in the basic multilingual plane.
 *
 *  option values are either required or not allowed altogether,
 *  and the first character of an option value may not be a hyphen.
 *  if you need the first character of an option value to be a
 *  hyphen, then make sure you escape it somehow (for instance by
 *  enclosing it in quotation marks).
 *
 *  a short option and its value must reside in two separate
 *  argv[] elements (in other words: -ooutput is illegal).
 *
 *  a long option and its value must reside in the same argv[]
 *  element and be separated by a single equal sign.
 *
 *  Examples of valid options and option values:
 *  --------------------------------------------
 *  -o
 *  -o value
 *  --long-option-with-no-value
 *  --long-option=value
**/

#define HYPHEN		0x2D
#define EQUAL_SIGN	0x3D


static int __inline__ __fastcall __is_bmp_code_point(wchar16_t code_point)
{
	return (((code_point >= 0x0000) && (code_point < 0xD800)) \
		|| ((code_point >= 0xE000) && (code_point < 0x10000)));
}


static int __inline__ __fastcall __is_last_program_option(
	__in	nt_program_option *	option)
{
	return (!(option->short_name_code))
		&& (!(option->long_name))
		&& (!(option->long_name_hash));
}


static int __fastcall __is_short_option(wchar16_t * wch)
{
	return ((wch) && (*wch == HYPHEN)
		&& __is_bmp_code_point(*++wch)
		&& (*++wch == 0));
}

static int __fastcall __is_long_option(wchar16_t * wch)
{
	return ((wch) && (*wch == HYPHEN)
		&& (++wch) && (*wch == HYPHEN)
		&& (*++wch));
}


static int __fastcall __is_last_option_argument(wchar16_t * wch)
{
	return ((wch) && (*wch == HYPHEN)
		&& (*++wch == HYPHEN)
		&& (*++wch == 0));
}


static uint32_t __fastcall __compute_crc32_utf16_str(
	__in	const uint32_t *	crc32_table,
	__in	wchar16_t *		wch)
{
	uint32_t	crc32;
	unsigned char *	byte_buffer;

	/* crc32 hash... */
	crc32 = 0 ^ 0xFFFFFFFF;

	/* initialize byte_buffer */
	byte_buffer = (unsigned char *)wch;

	/* iterate */
	while (*byte_buffer) {
		/* two bytes at a time */
		crc32 = (crc32 >> 8) ^ crc32_table[(crc32 ^ *byte_buffer) & 0xFF];
		byte_buffer++;
		crc32 = (crc32 >> 8) ^ crc32_table[(crc32 ^ *byte_buffer) & 0xFF];
		byte_buffer++;
	}

	return crc32;
}


static uint32_t __fastcall __compute_crc32_long_option_name(
	__in	const uint32_t *	crc32_table,
	__in	wchar16_t *		wch_arg,
	__in	wchar16_t *		wch_termination)
{
	uint32_t	crc32;
	unsigned char *	byte_buffer;

	/* crc32 hash... */
	crc32 = 0 ^ 0xFFFFFFFF;

	/* initialize byte_buffer */
	byte_buffer = (unsigned char *)wch_arg;

	/* iterate */
	while ((uintptr_t)byte_buffer < (uintptr_t)wch_termination) {
		/* two bytes at a time */
		crc32 = (crc32 >> 8) ^ crc32_table[(crc32 ^ *byte_buffer) & 0xFF];
		byte_buffer++;
		crc32 = (crc32 >> 8) ^ crc32_table[(crc32 ^ *byte_buffer) & 0xFF];
		byte_buffer++;
	}

	return crc32;
}


static void __fastcall __init_cmd_option_meta_utf16(
	__in	nt_cmd_option_meta_utf16 *	cmd_opt_meta)
{
	cmd_opt_meta->short_name	= (wchar16_t *)0;
	cmd_opt_meta->short_name_code	= 0;
	cmd_opt_meta->long_name		= (wchar16_t *)0;
	cmd_opt_meta->long_name_hash	= 0;
	cmd_opt_meta->value		= (wchar16_t *)0;
	cmd_opt_meta->value_hash	= 0;
	cmd_opt_meta->argv_index	= 0;
	cmd_opt_meta->flags		= 0;

	return;
}


int32_t __stdcall __ntapi_tt_get_short_option_meta_utf16(
	__in	const uint32_t *		crc32_table,
	__in	wchar16_t			option_name,
	__in	wchar16_t *			argv[],
	__out	nt_cmd_option_meta_utf16 *	cmd_opt_meta)
{
	int		idx;
	wchar16_t *	wch;

	if (!crc32_table)
		return NT_STATUS_INVALID_PARAMETER_1;
	else if (!option_name)
		return NT_STATUS_INVALID_PARAMETER_2;
	else if (!argv)
		return NT_STATUS_INVALID_PARAMETER_3;

	/* initialize cmd_opt_meta  */
	__init_cmd_option_meta_utf16(cmd_opt_meta);

	/* step 1: attempt to find the short option in argv[] */
	idx = 0;
	while (argv[idx] && (!cmd_opt_meta->short_name_code)) {
		wch = argv[idx];

		/* is this our option? */
		if ((*wch == HYPHEN)
			&& (*++wch == option_name)
			&& (*++wch == 0)) {

			/* found it, get ready to hash the value */
			cmd_opt_meta->short_name_code = option_name;
			cmd_opt_meta->short_name = argv[idx];
			cmd_opt_meta->argv_index = idx;
		} else {
			idx++;
		}
	}

	/* if the next argument is also an option (or is null), just exit */
	idx++;
	if ((!argv[idx]) || (*argv[idx] == HYPHEN))
		return NT_STATUS_SUCCESS;

	/* step 2: hash the value */
	cmd_opt_meta->value = argv[idx];
	cmd_opt_meta->value_hash =
		__compute_crc32_utf16_str(
			crc32_table,
			argv[idx]);

	return NT_STATUS_SUCCESS;
}


int32_t __stdcall __ntapi_tt_get_long_option_meta_utf16(
	__in	const uint32_t *		crc32_table,
	__in	wchar16_t *			option_name,
	__in	uint32_t			option_name_hash __optional,
	__in	wchar16_t *			argv[],
	__out	nt_cmd_option_meta_utf16 *	cmd_opt_meta)
{
	/**
	 *  option_name must always include the two-hyphen prefix;
	 *  and the option value must be preceded by an equal sign.
	 *
	 *  the only valid long option forms in argv[] are therefore:
	 *  --long-option
	 *  --long-option=value
	**/

	int		idx;
	uint32_t	crc32;
	wchar16_t *	wch;

	/* validation */
	if (!crc32_table)
		return NT_STATUS_INVALID_PARAMETER_1;
	else if ((!option_name) && (!option_name_hash))
		return NT_STATUS_INVALID_PARAMETER;
	else if ((option_name) && (option_name_hash))
		return NT_STATUS_INVALID_PARAMETER_MIX;
	else if (!argv)
		return NT_STATUS_INVALID_PARAMETER_4;

	/* initialize cmd_opt_meta  */
	__init_cmd_option_meta_utf16(cmd_opt_meta);

	/* step 1: crc32 of the target option_name */
	if (option_name_hash)
		crc32 = option_name_hash;
	else
		option_name_hash =
			__compute_crc32_utf16_str(
				crc32_table,
				option_name);

	/* step 2: attempt to find the long option in argv[] */
	idx = 0;
	while (argv[idx] && (!cmd_opt_meta->value))  {
		wch = argv[idx];

		if (__is_long_option(wch)) {
			/* find the equal sign or null termination */
			while ((*wch) && (*wch != EQUAL_SIGN))
				wch++;

			crc32 = __compute_crc32_long_option_name(
				crc32_table,
				argv[idx],
				wch);

			if (crc32 == option_name_hash) {
				/* found it, get ready to hash the value */
				cmd_opt_meta->long_name_hash = option_name_hash;
				cmd_opt_meta->long_name	 = argv[idx];
				cmd_opt_meta->argv_index = idx;

				if (*wch)
					/* skip the equal sign */
					wch++;

				cmd_opt_meta->value = wch;
			} else
				idx++;
		}
	}

	if (cmd_opt_meta->value)
		cmd_opt_meta->value_hash =
			__compute_crc32_utf16_str(
				crc32_table,
				cmd_opt_meta->value);

	return NT_STATUS_SUCCESS;
}


int32_t __stdcall __ntapi_tt_validate_program_options(
	__in	const uint32_t *		crc32_table,
	__in	wchar16_t *			argv[],
	__in	nt_program_option *		options[],
	__in	nt_program_options_meta *	options_meta)
{
	int				idx;
	int				idx_arg;
	int				idx_option;
	int				idx_max;
	uint32_t			crc32;
	nt_program_option *		option;
	wchar16_t *			parg;
	wchar16_t *			pvalue;

	/* validation */
	if (!crc32_table)
		return NT_STATUS_INVALID_PARAMETER_1;
	else if (!argv)
		return NT_STATUS_INVALID_PARAMETER_2;
	else if (!options)
		return NT_STATUS_INVALID_PARAMETER_3;
	else if (!options_meta)
		return NT_STATUS_INVALID_PARAMETER_4;


	/* step 1: validate options[] hash the long option names */
	idx		= 0;
	idx_option	= 0;
	option		= options[0];
	pvalue  	= (wchar16_t *)0;

	while (!__is_last_program_option(option)) {
		if (option->short_name_code) {
			if (!(__is_bmp_code_point(option->short_name_code))) {
				options_meta->idx_invalid_short_name = idx;
				return NT_STATUS_INVALID_PARAMETER;
			}
		}

		if (option->long_name) {
			if (!(__is_long_option(option->long_name))) {
				options_meta->idx_invalid_long_name = idx;
				return NT_STATUS_INVALID_PARAMETER;
			}

			/* update the long name hash (unconditionally) */
			option->long_name_hash =
				__compute_crc32_utf16_str(
					crc32_table,
					option->long_name);
		}

		idx++;
		option++;
	}

	/* book keeping */
	idx_max = idx;

	/* step 2: validate argv[] */
	parg	= argv[0];
	idx_arg	= 0;

	while ((parg) && (!(__is_last_option_argument(parg)))) {
		if (__is_short_option(parg)) {
			idx = 0;
			idx_option = 0;

			while ((idx < idx_max) && (!idx_option)) {
				option = options[idx];

				if (*(parg+1) == option->short_name_code)
					idx_option = idx;
				else
					idx++;
			}

			if (idx == idx_max) {
				options_meta->idx_invalid_argument = idx_arg;
				return NT_STATUS_INVALID_PARAMETER;
			} else {
				/* get ready for the next element (or value) */
				parg++;
				idx_arg++;
				pvalue = parg;
			}
		} else if (__is_long_option(parg)) {
			idx = 0;
			idx_option = 0;
			/* find the equal sign or null termination */
			pvalue = parg;
			while ((*pvalue) && (*pvalue != EQUAL_SIGN))
				pvalue++;

			while ((idx < idx_max) && (!idx_option)) {
				option = options[idx];
				crc32  = __compute_crc32_long_option_name(
						crc32_table,
						parg,
						pvalue);

				if (crc32 == option->long_name_hash)
					idx_option = idx;
				else
					idx++;
			}

			if (idx == idx_max) {
				options_meta->idx_invalid_argument = idx_arg;
				return NT_STATUS_INVALID_PARAMETER;
			} else {
				if (*pvalue != EQUAL_SIGN)
					/* skip the equal sign */
					pvalue++;
				pvalue = (wchar16_t *)0;
			}
		}

		/* validate the occurrence */
		if (idx_option) {
			if (option->flags && NT_OPTION_ALLOWED_ONCE) {
				if (option->option_count) {
					options_meta->idx_invalid_argument
						= idx_arg;
					return NT_STATUS_INVALID_PARAMETER;
				} else {
					option->option_count++;
				}
			}

			if (option->flags && NT_OPTION_VALUE_REQUIRED) {
				if ((!(*pvalue)) || (*pvalue == HYPHEN)) {
					options_meta->idx_missing_option_value
						= idx_arg;
					return NT_STATUS_INVALID_PARAMETER;
				} else {
					option->value 	   = pvalue;
					option->value_hash =
						__compute_crc32_utf16_str(
							crc32_table,
							option->value);
				}
			}
		}

		parg++;
		idx_arg++;
	}

	return NT_STATUS_SUCCESS;
}
