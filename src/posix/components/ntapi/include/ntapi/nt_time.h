#ifndef _NT_TIME_H_
#define _NT_TIME_H_

#include <psxtypes/psxtypes.h>
#include "nt_object.h"

typedef struct _nt_itimerval {
	nt_timeout	interval;
	nt_timeout	value;
} nt_itimerval;

typedef int32_t __stdcall ntapi_zw_query_system_time(
	__out	nt_large_integer *	current_time);


typedef int32_t __stdcall ntapi_zw_set_system_time(
	__in	nt_large_integer *	new_time,
	__out	nt_large_integer *	old_time	__optional);


typedef int32_t __stdcall ntapi_zw_query_performance_counter(
	__out	nt_large_integer *	performance_count,
	__out	nt_large_integer *	performance_frequency	__optional);


typedef int32_t __stdcall ntapi_zw_set_timer_resolution(
	__in	uint32_t	requested_resolution,
	__in	int32_t		set,
	__out	uint32_t *	actual_resolution);


typedef int32_t __stdcall ntapi_zw_query_timer_resolution(
	__out	uint32_t *	coarsest_resolution,
	__out	uint32_t *	finest_resolution,
	__out	uint32_t *	actual_resolution);


typedef int32_t __stdcall ntapi_zw_delay_execution(
	__in	int32_t *		alertable,
	__in	nt_large_integer *	interval);

typedef int32_t __stdcall ntapi_zw_yield_execution(void);


#endif
