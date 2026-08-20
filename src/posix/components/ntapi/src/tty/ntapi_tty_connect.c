/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_object.h>
#include <ntapi/nt_port.h>
#include <ntapi/nt_string.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

int32_t __stdcall __ntapi_tty_connect(
	__out	void **			hport,
	__in	wchar16_t *		tty_port_name,
	__in	int32_t			impersonation_level)
{
	nt_object_attributes		oa;
	nt_unicode_string		name;
	nt_security_quality_of_service	sqos;

	__ntapi->tt_init_unicode_string_from_utf16(
		&name,tty_port_name);

	sqos.length 			= sizeof(sqos);
	sqos.impersonation_level	= impersonation_level;
	sqos.context_tracking_mode	= NT_SECURITY_TRACKING_DYNAMIC;
	sqos.effective_only		= 1;

	oa.len		= sizeof(oa);
	oa.root_dir	= (void *)0;
	oa.obj_name	= &name;
	oa.obj_attr	= 0;
	oa.sec_desc	= (nt_security_descriptor *)0;
	oa.sec_qos	= &sqos;

	return __ntapi->zw_connect_port(
		hport,
		&name,
		&sqos,
		(nt_port_section_write *)0,
		(nt_port_section_read *)0,
		(uint32_t *)0,
		(void *)0,
		(uint32_t *)0);
}
