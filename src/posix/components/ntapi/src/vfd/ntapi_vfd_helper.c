/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_status.h>
#include <ntapi/nt_object.h>
#include <ntapi/nt_vfd.h>
#include "ntapi_impl.h"

void __stdcall __ntapi_vfd_dev_name_init(
	__out	nt_vfd_dev_name *	devname,
	__in	const nt_guid *		guid)
{
	uint32_t * prefix = (uint32_t *)devname->prefix;

	/* compiler-independent */
	prefix[0] = 0x44005C;
	prefix[1] = 0x760065;
	prefix[2] = 0x630069;
	prefix[3] = 0x5C0065;

	__ntapi->tt_guid_to_utf16_string(
		guid,
		&devname->guid);

	devname->name.strlen = sizeof(devname->prefix) + sizeof(devname->guid);
	devname->name.maxlen = 0;
	devname->name.buffer = (uint16_t *)&devname->prefix;

	return;
}
