/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_tty.h>
#include <pemagine/pemagine.h>
#include "psx_systypes.h"
#include "psx_ioctl.h"
#include "psx_errno.h"
#include "psx.h"

/* console std handle termios/winsize mapping (via kernel32) */
#define __WIN_ENABLE_PROCESSED_INPUT	0x0001
#define __WIN_ENABLE_LINE_INPUT		0x0002
#define __WIN_ENABLE_ECHO_INPUT		0x0004

typedef int (__stdcall *__win_get_console_mode_fn)(void * handle, uint32_t * mode);
typedef int (__stdcall *__win_set_console_mode_fn)(void * handle, uint32_t mode);
typedef int (__stdcall *__win_get_console_csbi_fn)(void * handle, void * info);

struct __win_console_screen_buffer_info {
	short	dwSizeX, dwSizeY;		/* COORD dwSize */
	short	dwCursorX, dwCursorY;		/* COORD dwCursorPosition */
	short	wAttributes;			/* WORD wAttributes */
	short	srLeft, srTop, srRight, srBottom; /* SMALL_RECT srWindow */
	short	maxX, maxY;			/* COORD dwMaximumWindowSize */
};

static intptr_t __ioctl_console(struct __ofd * ofd, unsigned long request, void * ptra)
{
	static __win_get_console_mode_fn	pGetConsoleMode;
	static __win_set_console_mode_fn	pSetConsoleMode;
	static __win_get_console_csbi_fn	pGetCsbi;
	uint32_t				mode;
	uint32_t				geomode;
	struct tty_termios			term;
	struct __win_console_screen_buffer_info	csbi;

	if (!pGetConsoleMode)
		pGetConsoleMode = (__win_get_console_mode_fn)pe_get_procedure_address(
			pe_get_kernel32_module_handle(), "GetConsoleMode");
	if (!pGetConsoleMode)
		return -ENOTTY;

	/* not a console (pipe/file) */
	if (!pGetConsoleMode(ofd->info.hfile, &mode))
		return -ENOTTY;

	switch (request) {

		case TCGETS:
		case TCGETA:
			__ntapi->tt_aligned_block_memset(&term, 0, sizeof(term));
			term.c_iflag  = TTY_BRKINT | TTY_ICRNL | TTY_IXON | TTY_IMAXBEL;
			term.c_oflag  = TTY_OPOST | TTY_ONLCR;
			term.c_cflag  = TTY_CS8 | TTY_CREAD | TTY_B9600;
			term.c_lflag  = TTY_ISIG | TTY_ICANON | TTY_ECHO |
					TTY_ECHOE | TTY_ECHOK | TTY_IEXTEN;
			if (!(mode & __WIN_ENABLE_ECHO_INPUT))
				term.c_lflag &= ~(TTY_ECHO | TTY_ECHOE | TTY_ECHOK);
			if (!(mode & __WIN_ENABLE_LINE_INPUT))
				term.c_lflag &= ~TTY_ICANON;
			if (!(mode & __WIN_ENABLE_PROCESSED_INPUT))
				term.c_lflag &= ~TTY_ISIG;

			term.c_cc[TTY_VINTR]   = '\003';
			term.c_cc[TTY_VQUIT]   = '\034';
			term.c_cc[TTY_VERASE]  = '\177';
			term.c_cc[TTY_VKILL]   = '\025';
			term.c_cc[TTY_VEOF]    = '\004';
			term.c_cc[TTY_VMIN]    = 6;
			term.c_cc[TTY_VTIME]   = 0;
			term.c_cc[TTY_VSTART]  = '\021';
			term.c_cc[TTY_VSTOP]   = '\023';
			term.c_cc[TTY_VSUSP]   = '\032';
			term.c_cc[TTY_VEOL]    = 0;
			term.c_cc[TTY_VEOL2]   = 0;

			term.__c_ispeed = TTY_B9600;
			term.__c_ospeed = TTY_B9600;
			__ntapi->tt_generic_memcpy(
				(char *)ptra, (char *)&term, sizeof(struct tty_termios));
			return 0;

		case TCSETS:
		case TCSETSW:
		case TCSETSF:
		case TCSETA:
		case TCSETAW:
		case TCSETAF:
			if (!pSetConsoleMode)
				pSetConsoleMode = (__win_set_console_mode_fn)pe_get_procedure_address(
					pe_get_kernel32_module_handle(), "SetConsoleMode");
			if (!pSetConsoleMode)
				return -ENOTTY;
			__ntapi->tt_aligned_block_memset(&term, 0, sizeof(term));
			__ntapi->tt_generic_memcpy(
				(char *)&term, (char *)ptra, sizeof(struct tty_termios));
			geomode = /* start from current input mode */
				mode &
				~(__WIN_ENABLE_ECHO_INPUT | __WIN_ENABLE_LINE_INPUT |
					__WIN_ENABLE_PROCESSED_INPUT);
			if (term.c_lflag & TTY_ECHO)
				geomode |= __WIN_ENABLE_ECHO_INPUT;
			if (term.c_lflag & TTY_ICANON)
				geomode |= __WIN_ENABLE_LINE_INPUT;
			if (term.c_lflag & TTY_ISIG)
				geomode |= __WIN_ENABLE_PROCESSED_INPUT;
			pSetConsoleMode(ofd->info.hfile, geomode);
			return 0;

		case TIOCGWINSZ:
			if (!pGetCsbi)
				pGetCsbi = (__win_get_console_csbi_fn)pe_get_procedure_address(
					pe_get_kernel32_module_handle(),
					"GetConsoleScreenBufferInfo");
			if (!pGetCsbi)
				return -ENOTTY;
			__ntapi->tt_aligned_block_memset(&csbi, 0, sizeof(csbi));
			if (!pGetCsbi(ofd->info.hfile, &csbi))
				return -ENOTTY;
			else {
				struct tty_winsize	wsz;
				__ntapi->tt_aligned_block_memset(&wsz, 0, sizeof(wsz));
				wsz.ws_row = (uint16_t)(csbi.srBottom - csbi.srTop + 1);
				wsz.ws_col = (uint16_t)(csbi.srRight - csbi.srLeft + 1);
				wsz.ws_xpixel = 0;
				wsz.ws_ypixel = 0;
				__ntapi->tt_generic_memcpy(
					(char *)ptra, (char *)&wsz, sizeof(struct tty_winsize));
			}
			return 0;

		default:
			return -ENOTTY;
	}

	return -ENOTTY;
}

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
		/* console (stdin/stdout/stderr) termios/winsize mapping;
		   non-console (pipe/file/socket) falls back to ENOTTY */
		ret = __ioctl_console(ofd,request,ptra);
		status = EPSXONLY;
	}

	__psx_ofd_ref_dec(tlca->ctx,ofd);
	return __psx_sig_epilog(tlca,ret,status);
}
