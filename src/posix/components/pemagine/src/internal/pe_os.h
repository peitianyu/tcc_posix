#ifndef PE_OS_H
#define PE_OS_H

#include <psxtypes/psxtypes.h>
#include <pemagine/pe_structs.h>

#define OS_STATUS_SUCCESS			(int32_t)0x00000000
#define OS_STATUS_INVALID_PARAMETER		(int32_t)0xC000000D
#define OS_STATUS_ILLEGAL_CHARACTER		(int32_t)0xC0000161
#define OS_STATUS_NO_MATCH			(int32_t)0xC0000272
#define OS_STATUS_INVALID_ADDRESS		(int32_t)0xC0000141
#define OS_STATUS_CONTEXT_MISMATCH		(int32_t)0xC0000719
#define OS_STATUS_COULD_NOT_INTERPRET		(int32_t)0xC00000B9
#define OS_STATUS_NOT_SUPPORTED			(int32_t)0xC00000BB
#define OS_STATUS_NAME_TOO_LONG			(int32_t)0xC0000106
#define OS_STATUS_BUFFER_TOO_SMALL		(int32_t)0xC0000023
#define OS_STATUS_INTERNAL_ERROR		(int32_t)0xC00000E5
#define OS_STATUS_BAD_FILE_TYPE			(int32_t)0xC0000903
#define OS_STATUS_OBJECT_NAME_NOT_FOUND 	(int32_t)0xC0000034
#define OS_STATUS_OBJECT_PATH_NOT_FOUND		(int32_t)0xC000003A
#define OS_STATUS_MORE_PROCESSING_REQUIRED	(int32_t)0xC0000016

#define OS_OBJ_INHERIT	 			0x00000002
#define OS_OBJ_CASE_INSENSITIVE			0x00000040

#define OS_SEC_SYNCHRONIZE			0x00100000
#define	OS_FILE_READ_ACCESS			0x00000001
#define	OS_FILE_READ_ATTRIBUTES			0x00000080

#define OS_FILE_DIRECTORY_FILE			0x00000001
#define OS_FILE_NON_DIRECTORY_FILE		0x00000040
#define OS_FILE_SYNCHRONOUS_IO_ALERT		0x00000010

#define OS_FILE_SHARE_READ			0x00000001
#define OS_FILE_SHARE_WRITE			0x00000002
#define OS_FILE_SHARE_DELETE			0x00000004

#define OS_CURRENT_PROCESS_HANDLE		(void *)(uintptr_t)(-1)
#define OS_CURRENT_THREAD_HANDLE		(void *)(uintptr_t)(-2)


enum os_object_info_class {
	OS_OBJECT_BASIC_INFORMATION	= 0,
	OS_OBJECT_NAME_INFORMATION	= 1,
	OS_OBJECT_TYPE_INFORMATION	= 2,
	OS_OBJECT_ALL_TYPES_INFORMATION	= 3,
	OS_OBJECT_HANDLE_INFORMATION	= 4
};


enum os_memory_info_class {
	OS_MEMORY_BASIC_INFORMATION,
	OS_MEMORY_WORKING_SET_LIST,
	OS_MEMORY_SECTION_NAME,
	OS_MEMORY_BASIC_VLM_INFORMATION
};


struct os_oa {
	uint32_t		len;
	void *			root_dir;
	struct pe_unicode_str *	obj_name;
	uint32_t		obj_attr;
	void *			sec_desc;
	void *			sec_qos;
};


struct os_iosb {
	union {
		int32_t		status;
		void *		pointer;
	};
	intptr_t		info;
};


struct os_proc_params {
	uint32_t		alloc_size;
	uint32_t		used_size;
	uint32_t		flags;
	uint32_t		reserved;
	void *			hconsole;
	uintptr_t		console_flags;
	void *			hstdin;
	void *			hstdout;
	void *			hstderr;
	struct pe_unicode_str	cwd_name;
	void *			cwd_handle;
	struct pe_unicode_str	__attr_ptr_size_aligned__ dll_path;
	struct pe_unicode_str	__attr_ptr_size_aligned__ image_file_name;
	struct pe_unicode_str	__attr_ptr_size_aligned__ command_line;
	wchar16_t *		environment;
	uint32_t		dwx;
	uint32_t		dwy;
	uint32_t		dwx_size;
	uint32_t		dwy_size;
	uint32_t		dwx_count_chars;
	uint32_t		dwy_count_chars;
	uint32_t		dw_fill_attribute;
	uint32_t		dw_flags;
	uint32_t		wnd_show;
	struct pe_unicode_str	wnd_title;
	struct pe_unicode_str	__attr_ptr_size_aligned__ desktop;
	struct pe_unicode_str	__attr_ptr_size_aligned__ shell_info;
	struct pe_unicode_str	__attr_ptr_size_aligned__ runtime_data;
};


struct os_peb {
	unsigned char		reserved_1st[2];
	unsigned char         	debugged;
	unsigned char		reserved_2nd[1];
	void *			reserved_3rd[2];
	struct pe_peb_ldr_data*	peb_ldr_data;
	struct os_proc_params *	process_params;
	unsigned char		reserved_4th[104];
	void *			reserved_5th[52];
	void * 			post_process_init_routine;
	unsigned char		reserved_6th[128];
	void *			reserved_7th[1];
	uint32_t		session_id;
};


typedef int32_t __stdcall os_zw_close(
	__in	void *			handle);


typedef int32_t __stdcall os_zw_query_object(
	__in	void *			handle,
	__in	int			obj_info_class,
	__out	void *			obj_info,
	__in	size_t			obj_info_length,
	__out	uint32_t *		returned_length		__optional);


typedef int32_t __stdcall os_zw_read_virtual_memory(
	__in	void *			hprocess,
	__in	void *			base_address,
	__out	char *			buffer,
	__in	size_t			buffer_length,
	__out	size_t *		bytes_written);


typedef int32_t __stdcall os_zw_open_file(
	__out	void **			hfile,
	__in	uint32_t		desired_access,
	__in	struct os_oa *		obj_attr,
	__out	struct os_iosb *	io_status_block,
	__in	uint32_t		share_access,
	__in	uint32_t		open_options);


typedef int32_t __stdcall os_ldr_load_dll(
	__in	wchar16_t *		image_path	__optional,
	__in	uint32_t *		image_flags	__optional,
	__in	struct pe_unicode_str *	image_name,
	__out	void **			image_base);


typedef int32_t __stdcall os_zw_terminate_process(
	__in	void *		hprocess,
	__in	int32_t		status);

#endif
