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

int32_t __cdecl __ntapi_sc_send(
	__in		nt_socket *		hssocket,
	__in		const void *		buffer,
	__in		size_t			len,
	__out		ssize_t *		bytes_sent	__optional,
	__in		uintptr_t		afdflags	__optional,
	__in		uintptr_t		tdiflags	__optional,
	__out		nt_io_status_block *	iosb		__optional)
{
	nt_afd_buffer		afd_buffer;
	nt_afd_send_info	afd_send;
	nt_io_status_block	siosb;

	iosb = iosb ? iosb : &siosb;

	/* afd_buffer */
	afd_buffer.length = len;
	afd_buffer.buffer = (char *)buffer;

	/* afd_send */
	afd_send.afd_buffer_array = &afd_buffer;
	afd_send.buffer_count     = 1;

	afd_send.afd_flags = (uint32_t)afdflags;
	afd_send.tdi_flags = (uint32_t)tdiflags;

	hssocket->iostatus = __ntapi->zw_device_io_control_file(
			hssocket->hsocket,
			hssocket->hevent,
			0,
			0,
			iosb,
			NT_AFD_IOCTL_SEND,
			&afd_send,
			sizeof(afd_send),
			0,
			0);

	if (hssocket->iostatus && (hssocket->ntflags & __NT_FILE_SYNC_IO))
		__ntapi->sc_wait(hssocket,iosb,&hssocket->timeout);

	if (!hssocket->iostatus && bytes_sent)
		*bytes_sent = iosb->info;

	return hssocket->iostatus;
}
