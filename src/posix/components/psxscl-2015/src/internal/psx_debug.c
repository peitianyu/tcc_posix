/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_tlca.h"
#include "psx_signal.h"
#include "psx_errno.h"
#include "psx_debug.h"
#include "psx_fcntl.h"
#include "psx_ofd.h"
#include "psx_io.h"
#include "psx.h"

static void * __dbg_event = 0;

ssize_t __psx_dbg_write(int fd, void * buf, size_t bytes)
{
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __ofd *		ofd;
	void **			pevent;
	void *			hevent;
	nt_iosb			iosb;
	nt_iosb			cancel;
	uint32_t		nbytes;
	int32_t			status;


	if (0) return 0;



	nbytes  = (bytes == -1)
		? (uint32_t)__ntapi->tt_string_null_offset_multibyte((const char *)buf)
		: (uint32_t)bytes;




	/* temporary workaround */
	if (fd == -1) {
		if (!rtdata->hlog && (status = __ntapi->zw_duplicate_object(
				rtdata->hprocess_self,
				rtctx.ofd_slots[2].info.hfile,
				rtdata->hprocess_self,
				&rtdata->hlog,
				0,0,NT_DUPLICATE_SAME_ACCESS|NT_DUPLICATE_SAME_ATTRIBUTES)))
			return status;

		if (!__dbg_event && (status = __ntapi->tt_create_private_event(
				&__dbg_event,
				NT_NOTIFICATION_EVENT,
				NT_EVENT_NOT_SIGNALED)))
			return -ENOMEM;

		status = __ntapi->zw_write_file(
			rtdata->hlog,
			__dbg_event,0,0,
			&iosb,buf,nbytes,
			0,0);

		if (status == NT_STATUS_PENDING)
			status = __ntapi->zw_wait_for_single_object(
				__dbg_event,
				NT_SYNC_NON_ALERTABLE,
				0);

		if (status)
			__ntapi->zw_cancel_io_file(
				rtdata->hlog,
				&cancel);

		return status ? -EIO : iosb.info;
	}












	tlca = __tlca_self();

	if (tlca) {
		ctx = tlca->ctx;
		pevent = &tlca->hdbgevt;
	} else {
		ctx = &rtctx;
		pevent = &hevent;
		hevent = 0;
	}

	if (!(ofd  = __psx_ofd_ref_inc(ctx,fd)))
		return -EBADF;

	if (!*pevent && (status = __ntapi->tt_create_private_event(
			pevent,
			NT_NOTIFICATION_EVENT,
			NT_EVENT_NOT_SIGNALED)))
		return -ENOMEM;

	status = __iovtbl[ofd->info.fdtype].write(
		ofd->info.hfile,
		*pevent,0,0,
		&iosb,buf,nbytes,
		0,0);

	if (status == NT_STATUS_PENDING)
		status = __ntapi->zw_wait_for_single_object(
			*pevent,
			NT_SYNC_NON_ALERTABLE,
			0);

	if (status)
		__iovtbl[ofd->info.fdtype].cancel(
			ofd->info.hfile,
			&cancel);

	__psx_ofd_ref_dec(ctx,ofd);

	if (!tlca)
		__ntapi->zw_close(hevent);

	return status ? -EIO : iosb.info;
}
