#ifndef _NT_DEBUG_H_
#define _NT_DEBUG_H_

#include <psxtypes/psxtypes.h>
#include "nt_file.h"

typedef ssize_t __cdecl ntapi_dbg_write(
	__in	void *		hfile,
	__in	const void *	buf,
	__in	size_t		bytes);


typedef int32_t __cdecl ntapi_dbg_fn_call(
	__in	void *			hfile		__optional,
	__in	char *			fn_caller_name,
	__in	void *			fn_callee_addr,
	__in	uintptr_t		fn_ret,
	__in	ntapi_dbg_write*	pfn_dbg_write	__optional,
	__in	char *			source		__optional,
	__in	int			line		__optional);


typedef int32_t __cdecl ntapi_dbg_msg(
	__in	void *			hfile		__optional,
	__in	char *			source		__optional,
	__in	int			line		__optional,
	__in	char *			fn_caller_name,
	__in	char *			fmt,
	__in	uintptr_t		arg1,
	__in	uintptr_t		arg2,
	__in	uintptr_t		arg3,
	__in	uintptr_t		arg4,
	__in	uintptr_t		arg5,
	__in	uintptr_t		arg6,
	__in	ntapi_dbg_write*	pfn_dbg_write	__optional);

#endif
