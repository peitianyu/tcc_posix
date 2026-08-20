#ifndef _NT_SECTION_H_
#define _NT_SECTION_H_

#include <psxtypes/psxtypes.h>
#include "nt_object.h"
#include "nt_memory.h"

typedef enum _nt_section_info_class {
	NT_SECTION_BASIC_INFORMATION,
	NT_SECTION_IMAGE_INFORMATION
} nt_section_info_class;


typedef enum _nt_section_inherit {
	NT_VIEW_SHARE	= 1,
	NT_VIEW_UNMAP	= 2
} nt_section_inherit;

/* section attributes */
#define NT_SEC_BASED			0x00200000
#define NT_SEC_NO_CHANGE		0x00400000
#define NT_SEC_FILE			0x00800000
#define NT_SEC_IMAGE			0x01000000
#define NT_SEC_VLM			0x02000000
#define NT_SEC_RESERVE			0x04000000
#define NT_SEC_COMMIT			0x08000000
#define NT_SEC_NOCACHE			0x10000000
#define NT_SEC_IMAGE_NO_EXECUTE		0x11000000
#define NT_SEC_LARGE_PAGES		0x80000000
#define NT_SEC_WRITECOMBINE		0x40000000

/* section memory allocation attributes */
#define NT_SEC_AT_EXTENDABLE_FILE	0x00002000 /* view may exceed section size */
#define NT_SEC_AT_RESERVED		0x20000000 /* ignored */
#define NT_SEC_AT_ROUND_TO_PAGE		0x40000000 /* adjust address and/or size as necessary */


/* section access bits */
#define NT_SECTION_QUERY        	0x00000001
#define NT_SECTION_MAP_WRITE        	0x00000002
#define NT_SECTION_MAP_READ         	0x00000004
#define NT_SECTION_MAP_EXECUTE      	0x00000008
#define NT_SECTION_EXTEND_SIZE      	0x00000010
#define NT_SECTION_MAP_EXECUTE_EXPLICIT 0x00000020
#define NT_STANDARD_RIGHTS_REQUIRED	0x000F0000
#define NT_SECTION_ALL_ACCESS 		NT_STANDARD_RIGHTS_REQUIRED \
						| NT_SECTION_QUERY \
						| NT_SECTION_MAP_WRITE \
						| NT_SECTION_MAP_READ \
						| NT_SECTION_MAP_EXECUTE \
						| NT_SECTION_EXTEND_SIZE


typedef struct _nt_section_basic_information {
	void *			base_address;
	uint32_t		section_attr;
	nt_large_integer	section_size;
} nt_section_basic_information, nt_sbi;

typedef struct _nt_section_image_information {
	void *			entry_point;
	uint32_t		stack_zero_bits;
	size_t			stack_reserve;
	size_t			stack_commit;
	uint32_t		subsystem;
	uint16_t		subsystem_minor_version;
	uint16_t		subsystem_major_version;
	uint32_t		unknown;
	uint32_t		characteristics;
	uint16_t		image_number;
	unsigned char		executable;
	unsigned char		image_flags;
	uint32_t		loader_flags;
	uint32_t		image_file_size;
	uint32_t		image_checksum;
} nt_section_image_information, nt_sec_img_inf;


typedef int32_t __stdcall ntapi_zw_create_section(
	__out	void **			hsection,
	__in	uint32_t		desired_access,
	__in	nt_object_attributes *	obj_attr,
	__in	nt_large_integer *	section_size	__optional,
	__in	uint32_t		section_protect,
	__in	uint32_t		section_attr,
	__in	void *			hfile);

typedef int32_t __stdcall ntapi_zw_open_section(
	__out	void **			hsection,
	__in	uint32_t		desired_access,
	__in	nt_object_attributes *	obj_attr);


typedef int32_t __stdcall ntapi_zw_query_section(
	__in	void *			hsection,
	__in	nt_section_info_class	sec_info_class,
	__out	void *			sec_info,
	__in	size_t			sec_info_length,
	__out	size_t *		returned_length	__optional);


typedef int32_t __stdcall ntapi_zw_extend_section(
	__in	void *				hsection,
	__in	nt_large_integer *		section_size);


typedef int32_t __stdcall ntapi_zw_map_view_of_section(
	__in		void *			hsection,
	__in		void *			hprocess,
	__in_out	void **			base_address,
	__in		uint32_t		zero_bits,
	__in		size_t			commit_size,
	__in_out	nt_large_integer *	section_offset	__optional,
	__in_out	size_t *		view_size,
	__in		nt_section_inherit	section_inherit_disposition,
	__in		uint32_t		allocation_type,
	__in		uint32_t		protect);



typedef int32_t __stdcall ntapi_zw_unmap_view_of_section(
	__in		void *			hprocess,
	__in		void *			base_address);


typedef int32_t __stdcall ntapi_zw_are_mapped_files_the_same(
	__in		void *			addr_1st,
	__in		void *			addr_2nd);


/* extensions */
typedef int32_t __stdcall ntapi_tt_get_section_name(
	__in	void *			addr,
	__out	nt_mem_sec_name *	buffer,
	__in	uint32_t		buffer_size);

#endif
