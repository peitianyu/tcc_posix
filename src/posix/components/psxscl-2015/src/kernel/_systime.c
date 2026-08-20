/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_tlca.h"
#include "psx_errno.h"
#include "psx_time.h"
#include "psx.h"

/* easy, low-priority */

__psx_api
intptr_t __sys_clock_getres(clockid_t clock_id, struct timespec * res)
{
	return -ENOSYS;
}

__psx_api
intptr_t __sys_clock_gettime(clockid_t clock_id, struct timespec * tp)
{
	int32_t			status;
	struct __psx_tlca *	tlca;
	nt_large_integer		systime;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if ((status = __ntapi->zw_query_system_time(&systime)))
		return __psx_sig_epilog(tlca,-EINVAL,status);

	__psx_time_convert_native_to_timespec(systime,tp);

	return __psx_sig_epilog(tlca,0,status);
}

__psx_api
intptr_t __sys_clock_settime(clockid_t clock_id, const struct timespec * tp)
{
	return -ENOSYS;
}

__psx_api
intptr_t __sys_gettimeofday(struct timeval * tp, void * tzp)
{
	int32_t			status;
	struct __psx_tlca *	tlca;
	nt_large_integer		systime;

	(void)tzp;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if ((status = __ntapi->zw_query_system_time(&systime)))
		return __psx_sig_epilog(tlca,-EINVAL,status);

	__psx_time_convert_native_to_timeval(systime,tp);

	return __psx_sig_epilog(tlca,0,status);
}


__psx_api
intptr_t __sys_nanosleep(const struct timespec * req, struct timespec * rem)
{
	int32_t			status;
	struct __psx_tlca *	tlca;
	nt_large_integer		delay;

	(void)rem;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	delay.quad = (int64_t)req->tv_sec * 10000000LL + (req->tv_nsec + 99) / 100;

	if ((status = __ntapi->zw_delay_execution(0,&delay)))
		return __psx_sig_epilog(tlca,-EINTR,status);

	return __psx_sig_epilog(tlca,0,status);
}

__psx_api
intptr_t __sys_time(int * tloc)
{
	struct timeval tv;
	intptr_t ret;

	ret = __sys_gettimeofday(&tv,0);
	if (tloc)
		*tloc = (int)tv.tv_sec;
	return ret ? ret : (intptr_t)tv.tv_sec;
}

__psx_api
intptr_t __sys_clock_nanosleep(int clockid, int flags, const struct timespec * req, struct timespec * rem)
{
	/* musl nanosleep 走 SYS_clock_nanosleep (230) */
	(void)clockid;
	(void)flags;
	return __sys_nanosleep(req,rem);
}
