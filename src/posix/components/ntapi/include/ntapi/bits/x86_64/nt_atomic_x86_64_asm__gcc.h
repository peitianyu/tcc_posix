#include <psxtypes/psxtypes.h>

static __inline__ void at_locked_inc(
	intptr_t volatile * ptr)
{
	__asm__(
		"lock;"
		"incq %0"
		: "=m" (*ptr)
		: "m"  (*ptr)
		: "memory");
}


static __inline__ void at_locked_inc_32(
	int32_t volatile * ptr)
{
	__asm__(
		"lock;"
		"incl %0"
		: "=m" (*ptr)
		: "m"  (*ptr)
		: "memory");
}


static __inline__ void at_locked_inc_64(
	int64_t volatile * ptr)
{
	__asm__(
		"lock;"
		"incq %0"
		: "=m" (*ptr)
		: "m"  (*ptr)
		: "memory");
}


static __inline__ void at_locked_dec(
	intptr_t volatile * ptr)
{
	__asm__(
		"lock;"
		"decq %0"
		: "=m" (*ptr)
		: "m"  (*ptr)
		: "memory");
}


static __inline__ void at_locked_dec_32(
	int32_t volatile * ptr)
{
	__asm__(
		"lock;"
		"decl %0"
		: "=m" (*ptr)
		: "m"  (*ptr)
		: "memory");
}


static __inline__ void at_locked_dec_64(
	int64_t volatile * ptr)
{
	__asm__(
		"lock;"
		"decq %0"
		: "=m" (*ptr)
		: "m"  (*ptr)
		: "memory");
}


static __inline__ void at_locked_add(
	intptr_t volatile *	ptr,
	intptr_t		val)
{
	__asm__(
		"lock;"
		"xaddq %1, %0"
		: "=m" (*ptr), "=r" (val)
		: "1"  (val)
		: "memory");
}


static __inline__ void at_locked_add_32(
	int32_t volatile *	ptr,
	int32_t			val)
{
	__asm__(
		"lock;"
		"xaddl %1, %0"
		: "=m" (*ptr), "=r" (val)
		: "1"  (val)
		: "memory");
}


static __inline__ void at_locked_add_64(
	int64_t volatile *	ptr,
	int64_t			val)
{
	__asm__(
		"lock;"
		"xaddq %1, %0"
		: "=m" (*ptr), "=r" (val)
		: "1"  (val)
		: "memory");
}


static __inline__ void at_locked_sub(
	intptr_t volatile *	ptr,
	intptr_t		val)
{
	val = -val;

	__asm__(
		"lock;"
		"xaddq %1, %0"
		: "=m" (*ptr), "=r" (val)
		: "1"  (val)
		: "memory");
}


static __inline__ void at_locked_sub_32(
	int32_t volatile *	ptr,
	int32_t			val)
{
	val = -val;

	__asm__(
		"lock;"
		"xaddl %1, %0"
		: "=m" (*ptr), "=r" (val)
		: "1"  (val)
		: "memory");
}


static __inline__ void at_locked_sub_64(
	int64_t volatile *	ptr,
	int64_t			val)
{
	val = -val;

	__asm__(
		"lock;"
		"xaddq %1, %0"
		: "=m" (*ptr), "=r" (val)
		: "1"  (val)
		: "memory");
}


static __inline__ intptr_t at_locked_xadd(
	intptr_t volatile *	ptr,
	intptr_t		val)
{
	__asm__(
		"lock;"
		"xaddq %1, %0"
		: "=m" (*ptr), "=r" (val)
		: "1"  (val)
		: "memory");
	return val;
}


static __inline__ int32_t at_locked_xadd_32(
	int32_t volatile *	ptr,
	int32_t			val)
{
	__asm__(
		"lock;"
		"xaddl %1, %0"
		: "=m" (*ptr), "=r" (val)
		: "1"  (val)
		: "memory");
	return val;
}


static __inline__ int64_t at_locked_xadd_64(
	int64_t volatile *	ptr,
	int64_t			val)
{
	__asm__(
		"lock;"
		"xaddq %1, %0"
		: "=m" (*ptr), "=r" (val)
		: "1"  (val)
		: "memory");
	return val;
}


static __inline__ intptr_t at_locked_xsub(
	intptr_t volatile *	ptr,
	intptr_t		val)
{
	val = -val;

	__asm__(
		"lock;"
		"xaddq %1, %0"
		: "=m" (*ptr), "=r" (val)
		: "1"  (val)
		: "memory");
	return val;
}


static __inline__ int32_t at_locked_xsub_32(
	int32_t volatile *	ptr,
	int32_t			val)
{
	val = -val;

	__asm__(
		"lock;"
		"xaddl %1, %0"
		: "=m" (*ptr), "=r" (val)
		: "1"  (val)
		: "memory");
	return val;
}


static __inline__ int64_t at_locked_xsub_64(
	int64_t volatile *	ptr,
	int64_t			val)
{
	val = -val;

	__asm__(
		"lock;"
		"xaddq %1, %0"
		: "=m" (*ptr), "=r" (val)
		: "1"  (val)
		: "memory");
	return val;
}


static __inline__ intptr_t at_locked_cas(
	intptr_t volatile * 	dst,
	intptr_t		cmp,
	intptr_t		xchg)
{
	intptr_t ret;

	__asm__(
		"lock;"
		"cmpxchgq %3, %0"
		: "=m" (*dst), "=a" (ret)
		: "a"  (cmp),  "r"  (xchg)
		: "memory");

	return ret;
}


static __inline__ int32_t at_locked_cas_32(
	int32_t volatile * 	dst,
	int32_t			cmp,
	int32_t			xchg)
{
	int32_t ret;

	__asm__(
		"lock;"
		"cmpxchg %3, %0"
		: "=m" (*dst), "=a" (ret)
		: "a"  (cmp),  "r"  (xchg)
		: "memory");

	return ret;
}


static __inline__ int64_t at_locked_cas_64(
	int64_t volatile * 	dst,
	int64_t			cmp,
	int64_t			xchg)
{
	int64_t ret;

	__asm__(
		"lock;"
		"cmpxchgq %3, %0"
		: "=m" (*dst), "=a" (ret)
		: "a"  (cmp),  "r"  (xchg)
		: "memory");

	return ret;
}


static __inline__ intptr_t at_locked_and(
	intptr_t volatile *	dst,
	intptr_t		mask)
{
	intptr_t ret;

	__asm__(
		"lock;"
		"andq %1, %0"
		: "=m" (*dst), "=a" (ret)
		: "r"  (mask)
		: "memory");

	return ret;
}


static __inline__ int32_t at_locked_and_32(
	int32_t volatile *	dst,
	int32_t			mask)
{
	int32_t ret;

	__asm__(
		"lock;"
		"andl %1, %0"
		: "=m" (*dst), "=a" (ret)
		: "r"  (mask)
		: "memory");

	return ret;
}


static __inline__ int64_t at_locked_and_64(
	int64_t volatile *	dst,
	int64_t			mask)
{
	int64_t ret;

	__asm__(
		"lock;"
		"andq %1, %0"
		: "=m" (*dst), "=a" (ret)
		: "r"  (mask)
		: "memory");

	return ret;
}


static __inline__ intptr_t at_locked_or(
	intptr_t volatile *	dst,
	intptr_t		mask)
{
	intptr_t ret;

	__asm__(
		"lock;"
		"orq %1, %0"
		: "=m" (*dst), "=a" (ret)
		: "r"  (mask)
		: "memory");

	return ret;
}


static __inline__ int32_t at_locked_or_32(
	int32_t volatile *	dst,
	int32_t			mask)
{
	int32_t ret;

	__asm__(
		"lock;"
		"orl %1, %0"
		: "=m" (*dst), "=a" (ret)
		: "r"  (mask)
		: "memory");

	return ret;
}


static __inline__ int64_t at_locked_or_64(
	int64_t volatile *	dst,
	int64_t			mask)
{
	int64_t ret;

	__asm__(
		"lock;"
		"orq %1, %0"
		: "=m" (*dst), "=a" (ret)
		: "r"  (mask)
		: "memory");

	return ret;
}


static __inline__ intptr_t at_locked_xor(
	intptr_t volatile *	dst,
	intptr_t		mask)
{
	intptr_t ret;

	__asm__(
		"lock;"
		"xorq %1, %0"
		: "=m" (*dst), "=a" (ret)
		: "r"  (mask)
		: "memxory");

	return ret;
}


static __inline__ int32_t at_locked_xor_32(
	int32_t volatile *	dst,
	int32_t			mask)
{
	int32_t ret;

	__asm__(
		"lock;"
		"xorl %1, %0"
		: "=m" (*dst), "=a" (ret)
		: "r"  (mask)
		: "memxory");

	return ret;
}


static __inline__ int64_t at_locked_xor_64(
	int64_t volatile *	dst,
	int64_t			mask)
{
	int64_t ret;

	__asm__(
		"lock;"
		"xorq %1, %0"
		: "=m" (*dst), "=a" (ret)
		: "r"  (mask)
		: "memxory");

	return ret;
}


static __inline__ void at_store(
	volatile intptr_t *	dst,
	intptr_t		val)
{
	__asm__(
		"mov %1, %0"
		: "=m" (*dst)
		: "r"  (val)
		: "memory");
}


static __inline__ void at_store_32(
	volatile int32_t *	dst,
	int32_t			val)
{
	__asm__(
		"mov %1, %0"
		: "=m" (*dst)
		: "r"  (val)
		: "memory");
}


static __inline__ void at_store_64(
	volatile int64_t *	dst,
	int64_t			val)
{
	__asm__(
		"mov %1, %0"
		: "=m" (*dst)
		: "r"  (val)
		: "memory");
}


static __inline__ int at_bsf(
	unsigned int *		index,
	uintptr_t		mask)
{
	if (mask) {
		__asm__(
			"bsf %1, %0"
			: "=r" (mask)
			: "r" (mask));

		*index = (int)mask;
		return 1;
	} else
		return 0;
}


static __inline__ int at_bsr(
	unsigned int *		index,
	uintptr_t		mask)
{
	if (mask) {
		__asm__(
			"bsr %1, %0"
			: "=r" (mask)
			: "r" (mask));

		*index = (int)mask;
		return 1;
	} else
		return 0;
}


static __inline__ size_t at_popcount(
	uintptr_t		mask)
{
	__asm__(
		"popcntq %0, %0"
		: "=r" (mask)
		: "0"  (mask)
		: "memory");
	return mask;
}


static __inline__ size_t at_popcount_16(
	uint16_t		mask)
{
	__asm__(
		"popcnt %0, %0"
		: "=r" (mask)
		: "0"  (mask)
		: "memory");
	return mask;
}


static __inline__ size_t at_popcount_32(
	uint32_t		mask)
{
	__asm__(
		"popcnt %0, %0"
		: "=r" (mask)
		: "0"  (mask)
		: "memory");
	return mask;
}


static __inline__ size_t at_popcount_64(
	uint64_t		mask)
{
	__asm__(
		"popcntq %0, %0"
		: "=r" (mask)
		: "0"  (mask)
		: "memory");
	return mask;
}
