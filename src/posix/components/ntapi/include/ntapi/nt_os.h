#ifndef _NT_OS_H_
#define _NT_OS_H_

#include <psxtypes/psxtypes.h>
#include "nt_object.h"


typedef enum _nt_hard_error_response_option {
	NT_OPTION_ABORT_RETRY_IGNORE,
	NT_OPTION_OK_,
	NT_OPTION_OK_CANCEL,
	NT_OPTION_RETRY_CANCEL,
	NT_OPTION_YES_NO,
	NT_OPTION_YES_NO_CANCEL,
	NT_OPTION_SHUTDOWN_SYSTEM
} nt_hard_error_response_option;


typedef enum _nt_hard_error_response {
	NT_RESPONSE_RETURN_TO_CALLER,
	NT_RESPONSE_NOT_HANDLED,
	NT_RESPONSE_ABORT,
	NT_RESPONSE_CANCEL,
	NT_RESPONSE_IGNORE,
	NT_RESPONSE_NO,
	NT_RESPONSE_OK,
	NT_RESPONSE_RETRY,
	NT_RESPONSE_YES
} nt_hard_error_response;


typedef struct _nt_ldt_entry {
	int32_t	limit_low;
	int32_t	base_low;

	union {
		struct {
			unsigned char	base_mid;
			unsigned char	flags1;
			unsigned char	flags2;
			unsigned char	base_hi;
		} bytes;

		struct {
			uint32_t base_mid	:8;
			uint32_t type		:5;
			uint32_t dpl		:2;
			uint32_t pres		:1;
			uint32_t limit_hi	:4;
			uint32_t sys		:1;
			uint32_t reserved	:1;
			uint32_t default_big	:1;
			uint32_t granularity	:1;
			uint32_t base_hi	:8;
		} bits;
	} high_word;
} nt_ldt_entry;


typedef int32_t __stdcall ntapi_zw_flush_write_buffer(void);


/* interface requires further studying */
typedef int32_t __stdcall ntapi_zw_raise_hard_error(
	__in	int32_t				status,
	__in	uint32_t			number_of_args,
	__in	uint32_t			string_arg_mask,
	__in	uint32_t *			args,
	__in	nt_hard_error_response_option	response_option,
	__out	nt_hard_error_response *	response_received);


typedef int32_t __stdcall ntapi_zw_set_default_hard_error_port(
	__in	void *	hport);


typedef int32_t __stdcall ntapi_zw_display_string(
	__in	nt_unicode_string *	display_string);


typedef int32_t __stdcall ntapi_zw_create_paging_file(
	__in	nt_unicode_string *	file_name,
	__in	nt_large_integer *	initial_size,
	__in	nt_large_integer *	maximum_size,
	__in	uintptr_t		reserved);


typedef int32_t __stdcall ntapi_zw_set_ldt_entries(
	__in	uint32_t	selector_1st,
	__in	nt_ldt_entry *	ldt_entry_1st,
	__in	uint32_t	selector_2nd,
	__in	nt_ldt_entry *	ldt_entry_2nd);


typedef int32_t __stdcall ntapi_zw_vdm_control(
	__in	uint32_t	vdm_control_code,
	__in	void *		vdm_control_data);

#endif
