/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

int32_t __stdcall __ntapi_tty_join_session(
	__out	void **			hport,
	__out	nt_port_name *		port_name,
	__in	nt_port_attr *		port_attr,
	__in	nt_tty_session_type	type)
{
	nt_status			status;
	ntapi_internals *		__internals;

	/* init */
	__internals = __ntapi_internals();

	if (type == NT_TTY_SESSION_PRIMARY) {
		hport = hport ? hport : &__internals->hport_tty_session;
		port_name = port_name ? port_name : __internals->subsystem;
	}

	/* port name */
	__ntapi->tt_port_name_from_attributes(
		port_name,
		port_attr);

	/* connect to subsystem */
	if ((status = __ntapi->tty_connect(
			hport,
			(wchar16_t *)port_name,
			NT_SECURITY_IMPERSONATION)))
		return status;

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

	return status;
}
