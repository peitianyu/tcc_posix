#ifndef _NT_PROCESS_H_
#define _NT_PROCESS_H_

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include "nt_object.h"
#include "nt_memory.h"
#include "nt_section.h"

typedef enum _nt_process_info_class {
	NT_PROCESS_BASIC_INFORMATION,
	NT_PROCESS_QUOTA_LIMITS,
	NT_PROCESS_IO_COUNTERS,
	NT_PROCESS_VM_COUNTERS,
	NT_PROCESS_TIMES,
	NT_PROCESS_BASE_PRIORITY,
	NT_PROCESS_RAISE_PRIORITY,
	NT_PROCESS_DEBUG_PORT,
	NT_PROCESS_EXCEPTION_PORT,
	NT_PROCESS_ACCESS_TOKEN,
	NT_PROCESS_LDT_INFORMATION,
	NT_PROCESS_LDT_SIZE,
	NT_PROCESS_DEFAULT_HARD_ERROR_MODE,
	NT_PROCESS_IO_PORT_HANDLERS,
	NT_PROCESS_POOLED_USAGE_AND_LIMITS,
	NT_PROCESS_WORKING_SET_WATCH,
	NT_PROCESS_USER_MODE_IOPL,
	NT_PROCESS_ENABLE_ALIGNMENT_FAULT_FIXUP,
	NT_PROCESS_PRIORITY_CLASS,
	NT_PROCESS_WX86_INFORMATION,
	NT_PROCESS_HANDLE_COUNT,
	NT_PROCESS_AFFINITY_MASK,
	NT_PROCESS_PRIORITY_BOOST,
	NT_PROCESS_DEVICE_MAP,
	NT_PROCESS_SESSION_INFORMATION,
	NT_PROCESS_FOREGROUND_INFORMATION,
	NT_PROCESS_WOW64_INFORMATION,
	NT_PROCESS_IMAGE_FILE_NAME
} nt_process_info_class;


typedef enum _nt_process_create_info_class {
	NT_PROCESS_CREATE_INITIAL_STATE,
	NT_PROCESS_CREATE_FAIL_ON_FILE_OPEN,
	NT_PROCESS_CREATE_FAIL_ON_SECTION_CREATE,
	NT_PROCESS_CREATE_FAIL_EXE_FORMAT,
	NT_PROCESS_CREATE_FAIL_MACHINE_MISMATCH,
	NT_PROCESS_CREATE_FAIL_EXE_NAME,
	NT_PROCESS_CREATE_SUCCESS,
	NT_PROCESS_CREATE_MAXIMUM_STATES,
} nt_process_create_info_class;



/* special handles */
#define NT_CURRENT_PROCESS_HANDLE (void *)(uintptr_t)-1


/* process access bits */
#define NT_PROCESS_CREATE_PROCESS	0x00000080U
#define NT_PROCESS_CREATE_THREAD	0x00000002U
#define NT_PROCESS_DUP_HANDLE		0x00000040U
#define NT_PROCESS_QUERY_INFORMATION	0x00000400U
#define NT_PROCESS_SET_INFORMATION	0x00000200U
#define NT_PROCESS_SET_QUOTA		0x00000100U
#define NT_PROCESS_SUSPEND_RESUME	0x00000800U
#define NT_PROCESS_TERMINATE		0x00000001U
#define NT_PROCESS_VM_OPERATION		0x00000008U
#define NT_PROCESS_VM_READ		0x00000010U
#define NT_PROCESS_VM_WRITE		0x00000020U
#define NT_PROCESS_SYNCHRONIZE		0x00100000U
#define NT_PROCESS_PRESERVE_AUTHZ_LEVEL	0x02000000U
#define NT_PROCESS_ALL_ACCESS		NT_PROCESS_CREATE_PROCESS \
					| NT_PROCESS_CREATE_THREAD \
					| NT_PROCESS_DUP_HANDLE \
					| NT_PROCESS_QUERY_INFORMATION \
					| NT_PROCESS_SET_INFORMATION \
					| NT_PROCESS_SET_QUOTA \
					| NT_PROCESS_SUSPEND_RESUME \
					| NT_PROCESS_TERMINATE \
					| NT_PROCESS_VM_OPERATION \
					| NT_PROCESS_VM_READ \
					| NT_PROCESS_VM_WRITE \
					| NT_PROCESS_SYNCHRONIZE



/* set error mode */
#define NT_SEM_FAIL_CRITICAL_ERRORS		0x0001
#define NT_SEM_NO_GP_FAULT_ERROR_BOX		0x0002
#define NT_SEM_NO_ALIGNMENT_FAULT_EXCEPT	0x0004
#define NT_SEM_NO_OPEN_FILE_ERROR_BOX		0x8000


/* process priority class (information class) */
#define NT_PC_IDLE		0x00
#define NT_PC_NORMAL		0x02
#define NT_PC_HIGH		0x03
#define NT_PC_REALTIME		0x04
#define NT_PC_BELOW_NORMAL	0x05
#define NT_PC_ABOVE_NORMAL	0x05


/* process device map drive type */
#define NT_DRIVE_UNKNOWN	0x00
#define NT_NO_ROOT_DIR		0x01
#define NT_DRIVE_REMOVABLE	0x02
#define NT_DRIVE_FIXED		0x03
#define NT_DRIVE_REMOTE		0x04
#define NT_DRIVE_CDROM		0x05
#define NT_DRIVE_RAMDISK	0x06


/* process debug info class mask */
#define NT_PDI_MODULES		0x0001
#define NT_PDI_BACKTRACE	0x0002
#define NT_PDI_HEAPS		0x0004
#define NT_PDI_HEAP_TAGS	0x0008
#define NT_PDI_HEAP_BLOCKS	0x0010
#define NT_PDI_LOCKS		0x0020


/* process debug module information flags */
#define NT_LDRP_STATIC_LINK			0x00000002
#define NT_LDRP_IMAGE_DLL			0x00000004
#define NT_LDRP_LOAD_IN_PROGRESS		0x00001000
#define NT_LDRP_UNLOAD_IN_PROGRESS		0x00002000
#define NT_LDRP_ENTRY_PROCESSED			0x00004000
#define NT_LDRP_ENTRY_INSERTED			0x00008000
#define NT_LDRP_CURRENT_LOAD			0x00010000
#define NT_LDRP_FAILED_BUILTIN_LOAD		0x00020000
#define NT_LDRP_DONT_CALL_FOR_THREADS		0x00040000
#define NT_LDRP_PROCESS_ATTACH_CALLED		0x00080000
#define NT_LDRP_DEBUG_SYMBOLS_LOADED		0x00100000
#define NT_LDRP_IMAGE_NOT_AT_BASE		0x00200000
#define NT_LDRP_WX86_IGNORE_MACHINETYPE		0x00400000


/* create process info bits */
#define NT_PROCESS_CREATE_INFO_WRITE_OUTPUT	0x00000001
#define NT_PROCESS_CREATE_INFO_OBTAIN_OUTPUT	0x20000003

/* zw_create_user_process: creation flags */
#define NT_PROCESS_CREATE_FLAGS_CREATE_THREAD_SUSPENDED		(0x00000001)
#define NT_PROCESS_CREATE_FLAGS_RESET_DEBUG_PORT		(0x00000002)
#define NT_PROCESS_CREATE_FLAGS_INHERIT_HANDLES			(0x00000004)
#define NT_PROCESS_CREATE_FLAGS_NO_OBJECT_SYNC			(0x00000100)

/* zw_create_user_process: extended parameters */
#define NT_CREATE_PROCESS_EXT_PARAM_SET_FILE_NAME		(0x00020005)
#define NT_CREATE_PROCESS_EXT_PARAM_SET_VIRTUAL_ADDR_RANGES	(0x00020007)
#define NT_CREATE_PROCESS_EXT_PARAM_SET_BASE_PRIORITY		(0x00020008)
#define NT_CREATE_PROCESS_EXT_PARAM_SET_HARD_ERROR_MODE		(0x00020009)
#define NT_CREATE_PROCESS_EXT_PARAM_SET_CONSOLE_FLAGS		(0x0002000A)
#define NT_CREATE_PROCESS_EXT_PARAM_SET_INHERITED_HANDLES	(0x0002000B)
#define NT_CREATE_PROCESS_EXT_PARAM_SET_PARENT			(0x00060000)
#define NT_CREATE_PROCESS_EXT_PARAM_SET_DEBUG			(0x00060001)
#define NT_CREATE_PROCESS_EXT_PARAM_SET_TOKEN			(0x00060002)

#define NT_CREATE_PROCESS_EXT_PARAM_GET_SECTION_IMAGE_INFO	(0x00000006)
#define NT_CREATE_PROCESS_EXT_PARAM_GET_CLIENT_ID		(0x00010003)
#define NT_CREATE_PROCESS_EXT_PARAM_GET_TEB_ADDRESS		(0x00010004)


/* zw_create_user_process: console flag bits */
#define NT_CREATE_PROCESS_EXT_CONSOLE_FLAG_DEFAULT		(0x00)
#define NT_CREATE_PROCESS_EXT_CONSOLE_FLAG_DO_NOT_USE_HANDLES	(0x00)
#define NT_CREATE_PROCESS_EXT_CONSOLE_FLAG_INHERIT_HANDLES	(0x01)
#define NT_CREATE_PROCESS_EXT_CONSOLE_FLAG_USE_ARG_HANDLES	(0x02)
#define NT_CREATE_PROCESS_EXT_CONSOLE_FLAG_INHERIT_STDIN	(0x04)
#define NT_CREATE_PROCESS_EXT_CONSOLE_FLAG_INHERIT_STDOUT	(0x08)
#define NT_CREATE_PROCESS_EXT_CONSOLE_FLAG_INHERIT_STDERR	(0x10)

/* nt_runtime_data_block flag bits */
#define NT_RUNTIME_DATA_DUPLICATE_SESSION_HANDLES		(0x01)

/* tt_get_runtime_data flag bits */
#define NT_RUNTIME_DATA_ALLOW_BUILTIN_DEFAULT			(0x01)

/* runtime data convenience storage */
#define NT_RUNTIME_DATA_USER_PTRS				(0x10)
#define NT_RUNTIME_DATA_USER_INT32_SLOTS			(0x10)
#define NT_RUNTIME_DATA_USER_INT64_SLOTS			(0x10)

/* friendly process guids */
#define NT_PROCESS_GUID_NTPGRP		{0xfa383cc0,0xa25b,0x4448,{0x83,0x45,0x51,0x45,0x4d,0xa8,0x2f,0x30}}
#define NT_PROCESS_GUID_PIDMAP		{0xba054c90,0x8b4f,0x4989,{0xa0,0x52,0x32,0xce,0x41,0x9e,0xbf,0x97}}
#define NT_PROCESS_GUID_PIDANY		{0x431bf6a6,0x65c4,0x4eb0,{0x88,0xca,0x16,0xfe,0xc0,0x18,0xc8,0xb7}}

/* friendly process object directory prefixes */
#define NT_PROCESS_OBJDIR_PREFIX_NTPGRP	{'n','t','p','g','r','p'}
#define NT_PROCESS_OBJDIR_PREFIX_PIDMAP	{'p','i','d','m','a','p'}
#define NT_PROCESS_OBJDIR_PREFIX_PIDANY	{'p','i','d','a','n','y'}

typedef struct _nt_process_information {
	void *		hprocess;
	void *		hthread;
	uintptr_t	process_id;
	uintptr_t	thread_id;
} nt_process_information, nt_process_info;


typedef struct _nt_process_parameters {
	uint32_t		alloc_size;
	uint32_t		used_size;
	uint32_t		flags;
	uint32_t		reserved;
	void *			hconsole;
	uintptr_t		console_flags;
	void *			hstdin;
	void *			hstdout;
	void *			hstderr;
	nt_unicode_string	cwd_name;
	void *			cwd_handle;
	nt_unicode_string	__attr_ptr_size_aligned__ dll_path;
	nt_unicode_string	__attr_ptr_size_aligned__ image_file_name;
	nt_unicode_string	__attr_ptr_size_aligned__ command_line;
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
	nt_unicode_string	wnd_title;
	nt_unicode_string	__attr_ptr_size_aligned__ desktop;
	nt_unicode_string	__attr_ptr_size_aligned__ shell_info;
	nt_unicode_string	__attr_ptr_size_aligned__ runtime_data;
} nt_process_parameters;


typedef struct _nt_peb {
	unsigned char		reserved_1st[2];
	unsigned char         	debugged;
	unsigned char		reserved_2nd[1];
	void *			reserved_3rd[2];
	struct pe_peb_ldr_data*	peb_ldr_data;
	nt_process_parameters * process_params;
	unsigned char		reserved_4th[104];
	void *			reserved_5th[52];
	void * 			post_process_init_routine;
	unsigned char		reserved_6th[128];
	void *			reserved_7th[1];
	uint32_t		session_id;
} nt_peb;


typedef struct _nt_process_basic_information {
	int32_t		exit_status;
	nt_peb *	peb_base_address;
	intptr_t	affinity_mask;
	uint32_t	base_priority;
	uintptr_t	unique_process_id;
	uintptr_t	inherited_from_unique_process_id;
} nt_process_basic_information, nt_pbi;


typedef struct _nt_process_access_token {
	void *	token;
	void *	thread;
} nt_process_access_token;


typedef struct _nt_process_ws_watch_information {
	void *	faulting_pc;
	void *	faulting_va;
} nt_process_ws_watch_information;


typedef struct _nt_process_priority_class {
	int32_t		foreground;
	uint32_t	priority;
} nt_process_priority_class;


typedef struct _nt_process_device_map_information {
	union {
		struct {
			void *	directory_handle;
		} set;

		struct {
			uint32_t	drive_map;
			unsigned char	drive_type[32];
		} query;
	};
} nt_process_device_map_information;


typedef struct _nt_debug_buffer {
	void *		hsection;
	void *		section_base;
	void *		remote_section_base;
	size_t		section_base_delta;
	void *		hevent_pair;
	void *		unknown[2];
	void *		hthread_remote;
	uint32_t	info_class_mask;
	size_t		info_size;
	size_t		allocated_size;
	size_t		section_size;
	void *		module_information;
	void *		back_trace_information;
	void *		heap_information;
	void *		lock_information;
	void *		reserved[8];
} nt_debug_buffer;


typedef struct _nt_debug_module_information {
	void *		reserved[2];
	size_t		base;
	size_t		size;
	uint32_t	flags;
	uint16_t	index;
	uint16_t	unknown;
	uint16_t	load_count;
	uint16_t	module_name_offset;
	char		image_name[256];
} nt_debug_module_information;


typedef struct _nt_debug_heap_information {
	size_t		base;
	uint32_t	flags;
	uint16_t	granularity;
	uint16_t	unknown;
	size_t		allocated;
	size_t		committed;
	uint32_t	tag_count;
	uint32_t	block_count;
	void *		reserved[7];
	void *		tags;
	void *		blocks;
} nt_debug_heap_information;


typedef struct _nt_debug_lock_information {
	void *		address;
	uint16_t	type;
	uint16_t	creator_back_trace_index;
	uintptr_t	owner_thread_id;
	uint32_t	active_count;
	uint32_t	contention_count;
	uint32_t	entry_count;
	uint32_t	recursion_count;
	uint32_t	number_of_share_waiters;
	uint32_t	number_of_exclusive_waiters;
} nt_debug_lock_information;


typedef struct _nt_executable_image {
	void *		hfile;
	void *		hsection;
	void *		addr;
	size_t		size;
	uint16_t	characteristics;
	uint16_t	magic;
	uint16_t	subsystem;
	uint16_t	uflags;
} nt_executable_image;


typedef struct _nt_process_session_information {
	uintptr_t	session_id;
} nt_process_session_information;


typedef struct _nt_create_process_info {
	size_t		size;
	size_t		state;

	union {
		struct {
			uint32_t	init_flags;
			uint32_t	file_access_ext;
			uintptr_t	unused[8];
		} init_state;

		struct {
			uintptr_t	output_flags;
			void *		hfile;
			void *		hsection;
			uint64_t	unknown[6];
		} success_state;
	};
} nt_create_process_info;


typedef struct _nt_create_process_ext_param {
	size_t		ext_param_type;
	size_t		ext_param_size;

	union {
		uint32_t	ext_param_value;
		void *		ext_param_addr;
	};

	size_t		ext_param_returned_length;
} nt_create_process_ext_param;


typedef struct _nt_create_process_ext_params {
	size_t				ext_params_size;
	nt_create_process_ext_param	ext_param[];
} nt_create_process_ext_params;


typedef struct _nt_user_process_info {
	uint32_t			size;
	void *				hprocess;
	void *				hthread;
	nt_cid				cid;
	nt_section_image_information	sec_image_info;
} nt_user_process_info;


typedef struct _nt_process_alternate_client_id {
	void *		hpgrp;
	void *		hentry;
	void *		hsession;
	void *		hdaemon;
	void *		htarget;
	void *		hevent;
	int32_t		tid;
	int32_t		pid;
	int32_t		pgid;
	int32_t		sid;
	uintptr_t	reserved[8];
} nt_process_alternate_client_id, nt_alt_cid;

typedef struct _nt_runtime_data {
	void *		hprocess_self;
	void *		hprocess_parent;
	nt_cid		cid_self;
	nt_cid		cid_parent;
	nt_alt_cid	alt_cid_self;
	nt_alt_cid	alt_cid_parent;
	void *		himage;
	void *		hroot;
	void *		hcwd;
	void *		hdrive;
	void *		hstdin;
	void *		hstdout;
	void *		hstderr;
	void *		hjob;
	void *		hsession;
	void *		hdebug;
	void *		hlog;
	void *		hready;
	void *		srv_ready;
	nt_guid		srv_guid;
	int32_t		srv_type;
	int32_t		srv_subtype;
	uint32_t	srv_keys[6];
	int32_t		stdin_type;
	int32_t		stdout_type;
	int32_t		stderr_type;
	int32_t		session_type;
	uint32_t	dbg_type;
	uint32_t	log_type;
	void *		ctx_hsection;
	void *		ctx_addr;
	size_t		ctx_size;
	size_t		ctx_commit;
	ptrdiff_t	ctx_offset;
	size_t		ctx_counter;
	size_t		ctx_meta_size;
	size_t		ctx_buffer_size;
	uint32_t	ctx_options;
	uint32_t	ctx_flags;
	uint32_t	meta_hash;
	uint32_t	block_hash;
	size_t		stack_reserve;
	size_t		stack_commit;
	size_t		heap_reserve;
	size_t		heap_commit;
	int32_t		envc;
	int32_t		argc;
	char **		argv;
	char **		envp;
	wchar16_t **	wargv;
	wchar16_t **	wenvp;
	int32_t		peb_envc;
	int32_t		peb_argc;
	wchar16_t **	peb_wargv;
	wchar16_t **	peb_wenvp;
	void *		uptr  [NT_RUNTIME_DATA_USER_PTRS];
	void *		uclose[NT_RUNTIME_DATA_USER_PTRS];
	int32_t		udat32[NT_RUNTIME_DATA_USER_INT32_SLOTS];
	int64_t		udat64[NT_RUNTIME_DATA_USER_INT64_SLOTS];
	uintptr_t	buffer[];
} nt_runtime_data, nt_rtdata;


typedef struct _nt_runtime_data_block {
	void *	addr;
	size_t	size;
	void *	remote_addr;
	size_t	remote_size;
	int32_t	flags;
} nt_runtime_data_block;


typedef struct _nt_create_process_params {
	__out		void *				hprocess;
	__out		void *				hthread;
	__out		nt_client_id			cid;
	__out		nt_process_basic_information	pbi;
	__in		void *				himage;
	__in		wchar16_t *			image_name;
	__in		wchar16_t *			cmd_line;
	__in		wchar16_t *			environment;
	__in		nt_runtime_data_block *		rtblock;
	__in		uint32_t			desired_access_process;
	__in		uint32_t			desired_access_thread;
	__in		nt_object_attributes *		obj_attr_process;
	__in		nt_object_attributes *		obj_attr_thread;
	__in		uint32_t			creation_flags_process;
	__in		uint32_t			creation_flags_thread;
	__in		nt_process_parameters *		process_params;
	__in_out	nt_create_process_info *	create_process_info;
	__in		nt_create_process_ext_params *	create_process_ext_params;
	__in_out	uintptr_t *			buffer;
	__in		size_t				buflen;
} nt_create_process_params;


typedef int32_t	__stdcall ntapi_zw_create_process(
	__out	void **			hprocess,
	__in	uint32_t		desired_access,
	__in	nt_object_attributes *	obj_attr,
	__in	void *			hinherit_from_process,
	__in	unsigned char		inherit_handles,
	__in	void *			hsection 	__optional,
	__in	void *			hdebug_port	__optional,
	__in	void *			hexception_port __optional);


/* zw_create_user_process: newer OS versions only */
typedef int32_t __stdcall ntapi_zw_create_user_process(
	__out		void **				hprocess,
	__out		void **				hthread,
	__in		uint32_t			desired_access_process,
	__in		uint32_t			desired_access_thread,
	__in		nt_object_attributes *		obj_attr_process	__optional,
	__in		nt_object_attributes *		obj_attr_thread		__optional,
	__in		uint32_t			creation_flags_process,
	__in		uint32_t			creation_flags_thread,
	__in		nt_process_parameters *		process_params		__optional,
	__in_out	nt_create_process_info *	create_process_info,
	__in		nt_create_process_ext_params *	create_process_ext_params);


typedef int32_t	__stdcall ntapi_zw_open_process(
	__out	void **			hprocess,
	__in	uint32_t		desired_access,
	__in	nt_object_attributes *	obj_attr,
	__in	nt_client_id *		cid		__optional);


typedef int32_t	__stdcall ntapi_zw_terminate_process(
	__in	void *		hprocess	__optional,
	__in	int32_t		status);


typedef int32_t	__stdcall ntapi_zw_query_information_process(
	__in	void *			hprocess,
	__in	nt_process_info_class	process_info_class,
	__out	void *			process_info,
	__in	size_t			process_info_length,
	__out	uint32_t *		returned_length		__optional);


typedef int32_t	__stdcall ntapi_zw_set_information_process(
	__in	void *			hprocess,
	__in	nt_process_info_class	process_info_class,
	__in	void *			process_info,
	__in	uint32_t		process_info_length);


typedef int32_t __stdcall ntapi_zw_flush_instruction_cache(
	__in	void *	hprocess,
	__in	void *	base_addr	__optional,
	__in	size_t	flush_size);


typedef int32_t	__stdcall ntapi_rtl_create_process_parameters(
	__out 	nt_process_parameters **	process_params,
	__in	nt_unicode_string *		image_file,
	__in	nt_unicode_string *		dll_path		__optional,
	__in	nt_unicode_string *		current_directory	__optional,
	__in	nt_unicode_string *		command_line		__optional,
	__in	wchar16_t *			environment 		__optional,
	__in	nt_unicode_string *		window_title		__optional,
	__in	nt_unicode_string *		desktop_info		__optional,
	__in	nt_unicode_string *		shell_info		__optional,
	__in	nt_unicode_string *		runtime_info		__optional);


typedef void *  __stdcall ntapi_rtl_normalize_process_params(
	__in 	nt_process_parameters *	process_params);


typedef int32_t	__stdcall ntapi_rtl_destroy_process_parameters(
	__in 	nt_process_parameters *		process_params);


typedef nt_debug_buffer * __stdcall ntapi_rtl_create_query_debug_buffer(
	__in 	size_t	size,
	__in	int32_t	event_pair);


typedef int32_t	__stdcall ntapi_rtl_destroy_query_debug_buffer(
	__in 	nt_debug_buffer *	debug_buffer);


typedef int32_t	__stdcall ntapi_rtl_query_process_debug_information(
	__in		uintptr_t		process_id,
	__in 		uint32_t		debug_info_class_mask,
	__in_out	nt_debug_buffer *	debug_buffer);


typedef int32_t __stdcall ntapi_rtl_clone_user_process(
	__in	uint32_t		process_flags,
	__in	nt_sd *			process_sec_desc	__optional,
	__in	nt_sd *			thread_sec_desc		__optional,
	__in	void *			hport_debug		__optional,
	__out	nt_user_process_info *	process_info);


/* extensions */
typedef intptr_t __fastcall ntapi_tt_fork(
	__out	void **		hprocess,
	__out	void **		hthread);


typedef int32_t	__stdcall ntapi_tt_create_remote_process_params(
	__in	void *				hprocess,
	__out 	nt_process_parameters **	rprocess_params,
	__in	nt_unicode_string *		image_file,
	__in	nt_unicode_string *		dll_path		__optional,
	__in	nt_unicode_string *		current_directory	__optional,
	__in	nt_unicode_string *		command_line		__optional,
	__in	wchar16_t *			environment 		__optional,
	__in	nt_unicode_string *		window_title		__optional,
	__in	nt_unicode_string *		desktop_info		__optional,
	__in	nt_unicode_string *		shell_info		__optional,
	__in	nt_unicode_string *		runtime_data		__optional);


typedef int32_t __stdcall ntapi_tt_create_native_process(
	__out	nt_create_process_params *	params);


typedef int32_t __stdcall ntapi_tt_get_runtime_data(
	__out		nt_runtime_data **	pdata,
	__in		wchar16_t **		argv);

typedef int32_t __stdcall ntapi_tt_init_runtime_data(
	__in_out	nt_runtime_data *	rtdata);

typedef int32_t __stdcall ntapi_tt_update_runtime_data(
	__in_out	nt_runtime_data *	rtdata);

typedef int32_t __stdcall ntapi_tt_exec_map_image_as_data(
	__in_out	nt_executable_image *	image);


typedef int32_t	__stdcall ntapi_tt_exec_unmap_image(
	__in		nt_executable_image *	image);

#endif
