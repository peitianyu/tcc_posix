#ifndef _NT_LDR_H_
#define _NT_LDR_H_

#include <psxtypes/psxtypes.h>
#include <dalist/dalist.h>
#include <ntapi/nt_object.h>

typedef int32_t	__stdcall	ntapi_ldr_load_dll(
	__in	wchar16_t *		image_path	__optional,
	__in	uint32_t *		image_flags	__optional,
	__in	nt_unicode_string *	image_name,
	__out	void **			image_base);


typedef int32_t	__stdcall	ntapi_ldr_unload_dll(
	__in	void *	image_base);


/* extensions */
typedef int32_t	__stdcall	ntapi_ldr_load_system_dll(
	__in	void *			hsysdir		__optional,
	__in	wchar16_t *		base_name,
	__in	uint32_t		base_name_size,
	__in	uint32_t *		image_flags	__optional,
	__out	void **			image_base);

typedef int	__cdecl ntapi_ldr_create_state_snapshot(
	__out	struct dalist_ex *	ldr_state_snapshot);

typedef int	__cdecl ntapi_ldr_revert_state_to_snapshot(
	__in	struct dalist_ex *	ldr_state_snapshot);

#endif
