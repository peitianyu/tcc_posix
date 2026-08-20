#ifndef _NT_PROFILING_H_
#define _NT_PROFILING_H_

#include <psxtypes/psxtypes.h>
#include "nt_object.h"

typedef enum _nt_kprofile_source {
	NT_PROFILE_TIME
} nt_kprofile_source;


typedef int32_t __stdcall ntapi_zw_create_profile(
	__out	void **			hprofile,
	__in	void *			hprocess,
	__in	void *			base,
	__in	size_t			size,
	__in	uint32_t		bucket_shift,
	__in	uint32_t *		buffer,
	__in	size_t			buffer_length,
	__in	nt_kprofile_source	source,
	__in	uint32_t		process_mask);


typedef int32_t __stdcall ntapi_zw_set_interval_profile(
	__in	uint32_t		interval,
	__in	nt_kprofile_source	source);


typedef int32_t __stdcall ntapi_zw_query_interval_profile(
	__in	nt_kprofile_source	source,
	__out	uint32_t *		interval);


typedef int32_t __stdcall ntapi_zw_start_profile(
	__in	void *		hprofile);


typedef int32_t __stdcall ntapi_zw_stop_profile(
	__in	void *		hprofile);

#endif
