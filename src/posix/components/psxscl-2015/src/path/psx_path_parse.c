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

static void __fastcall __add_element(
	struct __path_info *	path_info,
	unsigned char *		element,
	int32_t			depth)
{
	path_info->elements[depth] = element;
	return;
}


static void __fastcall __add_element_utf16(
	struct __path_info *	path_info,
	wchar16_t *		element,
	int32_t			depth)
{
	path_info->elements_utf16[depth] = element;
	return;
}


int32_t __fastcall __psx_parse_normalized_path_utf8(
	struct __path_info *	path_info,
	int32_t			expected_depth)
{
	unsigned char * ch;

	if (expected_depth && (!path_info->arptrs))
		return NT_STATUS_BUFFER_TOO_SMALL;

	ch = (unsigned char *)path_info->inbuf;
	path_info->depth	 = -1;

	if (*ch=='/') {
		__add_element(
			path_info,
			ch,
			++(path_info->depth));

		if (*(++ch)=='/') {
			ch++;
		}
	}

	while ((*ch) && ((++(path_info->depth) < (path_info->arptrs - 1)))) {
		__add_element(
			path_info,
			ch,
			path_info->depth);

		do {} while (*(++ch) && (*ch!='/') && (*ch!='\\'));
	}

	if (*ch)
		return NT_STATUS_BUFFER_TOO_SMALL;

	__add_element(
		path_info,
		ch,
		++(path_info->depth));

	return NT_STATUS_SUCCESS;
}


int32_t __fastcall __psx_parse_native_path_utf16(
	struct __path_info *	path_info,
	int32_t			expected_depth)
{
	wchar16_t *	wch;

	if (expected_depth && (!path_info->arptrs))
		return NT_STATUS_BUFFER_TOO_SMALL;

	path_info->depth = -1;
	wch	         = (wchar16_t *)path_info->inbuf;

	if (*wch=='\\') {
		__add_element_utf16(
			path_info,
			wch,
			++(path_info->depth));

		if ((*(++wch)=='?') && (*(++wch)=='?') && (*(++wch)=='\\')) {
			wch++;
		} else {
			wch = (wchar16_t *)path_info->inbuf;
			wch++;
		}
	}

	while ((*wch) && ((++(path_info->depth) < (path_info->arptrs - 1)))) {
		__add_element_utf16(
			path_info,
			wch,
			path_info->depth);

		do {} while (*(++wch) && (*wch!='\\'));
	}

	if (*wch)
		return NT_STATUS_BUFFER_TOO_SMALL;

	__add_element_utf16(
		path_info,
		wch,
		++(path_info->depth));

	return NT_STATUS_SUCCESS;
}
