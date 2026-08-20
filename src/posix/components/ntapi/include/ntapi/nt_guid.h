#ifndef _NT_GUID_H_
#define _NT_GUID_H_

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_object.h>

typedef struct _nt_guid_str_utf16 {
	wchar16_t	lbrace;
	wchar16_t	group1[8];
	wchar16_t	dash1;
	wchar16_t	group2[4];
	wchar16_t	dash2;
	wchar16_t	group3[4];
	wchar16_t	dash3;
	wchar16_t	group4[4];
	wchar16_t	dash4;
	wchar16_t	group5[12];
	wchar16_t	rbrace;
} nt_guid_str_utf16, nt_uuid_str_utf16;

typedef void __fastcall ntapi_tt_guid_copy(
	__out	nt_guid *	pguid_dst,
	__in	const nt_guid *	pguid_src);

typedef int32_t __fastcall ntapi_tt_guid_compare(
	__in	const nt_guid *	pguid_dst,
	__in	const nt_guid *	pguid_src);

typedef void __fastcall ntapi_tt_guid_to_utf16_string(
	__in	const nt_guid *		guid,
	__out	nt_guid_str_utf16 *	guid_str);

typedef int32_t __fastcall ntapi_tt_utf16_string_to_guid(
	__in	nt_guid_str_utf16 *	guid_str,
	__out	nt_guid *		guid);

#endif
