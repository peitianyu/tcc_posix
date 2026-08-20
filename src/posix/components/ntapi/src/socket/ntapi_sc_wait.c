/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_sync.h>
#include <ntapi/nt_socket.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

int32_t __cdecl __ntapi_sc_wait(nt_socket * hssocket, nt_iosb * iosb, nt_timeout * timeout)
{
	nt_iosb	cancel;

	timeout = (timeout && timeout->quad)
		? timeout
		: 0;

	if (hssocket->hevent && (hssocket->iostatus == NT_STATUS_PENDING)) {
		hssocket->waitstatus = __ntapi->zw_wait_for_single_object(
				hssocket->hevent,
				!!(hssocket->ntflags & NT_FILE_SYNCHRONOUS_IO_ALERT),
				timeout);

		switch (hssocket->waitstatus) {
			case NT_STATUS_SUCCESS:
				hssocket->iostatus = NT_STATUS_SUCCESS;
				break;

			case NT_STATUS_ALERTED:
				hssocket->iostatus = NT_STATUS_ALERTED;
				__ntapi->zw_cancel_io_file(
					hssocket->hsocket,
					&cancel);
				break;
		}
	}

	return hssocket->iostatus;
}
