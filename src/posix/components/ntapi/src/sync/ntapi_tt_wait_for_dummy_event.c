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

int32_t __stdcall __ntapi_tt_wait_for_dummy_event(void)
{
	/* wait forever without setting a break point and without spinning */

	int32_t		status;
	void * 		hevent;

	status = __ntapi->tt_create_inheritable_event(
			&hevent,
			NT_NOTIFICATION_EVENT,
			NT_EVENT_NOT_SIGNALED);

	if (status != NT_STATUS_SUCCESS)
		return status;

	return __ntapi->zw_wait_for_single_object(hevent,0,0);

	return status;
}
