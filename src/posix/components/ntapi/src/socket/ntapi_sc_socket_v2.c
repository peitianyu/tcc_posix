/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_object.h>
#include <ntapi/nt_file.h>
#include <ntapi/nt_socket.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

typedef struct _nt_socket_attr {
	uint32_t	datagram;
	uint32_t	unknown;
	uint32_t	domain;
	uint32_t	type;
	uint32_t	protocol;
} nt_socket_attr;

typedef struct _nt_afd_socket_ea {
	uint32_t	next_entry_offset;
	unsigned char	ea_flags;
	unsigned char	ea_name_length;
	uint16_t	ea_value_length;
	char		afd_open_packet[16];
	nt_socket_attr	sattr;
	uint32_t	ea_ext[4];
} nt_afd_socket_ea;

int32_t __cdecl __ntapi_sc_socket_v2(
	__out	nt_socket *		hssocket,
	__in	uint16_t		domain,
	__in	uint16_t		type,
	__in	uint32_t		protocol,
	__in	uint32_t		desired_access	__optional,
	__in	nt_sqos *		sqos		__optional,
	__out	nt_io_status_block *	iosb		__optional)
{
	int32_t			status;
	nt_object_attributes	oa;
	nt_io_status_block	siosb;
	nt_sqos			ssqos;
	nt_unicode_string	nt_afdep;
	uint32_t		ea_length;
	void *			_hsocket;

	wchar16_t afd_end_point[] = {
		'\\','D','e','v','i','c','e',
		'\\','A','f','d',
		'\\','E','n','d','P','o','i','n','t',
		0};

	nt_afd_socket_ea afd_ea = {
		0,
		0,
		0x0f,
		0x20,
		{'A','f','d','O','p','e','n','P','a','c','k','e','t','X','X',0},
		{0},
		{0}};

	ea_length = sizeof(afd_ea);

	afd_ea.sattr.domain	= domain;
	afd_ea.sattr.type	= type;
	afd_ea.sattr.protocol	= protocol;

	afd_ea.sattr.datagram	= (type == NT_SOCK_DGRAM) ? protocol : 0;

	__ntapi->rtl_init_unicode_string(&nt_afdep,afd_end_point);

	if (!desired_access)
		desired_access = NT_GENERIC_READ \
				| NT_GENERIC_WRITE \
				| NT_SEC_SYNCHRONIZE \
				| NT_SEC_WRITE_DAC;

	if (!sqos) {
		ssqos.length 			= sizeof(ssqos);
		ssqos.impersonation_level	= NT_SECURITY_IMPERSONATION;
		ssqos.context_tracking_mode	= NT_SECURITY_TRACKING_DYNAMIC;
		ssqos.effective_only		= 1;
		sqos 				= &ssqos;
	}

	oa.len		= sizeof(oa);
	oa.root_dir	= (void *)0;
	oa.obj_name	= &nt_afdep;
	oa.obj_attr	= NT_OBJ_CASE_INSENSITIVE | NT_OBJ_INHERIT;
	oa.sec_desc	= (nt_security_descriptor *)0;
	oa.sec_qos	= sqos;

	iosb = iosb ? iosb : &siosb;

	if ((status = __ntapi->zw_create_file(
			&_hsocket,
			desired_access,
			&oa,
			iosb,
			0,
			0,
			NT_FILE_SHARE_READ | NT_FILE_SHARE_WRITE,
			NT_FILE_OPEN_IF,
			0,
			&afd_ea,
			ea_length)))
		return status;

	oa.obj_name = 0;
	oa.obj_attr = 0;

	hssocket->hsocket	= _hsocket;
	hssocket->ntflags	= 0;
	hssocket->domain 	= domain;
	hssocket->type		= type;
	hssocket->protocol	= protocol;
	hssocket->timeout.quad	= 0;
	hssocket->iostatus	= NT_STATUS_SUCCESS;
	hssocket->waitstatus	= NT_STATUS_SUCCESS;

	return status;
}
