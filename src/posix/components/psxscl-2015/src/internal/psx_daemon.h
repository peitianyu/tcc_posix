/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#ifndef _PSX_DAEMON_H_
#define _PSX_DAEMON_H_

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_sigfn.h"
#include "psx_timer.h"

enum __psx_daemon_opcodes {
	PSX_DAEMON_OPCODE_BASE	= 0x40000000,
	PSX_DAEMON_CONNECT	= PSX_DAEMON_OPCODE_BASE,
	PSX_DAEMON_DISCONNECT,
	PSX_DAEMON_TTYSIGNAL,
	PSX_DAEMON_IPCSIGNAL,
	PSX_DAEMON_SIGACTION,
	PSX_DAEMON_SIGPROCMASK,
	PSX_DAEMON_SIGSEND,
	PSX_DAEMON_SIGRECEIVE,
	PSX_DAEMON_SIGGETIMTER,
	PSX_DAEMON_SIGSETIMTER,
	PSX_DAEMON_SIGCHLD,
	PSX_DAEMON_THREADEXIT,
	PSX_DAEMON_OPCODE_CAP
};

struct __msg_info {
	uintptr_t	id;
	uint32_t	opcode;
	int32_t		status;
	uintptr_t	key;
};

struct __port_msg {
	nt_port_message		header;
	struct __msg_info	msginfo;
	struct __psx_tlca *	tlca;
	char			msgdata[128];
};

struct __sig_msg {
	nt_port_message		header;
	struct __msg_info	msginfo;
	struct __psx_tlca *	tlca;
	struct __ucontext *	ucontext;
	enum __psx_signal_type	sigtype;
};

struct __timer_msg {
	nt_port_message		header;
	struct __msg_info	msginfo;
	struct __psx_tlca *	tlca;
	struct __ucontext *	ucontext;
	enum __psx_signal_type	sigtype;
	enum __psx_timer_type	timertype;
	struct itimerval	itimer;
	struct itimerval	ctimer;
	void *			htimer;
	intptr_t		tinstance;
	struct __psx_ctx *	ctx;
};

typedef int32_t __stdcall psx_daemon_routine(struct __port_msg *);

int32_t __stdcall __psx_daemon_init(void *);
int32_t __stdcall __psx_daemon_loop(void *);
int32_t __stdcall __psx_daemon_connect(struct __port_msg *);
int32_t __stdcall __psx_daemon_setitimer(struct __timer_msg *);
int32_t __stdcall __psx_daemon_sigsend(struct __sig_msg *);
int32_t __stdcall __psx_daemon_sigdeliver(struct __sig_msg *);
int32_t __stdcall __psx_daemon_threadexit(struct __port_msg *);

#endif
