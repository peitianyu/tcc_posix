/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_object.h>
#include <ntapi/nt_sync.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"


static int32_t __cdecl __tt_create_event(
	__out	void **		hevent,
	__in 	nt_event_type	event_type,
	__in 	int32_t		initial_state,
	__in	uint32_t	obj_attr)
{
	int32_t	status;
	nt_sqos	sqos;
	nt_oa	oa;

	/* validation */
	if (!hevent)
		return NT_STATUS_INVALID_PARAMETER;

	/* security structure */
	sqos.length 			= sizeof(sqos);
	sqos.impersonation_level	= NT_SECURITY_IMPERSONATION;
	sqos.context_tracking_mode	= NT_SECURITY_TRACKING_DYNAMIC;
	sqos.effective_only		= 1;

	/* object attributes */
	oa.len		= sizeof(nt_object_attributes);
	oa.root_dir	= (void *)0;
	oa.obj_name	= (nt_unicode_string *)0;
	oa.obj_attr	= obj_attr;
	oa.sec_desc	= (nt_security_descriptor *)0;
	oa.sec_qos	= &sqos;

	status = __ntapi->zw_create_event(
		hevent,
		NT_EVENT_ALL_ACCESS,
		&oa,
		event_type,
		initial_state);

	return status;
}


int32_t __stdcall __ntapi_tt_create_inheritable_event(
	__out	void **		hevent,
	__in 	nt_event_type	event_type,
	__in 	int32_t		initial_state)
{
	return __tt_create_event(
		hevent,
		event_type,
		initial_state,
		NT_OBJ_INHERIT);
}


int32_t __stdcall __ntapi_tt_create_private_event(
	__out	void **		hevent,
	__in 	nt_event_type	event_type,
	__in 	int32_t		initial_state)
{
	return __tt_create_event(
		hevent,
		event_type,
		initial_state,
		0);
}
