#ifndef _NT_VFD_H_
#define _NT_VFD_H_

#include <psxtypes/psxtypes.h>
#include <dalist/dalist.h>
#include "nt_object.h"
#include "nt_guid.h"

typedef struct _nt_vfd_dev_name {
	nt_unicode_string	name;
	wchar16_t		prefix[8];
	nt_guid_str_utf16	guid;
} nt_vfd_dev_name;


typedef void __stdcall ntapi_vfd_dev_name_init(
	__out	nt_vfd_dev_name *	devname,
	__in	const nt_guid *		guid);

#endif
