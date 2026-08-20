#ifndef _PSX_TIME_H_
#define _PSX_TIME_H_

#include <ntapi/ntapi.h>
#include "psx_systypes.h"


/*************************************************************************
 *                                                               *
 * Weltende / Jakob van Hoddis, 1911                            *
 *                                                             *
 * Dem Bürger fliegt vom spitzen Kopf der Hut,                *
 * In allen Lüften hallt es wie Geschrei,                    *
 * Dachdecker stürzen ab und gehn entzwei                   *
 * Und an den Küsten – liest man – steigt die Flut.        *
 *                                                        *
 * Der Sturm ist da, die wilden Meere hupfen             *
 * An Land, um dicke Dämme zu zerdrücken.               *
 * Die meisten Menschen haben einen Schnupfen.         *
 * Die Eisenbahnen fallen von den Brücken.            *
 *                                                   *
 ****************************************************/

/**
 *  native file time is a 64-bit integer representing the
 *  number of 100-nanosecond intervals since 1/1/1601.
 *
 *  unix time (time_t) stores the number of seconds
 *  since 1/1/1970, commonly known as the epoch.
 *
 *  number of leap years between 1601 and 1970:
 *  369 / 4 - 3 = 89
 *  (1700, 1800 and 1900 are divisble by 100 but not by 400)
 *
 *  number of days between 1/1/1601 and 1970:
 *  369 * 365 + 89 = 134774
 *
 *  number of nanoseconds between 1/1/1601 and 1970:
 *  134774 * 24 * 60 * 60 * 1000000000 = 11644473600000000000
*/

static __inline__ void __psx_time_convert_unix_to_native(time_t unix, nt_filetime * native)
{
	native->quad = unix * 10000000LL + 116444736000000000;
}

static __inline__ void __psx_time_convert_native_to_unix(nt_filetime native, time_t * unix)
{
	*unix = (native.quad - 116444736000000000) / 10000000LL;
}

static __inline__ void __psx_time_convert_timespec_to_native(struct timespec ts, nt_filetime * native)
{
	native->quad = ts.tv_sec * 10000000LL + (ts.tv_nsec/100) + 116444736000000000;
}

static __inline__ void __psx_time_convert_native_to_timespec(nt_filetime native, struct timespec * ts)
{
	ts->tv_sec  = (native.quad - 116444736000000000) / 10000000LL;
	ts->tv_nsec = (native.quad - 116444736000000000) % 10000000LL;
	ts->tv_nsec *= 100;
}

static __inline__ void __psx_time_convert_timeval_to_native(struct timeval tv, nt_filetime * native)
{
	native->quad = tv.tv_sec * 10000000LL + (tv.tv_usec*10) + 116444736000000000;
}

static __inline__ void __psx_time_convert_native_to_timeval(nt_filetime native, struct timeval * tv)
{
	tv->tv_sec  = (native.quad - 116444736000000000) / 10000000LL;
	tv->tv_usec = (native.quad - 116444736000000000) % 10000000LL;
	tv->tv_usec /= 10;
}

static __inline__ void __psx_time_convert_itimerval_to_native(struct itimerval timer, nt_itimerval * native)
{
	native->interval.quad = (-1) * (timer.it_interval.tv_sec * 10000000LL + (timer.it_interval.tv_usec*10));
	native->value.quad = (-1) * (timer.it_value.tv_sec * 10000000LL + (timer.it_value.tv_usec*10));
}

static __inline__ void __psx_time_convert_timeval_to_period(struct timeval tv, int32_t * period)
{
	*period = (int32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

static __inline__ void __psx_time_convert_native_to_itimerval(nt_itimerval native, struct itimerval * timer)
{
}

static __inline__ int __psx_are_timeval_structs_equal(struct timeval * a, struct timeval * b)
{
	return ((a->tv_sec==b->tv_sec) && (a->tv_usec==b->tv_usec));
}

static __inline__ int __psx_are_itimerval_structs_equal(struct itimerval * a, struct itimerval * b)
{
	return (__psx_are_timeval_structs_equal(&a->it_interval,&b->it_interval)
		&& __psx_are_timeval_structs_equal(&a->it_value,&b->it_value));
}

static __inline__ int __psx_is_timer_active(struct itimerval * timer)
{
	return (timer->it_interval.tv_sec || timer->it_interval.tv_usec
		|| timer->it_value.tv_sec || timer->it_value.tv_usec);
}

static __inline__ int __psx_is_timer_disabled(struct itimerval * timer)
{
	return !(__psx_is_timer_active(timer));
}

#endif
