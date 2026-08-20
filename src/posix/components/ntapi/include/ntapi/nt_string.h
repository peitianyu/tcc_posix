#ifndef _NT_STRING_H_
#define _NT_STRING_H_

#include <psxtypes/psxtypes.h>
#include "nt_object.h"

typedef void * __cdecl ntapi_memset(
	void *dest,
	int c,
	size_t count);


typedef int __cdecl ntapi_sprintf(
	char * buffer,
	const char * format,
	...);


typedef size_t __cdecl ntapi_strlen(const char * str);


typedef size_t __cdecl ntapi_wcslen(const wchar16_t * str);


typedef void ntapi_rtl_init_unicode_string(
    __out 	nt_unicode_string *	str_dest,
    __in	wchar16_t *		str_src);


/* yes, there exists a reason (but feel free to scold me nonetheless) */
typedef size_t __cdecl ntapi_tt_string_null_offset_multibyte(
	__in	const char *	str);

typedef size_t __cdecl ntapi_tt_string_null_offset_short(
	__in	const int16_t *	str);

typedef size_t __cdecl ntapi_tt_string_null_offset_dword(
	__in	const int32_t *	str);

typedef size_t __cdecl ntapi_tt_string_null_offset_qword(
	__in	const int64_t *	str);

typedef size_t __cdecl ntapi_tt_string_null_offset_ptrsize(
	__in	const intptr_t *str);

typedef void __cdecl		ntapi_tt_init_unicode_string_from_utf16(
	__out 	nt_unicode_string *	str_dest,
	__in	wchar16_t *		str_src);


typedef void * __cdecl		ntapi_tt_aligned_block_memset(
	__in	void *			block,
	__in	uintptr_t		val,
	__in	size_t			bytes);

typedef uintptr_t * __cdecl	ntapi_tt_aligned_block_memcpy(
	__in	uintptr_t *		dst,
	__in	const uintptr_t *	src,
	__in	size_t			bytes);


typedef wchar16_t * __cdecl	ntapi_tt_memcpy_utf16(
	__in	wchar16_t *		dst,
	__in	const wchar16_t *	src,
	__in	size_t			bytes);


typedef wchar16_t * __cdecl	ntapi_tt_aligned_memcpy_utf16(
	__in	uintptr_t *		dst,
	__in	const uintptr_t *	src,
	__in	size_t			bytes);


typedef void * __cdecl		ntapi_tt_generic_memset(
	__in	void *			dst,
	__in	uintptr_t		val,
	__in	size_t			bytes);

typedef void * __cdecl	ntapi_tt_generic_memcpy(
	__in	void *			dst,
	__in	const void *		src,
	__in	size_t			bytes);

typedef void __fastcall	ntapi_tt_uint16_to_hex_utf16(
	__in	uint16_t	key,
	__out	wchar16_t *	formatted_key);


typedef void __fastcall	ntapi_tt_uint32_to_hex_utf16(
	__in	uint32_t	key,
	__out	wchar16_t *	formatted_key);


typedef void __fastcall	ntapi_tt_uint64_to_hex_utf16(
	__in	uint64_t	key,
	__out	wchar16_t *	formatted_key);


typedef void __fastcall	ntapi_tt_uintptr_to_hex_utf16(
	__in	uintptr_t	key,
	__out	wchar16_t *	formatted_key);


typedef int32_t __fastcall ntapi_tt_hex_utf16_to_uint16(
	__in	wchar16_t	hex_key_utf16[4],
	__out	uint16_t *	key);


typedef int32_t __fastcall ntapi_tt_hex_utf16_to_uint32(
	__in	wchar16_t	hex_key_utf16[8],
	__out	uint32_t *	key);


typedef int32_t __fastcall ntapi_tt_hex_utf16_to_uint64(
	__in	wchar16_t	hex_key_utf16[16],
	__out	uint64_t *	key);


typedef int32_t __fastcall ntapi_tt_hex_utf16_to_uintptr(
	__in	wchar16_t	hex_key_utf16[],
	__out	uintptr_t *	key);


typedef void __fastcall ntapi_tt_uint16_to_hex_utf8(
	__in	uint32_t	key,
	__out	unsigned char *	buffer);


typedef void __fastcall ntapi_tt_uint32_to_hex_utf8(
	__in	uint32_t	key,
	__out	unsigned char *	buffer);


typedef void __fastcall ntapi_tt_uint64_to_hex_utf8(
	__in	uint64_t	key,
	__out	unsigned char *	buffer);


typedef void __fastcall ntapi_tt_uintptr_to_hex_utf8(
	__in	uintptr_t	key,
	__out	unsigned char *	buffer);

#endif
