#ifndef _NT_SYSINFO_H_
#define _NT_SYSINFO_H_

#include <psxtypes/psxtypes.h>
#include "nt_object.h"
#include "nt_memory.h"

typedef enum _nt_system_info_class {
	NT_SYSTEM_INFORMATION_CLASS_MIN 		= 0,
	NT_SYSTEM_BASIC_INFORMATION 			= 0,
	NT_SYSTEM_PROCESSOR_INFORMATION 		= 1,
	NT_SYSTEM_PERFORMANCE_INFORMATION 		= 2,
	NT_SYSTEM_TIME_OF_DAY_INFORMATION 		= 3,
	NT_SYSTEM_NOT_IMPLEMENTED1 			= 4,
	NT_SYSTEM_PROCESS_INFORMATION 			= 5,
	NT_SYSTEM_CALL_COUNTS 				= 6,
	NT_SYSTEM_DEVICE_INFORMATION 			= 7,
	NT_SYSTEM_PROCESSOR_TIMES 			= 8,
	NT_SYSTEM_GLOBAL_FLAG 				= 9,
	NT_SYSTEM_NOT_IMPLEMENTED2 			= 10,
	NT_SYSTEM_CALL_TIME_INFORMATION 		= 10,
	NT_SYSTEM_MODULE_INFORMATION 			= 11,
	NT_SYSTEM_LOCK_INFORMATION 			= 12,
	NT_SYSTEM_NOT_IMPLEMENTED3 			= 13,
	NT_SYSTEM_NOT_IMPLEMENTED4 			= 14,
	NT_SYSTEM_NOT_IMPLEMENTED5 			= 15,
	NT_SYSTEM_HANDLE_INFORMATION 			= 16,
	NT_SYSTEM_OBJECT_INFORMATION 			= 17,
	NT_SYSTEM_PAGE_FILE_INFORMATION 		= 18,
	NT_SYSTEM_INSTRUCTION_EMULATION_COUNTS 		= 19,
	NT_SYSTEM_INVALID_INFO_CLASS1 			= 20,
	NT_SYSTEM_CACHE_INFORMATION 			= 21,
	NT_SYSTEM_POOL_TAG_INFORMATION 			= 22,
	NT_SYSTEM_PROCESSOR_STATISTICS 			= 23,
	NT_SYSTEM_DPC_INFORMATION 			= 24,
	NT_SYSTEM_NOT_IMPLEMENTED6 			= 25,
	NT_SYSTEM_LOAD_IMAGE 				= 26,
	NT_SYSTEM_UNLOAD_IMAGE 				= 27,
	NT_SYSTEM_TIME_ADJUSTMENT 			= 28,
	NT_SYSTEM_NOT_IMPLEMENTED7 			= 29,
	NT_SYSTEM_NOT_IMPLEMENTED8 			= 30,
	NT_SYSTEM_NOT_IMPLEMENTED9 			= 31,
	NT_SYSTEM_CRASH_DUMP_INFORMATION 		= 32,
	NT_SYSTEM_EXCEPTION_INFORMATION 		= 33,
	NT_SYSTEM_CRASH_DUMP_STATE_INFORMATION 		= 34,
	NT_SYSTEM_KERNEL_DEBUGGER_INFORMATION 		= 35,
	NT_SYSTEM_CONTEXT_SWITCH_INFORMATION 		= 36,
	NT_SYSTEM_REGISTRY_QUOTA_INFORMATION 		= 37,
	NT_SYSTEM_LOAD_AND_CALL_IMAGE 			= 38,
	NT_SYSTEM_PRIORITY_SEPARATION 			= 39,
	NT_SYSTEM_NOT_IMPLEMENTED10 			= 40,
	NT_SYSTEM_NOT_IMPLEMENTED11 			= 41,
	NT_SYSTEM_INVALID_INFO_CLASS2 			= 42,
	NT_SYSTEM_INVALID_INFO_CLASS3 			= 43,
	NT_SYSTEM_CURRENT_TIME_ZONE_INFORMATION 	= 44,
	NT_SYSTEM_TIME_ZONE_INFORMATION 		= 44,
	NT_SYSTEM_LOOKASIDE_INFORMATION 		= 45,
	NT_SYSTEM_SET_TIME_SLIP_EVENT 			= 46,
	NT_SYSTEM_CREATE_SESSION 			= 47,
	NT_SYSTEM_DELETE_SESSION 			= 48,
	NT_SYSTEM_INVALID_INFO_CLASS4 			= 49,
	NT_SYSTEM_RANGE_START_INFORMATION 		= 50,
	NT_SYSTEM_VERIFIER_INFORMATION 			= 51,
	NT_SYSTEM_ADD_VERIFIER 				= 52,
	NT_SYSTEM_SESSION_PROCESSES_INFORMATION		= 53,
	NT_SYSTEM_INFORMATION_CLASS_MAX
} nt_system_info_class;


typedef enum _nt_thread_state {
	NT_THREAD_STATE_INITIALIZED	= 0,
	NT_THREAD_STATE_READY		= 1,
	NT_THREAD_STATE_RUNNING		= 2,
	NT_THREAD_STATE_STANDBY		= 3,
	NT_THREAD_STATE_TERMINATED	= 4,
	NT_THREAD_STATE_WAIT		= 5,
	NT_THREAD_STATE_TRANSITION	= 6,
	NT_THREAD_STATE_UNKNOWN		= 7
} nt_thread_state;


typedef enum _nt_kwait_reason {
	NT_KWAIT_EXECUTIVE 		= 0,
	NT_KWAIT_FREE_PAGE 		= 1,
	NT_KWAIT_PAGE_IN 		= 2,
	NT_KWAIT_POOL_ALLOCATION 	= 3,
	NT_KWAIT_DELAY_EXECUTION 	= 4,
	NT_KWAIT_SUSPENDED 		= 5,
	NT_KWAIT_USER_REQUEST 		= 6,
	NT_KWAIT_WR_EXECUTIVE 		= 7,
	NT_KWAIT_WR_FREE_PAGE 		= 8,
	NT_KWAIT_WR_PAGE_IN 		= 9,
	NT_KWAIT_WR_POOL_ALLOCATION 	= 10,
	NT_KWAIT_WR_DELAY_EXECUTION 	= 11,
	NT_KWAIT_WR_SUSPENDED 		= 12,
	NT_KWAIT_WR_USER_REQUEST 	= 13,
	NT_KWAIT_WR_EVENT_PAIR 		= 14,
	NT_KWAIT_WR_QUEUE 		= 15,
	NT_KWAIT_WR_LPC_RECEIVE 	= 16,
	NT_KWAIT_WR_LPC_REPLY 		= 17,
	NT_KWAIT_WR_VIRTUAL_MEMORY 	= 18,
	NT_KWAIT_WR_PAGE_OUT 		= 19,
	NT_KWAIT_WR_RENDEZVOUS 		= 20,
	NT_KWAIT_SPARE2 		= 21,
	NT_KWAIT_SPARE3 		= 22,
	NT_KWAIT_SPARE4 		= 23,
	NT_KWAIT_SPARE5 		= 24,
	NT_KWAIT_WR_CALLOUT_STACK 	= 25,
	NT_KWAIT_WR_KERNEL 		= 26,
	NT_KWAIT_WR_RESOURCE 		= 27,
	NT_KWAIT_WR_PUSH_LOCK 		= 28,
	NT_KWAIT_WR_MUTEX 		= 29,
	NT_KWAIT_WR_QUANTUM_END 	= 30,
	NT_KWAIT_WR_DISPATCH_INT 	= 31,
	NT_KWAIT_WR_PREEMPTED 		= 32,
	NT_KWAIT_WR_YIELD_EXECUTION 	= 33,
	NT_KWAIT_WR_FAST_MUTEX 		= 34,
	NT_KWAIT_WR_GUARDED_MUTEX 	= 35,
	NT_KWAIT_WR_RUNDOWN 		= 36,
	NT_KWAIT_MAXIMUM_WAIT_REASON 	= 37
} nt_kwait_reason;


typedef enum _nt_pool_type {
	NT_NON_PAGED_POOL,
	NT_NON_PAGED_POOL_EXECUTE			= 0x0000 + NT_NON_PAGED_POOL,
	NT_PAGED_POOL,
	NT_NON_PAGED_POOL_MUST_SUCCEED			= 0x0002 + NT_NON_PAGED_POOL,
	NT_DONT_USE_THIS_TYPE,
	NT_NON_PAGED_POOL_CACHE_ALIGNED			= 0x0004 + NT_NON_PAGED_POOL,
	NT_PAGED_POOL_CACHE_ALIGNED,
	NT_NON_PAGED_POOL_CACHE_ALIGNED_MUST_S		= 0x0006 + NT_NON_PAGED_POOL,
	NT_MAX_POOL_TYPE,
	NT_NON_PAGED_POOL_BASE				= 0x0000,
	NT_NON_PAGED_POOL_BASE_MUST_SUCCEED		= 0x0002 + NT_NON_PAGED_POOL_BASE,
	NT_NON_PAGED_POOL_BASE_CACHE_ALIGNED		= 0x0004 + NT_NON_PAGED_POOL_BASE,
	NT_NON_PAGED_POOL_BASE_CACHE_ALIGNED_MUST_S	= 0x0006 + NT_NON_PAGED_POOL_BASE,
	NT_NON_PAGED_POOL_SESSION			= 0x0020,
	NT_PAGED_POOL_SESSION				= 0x0001 + NT_NON_PAGED_POOL_SESSION,
	NT_NON_PAGED_POOL_MUST_SUCCEED_SESSION		= 0x0001 + NT_PAGED_POOL_SESSION,
	NT_DONT_USE_THIS_TYPE_SESSION			= 0x0001 + NT_NON_PAGED_POOL_MUST_SUCCEED_SESSION,
	NT_NON_PAGED_POOL_CACHE_ALIGNED_SESSION		= 0x0001 + NT_DONT_USE_THIS_TYPE_SESSION,
	NT_PAGED_POOL_CACHE_ALIGNED_SESSION		= 0x0001 + NT_NON_PAGED_POOL_CACHE_ALIGNED_SESSION,
	NT_NON_PAGED_POOL_CACHE_ALIGNED_MUST_S_SESSION	= 0x0001 + NT_PAGED_POOL_CACHE_ALIGNED_SESSION,
	NT_NON_PAGED_POOL_NX				= 0x0200,
	NT_NON_PAGED_POOL_NX_CACHE_ALIGNED		= 0x0004 + NT_NON_PAGED_POOL_NX,
	NT_NON_PAGED_POOL_SESSION_NX			= 0x0020 + NT_NON_PAGED_POOL_NX
} nt_pool_type;


typedef enum _nt_shutdown_action {
	NT_SHUTDOWN_NO_REBOOT,
	NT_SHUTDOWN_REBOOT,
	NT_SHUTDOWN_POWER_OFF
} nt_shutdown_action;


typedef enum _nt_debug_control_code {
	NT_DEBUG_GET_TRACE_INFORMATION = 1,
	NT_DEBUG_SET_INTERNAL_BREAKPOINT,
	NT_DEBUG_SET_SPECIAL_CALL,
	NT_DEBUG_CLEAR_SPECIAL_CALLS,
	NT_DEBUG_QUERY_SPECIAL_CALLS,
	NT_DEBUG_DBG_BREAK_POINT,
	NT_DEBUG_MAXIMUM
} nt_debug_control_code;



/* nt_system_global_flag constants */
#define NT_FLGSTOP_ON_EXCEPTION			(uint32_t)0x00000001
#define NT_FLGSHOW_LDR_SNAPS			(uint32_t)0x00000002
#define NT_FLGDEBUG_INITIAL_COMMAND		(uint32_t)0x00000004
#define NT_FLGSTOP_ON_HUNG_GUI			(uint32_t)0x00000008
#define NT_FLGHEAP_ENABLE_TAIL_CHECK		(uint32_t)0x00000010
#define NT_FLGHEAP_ENABLE_FREE_CHECK		(uint32_t)0x00000020
#define NT_FLGHEAP_VALIDATE_PARAMETERS		(uint32_t)0x00000040
#define NT_FLGHEAP_VALIDATE_ALL			(uint32_t)0x00000080
#define NT_FLGPOOL_ENABLE_TAIL_CHECK		(uint32_t)0x00000100
#define NT_FLGPOOL_ENABLE_FREE_CHECK		(uint32_t)0x00000200
#define NT_FLGPOOL_ENABLE_TAGGING		(uint32_t)0x00000400
#define NT_FLGHEAP_ENABLE_TAGGING		(uint32_t)0x00000800
#define NT_FLGUSER_STACK_TRACE_DB		(uint32_t)0x00001000
#define NT_FLGKERNEL_STACK_TRACE_DB		(uint32_t)0x00002000
#define NT_FLGMAINTAIN_OBJECT_TYPELIST		(uint32_t)0x00004000
#define NT_FLGHEAP_ENABLE_TAG_BY_DLL		(uint32_t)0x00008000
#define NT_FLGIGNORE_DEBUG_PRIV			(uint32_t)0x00010000
#define NT_FLGENABLE_CSRDEBUG			(uint32_t)0x00020000
#define NT_FLGENABLE_KDEBUG_SYMBOL_LOAD		(uint32_t)0x00040000
#define NT_FLGDISABLE_PAGE_KERNEL_STACKS	(uint32_t)0x00080000
#define NT_FLGHEAP_ENABLE_CALL_TRACING		(uint32_t)0x00100000
#define NT_FLGHEAP_DISABLE_COALESCING		(uint32_t)0x00200000
#define NT_FLGENABLE_CLOSE_EXCEPTIONS		(uint32_t)0x00400000
#define NT_FLGENABLE_EXCEPTION_LOGGING		(uint32_t)0x00800000
#define NT_FLGENABLE_DBGPRINT_BUFFERING		(uint32_t)0x08000000

/* nt_system_handle_information constants */
/* FIXME: verify that these values are indeed reversed when compared with the flags returned by zw_query_object */
#define NT_HANDLE_PROTECT_FROM_CLOSE		(unsigned char)0x01
#define NT_HANDLE_INHERIT			(unsigned char)0x02


/* nt_system_object flag constants */
#define NT_FLG_SYSTEM_OBJECT_KERNEL_MODE            (uint32_t)0x02
#define NT_FLG_SYSTEM_OBJECT_CREATOR_INFO           (uint32_t)0x04
#define NT_FLG_SYSTEM_OBJECT_EXCLUSIVE              (uint32_t)0x08
#define NT_FLG_SYSTEM_OBJECT_PERMANENT              (uint32_t)0x10
#define NT_FLG_SYSTEM_OBJECT_DEFAULT_SECURITY_QUOTA (uint32_t)0x20
#define NT_FLG_SYSTEM_OBJECT_SINGLE_HANDLE_ENTRY    (uint32_t)0x40


typedef struct _nt_system_information_buffer {
	size_t		count;
	size_t		mark;
} nt_system_information_buffer;


typedef struct _nt_system_information_snapshot {
	nt_system_information_buffer *	buffer;
	void *				pcurrent;
	size_t				info_len;
	size_t				max_len;
	nt_system_info_class		sys_info_class;
} nt_system_information_snapshot;


typedef struct _nt_system_basic_information {
	uint32_t  	unknown;
	uint32_t  	max_increment;
	uint32_t  	physical_page_size;
	uint32_t  	physical_page_count;
	uint32_t  	physical_page_lowest;
	uint32_t  	physical_page_highest;
	uint32_t  	allocation_granularity;
	uint32_t  	user_address_lowest;
	uint32_t  	user_address_highest;
	uint32_t  	active_processors;
	unsigned char	processor_count;
} nt_system_basic_information;


typedef struct _nt_system_processor_information {
	uint16_t	processor_architecture;
	uint16_t	processor_level;
	uint16_t	processor_revision;
	uint16_t	unknown;
	uint32_t	feature_bits;
} nt_system_processor_information;


typedef struct _nt_system_performance_information {
	nt_large_integer	idle_time;
	nt_large_integer	read_transfer_count;
	nt_large_integer	write_transfer_count;
	nt_large_integer	other_transfer_count;
	uint32_t		read_operation_count;
	uint32_t		write_operation_count;
	uint32_t		other_operation_count;
	uint32_t		available_pages;
	uint32_t		total_committed_pages;
	uint32_t		total_commit_limit;
	uint32_t		peak_commitment;
	uint32_t		page_faults;
	uint32_t		write_copy_faults;
	uint32_t		transition_faults;
	uint32_t		cache_transition_faults;
	uint32_t		demand_zero_faults;
	uint32_t		pages_read;
	uint32_t		page_read_ios;
	uint32_t		cache_reads;
	uint32_t		cache_ios;
	uint32_t		pagefile_pages_written;
	uint32_t		pagefile_page_write_ios;
	uint32_t		mapped_file_pages_written;
	uint32_t		mapped_file_page_write_ios;
	uint32_t		paged_pool_usage;
	uint32_t		non_paged_pool_usage;
	uint32_t		paged_pool_allocs;
	uint32_t		paged_pool_frees;
	uint32_t		non_paged_pool_allocs;
	uint32_t		non_paged_pool_frees;
	uint32_t		total_free_system_ptes;
	uint32_t		system_code_page;
	uint32_t		total_system_driver_pages;
	uint32_t		total_system_code_pages;
	uint32_t		small_non_paged_lookaside_list_allocate_hits;
	uint32_t		small_paged_lookaside_list_allocate_hits;
	uint32_t		reserved3;
	uint32_t		mm_system_cache_page;
	uint32_t		paged_pool_page;
	uint32_t		system_driver_page;
	uint32_t		fast_read_no_wait;
	uint32_t		fast_read_wait;
	uint32_t		fast_read_resource_miss;
	uint32_t		fast_read_not_possible;
	uint32_t		fast_mdl_read_no_wait;
	uint32_t		fast_mdl_read_wait;
	uint32_t		fast_mdl_read_resource_miss;
	uint32_t		fast_mdl_read_not_possible;
	uint32_t		map_data_no_wait;
	uint32_t		map_data_wait;
	uint32_t		map_data_no_wait_miss;
	uint32_t		map_data_wait_miss;
	uint32_t		pin_mapped_data_count;
	uint32_t		pin_read_no_wait;
	uint32_t		pin_read_wait;
	uint32_t		pin_read_no_wait_miss;
	uint32_t		pin_read_wait_miss;
	uint32_t		copy_read_no_wait;
	uint32_t		copy_read_wait;
	uint32_t		copy_read_no_wait_miss;
	uint32_t		copy_read_wait_miss;
	uint32_t		mdl_read_no_wait;
	uint32_t		mdl_read_wait;
	uint32_t		mdl_read_no_wait_miss;
	uint32_t		mdl_read_wait_miss;
	uint32_t		read_ahead_ios;
	uint32_t		lazy_write_ios;
	uint32_t		lazy_write_pages;
	uint32_t		data_flushes;
	uint32_t		data_pages;
	uint32_t		context_switches;
	uint32_t		first_level_tb_fills;
	uint32_t		second_level_tb_fills;
	uint32_t		system_calls;
} nt_system_performance_information;


typedef struct _nt_system_time_of_day_information {
	nt_large_integer	boot_time;
	nt_large_integer	current_time;
	nt_large_integer	time_zone_bias;
	uint32_t		current_time_zone_id;
} nt_system_time_of_day_information;


typedef struct _nt_system_threads {
	nt_large_integer	kernel_time;
	nt_large_integer	user_time;
	nt_large_integer	create_time;
	uint32_t		wait_time;
	void *			start_address;
	nt_client_id		client_id;
	uint32_t		priority;
	uint32_t		base_priority;
	uint32_t		context_switch_count;
	nt_thread_state		state;
	nt_kwait_reason		wait_reason;
} nt_system_threads;


typedef struct _nt_system_processes {
	uint32_t		next_entry_delta;
	uint32_t		thread_count;
	uint32_t		reserved_1st[6];
	nt_large_integer	create_time;
	nt_large_integer	user_time;
	nt_large_integer	kernel_time;
	nt_unicode_string	process_name;
	uint32_t		base_priority;
	uint32_t		process_id;
	uint32_t		inherited_from_process_id;
	uint32_t		handle_count;
	uint32_t		reserved_2nd[2];
	nt_vm_counters		vm_counters;
	nt_io_counters		io_counters;
	nt_system_threads	threads[];
} nt_system_processes;


typedef struct _nt_syscall_information {
	uint32_t	size;
	uint32_t	number_of_descriptor_tables;
	uint32_t	number_of_routines_in_table[1];
	uint32_t	syscall_counts[];
} nt_syscall_information;


typedef struct _nt_system_configuration_information {
	uint32_t	disk_count;
	uint32_t	floppy_count;
	uint32_t	cd_rom_count;
	uint32_t	tape_count;
	uint32_t	serial_count;
	uint32_t	parallel_count;
} nt_system_configuration_information;


typedef struct _nt_system_process_times {
	nt_large_integer	idle_time;
	nt_large_integer	kernel_time;
	nt_large_integer	user_time;
	nt_large_integer	dpc_time;
	nt_large_integer	interrupt_time;
	uint32_t		interrupt_count;
} nt_system_process_times;


typedef struct _nt_system_global_flag {
	uint32_t	global_flag;
} nt_system_global_flag;


typedef struct _nt_system_module_information {
	uint32_t	reserved_1st;
	uint32_t	reserved_2nd;
	void *		base;
	uint32_t	size;
	uint32_t	flags;
	uint16_t	index;
	uint16_t	unknown;
	uint16_t	load_count;
	uint16_t	path_length;
	char		image_name[256];
} nt_system_module_information_entry;


typedef struct _nt_system_lock_information {
	void *		address;
	uint16_t	type;
	uint16_t	reserved_1st;
	uint32_t	exclusive_owner_thread_id;
	uint32_t	active_count;
	uint32_t	contention_count;
	uint32_t	reserved_2nd;
	uint32_t	reserved_3rd;
	uint32_t	number_of_shared_waiters;
	uint32_t	number_of_exclusive_waiters;
} nt_system_lock_information;


typedef struct _nt_system_handle_information {
	uint32_t	process_id;
	unsigned char	object_type_number;
	unsigned char	flags;
	uint16_t	handle;
	void *		object;
	uint32_t	granted_access;
#if defined (__NT64)
	uint32_t	granted_access_padding;
#endif
} nt_system_handle_information;


typedef struct _nt_object_type_information {
	nt_unicode_string	name;
	uint32_t		object_count;
	uint32_t		handle_count;
	uint32_t		reserved1[4];
	uint32_t		peak_object_count;
	uint32_t		peak_handle_count;
	uint32_t		reserved2[4];
	uint32_t		invalid_attributes;
	nt_generic_mapping	generic_mapping;
	uint32_t		valid_access;
	unsigned char		unknown;
	unsigned char		maintain_handle_database;
	nt_pool_type		pool_type;
	uint32_t		paged_pool_usage;
	uint32_t		non_paged_pool_usage;
} nt_object_type_information, nt_oti;


typedef struct _nt_system_object_type_information {
	uint32_t		next_entry_offset;
	uint32_t		object_count;
	uint32_t		handle_count;
	uint32_t		type_number;
	uint32_t		invalid_attributes;
	nt_generic_mapping	generic_mapping;
	uint32_t		valid_access_mask;
	unsigned char		pool_type;
	unsigned char		unknown;
	nt_unicode_string	name;
} nt_system_object_type_information;


typedef struct _nt_system_object_information {
	uint32_t		next_entry_offset;
	void *			object;
	uint32_t		creator_process_id;
	uint16_t		unknown;
	uint16_t		flags;
	uint32_t		pointer_count;
	uint32_t		handle_count;
	uint32_t		paged_pool_usage;
	uint32_t		non_paged_pool_usage;
	uint32_t		exclusive_process_id;
	nt_security_descriptor *security_descriptor;
	nt_unicode_string	name;
} nt_system_object_information;


typedef struct _nt_system_pagefile_information {
	uint32_t		next_entry_offset;
	uint32_t		current_size;
	uint32_t		total_used;
	uint32_t		peak_used;
	nt_unicode_string	file_name;
} nt_system_pagefile_information;


typedef struct _nt_system_instruction_emulation_information {
	uint32_t  segment_not_present;
	uint32_t  two_byte_opcode;
	uint32_t  es_prefix;
	uint32_t  cs_prefix;
	uint32_t  ss_prefix;
	uint32_t  ds_prefix;
	uint32_t  fs_Prefix;
	uint32_t  gs_prefix;
	uint32_t  oper32_prefix;
	uint32_t  addr32_prefix;
	uint32_t  insb;
	uint32_t  insw;
	uint32_t  outsb;
	uint32_t  outsw;
	uint32_t  pushfd;
	uint32_t  popfd;
	uint32_t  int_nn;
	uint32_t  into;
	uint32_t  iretd;
	uint32_t  inb_imm;
	uint32_t  inw_imm;
	uint32_t  outb_imm;
	uint32_t  outw_imm;
	uint32_t  inb;
	uint32_t  inw;
	uint32_t  outb;
	uint32_t  outw;
	uint32_t  lock_prefix;
	uint32_t  repne_prefix;
	uint32_t  rep_prefix;
	uint32_t  hlt;
	uint32_t  cli;
	uint32_t  sti;
	uint32_t  generic_invalid_opcode;
} nt_system_instruction_emulation_information;


typedef struct _nt_system_pool_tag_information {
	char		tag[4];
	uint32_t	paged_pool_allocs;
	uint32_t	paged_pool_frees;
	uint32_t 	paged_pool_usage;
	uint32_t 	non_paged_pool_allocs;
	uint32_t 	non_paged_pool_frees;
	uint32_t 	non_paged_pool_usage;
} nt_system_pool_tag_information;


typedef struct _nt_system_processor_statistics {
	uint32_t  context_switches;
	uint32_t  dpc_count;
	uint32_t  dpc_request_rate;
	uint32_t  time_increment;
	uint32_t  dpc_bypass_count;
	uint32_t  apc_bypass_count;
} nt_system_processor_statistics;


typedef struct _nt_system_dpc_information {
	uint32_t	reserved;
	uint32_t	maximum_dpc_queue_depth;
	uint32_t	minimum_dpc_rate;
	uint32_t 	adjust_dpc_threshold;
	uint32_t	ideal_dpc_rate;
} nt_system_dpc_information;


typedef struct _nt_system_load_image {
	nt_unicode_string	module_name;
	void *			module_base;
	void *			section_pointer;
	void *			entry_point;
	void *			export_directory;
} nt_system_load_image;


typedef struct _nt_system_unload_image {
	void *	module_base;
} nt_system_unload_image;


typedef struct _nt_system_query_time_adjustment {
	uint32_t	time_adjustment;
	uint32_t	maximum_increment;
	int32_t		time_synchronization;
} nt_system_query_time_adjustment;


typedef struct _nt_system_set_time_adjustment {
	uint32_t	time_adjustment;
	int32_t		time_synchronization;
} nt_system_set_time_adjustment;


typedef struct _nt_system_crash_dump_information {
	void *	crash_dump_section_handle;
	void *	unknown;
} nt_system_crash_dump_information;


typedef struct _nt_system_exception_information {
	uint32_t	alignment_fixup_count;
	uint32_t	exception_dispatch_count;
	uint32_t	floating_emulation_count;
	uint32_t	reserved;
} nt_system_exception_information;


typedef struct _nt_system_crash_dump_state_information {
	uint32_t	crash_dump_section_exists;
	uint32_t	unknown;
} nt_system_crash_dump_state_information;


typedef struct _nt_system_kernel_debugger_information {
	unsigned char	debugger_enabled;
	unsigned char	debugger_not_present;
} nt_system_kernel_debugger_information;


typedef struct _nt_system_context_switch_information {
	uint32_t	context_switches;
	uint32_t	context_switch_counters[11];
} nt_system_context_switch_information;


typedef struct _nt_system_registry_quota_information {
	uint32_t	registry_quota;
	uint32_t	registry_quota_in_use;
	uint32_t	paged_pool_size;
} nt_system_registry_quota_information;


typedef struct _nt_system_load_and_call_image {
	nt_unicode_string	module_name;
} nt_system_load_and_call_image;


typedef struct _nt_system_priority_separation {
	uint32_t	priority_separation;
} nt_system_priority_separation;


typedef struct _nt_system_time_zone_information {
	int32_t			bias;
	wchar16_t		standard_name[32];
	nt_large_integer	standard_date;
	int32_t			standard_bias;
	wchar16_t		daylight_name[32];
	nt_large_integer	daylight_date;
	int32_t			daylight_bias;
} nt_system_time_zone_information;


typedef struct _nt_system_lookaside_information {
	uint16_t	depth;
	uint16_t	maximum_depth;
	uint32_t	total_allocates;
	uint32_t	allocate_misses;
	uint32_t	total_frees;
	uint32_t	free_misses;
	nt_pool_type	type;
	uint32_t	tag;
	uint32_t	size;
} nt_system_lookaside_information;


typedef struct _nt_system_set_time_slip_event {
	void *	time_slip_event;
} nt_system_set_time_slip_event;


typedef struct _nt_system_create_session {
	uint32_t	session_id;
} nt_system_create_session;


typedef struct _nt_system_delete_session {
	uint32_t	session_id;
} nt_system_delete_session;


typedef struct _nt_system_range_start_information {
	void *	system_range_start;
} nt_system_range_start_information;


typedef struct _nt_system_session_processes_information {
	uint32_t	session_id;
	uint32_t	buffer_size;
	void *		buffer;
} nt_system_session_processes_information;


typedef struct _nt_system_pool_block {
	int32_t		allocated;
	uint16_t	unknown;
	uint32_t	size;
	char		tag[4];
} nt_system_pool_block;


typedef struct _nt_system_pool_blocks_information {
	uint32_t		pool_size;
	void *			pool_base;
	uint16_t		unknown;
	uint32_t		number_of_blocks;
	nt_system_pool_block	pool_blocks[];
} nt_system_pool_blocks_information;


typedef struct _nt_system_memory_usage {
	void *		name;
	uint16_t	valid;
	uint16_t	standby;
	uint16_t	modified;
	uint16_t	page_tables;
} nt_system_memory_usage;


typedef struct _nt_system_memory_usage_information {
	uint32_t		reserved;
	void *			end_of_data;
	nt_system_memory_usage	memory_usage[];
} nt_system_memory_usage_information;



typedef int32_t __stdcall ntapi_zw_query_system_information(
	__in		nt_system_info_class	sys_info_class,
	__in_out	void *			sys_info,
	__in		size_t			sys_info_length,
	__out		size_t *		returned_length	__optional);


typedef int32_t __stdcall ntapi_zw_set_system_information(
	__in		nt_system_info_class	sys_info_class,
	__in_out	void *			sys_info,
	__in		uint32_t		sys_info_length);


typedef int32_t __stdcall ntapi_zw_query_system_environment_value(
	__in	nt_unicode_string *	name,
	__out	void *			value,
	__in	size_t			value_length,
	__out	size_t *		returned_length	__optional);


typedef int32_t __stdcall ntapi_zw_set_system_environment_value(
	__in	nt_unicode_string *	name,
	__in	nt_unicode_string *	value);


typedef int32_t __stdcall ntapi_zw_shutdown_system(
	__in	nt_shutdown_action	action);


typedef int32_t __stdcall ntapi_zw_system_debug_control(
	__in	nt_debug_control_code	control_code,
	__in	void *			input_buffer		__optional,
	__in	uint32_t		input_buffer_length,
	__out	void *			output_buffer		__optional,
	__in	uint32_t		output_buffer_length,
	__out	uint32_t *		returned_length		__optional);

/* extension functions */
typedef int32_t __stdcall ntapi_tt_get_system_directory_native_path(
	__out	nt_mem_sec_name *	buffer,
	__in	uint32_t		buffer_size,
	__in	wchar16_t *		base_name,
	__in	uint32_t		base_name_size,
	__out	nt_unicode_string *	nt_path		__optional);


typedef int32_t __stdcall ntapi_tt_get_system_directory_dos_path(
	__in	void *			hsysdir		__optional,
	__out	wchar16_t *		buffer,
	__in	uint32_t		buffer_size,
	__in	wchar16_t *		base_name,
	__in	uint32_t		base_name_size,
	__out	nt_unicode_string *	nt_path		__optional);


typedef int32_t __stdcall ntapi_tt_get_system_directory_handle(
	__out	void **			hsysdir,
	__out	nt_mem_sec_name *	buffer		__optional,
	__in	uint32_t		buffer_size	__optional);


typedef int32_t __stdcall ntapi_tt_get_system_info_snapshot(
	__in_out nt_system_information_snapshot * sys_info_snapshot);

#endif
