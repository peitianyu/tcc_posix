/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_tlca.h"
#include "psx_errno.h"
#include "psx_ofd.h"
#include "psx_io.h"
#include "psx.h"

__psx_api
ssize_t __sys_writev(int fd, const struct iovec * iov, int iovcnt)
{
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __ofd *		ctxofd;
	struct __ofd *		ofd;
	nt_iosb			iosb;
	int			iovc;
	char *			iov_buf;
	ssize_t			iov_bytes;
	ssize_t			writev_bytes;

	tlca = __tlca_self();
	ctx  = tlca->ctx;
	__psx_io_prolog(tlca);

	if (!(ctxofd  = __psx_ofd_ref_inc(ctx,fd)))
		return __psx_io_epilog(tlca,-EBADF,EPSXONLY);

	else if ((iosb.status = __iovtbl[ctxofd->info.fdtype].prolog(tlca,ctxofd,&ofd)))
		return __psx_io_epilog(tlca,-ENOMEM,iosb.status);

	ofd->info.iostatus = 0;

	for (writev_bytes=iovc=0; iovc<iovcnt && !iosb.status; iovc++) {
		iov_buf   = (char *)iov[iovc].iov_base;
		iov_bytes = iov[iovc].iov_len;

		do {
			ofd->info.iostatus = __iovtbl[ofd->info.fdtype].write(
				ofd->info.hfile,
				ofd->info.hevent,
				0,0,
				&iosb,
				iov_buf,
				(uint32_t)iov_bytes,
				0,0);

			__psx_io_set_status(tlca,ofd,&iosb);

			if (!iosb.status) {
				writev_bytes	+= iosb.info;
				iov_buf		+= iosb.info;
				iov_bytes	-= iosb.info;
			}
		} while (iov_bytes && !iosb.status);
	}

	iosb.info = writev_bytes ? writev_bytes : iosb.info;
	__iovtbl[ctxofd->info.fdtype].epilog(ctxofd,ofd);
	__psx_ofd_ref_dec(ctx,ctxofd);

	return __psx_io_epilog(tlca,iosb.info,tlca->ntstatus);
}
