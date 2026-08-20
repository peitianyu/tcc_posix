#ifndef _NT_BLITTER_H_
#define _NT_BLITTER_H_

#include <psxtypes/psxtypes.h>
#include "nt_object.h"

/* blt_alloc flag bits */
#define NT_BLITTER_ENABLE_BLOCK_ARRAY		0x01
#define NT_BLITTER_ENABLE_BLOCK_SECTIONS	0x02
#define NT_BLITTER_ENABLE_CACHE			0x04
#define NT_BLITTER_PRESERVE_BITS		0x08

/* blitter query type bits */
#define	NT_BLITTER_QUERY_INFO			0x01
#define	NT_BLITTER_QUERY_PARAMS			0x02

/* blitter context (opaque) */
typedef struct nt_blitter_context nt_blitter;

typedef struct _nt_blitter_params {
	size_t		params_size;
	size_t		block_size;
	intptr_t	block_count;
	intptr_t	cache_blocks_min;	__optional
	intptr_t	cache_blocks_max;	__optional
	size_t		zero_bits;		__optional
	uint32_t	srvtid;			__optional
	uint32_t	flags;			__optional
	int32_t		lock_tries;		__optional
	int32_t		round_trips;		__optional
	void *		hsignal_blt;		__optional
	void *		hsignal_app;		__optional
	void *		bitmap;			__optional
	void *		region;			__optional
} nt_blitter_params;


typedef struct _nt_blitter_info {
	size_t		info_size;
	void *		region_addr;
	size_t		region_size;
	size_t		block_size;
	intptr_t	block_count;
	intptr_t	blocks_used;
	intptr_t	blocks_avail;
	intptr_t	blocks_cached;
	intptr_t	lock_tries;
	intptr_t	busy;
} nt_blitter_info;


typedef struct _nt_blitter_query {
	size_t			size;
	int32_t			type;
	uint32_t		flags;
	nt_blitter_params *	params;
	nt_blitter_info *	info;
} nt_blitter_query;


typedef struct _nt_blitter_config {
	size_t			size;
	int32_t			type;
	uint32_t		flags;
	nt_blitter_params *	params;
} nt_blitter_config;


/* blitter functions */
typedef int32_t __fastcall	ntapi_blt_alloc(
	__out	nt_blitter **		blitter,
	__in	nt_blitter_params *	params);


typedef int32_t __fastcall	ntapi_blt_free(
	__in	nt_blitter *		blitter);


typedef int32_t __fastcall	ntapi_blt_query(
	__in	nt_blitter *		blitter,
	__in	nt_blitter_query *	blt_query);


typedef int32_t __fastcall	ntapi_blt_config(
	__in	nt_blitter *		blitter,
	__in	nt_blitter_config *	blt_config);


typedef int32_t __fastcall	ntapi_blt_acquire(
	__in	nt_blitter *		blitter,
	__out	intptr_t *		block_id);


typedef int32_t __fastcall	ntapi_blt_obtain(
	__in	nt_blitter *		blitter,
	__out	intptr_t *		block_id);


typedef int32_t __fastcall	ntapi_blt_possess(
	__in	nt_blitter *		blitter,
	__out	intptr_t *		block_id);


typedef int32_t __fastcall	ntapi_blt_release(
	__in	nt_blitter *		blitter,
	__in	intptr_t		block_id);


typedef void * __fastcall	ntapi_blt_region(
	__in	const nt_blitter *	blitter);


/**
 *  blt_get,blt_set:
 *  clients might prefr to write inlined
 *  alternatives that use a cached blt_region.
**/
typedef void * __fastcall	ntapi_blt_get(
	__in	const nt_blitter *	blitter,
	__in	intptr_t		block_id);


typedef void __fastcall		ntapi_blt_set(
	__in	const nt_blitter *	blitter,
	__in	intptr_t		block_id,
	__in	void *			val);


#endif
