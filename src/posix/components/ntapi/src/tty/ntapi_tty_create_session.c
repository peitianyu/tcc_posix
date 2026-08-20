/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

static int32_t __fastcall __tty_create_session_return(
	nt_create_process_params *	params,
	int32_t				status)
{
	if (status)
		__ntapi->zw_terminate_process(
			params->hprocess,
			NT_STATUS_UNEXPECTED_IO_ERROR);

	__ntapi->zw_close(params->hprocess);
	__ntapi->zw_close(params->hthread);

	return status;
}

int32_t __stdcall __ntapi_tty_create_session(
	__out	void **			hport,
	__out	nt_port_name *		port_name,
	__in	nt_tty_session_type	type,
	__in	const nt_guid *		guid		__optional,
	__in	wchar16_t *		image_name	__optional)
{
	nt_status			status;
	ntapi_internals *		__internals;

	nt_port_attr			port_attr;
	nt_runtime_data			ssattr;
	nt_runtime_data_block		rtblock;
	nt_create_process_params	params;

	wchar16_t __attr_aligned__(8) __tty_image_name_fallback[] =  {
		'\\','?','?','\\',
		'C',':',
		'\\','m','i','d','i','p','i','x',
		'\\','b','i','n',
		'\\','n','t','c','t','t','y',
		'.','e','x','e',
		0};

	/* init */
	__internals = __ntapi_internals();

	__ntapi->tt_aligned_block_memset(
		&port_attr,0,sizeof(port_attr));

	switch (type) {
		case NT_TTY_SESSION_PRIMARY:
			port_attr.type    = NT_PORT_TYPE_SUBSYSTEM;
			port_attr.subtype = NT_PORT_SUBTYPE_DEFAULT;

			if (!hport)
				hport = &__internals->hport_tty_session;

			if (!port_name)
				port_name = __internals->subsystem;

			if (!image_name)
				image_name = __tty_image_name_fallback;

			break;

		case NT_TTY_SESSION_PRIVATE:
			port_attr.type    = NT_PORT_TYPE_SUBSYSTEM;
			port_attr.subtype = NT_PORT_SUBTYPE_PRIVATE;
			break;

		default:
			return NT_STATUS_INVALID_PARAMETER;
	}

	/* port guid */
	if (guid)
		__ntapi->tt_guid_copy(
			&port_attr.guid,
			guid);
	else
		__ntapi->tt_port_guid_from_type(
			&port_attr.guid,
			port_attr.type,
			port_attr.subtype);

	/* port keys */
	if ((status = __ntapi->tt_port_generate_keys(&port_attr.keys)))
		return status;

	/* port name */
	__ntapi->tt_port_name_from_attributes(
		port_name,
		&port_attr);

	/* subsystem attributes */
	__ntapi->tt_aligned_block_memset(
		&ssattr,0,sizeof(ssattr));

	ssattr.srv_type		= port_attr.type;
	ssattr.srv_subtype	= port_attr.subtype;
	ssattr.srv_keys[0]	= port_attr.keys.key[0];
	ssattr.srv_keys[1]	= port_attr.keys.key[1];
	ssattr.srv_keys[2]	= port_attr.keys.key[2];
	ssattr.srv_keys[3]	= port_attr.keys.key[3];
	ssattr.srv_keys[4]	= port_attr.keys.key[4];
	ssattr.srv_keys[5]	= port_attr.keys.key[5];

	__ntapi->tt_guid_copy(
		&ssattr.srv_guid,
		&port_attr.guid);

	if ((status = __ntapi->tt_create_private_event(
			&ssattr.srv_ready,
			NT_SYNCHRONIZATION_EVENT,
			NT_EVENT_NOT_SIGNALED)))
		return status;

	/* create subsystem process */
	rtblock.addr		= &ssattr;
	rtblock.size		= sizeof(ssattr);
	rtblock.remote_addr	= 0;
	rtblock.remote_size	= 0;
	rtblock.flags		= NT_RUNTIME_DATA_DUPLICATE_SESSION_HANDLES;

	__ntapi->tt_aligned_block_memset(
		&params,0,sizeof(params));

	params.image_name	= image_name;
	params.rtblock		= &rtblock;

	if ((status = __ntapi->tt_create_native_process(&params)))
		return status;

	if ((status = __ntapi->zw_wait_for_single_object(
			ssattr.srv_ready,
			NT_SYNC_NON_ALERTABLE,
			0)))
		return __tty_create_session_return(&params,status);

	/* connect to subsystem */
	if ((status = __ntapi->tty_connect(
			hport,
			&port_name->base_named_objects[0],
			NT_SECURITY_IMPERSONATION)))
		return __tty_create_session_return(&params,status);

	/* finalize primary session */
	if (type == NT_TTY_SESSION_PRIMARY) {
		if (hport != &__internals->hport_tty_session)
			__internals->hport_tty_session = *hport;

		if (port_name != __internals->subsystem)
			__ntapi->tt_memcpy_utf16(
				__internals->subsystem->base_named_objects,
				port_name->base_named_objects,
				sizeof(*port_name));
	};

	return __tty_create_session_return(&params,NT_STATUS_SUCCESS);
}
