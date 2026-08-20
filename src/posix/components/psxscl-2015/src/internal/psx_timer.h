/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#ifndef _PSX_TIMER_H_
#define _PSX_TIMER_H_

#include "psx_systypes.h"

enum __psx_timer_type {
	__PSX_ITIMER_REAL,
	__PSX_ITIMER_VIRTUAL,
	__PSX_ITIMER_PROF,
	__PSX_ITIMER_INTERNAL,
	__PSX_ITIMER_CAP
};

struct __timer {
	void *			hthread;
	void *			htimer;
	struct itimerval	itimer;
	intptr_t		instance;
};

struct __timer_ctx {
	void *			htimer;
	struct itimerval	itimer;
	enum __psx_timer_type	type;
	intptr_t		tinstance;
	intptr_t *		dinstance;
	struct __psx_ctx *	ctx;
};

int32_t __stdcall __psx_timer_routine(struct __timer_ctx * ctx);

#endif
