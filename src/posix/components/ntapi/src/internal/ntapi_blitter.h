/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#ifndef ___NTAPI_BLITTER_H_
#define ___NTAPI_BLITTER_H_

#include <ntapi/nt_status.h>
#include <ntapi/nt_blitter.h>
#include <ntapi/nt_sync.h>

#define __NT_BLITTER_DEFAULT_LOCK_TRIES		256
#define __NT_BLITTER_DEFAULT_ROUND_TRIPS	64

typedef struct nt_blitter_context {
	struct nt_blitter_context *	addr;
	size_t				size;
	uintptr_t			ptrs;
	nt_blitter_info			info;
	nt_blitter_params		params;
	uintptr_t *			bitmap;
	uintptr_t			bits[];
} nt_blitter;

#endif
