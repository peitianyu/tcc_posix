#include <psxtypes/psxtypes.h>

long		_InterlockedIncrement(int32_t volatile * ptr);
int64_t		_InterlockedIncrement64(int64_t volatile * ptr);
long		_InterlockedDecrement(int32_t volatile * ptr);
int64_t		_InterlockedDecrement64(int64_t volatile * ptr);
long		_InterlockedExchangeAdd(int32_t volatile * ptr, int32_t val);
int64_t		_InterlockedExchangeAdd64(int64_t volatile * ptr, int64_t val);
long		_InterlockedCompareExchange(int32_t volatile * dst, int32_t xchg, int32_t cmp);
int64_t		_InterlockedCompareExchange64(int64_t volatile * dst, int64_t xchg, int64_t cmp);
long		_InterlockedAnd(int32_t volatile * dst, int32_t mask);
int64_t		_InterlockedAnd64(int64_t volatile * dst, int64_t mask);
long		_InterlockedOr(int32_t volatile * dst, int32_t mask);
int64_t		_InterlockedOr64(int64_t volatile * dst, int64_t mask);
long		_InterlockedXor(int32_t volatile * dst, int32_t mask);
int64_t		_InterlockedXor64(int64_t volatile * dst, int64_t mask);
uint16_t	__popcnt16(uint16_t mask);
uint32_t	__popcnt(uint32_t mask);
uint64_t	__popcnt64(uint64_t mask);
void		_ReadWriteBarrier(void);
unsigned char	_BitScanForward64(unsigned int * index, uintptr_t mask);
unsigned char	_BitScanReverse64(unsigned int * index, uintptr_t mask);

static __inline__ void at_locked_inc(
	intptr_t volatile * ptr)
{
	_InterlockedIncrement64(ptr);
	return;
}


static __inline__ void at_locked_inc_32(
	int32_t volatile * ptr)
{
	_InterlockedIncrement(ptr);
	return;
}


static __inline__ void at_locked_inc_64(
	int64_t volatile * ptr)
{
	_InterlockedIncrement64(ptr);
	return;
}


static __inline__ void at_locked_dec(
	intptr_t volatile * ptr)
{
	_InterlockedDecrement64(ptr);
	return;
}


static __inline__ void at_locked_dec_32(
	int32_t volatile * ptr)
{
	_InterlockedDecrement(ptr);
	return;
}


static __inline__ void at_locked_dec_64(
	int64_t volatile * ptr)
{
	_InterlockedDecrement64(ptr);
	return;
}


static __inline__ void at_locked_add(
	intptr_t volatile *	ptr,
	intptr_t		val)
{
	_InterlockedExchangeAdd64(ptr, val);
	return;
}


static __inline__ void at_locked_add_32(
	int32_t volatile *	ptr,
	int32_t			val)
{
	_InterlockedExchangeAdd(ptr, val);
	return;
}


static __inline__ void at_locked_add_64(
	int64_t volatile *	ptr,
	int64_t			val)
{
	_InterlockedExchangeAdd64(ptr, val);
	return;
}


static __inline__ void at_locked_sub(
	intptr_t volatile *	ptr,
	intptr_t		val)
{
	_InterlockedExchangeAdd64(ptr, -val);
	return;
}


static __inline__ void at_locked_sub_32(
	int32_t volatile *	ptr,
	int32_t			val)
{
	_InterlockedExchangeAdd(ptr, -val);
	return;
}


static __inline__ void at_locked_sub_64(
	int64_t volatile *	ptr,
	int64_t			val)
{
	_InterlockedExchangeAdd64(ptr, -val);
	return;
}


static __inline__ intptr_t at_locked_xadd(
	intptr_t volatile *	ptr,
	intptr_t		val)
{
	return _InterlockedExchangeAdd64(ptr, val);
}


static __inline__ int32_t at_locked_xadd_32(
	int32_t volatile *	ptr,
	int32_t			val)
{
	return _InterlockedExchangeAdd(ptr, val);
}


static __inline__ int64_t at_locked_xadd_64(
	int64_t volatile *	ptr,
	int64_t			val)
{
	return _InterlockedExchangeAdd64(ptr, val);
}


static __inline__ intptr_t at_locked_xsub(
	intptr_t volatile *	ptr,
	intptr_t		val)
{
	return _InterlockedExchangeAdd64(ptr, -val);
}


static __inline__ int32_t at_locked_xsub_32(
	int32_t volatile *	ptr,
	int32_t			val)
{
	return _InterlockedExchangeAdd(ptr, -val);
}


static __inline__ int64_t at_locked_xsub_64(
	int64_t volatile *	ptr,
	int64_t			val)
{
	return _InterlockedExchangeAdd64(ptr, -val);
}


static __inline__ intptr_t at_locked_cas(
	intptr_t volatile *	dst,
	intptr_t		cmp,
	intptr_t		xchg)
{
	return _InterlockedCompareExchange64(dst,xchg,cmp);
}


static __inline__ int32_t at_locked_cas_32(
	int32_t volatile *	dst,
	int32_t			cmp,
	int32_t			xchg)
{
	return _InterlockedCompareExchange(dst,xchg,cmp);
}


static __inline__ int64_t at_locked_cas_64(
	int64_t volatile *	dst,
	int64_t			cmp,
	int64_t			xchg)
{
	return _InterlockedCompareExchange64(dst,xchg,cmp);
}


static __inline__ intptr_t at_locked_and(
	intptr_t volatile *	dst,
	intptr_t		mask)
{
	return _InterlockedAnd64(dst,mask);
}


static __inline__ int32_t at_locked_and_32(
	int32_t volatile *	dst,
	int32_t			mask)
{
	return _InterlockedAnd(dst,mask);
}


static __inline__ int64_t at_locked_and_64(
	int64_t volatile *	dst,
	int64_t			mask)
{
	return _InterlockedAnd64(dst,mask);
}


static __inline__ intptr_t at_locked_or(
	intptr_t volatile *	dst,
	intptr_t		mask)
{
	return _InterlockedOr64(dst,mask);
}


static __inline__ int32_t at_locked_or_32(
	int32_t volatile *	dst,
	int32_t			mask)
{
	return _InterlockedOr(dst,mask);
}


static __inline__ int64_t at_locked_or_64(
	int64_t volatile *	dst,
	int64_t			mask)
{
	return _InterlockedOr64(dst,mask);
}


static __inline__ intptr_t at_locked_xor(
	intptr_t volatile *	dst,
	intptr_t		mask)
{
	return _InterlockedXor64(dst,mask);
}


static __inline__ int32_t at_locked_xor_32(
	int32_t volatile *	dst,
	int32_t			mask)
{
	return _InterlockedXor(dst,mask);
}


static __inline__ int64_t at_locked_xor_64(
	int64_t volatile *	dst,
	int64_t			mask)
{
	return _InterlockedXor64(dst,mask);
}


static __inline__ void at_store(
	volatile intptr_t *	dst,
	intptr_t		val)
{
	_ReadWriteBarrier();
	*dst = val;
	_ReadWriteBarrier();

	return;
}


static __inline__ void at_store_32(
	volatile int32_t *	dst,
	int32_t			val)
{
	_ReadWriteBarrier();
	*dst = val;
	_ReadWriteBarrier();

	return;
}


static __inline__ void at_store_64(
	volatile int64_t *	dst,
	int64_t			val)
{
	_ReadWriteBarrier();
	*dst = val;
	_ReadWriteBarrier();

	return;
}


static __inline__ int at_bsf(
	unsigned int *		index,
	uintptr_t		mask)
{
	return (int)_BitScanForward64(index,mask);
}


static __inline__ int at_bsr(
	unsigned int *		index,
	uintptr_t		mask)
{
	return (int)_BitScanReverse64(index,mask);
}


static __inline__ size_t at_popcount(
	uintptr_t		mask)
{
	return __popcnt64(mask);
}


static __inline__ size_t at_popcount_16(
	uint16_t		mask)
{
	return __popcnt16(mask);
}


static __inline__ size_t at_popcount_32(
	uint32_t		mask)
{
	return __popcnt(mask);
}


static __inline__ size_t at_popcount_64(
	uint64_t		mask)
{
	return __popcnt64(mask);
}
