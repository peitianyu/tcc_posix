#ifndef _NT_MEMORY_H_
#define _NT_MEMORY_H_

#include <psxtypes/psxtypes.h>
#include "nt_object.h"

typedef enum _nt_memory_info_class {
	NT_MEMORY_BASIC_INFORMATION,
	NT_MEMORY_WORKING_SET_LIST,
	NT_MEMORY_SECTION_NAME,
	NT_MEMORY_BASIC_VLM_INFORMATION
} nt_memory_info_class;

/* memory allocation granularity: same on all supported systems */
#define NT_ALLOCATION_GRANULARITY	(0x10000)

/* memory (de)allocation types */
#define NT_MEM_PAGE_GUARD       	0x00000100 /* protect */
#define NT_MEM_COMMIT			0x00001000 /* commit */
#define NT_MEM_RESERVE			0x00002000 /* reserve only */
#define NT_MEM_DECOMMIT			0x00004000 /* decommit but maintain reservavion */
#define NT_MEM_RELEASE			0x00008000 /* decommit and cancel reservation */
#define NT_MEM_RESET			0x00080000 /* make obsolete */
#define NT_MEM_TOP_DOWN			0x00100000 /* allocate at highest possible address using a slow and possibly buggy algorithm */
#define NT_MEM_WRITE_WATCH		0x00200000 /* track writes */
#define NT_MEM_PHYSICAL			0x00400000 /* physical view */
#define NT_MEM_RESET_UNDO AVOID		0x01000000 /* only after a successful NT_MEM_RESET */
#define NT_MEM_LARGE_PAGES		0x20000000 /* use large-page support */
#define NT_MEM_FREE			0x00010000 /* informational only: nt_memory_basic_information.state */
#define NT_MEM_IMAGE			0x01000000 /* informational only: nt_memory_basic_information.type */
#define NT_MEM_MAPPED			0x00040000 /* informational only: nt_memory_basic_information.type */
#define NT_MEM_PRIVATE			0x00020000 /* informational only: nt_memory_basic_information.type */


/* memory page access bits */
#define NT_PAGE_NOACCESS		(uint32_t)0x01
#define NT_PAGE_READONLY		(uint32_t)0x02
#define NT_PAGE_READWRITE		(uint32_t)0x04
#define NT_PAGE_WRITECOPY		(uint32_t)0x08
#define NT_PAGE_EXECUTE			(uint32_t)0x10
#define NT_PAGE_EXECUTE_READ		(uint32_t)0x20
#define NT_PAGE_EXECUTE_READWRITE	(uint32_t)0x40
#define NT_PAGE_EXECUTE_WRITECOPY	(uint32_t)0x80


/* working set list entries: basic attributes */
#define NT_WSLE_PAGE_NOT_ACCESSED		0x0000
#define NT_WSLE_PAGE_READONLY			0x0001
#define NT_WSLE_PAGE_EXECUTE			0x0002
#define NT_WSLE_PAGE_EXECUTE_READ		0x0003
#define NT_WSLE_PAGE_READWRITE			0x0004
#define NT_WSLE_PAGE_WRITECOPY			0x0005
#define NT_WSLE_PAGE_EXECUTE_READWRITE		0x0006
#define NT_WSLE_PAGE_EXECUTE_WRITECOPY		0x0007

/* working set list entries: extended attributes */
#define NT_WSLE_PAGE_NO_CACHE			0x0008
#define NT_WSLE_PAGE_GUARD_PAGE			0x0010
#define NT_WSLE_PAGE_SHARE_COUNT_MASK		0x00E0
#define NT_WSLE_PAGE_SHAREABLE			0x0100

/* ntapi_zw_lock_virtual_memory lock types */
#define NT_LOCK_VM_IN_WSL			0x0001
#define NT_LOCK_VM_IN_RAM			0x0002


typedef struct _nt_memory_basic_information {
	void *		base_address;
	void *		allocation_base;
	uint32_t	allocation_protect;
	size_t		region_size;
	uint32_t	state;
	uint32_t	protect;
	uint32_t	type;
} nt_memory_basic_information;


typedef struct _nt_memory_working_set_list {
	uintptr_t	number_of_pages;
	uintptr_t	nt_working_set_list_entry[];
} nt_memory_working_set_list;


typedef struct _nt_memory_section_name {
	nt_unicode_string	section_name;
	wchar16_t		section_name_buffer[];
} nt_memory_section_name, nt_mem_sec_name;


typedef int32_t __stdcall ntapi_zw_allocate_virtual_memory(
	__in		void *		hprocess,
	__in_out	void **		base_address,
	__in		uint32_t	zero_bits,
	__in_out	size_t *	allocation_size,
	__in		uint32_t	allocation_type,
	__in		uint32_t	protect);


typedef int32_t __stdcall ntapi_zw_free_virtual_memory(
	__in		void *		hprocess,
	__in_out	void **		base_address,
	__in_out	size_t *	free_size,
	__in		uint32_t	deallocation_type);


typedef int32_t __stdcall ntapi_zw_query_virtual_memory(
	__in	void *			hprocess,
	__in	void *			base_address,
	__in	nt_memory_info_class	mem_info_class,
	__out	void *			mem_info,
	__in	size_t			mem_info_length,
	__out	size_t *		returned_length	__optional);


typedef int32_t __stdcall ntapi_zw_protect_virtual_memory(
	__in	void *		hprocess,
	__in	void **		base_address,
	__in	size_t *	protect_size,
	__in	uint32_t	protect_type_new,
	__out	uint32_t *	protect_type_old);


typedef int32_t __stdcall ntapi_zw_read_virtual_memory(
	__in	void *		hprocess,
	__in	void *		base_address,
	__out	char *		buffer,
	__in	size_t		buffer_length,
	__out	size_t *	bytes_written);


typedef int32_t __stdcall ntapi_zw_write_virtual_memory(
	__in	void *		hprocess,
	__in	void *		base_address,
	__in	char *		buffer,
	__in	size_t		buffer_length,
	__out	size_t *	bytes_written);


typedef int32_t __stdcall ntapi_zw_lock_virtual_memory(
	__in		void *		hprocess,
	__in_out	void **		base_address,
	__in_out	size_t *	lock_size,
	__in		uint32_t	lock_type);


typedef int32_t __stdcall ntapi_zw_unlock_virtual_memory(
	__in		void *		hprocess,
	__in_out	void **		base_address,
	__in_out	size_t *	lock_size,
	__in		uint32_t	lock_type);


typedef int32_t __stdcall ntapi_zw_flush_virtual_memory(
	__in		void *			hprocess,
	__in_out	void **			base_address,
	__in_out	size_t *		flush_size,
	__in		nt_io_status_block *	flush_type);


typedef int32_t __stdcall ntapi_zw_allocate_user_physical_pages(
	__in		void *		hprocess,
	__in_out	uintptr_t *	number_of_pages,
	__out		uintptr_t *	arr_page_frame_numbers);


typedef int32_t __stdcall ntapi_zw_free_user_physical_pages(
	__in		void *		hprocess,
	__in_out	uintptr_t *	number_of_pages,
	__in		uintptr_t *	arr_page_frame_numbers);


typedef int32_t __stdcall ntapi_zw_map_user_physical_pages(
	__in		void *		base_address,
	__in_out	uintptr_t *	number_of_pages,
	__in		uintptr_t *	arr_page_frame_numbers);


typedef int32_t __stdcall ntapi_zw_map_user_physical_pages_scatter(
	__in		void **		virtual_addresses,
	__in_out	uintptr_t *	number_of_pages,
	__in		uintptr_t *	arr_page_options);


typedef uint32_t __stdcall ntapi_zw_get_write_watch(
	__in		void *		hprocess,
	__in		uint32_t	flags,
	__in		void *		base_address,
	__in		size_t		region_size,
	__out		uintptr_t *	buffer,
	__in_out	uintptr_t *	buffer_entries,
	__out		uintptr_t *	granularity);


typedef uint32_t __stdcall ntapi_zw_reset_write_watch(
	__in		void *		hprocess,
	__in		void *		base_address,
	__in		size_t		region_size);

#endif
