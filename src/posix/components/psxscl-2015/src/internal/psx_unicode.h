#ifndef _PSX_UNICODE_H_
#define _PSX_UNICODE_H_

#include "psx_systypes.h"
#include "psx_tlca.h"

int32_t __psx_strconv_utf16_to_utf8(
	struct __psx_tlca *	tlca,
	const wchar16_t *	wch,
	char *			ch,
	size_t			strlen,
	size_t			bufsize,
	char **			chnext);

int32_t __psx_strconv_utf8_to_utf16(
	struct __psx_tlca *	tlca,
	const char *		ch,
	wchar16_t *		wch,
	size_t			strlen,
	size_t			bufsize,
	wchar16_t **		wchnext);

#endif
