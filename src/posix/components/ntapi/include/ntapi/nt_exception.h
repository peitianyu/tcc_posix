#ifndef _NT_EXCEPTION_H_
#define _NT_EXCEPTION_H_

#include <psxtypes/psxtypes.h>
#include "nt_object.h"
#include "nt_thread.h"

/* limits */
#define NT_EXCEPTION_MAX_PARAMS		(0x0F)

typedef struct _nt_exception_record {
	uint32_t			exception_code;
	uint32_t			exception_flags;
	struct _nt_exception_record *	exception_record;
	void *				exception_address;
	uint32_t			number_of_params;
	uintptr_t			exception_information[NT_EXCEPTION_MAX_PARAMS];
} nt_exception_record;

typedef int32_t __stdcall ntapi_zw_raise_exception(
	__in	nt_exception_record *	exception_record,
	__in	nt_thread_context *	context,
	__in	unsigned char		search_frames);

typedef int32_t __stdcall ntapi_zw_continue(
	__in	nt_thread_context *	context,
	__in	unsigned char		test_alert);

#endif
