/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_port.h>
#include <ntapi/nt_tty.h>
#include <ntapi/nt_vmount.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"


static void __vms_port_name_from_server_info(
	__out	nt_port_name *		vms_port_name,
	__in	nt_tty_vms_info *	vmsinfo)
{
	nt_port_attr	port_attr;

	port_attr.type		= NT_PORT_TYPE_VMOUNT;
	port_attr.subtype	= NT_PORT_SUBTYPE_DEFAULT;

	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)&port_attr.keys,
		(uintptr_t *)&vmsinfo->vms_keys,
		sizeof(nt_port_keys));

	__ntapi->tt_port_guid_from_type(
		&port_attr.guid,
		port_attr.type,
		port_attr.subtype);

	__ntapi->tt_port_name_from_attributes(
		vms_port_name,
		&port_attr);
}


int32_t __stdcall __ntapi_vms_client_connect(
	__out	void **			hvms,
	__in	nt_tty_vms_info *	vmsinfo)
{
	int32_t			status;
	nt_port_name		vms_port_name;

	nt_unicode_string	name;
	nt_sqos			sqos;
	nt_oa			oa;

	/* vmount daemon port name */
	__vms_port_name_from_server_info(
		&vms_port_name,
		vmsinfo);

	/* port name init */
	name.buffer = (wchar16_t *)&vms_port_name;
	name.maxlen = 0;
	name.strlen = (uint16_t)(size_t)(&((nt_port_name *)0)->null_termination);

	/* init security structure */
	sqos.length 			= sizeof(sqos);
	sqos.impersonation_level	= NT_SECURITY_IMPERSONATION;
	sqos.context_tracking_mode	= NT_SECURITY_TRACKING_DYNAMIC;
	sqos.effective_only		= 1;

	/* init the port's object attributes */
	oa.len		= sizeof(oa);
	oa.root_dir	= (void *)0;
	oa.obj_name	= &name;
	oa.obj_attr	= 0;
	oa.sec_desc	= (nt_security_descriptor *)0;
	oa.sec_qos	= &sqos;

	status = __ntapi->zw_connect_port(
		hvms,
		&name,
		&sqos,
		(nt_port_section_write *)0,
		(nt_port_section_read *)0,
		(uint32_t *)0,
		(void *)0,
		(uint32_t *)0);

	return status;
}
