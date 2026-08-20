#ifndef _NT_ATOMIC_H_
#define _NT_ATOMIC_H_

#include <psxtypes/psxtypes.h>

static __inline__ void at_locked_inc(
	intptr_t volatile * ptr);

static __inline__ void at_locked_inc_32(
	int32_t volatile * ptr);

static __inline__ void at_locked_inc_64(
	int64_t volatile * ptr);

static __inline__ void at_locked_dec(
	intptr_t volatile * ptr);

static __inline__ void at_locked_dec_32(
	int32_t volatile * ptr);

static __inline__ void at_locked_dec_64(
	int64_t volatile * ptr);

static __inline__ void at_locked_add(
	intptr_t volatile *	ptr,
	intptr_t		val);

static __inline__ void at_locked_add_32(
	int32_t volatile *	ptr,
	int32_t			val);

static __inline__ void at_locked_add_64(
	int64_t volatile *	ptr,
	int64_t			val);

static __inline__ void at_locked_sub(
	intptr_t volatile *	ptr,
	intptr_t		val);

static __inline__ void at_locked_sub_32(
	int32_t volatile *	ptr,
	int32_t		val);

static __inline__ void at_locked_sub_64(
	int64_t volatile *	ptr,
	int64_t		val);

static __inline__ intptr_t at_locked_xadd(
	intptr_t volatile *	ptr,
	intptr_t		val);

static __inline__ int32_t at_locked_xadd_32(
	int32_t volatile *	ptr,
	int32_t			val);

static __inline__ int64_t at_locked_xadd_64(
	int64_t volatile *	ptr,
	int64_t			val);

static __inline__ intptr_t at_locked_xsub(
	intptr_t volatile *	ptr,
	intptr_t		val);

static __inline__ int32_t at_locked_xsub_32(
	int32_t volatile *	ptr,
	int32_t			val);

static __inline__ int64_t at_locked_xsub_64(
	int64_t volatile *	ptr,
	int64_t			val);

static __inline__ intptr_t at_locked_cas(
	intptr_t volatile *	dst,
	intptr_t		cmp,
	intptr_t		xchg);

static __inline__ int32_t at_locked_cas_32(
	int32_t volatile *	dst,
	int32_t			cmp,
	int32_t			xchg);

static __inline__ int64_t at_locked_cas_64(
	int64_t volatile *	dst,
	int64_t			cmp,
	int64_t			xchg);

static __inline__ intptr_t at_locked_and(
	intptr_t volatile *	dst,
	intptr_t		mask);


static __inline__ int32_t at_locked_and_32(
	int32_t volatile *	dst,
	int32_t			mask);


static __inline__ int64_t at_locked_and_64(
	int64_t volatile *	dst,
	int64_t			mask);


static __inline__ intptr_t at_locked_or(
	intptr_t volatile *	dst,
	intptr_t		mask);


static __inline__ int32_t at_locked_or_32(
	int32_t volatile *	dst,
	int32_t			mask);


static __inline__ int64_t at_locked_or_64(
	int64_t volatile *	dst,
	int64_t			mask);


static __inline__ intptr_t at_locked_xor(
	intptr_t volatile *	dst,
	intptr_t		mask);


static __inline__ int32_t at_locked_xor_32(
	int32_t volatile *	dst,
	int32_t			mask);


static __inline__ int64_t at_locked_xor_64(
	int64_t volatile *	dst,
	int64_t			mask);

static __inline__ void at_store(
	volatile intptr_t *	dst,
	intptr_t		val);

static __inline__ int at_bsf(
	unsigned int *		index,
	uintptr_t		mask);

static __inline__ int at_bsr(
	unsigned int *		index,
	uintptr_t		mask);

static __inline__ size_t at_popcount(
	uintptr_t		mask);

static __inline__ size_t at_popcount_16(
	uint16_t		mask);

static __inline__ size_t at_popcount_32(
	uint32_t		mask);

static __inline__ size_t at_popcount_64(
	uint64_t		mask);

#include "bits/nt_atomic_inline_asm.h"

#endif
