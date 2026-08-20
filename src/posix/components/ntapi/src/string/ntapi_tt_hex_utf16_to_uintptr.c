/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_status.h>

int32_t __fastcall __ntapi_tt_hex_utf16_to_uint32(
	__in	wchar16_t	hex_key_utf16[8],
	__out	uint32_t *	key)
{
	int		i;
	unsigned char	uch[8];
	unsigned char	ubytes[4];
	uint32_t *	key_ret;

	/* input validation */
	i = 0;
	do {
		if (/* [a-f],[[A-F],[0-9] */
			((hex_key_utf16[i] >= 'a') && (hex_key_utf16[i] <= 'f'))
			|| ((hex_key_utf16[i] >= 'A') && (hex_key_utf16[i] <= 'F'))
			|| ((hex_key_utf16[i] >= '0') && (hex_key_utf16[i] <= '9')))
			/* valid hex character */
			i++;
		else
			return NT_STATUS_ILLEGAL_CHARACTER;
	} while (i < 8);

	/* intermediate step: little endian byte order */
	uch[0] = (unsigned char)hex_key_utf16[6];
	uch[1] = (unsigned char)hex_key_utf16[7];
	uch[2] = (unsigned char)hex_key_utf16[4];
	uch[3] = (unsigned char)hex_key_utf16[5];
	uch[4] = (unsigned char)hex_key_utf16[2];
	uch[5] = (unsigned char)hex_key_utf16[3];
	uch[6] = (unsigned char)hex_key_utf16[0];
	uch[7] = (unsigned char)hex_key_utf16[1];

	for (i=0; i<8; i++) {
		/* 'a' > 'A' > '0' */
		if (uch[i] >= 'a')
			uch[i] -= ('a' - 0x0a);
		else if (uch[i] >= 'A')
			uch[i] -= ('A' - 0x0a);
		else
			uch[i] -= '0';
	}

	ubytes[0] = uch[0] * 0x10 + uch[1];
	ubytes[1] = uch[2] * 0x10 + uch[3];
	ubytes[2] = uch[4] * 0x10 + uch[5];
	ubytes[3] = uch[6] * 0x10 + uch[7];

	key_ret = (uint32_t *)ubytes;
	*key = *key_ret;

	return NT_STATUS_SUCCESS;
}


int32_t __fastcall __ntapi_tt_hex_utf16_to_uint64(
	__in	wchar16_t	hex_key_utf16[16],
	__out	uint64_t *	key)
{
	int32_t		status;
	uint32_t	x64_key[2];
	uint64_t *	key_ret;

	status = __ntapi_tt_hex_utf16_to_uint32(
			&hex_key_utf16[0],
			&x64_key[1]);

	if (status != NT_STATUS_SUCCESS)
		return status;

	status = __ntapi_tt_hex_utf16_to_uint32(
			&hex_key_utf16[8],
			&x64_key[0]);

	if (status != NT_STATUS_SUCCESS)
		return status;

	key_ret = (uint64_t *)x64_key;
	*key = *key_ret;

	return NT_STATUS_SUCCESS;
}


int32_t __fastcall __ntapi_tt_hex_utf16_to_uintptr(
	__in	wchar16_t	hex_key_utf16[],
	__out	uintptr_t *	key)
{
	#if defined (__NT32)
		return __ntapi_tt_hex_utf16_to_uint32(hex_key_utf16,key);
	#elif defined (__NT64)
		return __ntapi_tt_hex_utf16_to_uint64(hex_key_utf16,key);
	#endif
}


int32_t __fastcall __ntapi_tt_hex_utf16_to_uint16(
	__in	wchar16_t	hex_key_utf16[4],
	__out	uint16_t *	key)
{
	int32_t		ret;
	uint32_t	dword_key;
	wchar16_t	hex_buf[8] = {'0','0','0','0'};

	hex_buf[4] = hex_key_utf16[0];
	hex_buf[5] = hex_key_utf16[1];
	hex_buf[6] = hex_key_utf16[2];
	hex_buf[7] = hex_key_utf16[3];

	ret = __ntapi_tt_hex_utf16_to_uint32(hex_buf,&dword_key);

	if (ret == NT_STATUS_SUCCESS)
		*key = (uint16_t)dword_key;

	return ret;
}
