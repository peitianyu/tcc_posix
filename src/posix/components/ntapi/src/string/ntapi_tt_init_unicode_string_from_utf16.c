/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include <ntapi/nt_object.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

void __ntapi_tt_init_unicode_string_from_utf16(
    __out 	nt_unicode_string *	str_dest,
    __in	wchar16_t *		str_src)
{
	if ((intptr_t)str_src) {
		str_dest->strlen = (uint16_t)__ntapi->tt_string_null_offset_short((const int16_t *)str_src);
		str_dest->maxlen = str_dest->strlen + sizeof(uint16_t);
		str_dest->buffer = (uint16_t *)str_src;
	} else {
		str_dest->strlen = 0;
		str_dest->maxlen = 0;
		str_dest->buffer = (uint16_t *)0;
	}
}