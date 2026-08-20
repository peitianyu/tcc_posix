/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_time.h>
#include <ntapi/nt_guid.h>
#include "ntapi_impl.h"


void __fastcall __ntapi_tt_guid_copy(
	__out	nt_guid *	pguid_dst,
	__in	const nt_guid *	pguid_src)
{
	uint64_t *	dst;
	uint64_t *	src;

	dst = (uint64_t *)pguid_dst;
	src = (uint64_t *)pguid_src;

	*dst = *src;
	src++;	dst++;
	*dst = *src;
}


void __fastcall __ntapi_tt_guid_to_utf16_string(
	__in	const nt_guid *		guid,
	__out	nt_guid_str_utf16 *	guid_str)
{
	uint16_t	key;
	wchar16_t *	wch;

	wch = &(guid_str->group5[0]);

	__ntapi_tt_uint32_to_hex_utf16(
		guid->data1,
		&guid_str->group1[0]);

	__ntapi_tt_uint16_to_hex_utf16(
		guid->data2,
		&guid_str->group2[0]);

	__ntapi_tt_uint16_to_hex_utf16(
		guid->data3,
		&guid_str->group3[0]);

	key = guid->data4[0] * 0x100 + guid->data4[1];

	__ntapi_tt_uint16_to_hex_utf16(
		key,
		&guid_str->group4[0]);

	key = guid->data4[2] * 0x100 + guid->data4[3];

	__ntapi_tt_uint16_to_hex_utf16(
		key,
		&guid_str->group5[0]);

	key = guid->data4[4] * 0x100 + guid->data4[5];

	__ntapi_tt_uint16_to_hex_utf16(
		key,
		&(wch[4]));

	key = guid->data4[6] * 0x100 + guid->data4[7];

	__ntapi_tt_uint16_to_hex_utf16(
		key,
		&(wch[8]));

	guid_str->lbrace = '{';
	guid_str->rbrace = '}';
	guid_str->dash1 = '-';
	guid_str->dash2 = '-';
	guid_str->dash3 = '-';
	guid_str->dash4 = '-';

	return;
}


int32_t __fastcall __ntapi_tt_guid_compare(
	__in	const nt_guid *	pguid_dst,
	__in	const nt_guid *	pguid_src)
{
	uint64_t *	dst;
	uint64_t *	src;

	dst = (uint64_t *)pguid_dst;
	src = (uint64_t *)pguid_src;

	if ((*dst != *src) || (*(++dst) != *(++src)))
		return NT_STATUS_OBJECT_TYPE_MISMATCH;

	return NT_STATUS_SUCCESS;
}


int32_t __fastcall __ntapi_tt_utf16_string_to_guid(
	__out	nt_guid_str_utf16 *	guid_str,
	__in	nt_guid *		guid)
{
	int32_t		status;
	wchar16_t *	wch;
	uint16_t	key;

	if ((guid_str->lbrace != '{')
			|| (guid_str->rbrace != '}')
			|| (guid_str->dash1 != '-')
			|| (guid_str->dash2 != '-')
			|| (guid_str->dash3 != '-')
			|| (guid_str->dash4 != '-'))
			return NT_STATUS_INVALID_PARAMETER;

	wch = &(guid_str->group5[0]);

	status = __ntapi_tt_hex_utf16_to_uint32(
		guid_str->group1,
		&guid->data1);

	if (status != NT_STATUS_SUCCESS)
		return status;

	status = __ntapi_tt_hex_utf16_to_uint16(
		guid_str->group2,
		&guid->data2);

	if (status != NT_STATUS_SUCCESS)
		return status;

	status = __ntapi_tt_hex_utf16_to_uint16(
		guid_str->group3,
		&guid->data3);

	if (status != NT_STATUS_SUCCESS)
		return status;

	status = __ntapi_tt_hex_utf16_to_uint16(
		guid_str->group4,
		&key);

	if (status != NT_STATUS_SUCCESS)
		return status;

	guid->data4[0] = key / 0x100;
	guid->data4[1] = key % 0x100;

	status = __ntapi_tt_hex_utf16_to_uint16(
		&(wch[0]),
		&key);

	if (status != NT_STATUS_SUCCESS)
		return status;

	guid->data4[2] = key / 0x100;
	guid->data4[3] = key % 0x100;

	status = __ntapi_tt_hex_utf16_to_uint16(
		&(wch[4]),
		&key);

	if (status != NT_STATUS_SUCCESS)
		return status;

	guid->data4[4] = key / 0x100;
	guid->data4[5] = key % 0x100;

	status = __ntapi_tt_hex_utf16_to_uint16(
		&(wch[8]),
		&key);

	if (status != NT_STATUS_SUCCESS)
		return status;

	guid->data4[6] = key / 0x100;
	guid->data4[7] = key % 0x100;

	return NT_STATUS_SUCCESS;
}
