/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_tlca.h"

int32_t __psx_strconv_utf16_to_utf8(
	struct __psx_tlca *	tlca,
	const wchar16_t *	wch,
	char *			ch,
	size_t			strlen,
	size_t			bufsize,
	char **			chnext)
{
	int32_t status;
	nt_strconv_nativetomb params = {0};
	chnext = chnext ? chnext : &ch;

	params.src		 = wch;
	params.src_size_in_bytes = strlen;
	params.dst		 = (unsigned char *)ch;
	params.dst_size_in_bytes = bufsize - 1;

	/* stateless */
	if ((status = __ntapi->uc_convert_unicode_stream_utf16_to_utf8(&params)))
		return status;
	else if (params.leftover_count)
		return NT_STATUS_ILLEGAL_CHARACTER;

	/* null termination */
	*chnext = ch + params.bytes_written;
	**chnext = 0;

	return NT_STATUS_SUCCESS;
}

int32_t __psx_strconv_utf8_to_utf16(
	struct __psx_tlca *	tlca,
	const char *		ch,
	wchar16_t *		wch,
	size_t			strlen,
	size_t			bufsize,
	wchar16_t **		wchnext)
{
	int32_t status;
	nt_strconv_mbtonative params = {0};
	wchnext = wchnext ? wchnext : &wch;

	params.src		 = (const unsigned char *)ch;
	params.src_size_in_bytes = strlen;
	params.dst		 = wch;
	params.dst_size_in_bytes = bufsize - sizeof(wchar16_t);

	/* stateless */
	if ((status = __ntapi->uc_convert_unicode_stream_utf8_to_utf16(&params)))
		return status;
	else if (params.leftover_count)
		return NT_STATUS_ILLEGAL_CHARACTER;

	/* null termination */
	*wchnext = wch + (params.bytes_written/sizeof(wchar16_t));
	**wchnext = 0;

	return NT_STATUS_SUCCESS;
}
