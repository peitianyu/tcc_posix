/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_object.h>
#include <ntapi/nt_guid.h>
#include <ntapi/nt_string.h>
#include "ntapi_impl.h"

typedef ntapi_zw_open_directory_object objdir_open_fn;

static int32_t __stdcall __tt_create_keyed_object_directory(
	__out	void **			hdir,
	__in	uint32_t		desired_access,
	__in	const wchar16_t		prefix[6],
	__in	nt_guid *		guid,
	__in	uint32_t		key,
	__in	objdir_open_fn *	openfn)
{
	nt_keyed_objdir_name	objdir_name = {__NT_BASED_NAMED_OBJECTS};
	nt_unicode_string	name;
	nt_oa			oa;
	nt_sqos			sqos = {
					sizeof(sqos),
					NT_SECURITY_IMPERSONATION,
					NT_SECURITY_TRACKING_DYNAMIC,
					1};

	__ntapi->tt_memcpy_utf16(
		objdir_name.prefix,
		prefix,
		sizeof(objdir_name.prefix));

	__ntapi->tt_guid_to_utf16_string(
		guid,
		(nt_guid_str_utf16 *)&objdir_name.objdir_guid);

	__ntapi->tt_uint32_to_hex_utf16(
		key,objdir_name.key);

	objdir_name.backslash = '\\';
	objdir_name.objdir_guid.uscore_guid = '_';
	objdir_name.objdir_guid.uscore_key  = '_';

	name.strlen	= sizeof(objdir_name);
	name.maxlen	= 0;
	name.buffer	= (uint16_t *)&objdir_name;

	oa.len		= sizeof(oa);
	oa.root_dir	= 0;
	oa.obj_name	= &name;
	oa.obj_attr	= NT_OBJ_INHERIT;
	oa.sec_desc	= 0;
	oa.sec_qos	= &sqos;

	return openfn(hdir,desired_access,&oa);
}


int32_t __stdcall __ntapi_tt_create_keyed_object_directory_entry(
	__out	void **			hentry,
	__in	uint32_t		desired_access,
	__in	void *			hdir,
	__in	void *			htarget,
	__in	nt_unicode_string *	target_name,
	__in	uint32_t		key)
{
	int32_t			status;
	nt_oa			oa;
	nt_unicode_string	name;
	wchar16_t		keystr[8];
	uintptr_t		buffer[2048/sizeof(uintptr_t)];
	nt_sqos			sqos = {
					sizeof(sqos),
					NT_SECURITY_IMPERSONATION,
					NT_SECURITY_TRACKING_DYNAMIC,
					1};

	if (!target_name) {
		if ((status = __ntapi->zw_query_object(
				htarget,
				NT_OBJECT_NAME_INFORMATION,
				buffer,sizeof(buffer),0)))
			return status;
		target_name = (nt_unicode_string *)buffer;
	}

	__ntapi->tt_uint32_to_hex_utf16(key,keystr);

	name.strlen = sizeof(keystr);
	name.maxlen = 0;
	name.buffer = keystr;

	oa.len		= sizeof(oa);
	oa.root_dir	= hdir;
	oa.obj_name	= &name;
	oa.obj_attr	= 0;
	oa.sec_desc	= 0;
	oa.sec_qos	= &sqos;

	return __ntapi->zw_create_symbolic_link_object(
		hentry,
		desired_access,
		&oa,target_name);
}

int32_t __stdcall __ntapi_tt_create_keyed_object_directory(
	__out	void **			hdir,
	__in	uint32_t		desired_access,
	__in	const wchar16_t		prefix[6],
	__in	nt_guid *		guid,
	__in	uint32_t		key)
{
	return __tt_create_keyed_object_directory(
		hdir,desired_access,
		prefix,guid,key,
		__ntapi->zw_create_directory_object);
}

int32_t __stdcall __ntapi_tt_open_keyed_object_directory(
	__out	void **			hdir,
	__in	uint32_t		desired_access,
	__in	const wchar16_t		prefix[6],
	__in	nt_guid *		guid,
	__in	uint32_t		key)
{
	return __tt_create_keyed_object_directory(
		hdir,desired_access,
		prefix,guid,key,
		__ntapi->zw_open_directory_object);
}
