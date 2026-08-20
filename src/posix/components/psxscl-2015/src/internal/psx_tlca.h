/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#ifndef _PSX_TLCA_H_
#define _PSX_TLCA_H_

#include <psxtypes/psxtypes.h>
#include <ntapi/ntapi.h>
#include "psx_limits.h"
#include "psx_flags.h"
#include "psx_ofd.h"
#include "psx_impl.h"

struct __psx_tlca {
	void *			pthread_self;
	int *			pthread_set_child_tid;
	int *			pthread_clear_child_tid;
	char *			pthread_tls;
	char **			pthread_dtls;

	int32_t			ntstatus;
	int32_t			gdistatus;
	int32_t			psxstatus;
	int32_t			exitcode;

	nt_tib			tib_system;
	nt_tib			tib_posix;
	nt_tib			tib_signal;

	void *			hthread;
	void *			hpeer;
	void *			hfutex;
	void *			hevent;
	void *			hdbglog;
	void *			hdbgevt;
	void *			entry_routine;
	void *			entry_routine_ctx;

	void *			tlca_addr;
	size_t			tlca_size;

	sigset_t		sig_mask;
	int32_t			sig_dlock;
	int32_t			sig_tlock;
	int32_t			sig_prolog;
	int32_t			sig_count;
	struct __ucontext *	sig_ucontext;

	void *			wait_handle;
	pid_t			wait_pid;
	int32_t			wait_status;
	siginfo_t *		wait_info;
	struct __rusage *	wait_rusage;
	int32_t			wait_count;
	int32_t			wait_ecode;

	nt_process_information	clonevm;
	nt_process_information *execve;

	int32_t			frestart;
	int32_t			freserved;

	int32_t			cfalert;
	int32_t			cfnonalert;
	nt_timeout		zerowait;
	nt_timeout *		cfzerowait;
	nt_timeout *		cfinfinity;

	struct __psx_ctx *	ctx;
	struct __ofd		ofd;

	intptr_t		cheating[8];

	unsigned char		strace[4096];

	size_t			buflen;
	uintptr_t		buffer[];
};

static __inline__ struct __psx_tlca ** __tls_slot_addr(void)
{
	struct __os_tib *	tib;
	struct __psx_tlca **	slots;
	struct __psx_tlca ***	xslots;
	uintptr_t		sys_idx;

	tib = (struct __os_tib *)pe_get_teb_address();
	sys_idx = wintls_sys_idx;

	if (sys_idx < 64) {
		slots = (struct __psx_tlca **)((uintptr_t)tib + 0x1480);
		return &slots[sys_idx];
	} else {
		xslots = (struct __psx_tlca ***)((uintptr_t)tib + 0x1780);
		slots  = *xslots;
		return &slots[sys_idx - 64];
	}
}

static __inline__ struct __psx_tlca * __tlca_self(void)
{
	return *__tls_slot_addr();
}


static __inline__ struct __psx_ctx * __tlca_shared_ctx(struct __psx_tlca * tlca)
{
	/* ban threads created with (CLONE_VM && !CLONE_THREAD) */
	return (tlca->ctx == &rtctx) ? &rtctx : 0;
}

int32_t __fastcall __attr_hidden__ __psx_tlca_init(struct __psx_tlca ** tlca);
void	__fastcall __attr_hidden__ __psx_tlca_prolog(void * fn, void * stack);
void	__fastcall __attr_hidden__ __psx_tlca_epilog(void * stack, void * pexit);

#endif
