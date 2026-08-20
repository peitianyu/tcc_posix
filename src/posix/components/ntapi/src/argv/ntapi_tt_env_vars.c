/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

int32_t __stdcall __ntapi_tt_get_env_var_meta_utf16(
	__in	const uint32_t *	crc32_table,
	__in	wchar16_t *		env_var_name,
	__in	uint32_t		env_var_name_hash __optional,
	__in	wchar16_t **		envp,
	__out	nt_env_var_meta_utf16 *	env_var_meta)
{
	int		idx;
	uint32_t	crc32;
	unsigned char *	byte_buffer;
	wchar16_t *	wch;

	#define EQUAL_SIGN	0x3D

	/* step 1: crc32 of the target env_var_name */
	if (env_var_name_hash)
		crc32 = env_var_name_hash;
	else {
		crc32 = 0 ^ 0xFFFFFFFF;

		/* initialize byte_buffer */
		byte_buffer = (unsigned char *)env_var_name;

		/* iterate */
		while (*byte_buffer) {
			/* two bytes at a time */
			crc32 = (crc32 >> 8) ^ crc32_table[(crc32 ^ *byte_buffer) & 0xFF];
			byte_buffer++;
			crc32 = (crc32 >> 8) ^ crc32_table[(crc32 ^ *byte_buffer) & 0xFF];
			byte_buffer++;
		}
		crc32 = (crc32 ^ 0xFFFFFFFF);
	}

	/* initialize the env_var_meta structure */
	env_var_meta->name_hash		= crc32;
	env_var_meta->name		= (wchar16_t *)0;
	env_var_meta->value		= (wchar16_t *)0;
	env_var_meta->value_hash	= 0;
	env_var_meta->envp_index	= 0;
	env_var_meta->flags		= 0;

	/* step 2: look for the environment variable in envp[] */
	idx = 0;
	while (envp[idx] && (!env_var_meta->value)) {
		wch = envp[idx];

		/* find the equal sign */
		while ((*wch) && (*wch != EQUAL_SIGN))
			wch++;

		if (*wch != EQUAL_SIGN)
			return NT_STATUS_ILLEGAL_CHARACTER;

		/* hash the current environment variable */
		crc32 = 0 ^ 0xFFFFFFFF;

		/* initialize byte_buffer */
		byte_buffer = (unsigned char *)envp[idx];

		/* iterate */
		while ((uintptr_t)(byte_buffer) < (uintptr_t)wch) {
			/* two bytes at a time */
			crc32 = (crc32 >> 8) ^ crc32_table[(crc32 ^ *byte_buffer) & 0xFF];
			byte_buffer++;
			crc32 = (crc32 >> 8) ^ crc32_table[(crc32 ^ *byte_buffer) & 0xFF];
			byte_buffer++;
		}

		if (env_var_meta->name_hash == (crc32 ^ 0xFFFFFFFF)) {
			/* found it, get ready to hash the value */
			wch++;
			env_var_meta->name	 = envp[idx];
			env_var_meta->value	 = wch;
			env_var_meta->envp_index = idx;
		} else {
			idx++;
		}
	}

	if (env_var_meta->value) {
		/* hash the value: utf-16, null-terminated */
		crc32 = 0 ^ 0xFFFFFFFF;

		/* initialize byte_buffer */
		byte_buffer = (unsigned char *)env_var_meta->value;

		/* iterate */
		while (*byte_buffer) {
			/* two bytes at a time */
			crc32 = (crc32 >> 8) ^ crc32_table[(crc32 ^ *byte_buffer) & 0xFF];
			byte_buffer++;
			crc32 = (crc32 >> 8) ^ crc32_table[(crc32 ^ *byte_buffer) & 0xFF];
			byte_buffer++;
		}

		env_var_meta->value_hash = (crc32 ^ 0xFFFFFFFF);
	}

	return NT_STATUS_SUCCESS;
}

