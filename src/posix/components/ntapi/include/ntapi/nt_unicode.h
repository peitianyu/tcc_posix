#ifndef _NT_UNICODE_H_
#define _NT_UNICODE_H_

/**
 *  the conversion functions are based on the validation functions;
 *  each of the conversion functions passes its peer validation
 *  function an array of callback handlers, with each member of
 *  the array corresponding to the number of units making the
 *  current code point in utf-8, be that source or destination.
 *  following this logic, then, callback_fn[1] handles 1-byte
 *  code points [00..7F], callback_fn[2] handles 2-byte code
 *  points [C2..DF,80..BF], and so on.  the first member of
 *  the array, callback_fn[0], is invoked once the null
 *  termination of the source stream has been reached.
**/

typedef struct _nt_utf8_callback_args {
	const unsigned char *	src;
	void *			dst;
	void *			dst_cap;
	uint32_t		byte_count;
	uint32_t		code_point;
	size_t			bytes_written;
} nt_utf8_callback_args;


typedef struct _nt_utf16_callback_args {
	const wchar16_t *	src;
	void *			dst;
	void *			dst_cap;
	uint32_t		byte_count;
	uint32_t		code_point;
	size_t			bytes_written;
} nt_utf16_callback_args;


typedef struct _nt_unicode_conversion_params_utf8_to_utf16 {
	const unsigned char *	src;
	size_t			src_size_in_bytes;
	wchar16_t *		dst;
	size_t			dst_size_in_bytes;
	size_t			code_points;
	size_t			bytes_written;
	void *			addr_failed;
	uintptr_t		leftover_count;
	uintptr_t		leftover_bytes;
} nt_unicode_conversion_params_utf8_to_utf16, nt_strconv_mbtonative;


typedef struct _nt_unicode_conversion_params_utf8_to_utf32 {
	const unsigned char *	src;
	size_t			src_size_in_bytes;
	wchar32_t *		dst;
	size_t			dst_size_in_bytes;
	size_t			code_points;
	size_t			bytes_written;
	void *			addr_failed;
	uintptr_t		leftover_count;
	uintptr_t		leftover_bytes;
} nt_unicode_conversion_params_utf8_to_utf32, nt_strconv_mbtowide;


typedef struct _nt_unicode_conversion_params_utf16_to_utf8 {
	const wchar16_t *	src;
	size_t			src_size_in_bytes;
	unsigned char *		dst;
	size_t			dst_size_in_bytes;
	size_t			code_points;
	size_t			bytes_written;
	void *			addr_failed;
	uintptr_t		leftover_count;
	uintptr_t		leftover_bytes;
} nt_unicode_conversion_params_utf16_to_utf8, nt_strconv_nativetomb;


typedef struct _nt_unicode_conversion_params_utf16_to_utf32 {
	const wchar16_t *	src;
	size_t			src_size_in_bytes;
	wchar32_t *		dst;
	size_t			dst_size_in_bytes;
	size_t			code_points;
	size_t			bytes_written;
	void *			addr_failed;
	uintptr_t		leftover_count;
	uintptr_t		leftover_bytes;
} nt_unicode_conversion_params_utf16_to_utf32, nt_strconv_nativetowide;

__assert_aligned_size(nt_utf8_callback_args,__SIZEOF_POINTER__);
__assert_aligned_size(nt_utf16_callback_args,__SIZEOF_POINTER__);
__assert_aligned_size(nt_unicode_conversion_params_utf8_to_utf16,__SIZEOF_POINTER__);
__assert_aligned_size(nt_unicode_conversion_params_utf8_to_utf32,__SIZEOF_POINTER__);
__assert_aligned_size(nt_unicode_conversion_params_utf16_to_utf8,__SIZEOF_POINTER__);
__assert_aligned_size(nt_unicode_conversion_params_utf16_to_utf32,__SIZEOF_POINTER__);


typedef int32_t __fastcall	ntapi_uc_utf8_callback_fn(
	__in	nt_utf8_callback_args *		callback_args);


typedef int32_t __fastcall	ntapi_uc_utf16_callback_fn(
	__in	nt_utf16_callback_args *	callback_args);


typedef int32_t __stdcall 	ntapi_uc_validate_unicode_stream_utf8(
	__in	const unsigned char *		ch,
	__in	size_t				size_in_bytes	__optional,
	__out	size_t *			code_points	__optional,
	__out	void **				addr_failed	__optional,
	__in	ntapi_uc_utf8_callback_fn **	callback_fn	__optional,
	__in	nt_utf8_callback_args *		callback_args	__optional);


typedef int32_t __stdcall 	ntapi_uc_validate_unicode_stream_utf16(
	__in	const wchar16_t *		wch,
	__in	size_t				size_in_bytes	__optional,
	__out	size_t *			code_points	__optional,
	__out	void **				addr_failed	__optional,
	__in	ntapi_uc_utf16_callback_fn **	callback_fn	__optional,
	__in	nt_utf16_callback_args *	callback_args	__optional);


typedef int __stdcall 		ntapi_uc_get_code_point_byte_count_utf8(
	__in	uint32_t	code_point);


typedef int __stdcall		ntapi_uc_get_code_point_byte_count_utf16(
	__in	uint32_t	code_point);


typedef int32_t __stdcall 	ntapi_uc_convert_unicode_stream_utf8_to_utf16(
	__in_out	nt_unicode_conversion_params_utf8_to_utf16 *	params);


typedef int32_t __stdcall 	ntapi_uc_convert_unicode_stream_utf8_to_utf32(
	__in_out	nt_unicode_conversion_params_utf8_to_utf32 *	params);


typedef int32_t __stdcall 	ntapi_uc_convert_unicode_stream_utf16_to_utf8(
	__in_out	nt_unicode_conversion_params_utf16_to_utf8 *	params);


typedef int32_t __stdcall 	ntapi_uc_convert_unicode_stream_utf16_to_utf32(
	__in_out	nt_unicode_conversion_params_utf16_to_utf32 *	params);


#endif
