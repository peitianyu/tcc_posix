/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_ioctl.h"
#include "psx_errno.h"
#include "psx.h"

static intptr_t __ioctl_pty(struct __ofd * ofd, unsigned long request, void * ptra, void * ptrb, void * ptrc, void * ptrd)
{

	nt_tty_sigctl_info	ctlinfo;
	nt_iosb			iosb;

	__ntapi->tt_aligned_block_memset(
		&ctlinfo,0,sizeof(ctlinfo));

	if (request == TIOCSTI)
		ctlinfo.ctxarg[0] = *(char *)ptra;
	else if ((request == TIOCSCTTY) || (request == TCFLSH))
		ctlinfo.ctxarg[0] = (int32_t)(intptr_t)ptra;
	else {
		ctlinfo.ctxarg[0] = ptra ? *(int32_t *)ptra : 0;
		ctlinfo.ctxarg[1] = ptrb ? *(int32_t *)ptrb : 0;
		ctlinfo.ctxarg[2] = ptrc ? *(int32_t *)ptrc : 0;
		ctlinfo.ctxarg[3] = ptrd ? *(int32_t *)ptrd : 0;
	}

	switch (request) {
		case TTY_TCSETS:
		case TTY_TCSETSW:
		case TTY_TCSETSF:
		case TTY_TCSETA:
		case TTY_TCSETAW:
		case TTY_TCSETAF:
			__ntapi->tt_generic_memcpy(
				(char *)&ctlinfo.terminfo,
				(char *)ptra,
				sizeof(struct tty_termios));
			break;

		case TTY_TIOCSWINSZ:
			__ntapi->tt_generic_memcpy(
				(char *)&ctlinfo.winsize,
				(char *)ptra,
				sizeof(struct tty_winsize));
			break;

		default:
			break;
	}


	ofd->info.iostatus = __ntapi->pty_ioctl(
		ofd->info.hpty,
		ofd->info.hevent,0,0,
		&iosb,
		request,
		&ctlinfo,sizeof(ctlinfo),
		&ctlinfo,sizeof(ctlinfo));

	if (ofd->info.iostatus == NT_STATUS_INVALID_PARAMETER)
		return -EINVAL;
	else if (ofd->info.iostatus)
		return -ENXIO;


	switch (request) {
		case TTY_TCGETS:
		case TTY_TCGETA:
			__ntapi->tt_generic_memcpy(
				(char *)ptra,
				(char *)&ctlinfo.terminfo,
				sizeof(struct tty_termios));
			break;

		case TTY_TIOCGWINSZ:
			__ntapi->tt_generic_memcpy(
				(char *)ptra,
				(char *)&ctlinfo.winsize,
				sizeof(struct tty_winsize));
			break;

		default:
			break;
	}

	if (iosb.status == NT_STATUS_ALPC_CHECK_COMPLETION_LIST)
		*(uint32_t *)ptra = (uint32_t)iosb.info;

	return 0;
}

__psx_api
intptr_t __sys_ioctl(int fdidx, unsigned long request, void * ptra, void * ptrb, void * ptrc, void * ptrd)
{
	struct __psx_tlca *	tlca;
	struct __ofd *		ofd;
	int32_t			status;
	intptr_t		ret;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if (!(ofd  = __psx_ofd_ref_inc(tlca->ctx,fdidx)))
		return __psx_sig_epilog(tlca,-EBADF,EPSXONLY);

	if (ofd->info.fdtype == PSX_FD_PTY) {
		ret    = __ioctl_pty(ofd,request,ptra,ptrb,ptrc,ptrd);
		status = ofd->info.iostatus;
	} else {
		ret    = -ENOTTY;
		status = EPSXONLY;
	}

	__psx_ofd_ref_dec(tlca->ctx,ofd);
	return __psx_sig_epilog(tlca,ret,status);
}
