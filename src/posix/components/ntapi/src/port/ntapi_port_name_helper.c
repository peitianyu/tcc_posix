/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_guid.h>
#include <ntapi/nt_port.h>
#include <ntapi/nt_string.h>
#include "ntapi_impl.h"

typedef wchar16_t __port_service_prefix[6];

static const __port_service_prefix	__port_service_null = {0};
static const __port_service_prefix	__port_service_prefixes[4][NT_PORT_TYPE_CAP][NT_PORT_SUBTYPE_CAP] = {
					{{{'s','v','c','a','n','y'}}},
					{{{'n','t','c','t','t','y'}}},
					{{{'v','m','o','u','n','t'}}},
					{{{'d','a','e','m','o','n'}}}};

static const nt_guid		__port_guids[NT_PORT_TYPE_CAP][NT_PORT_SUBTYPE_CAP] = {
					{NT_PORT_GUID_DEFAULT},
					{NT_PORT_GUID_SUBSYSTEM},
					{NT_PORT_GUID_VMOUNT},
					{NT_PORT_GUID_DAEMON}};

int32_t __stdcall __ntapi_tt_port_guid_from_type(
	__out	nt_guid *		guid,
	__in	nt_port_type		type,
	__in	nt_port_subtype		subtype)
{
	const nt_guid *	src_guid;

	if ((type >= NT_PORT_TYPE_CAP) || (subtype >= NT_PORT_SUBTYPE_CAP))
		return NT_STATUS_INVALID_PARAMETER;

	src_guid = &(__port_guids[type][subtype]);

	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)guid,
		(uintptr_t *)src_guid,
		sizeof(nt_guid));

	return NT_STATUS_SUCCESS;
}


int32_t __stdcall __ntapi_tt_port_type_from_guid(
	__out	nt_port_type *		type,
	__out	nt_port_subtype *	subtype,
	__in	nt_guid *		guid)
{
	int		itype;
	int		isubtype;
	const nt_guid *	src_guid;
	uint32_t	guid_hash;
	uint32_t	src_hash;

	guid_hash = __ntapi->tt_buffer_crc32(0,guid,sizeof(nt_guid));

	for (itype=0; itype<NT_PORT_TYPE_CAP; itype++) {
		for (isubtype=0; isubtype<NT_PORT_SUBTYPE_CAP; isubtype++) {
			src_guid = &(__port_guids[itype][isubtype]);
			src_hash = __ntapi->tt_buffer_crc32(0,src_guid,sizeof(nt_guid));

			if (guid_hash == src_hash) {
				*type	 = (nt_port_type)itype;
				*subtype = (nt_port_subtype)isubtype;

				return NT_STATUS_SUCCESS;
			}
		}
	}

	return NT_STATUS_INVALID_PARAMETER;

}


int32_t __stdcall __ntapi_tt_port_generate_keys(
	__out	nt_port_keys *		keys)
{
	int32_t			status;
	nt_large_integer	systime;
	nt_luid			luid;

	status = __ntapi->zw_query_system_time(&systime);
	if (status) return status;

	status = __ntapi->zw_allocate_locally_unique_id(&luid);
	if (status) return status;

	keys->key[0] = pe_get_current_process_id();
	keys->key[1] = pe_get_current_thread_id();
	keys->key[2] = systime.ihigh;
	keys->key[3] = systime.ulow;
	keys->key[4] = luid.high;
	keys->key[5] = luid.low;

	return NT_STATUS_SUCCESS;
}


void __stdcall	__ntapi_tt_port_format_keys(
	__in	nt_port_keys *		keys,
	__out	nt_port_name_keys *	name_keys)
{
	__ntapi->tt_uint32_to_hex_utf16(keys->key[0],name_keys->key_1st);
	__ntapi->tt_uint32_to_hex_utf16(keys->key[1],name_keys->key_2nd);
	__ntapi->tt_uint32_to_hex_utf16(keys->key[2],name_keys->key_3rd);
	__ntapi->tt_uint32_to_hex_utf16(keys->key[3],name_keys->key_4th);
	__ntapi->tt_uint32_to_hex_utf16(keys->key[4],name_keys->key_5th);
	__ntapi->tt_uint32_to_hex_utf16(keys->key[5],name_keys->key_6th);

	return;
}


void __stdcall __ntapi_tt_port_name_from_attributes(
	__out	nt_port_name *		name,
	__in	nt_port_attr *		attr)
{
	wchar16_t bno[] = __NT_BASED_NAMED_OBJECTS;

	/* base named objects */
	__ntapi->tt_memcpy_utf16(
		name->base_named_objects,
		bno,sizeof(bno));

	/* service prefix */
	if (attr && (attr->type < NT_PORT_TYPE_CAP) && (attr->subtype < NT_PORT_SUBTYPE_CAP))
		__ntapi->tt_memcpy_utf16(
			name->svc_prefix,
			&(__port_service_prefixes[attr->type][attr->subtype][0][0]),
			sizeof(name->svc_prefix));
	else
		__ntapi->tt_memcpy_utf16(
			name->svc_prefix,
			__port_service_null,
			sizeof(name->svc_prefix));

	/* port guid */
	__ntapi->tt_guid_to_utf16_string(
		&attr->guid,
		(nt_guid_str_utf16 *)&name->port_guid);

	/* port name keys */
	__ntapi_tt_port_format_keys(
		&attr->keys,
		&name->port_name_keys);

	/* backslash and underscores */
	name->backslash = '\\';
	name->port_guid.uscore_guid = '_';
	name->port_guid.uscore_keys = '_';
	name->port_name_keys.uscore_1st  = '_';
	name->port_name_keys.uscore_2nd  = '_';
	name->port_name_keys.uscore_3rd  = '_';
	name->port_name_keys.uscore_4th  = '_';
	name->port_name_keys.uscore_5th  = '_';

	/* null termination */
	name->null_termination = 0;

	return;
}
