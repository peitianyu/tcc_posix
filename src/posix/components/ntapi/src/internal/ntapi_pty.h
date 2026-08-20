/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#ifndef ___NTAPI_PTY_H_
#define ___NTAPI_PTY_H_

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_status.h>
#include <ntapi/nt_guid.h>
#include <ntapi/nt_sync.h>
#include <ntapi/nt_tty.h>

#define __PTY_READ	0
#define __PTY_WRITE	1

typedef struct nt_pty_context {
	nt_sync_block	sync[2];
	void *		addr;
	size_t		size;
	void *		hport;
	void *		hpty;
	void *		section;
	void *		section_addr;
	size_t		section_size;
	nt_guid		guid;
	nt_luid		luid;
	uint32_t	access;
	uint32_t	flags;
	uint32_t	share;
	uint32_t	options;
	nt_iosb		iosb;
} nt_pty;

#endif
