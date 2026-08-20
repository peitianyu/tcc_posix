#ifndef _NT_JOB_H_
#define _NT_JOB_H_

#include <psxtypes/psxtypes.h>
#include "nt_object.h"

typedef enum _nt_job_object_info_class {
	NT_JOB_OBJECT_BASIC_ACCOUNTING_INFORMATION		= 0x01,
	NT_JOB_OBJECT_BASIC_LIMIT_INFORMATION			= 0x02,
	NT_JOB_OBJECT_BASIC_PROCESS_ID_LIST			= 0x03,
	NT_JOB_OBJECT_BASIC_U_I_RESTRICTIONS			= 0x04,
	NT_JOB_OBJECT_SECURITY_LIMIT_INFORMATION		= 0x05,
	NT_JOB_OBJECT_END_OF_JOB_TIME_INFORMATION		= 0x06,
	NT_JOB_OBJECT_ASSOCIATE_COMPLETION_PORT_INFORMATION	= 0x07,
	NT_JOB_OBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION	= 0x08,
	NT_JOB_OBJECT_EXTENDED_LIMIT_INFORMATION		= 0x09,

	NT_JOB_OBJECT_GROUP_INFORMATION				= 0x0B,
	NT_JOB_OBJECT_NOTIFICATION_LIMIT_INFORMATION		= 0x0C,
	NT_JOB_OBJECT_LIMIT_VIOLATION_INFORMATION		= 0x0D,
	NT_JOB_OBJECT_GROUP_INFORMATION_EX			= 0x0E,
	NT_JOB_OBJECT_CPU_RATE_CONTROL_INFORMATION		= 0x0F,
} nt_job_object_info_class;

/* job access bits */
#define NT_JOB_OBJECT_ASSIGN_PROCESS			0x000001
#define NT_JOB_OBJECT_SET_ATTRIBUTES			0x000002
#define NT_JOB_OBJECT_QUERY				0x000004
#define NT_JOB_OBJECT_TERMINATE				0x000008
#define NT_JOB_OBJECT_SET_SECURITY_ATTRIBUTES 		0x000010
#define NT_JOB_OBJECT_ALL_ACCESS			0x1F001F

/* job limit flags */
#define NT_JOB_OBJECT_LIMIT_WORKINGSET			0x00000001
#define NT_JOB_OBJECT_LIMIT_PROCESS_TIME		0x00000002
#define NT_JOB_OBJECT_LIMIT_JOB_TIME			0x00000004
#define NT_JOB_OBJECT_LIMIT_ACTIVE_PROCESS		0x00000008
#define NT_JOB_OBJECT_LIMIT_AFFINITY			0x00000010
#define NT_JOB_OBJECT_LIMIT_PRIORITY_CLASS		0x00000020
#define NT_JOB_OBJECT_LIMIT_PRESERVE_JOB_TIME		0x00000040
#define NT_JOB_OBJECT_LIMIT_SCHEDULING_CLASS		0x00000080
#define NT_JOB_OBJECT_LIMIT_PROCESS_MEMORY		0x00000100
#define NT_JOB_OBJECT_LIMIT_JOB_MEMORY			0x00000200
#define NT_JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION	0x00000400
#define NT_JOB_OBJECT_LIMIT_BREAKAWAY_OK		0x00000800
#define NT_JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK		0x00001000
#define NT_JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE		0x00002000
#define NT_JOB_OBJECT_LIMIT_SUBSET_AFFINITY		0x00004000


/* job object cpu rate control bits */
#define NT_JOB_OBJECT_CPU_RATE_CONTROL_ENABLE		0x0001
#define NT_JOB_OBJECT_CPU_RATE_CONTROL_WEIGHT_BASED	0x0002
#define NT_JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP		0x0004
#define NT_JOB_OBJECT_CPU_RATE_CONTROL_NOTIFY		0x0008


/* job object basic user interface restrictions bits */
#define NT_JOB_OBJECT_UILIMIT_HANDLES		0x00000001
#define NT_JOB_OBJECT_UILIMIT_READCLIPBOARD	0x00000002
#define NT_JOB_OBJECT_UILIMIT_WRITECLIPBOARD	0x00000004
#define NT_JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS	0x00000008
#define NT_JOB_OBJECT_UILIMIT_DISPLAYSETTINGS	0x00000010
#define NT_JOB_OBJECT_UILIMIT_GLOBALATOMS	0x00000020
#define NT_JOB_OBJECT_UILIMIT_DESKTOP		0x00000040
#define NT_JOB_OBJECT_UILIMIT_EXITWINDOWS	0x00000080


/* job security limit bits */
#define NT_JOB_OBJECT_SECURITY_NO_ADMIN		0x0001
#define NT_JOB_OBJECT_SECURITY_RESTRICTED_TOKEN	0x0002
#define NT_JOB_OBJECT_SECURITY_ONLY_TOKEN	0x0004
#define NT_JOB_OBJECT_SECURITY_FILTER_TOKENS	0x0008


/* end of job actions */
#define NT_JOB_OBJECT_TERMINATE_AT_END_OF_JOB	0
#define NT_JOB_OBJECT_POST_AT_END_OF_JOB	1


/* job associate completion port events */
#define NT_JOB_OBJECT_MSG_END_OF_JOB_TIME		1
#define NT_JOB_OBJECT_MSG_END_OF_PROCESS_TIME		2
#define NT_JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT		3
#define NT_JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO		4
#define NT_JOB_OBJECT_MSG_NEW_PROCESS			6
#define NT_JOB_OBJECT_MSG_EXIT_PROCESS			7
#define NT_JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS		8
#define NT_JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT		9
#define NT_JOB_OBJECT_MSG_JOB_MEMORY_LIMIT		10


typedef struct _nt_job_object_basic_accounting_information {
	nt_large_integer	total_user_time;
	nt_large_integer	total_kernel_time;
	nt_large_integer	this_period_total_user_time;
	nt_large_integer	this_period_total_kernel_time;
	int32_t	         	total_page_fault_count;
	int32_t	         	total_processes;
	int32_t	         	active_processes;
	int32_t	         	total_terminated_processes;
} nt_job_object_basic_accounting_information;


typedef struct _nt_job_object_basic_limit_information {
	nt_large_integer	per_process_user_time_limit;
	nt_large_integer	per_job_user_time_limit;
	uint32_t		limit_flags;
	size_t			minimum_working_set_size;
	size_t			maximum_working_set_size;
	uint32_t		active_process_limit;
	uintptr_t		affinity;
	uint32_t		priority_class;
	uint32_t		scheduling_class;
} nt_job_object_basic_limit_information;


typedef struct _nt_job_object_basic_and_io_accounting_information {
	nt_job_object_basic_accounting_information	basic_info;
	nt_io_counters					io_info;
} nt_job_object_basic_and_io_accounting_information;


typedef struct _nt_job_object_extended_limit_information {
	nt_job_object_basic_limit_information	basic_limit_information;
	nt_io_counters				io_info;
	size_t					process_memory_limit;
	size_t					job_memory_limit;
	size_t					peak_process_memory_used;
	size_t					peak_job_memory_used;
} nt_job_object_extended_limit_information;


typedef struct _nt_job_object_basic_process_id_list {
	uint32_t	number_of_assigned_processes;
	uint32_t	number_of_process_ids_in_list;
	uintptr_t	process_id_list[];
} nt_job_object_basic_process_id_list;


typedef struct _nt_job_object_basic_ui_restrictions {
	uint32_t	ui_restrictions_class;
} nt_job_object_basic_ui_restrictions;


typedef struct _nt_job_object_security_limit_information {
	uint32_t		security_limit_flags;
	void *			job_token;
	nt_token_groups *	sids_to_disable;
	nt_token_privileges *	privileges_to_delete;
	nt_token_groups *	restricted_sids;
} nt_job_object_security_limit_information;


typedef struct _nt_job_object_end_of_job_time_information {
	uint32_t	end_of_job_time_action;
} nt_job_object_end_of_job_time_information;


typedef struct _nt_job_object_associate_completion_port {
	void *	completion_key;
	void *	completion_port;
} nt_job_object_associate_completion_port;


typedef struct _nt_job_object_cpu_rate_control_information {
	uint32_t	control_flags;
	union {
		uint32_t	cpu_rate;
		uint32_t	weight;
	};
} nt_job_object_cpu_rate_control_information;



typedef int32_t __stdcall ntapi_zw_create_job_object(
	__out	void **			hjob,
	__in	uint32_t		desired_access,
	__in	nt_object_attributes *	obj_attr);


typedef int32_t __stdcall ntapi_zw_open_job_object(
	__out	void **			hjob,
	__in	uint32_t		desired_access,
	__in	nt_object_attributes *	obj_attr);


typedef int32_t __stdcall ntapi_zw_terminate_job_object(
	__in	void *		hjob,
	__in	int32_t		exit_status);


typedef int32_t __stdcall ntapi_zw_assign_process_to_job_object(
	__in	void *		hjob,
	__in	void *		hprocess);


typedef int32_t	__stdcall ntapi_zw_query_information_job_object(
	__in	void *				hjob,
	__in	nt_job_object_info_class	job_info_class,
	__out	void *				job_info,
	__in	size_t				job_info_length,
	__out	size_t *			returned_length	__optional);


typedef int32_t	__stdcall ntapi_zw_set_information_job_object(
	__in	void *				hjob,
	__in	nt_job_object_info_class	job_info_class,
	__in	void *				job_info,
	__in	size_t				job_info_length);

#endif
