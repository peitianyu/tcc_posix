/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <ntapi/nt_status.h>
#include <ntapi/nt_blitter.h>
#include <ntapi/nt_sync.h>
#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "ntapi_blitter.h"
#include "ntapi_impl.h"

static int32_t __fastcall __blt_bitbite(
	__in	nt_blitter *	blitter,
	__in	unsigned int	bit,
	__in	size_t		byte)
{
	uint32_t	locktry;
	uintptr_t	test;
	uintptr_t	cmp;
	uintptr_t	xchg;
	uintptr_t	mask;

	mask    = ((uintptr_t)1 << bit);
	locktry = blitter->params.lock_tries;

	for (; locktry; locktry--) {
		cmp  = blitter->bitmap[byte] | mask;
		xchg = cmp ^ mask;

		test  = at_locked_cas(
			(intptr_t *)&blitter->bitmap[byte],
			cmp,xchg);

		if (test == cmp) {
			at_locked_dec(&blitter->info.blocks_avail);
			at_locked_inc(&blitter->info.blocks_used);
			return NT_STATUS_SUCCESS;

		} else if (test ^ mask)
			return NT_STATUS_TRANSACTIONAL_CONFLICT;
	}

	if (!locktry) {
		blitter->info.busy = 1;
		blitter->info.lock_tries = blitter->params.lock_tries;
		return NT_STATUS_DEVICE_BUSY;
	}

	return NT_STATUS_MORE_PROCESSING_REQUIRED;
}

static int32_t __fastcall __blt_acquire(
	__in	nt_blitter *	blitter,
	__out	intptr_t *	blkid)
{
	unsigned int	bit;
	uintptr_t		i,n;

	if (blitter->info.blocks_avail == 0)
		return NT_STATUS_ALLOCATE_BUCKET;

	for (n=0,bit=0; blitter->info.blocks_avail && (n < blitter->params.round_trips); n++) {
		for (i=*blkid/(8*sizeof(size_t)); (i<blitter->ptrs); i++)
			if (at_bsf(&bit,blitter->bitmap[i]))
				break;

		if (i == blitter->ptrs)
			return NT_STATUS_ALLOCATE_BUCKET;

		switch (__blt_bitbite(blitter,bit,i)) {
			case NT_STATUS_SUCCESS:
				*blkid = bit + (i * 8 * sizeof(size_t));
				return NT_STATUS_SUCCESS;

			case NT_STATUS_DEVICE_BUSY:
				return NT_STATUS_DEVICE_BUSY;

			default:
				break;
		}
	}

	return NT_STATUS_ALLOCATE_BUCKET;
}


int32_t __fastcall __ntapi_blt_obtain(
	__in	nt_blitter *	blitter,
	__out	intptr_t *	blkid)
{
	unsigned int	bit;
	uintptr_t	i,n;
	uintptr_t	mask;

	if (blitter->info.blocks_avail == 0)
		return NT_STATUS_ALLOCATE_BUCKET;
	else if ((bit = *blkid % sizeof(size_t)) == 0)
		return __ntapi_blt_acquire(blitter,blkid);

	for (n=0,mask=(uintptr_t)-1; n<bit; n++)
		mask ^= ((size_t)1 << n);

	i = *blkid / (8*sizeof(size_t));

	for (n=0; blitter->info.blocks_avail && (n < blitter->params.round_trips); n++) {
		if (!(at_bsf(&bit,(mask & blitter->bitmap[i]))))
			break;

		switch (__blt_bitbite(blitter,bit,i)) {
			case NT_STATUS_SUCCESS:
				*blkid = bit + (i * 8 * sizeof(size_t));
				return NT_STATUS_SUCCESS;

			case NT_STATUS_DEVICE_BUSY:
				return NT_STATUS_DEVICE_BUSY;

			default:
				break;
		}
	}

	*blkid = ++i * 8 * sizeof(size_t);
	return __blt_acquire(blitter,blkid);
}


int32_t __fastcall __ntapi_blt_possess(
	__in	nt_blitter *	blitter,
	__out	intptr_t *	blkid)
{
	int		bit;
	size_t		byte;
	uintptr_t 	test;
	uintptr_t	mask;

	bit  = *blkid % (8*sizeof(size_t));
	byte = *blkid / (8*sizeof(size_t));

	mask = ((uintptr_t)1 << bit);
	test  = at_locked_and(
		(intptr_t *)&blitter->bitmap[byte],
		~mask);

	if (test & mask) {
		at_locked_dec(&blitter->info.blocks_avail);
		at_locked_inc(&blitter->info.blocks_used);
	}

	return NT_STATUS_SUCCESS;
}


int32_t __fastcall __ntapi_blt_acquire(
	__in	nt_blitter *	blitter,
	__out	intptr_t *	blkid)
{
	*blkid = 0;
	return __blt_acquire(blitter,blkid);
}


int32_t __fastcall	__ntapi_blt_release(
	__in	nt_blitter *	blitter,
	__out	intptr_t	blkid)
{
	size_t		i;
	unsigned int	idx;
	uintptr_t	bit;

	i   = blkid / (8 * sizeof(uintptr_t));
	idx = blkid % (8 * sizeof(uintptr_t));
	bit = ((uintptr_t)1 << idx);

	at_locked_or((intptr_t *)&blitter->bitmap[i],bit);
	at_locked_dec(&blitter->info.blocks_used);
	at_locked_inc(&blitter->info.blocks_avail);

	return NT_STATUS_SUCCESS;
}


void * __fastcall __ntapi_blt_get(
	__in	const nt_blitter *	blitter,
	__in	intptr_t		block_id)
{
	size_t * addr = (size_t *)blitter->info.region_addr;
	addr += block_id;
	return addr;
}


void __fastcall __ntapi_blt_set(
	__in	const nt_blitter *	blitter,
	__in	intptr_t		block_id,
	__in	void *			val)
{
	size_t * addr = (size_t *)blitter->info.region_addr;
	addr += block_id;
	*addr = (size_t)val;
	return;
}
