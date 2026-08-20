/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_iovtbl.h"
#include "psx_fcntl.h"
#include "psx_ofd.h"
#include "psx_io.h"
#include "psx_errno.h"
#include "psx_impl.h"
#include "psx.h"

void __psx_io_set_status(struct __psx_tlca * tlca, struct __ofd * ofd, nt_iosb * iosb)
{
	nt_iosb cancel;
	int32_t waitstatus;

	if (!(tlca->ntstatus = ofd->info.iostatus))
		return;

	if ((ofd->info.iostatus == NT_STATUS_PENDING) && (ofd->info.psxflags & O_NONBLOCK)) {
		ofd->info.iostatus = __iovtbl[ofd->info.fdtype].cancel(
			ofd->info.hfile,
			&cancel);

		if (iosb->status == NT_STATUS_CANCELLED)
			iosb->info = -EAGAIN;
		else if (iosb->status)
			iosb->info = -ENXIO;

	} else if (ofd->info.iostatus == NT_STATUS_PENDING) {
		waitstatus = __ntapi->zw_wait_for_single_object(
			ofd->info.hevent,
			NT_SYNC_ALERTABLE,
			0);

		if (waitstatus == NT_STATUS_ALERTED)
			__ntapi->zw_delay_execution(&tlca->cfalert,0);

		while ((waitstatus == NT_STATUS_ALERTED) && tlca->frestart) {
			waitstatus = __ntapi->zw_wait_for_single_object(
				ofd->info.hevent,
				NT_SYNC_ALERTABLE,
				0);

			if (waitstatus == NT_STATUS_ALERTED)
				__ntapi->zw_delay_execution(&tlca->cfalert,0);
		}

		if (waitstatus) {
			ofd->info.iostatus = __iovtbl[ofd->info.fdtype].cancel(
				ofd->info.hfile,
				&cancel);

			if (iosb->status == NT_STATUS_CANCELLED)
				iosb->info = -EINTR;
			else if (iosb->status)
				iosb->info = -ENXIO;
		} else
			ofd->info.iostatus = NT_STATUS_SUCCESS;

	} else if ((ofd->info.iostatus == NT_STATUS_CANCELLED) && (ofd->info.psxflags & O_NONBLOCK))
		iosb->info = -EAGAIN;

	else if (ofd->info.iostatus == NT_STATUS_CANCELLED)
		iosb->info = -EINTR;

	else if (ofd->info.iostatus)
		iosb->status = ofd->info.iostatus;


	if (iosb->status == NT_STATUS_END_OF_FILE)
		iosb->info = 0;

	else if ((iosb->status == NT_STATUS_PIPE_BROKEN) && (ofd->info.reserved == __IO_READ))
		iosb->info = 0;

	else if ((iosb->status == NT_STATUS_PIPE_BROKEN) && (ofd->info.reserved == __IO_WRITE))
		iosb->info = -EPIPE;

	else if (iosb->status)
		iosb->info = -ENXIO;

	tlca->ntstatus = iosb->status;
}
