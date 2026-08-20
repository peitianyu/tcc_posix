#ifndef _NT_SYNC_H_
#define _NT_SYNC_H_

#include <psxtypes/psxtypes.h>
#include "nt_object.h"

typedef enum _nt_wait_type {
	NT_WAIT_ALL,
	NT_WAIT_ANY
} nt_wait_type;


typedef enum _nt_timer_type {
	NT_NOTIFICATION_TIMER,
	NT_SYNCHRONIZATION_TIMER
} nt_timer_type;


typedef enum _nt_timer_info_class {
	NT_TIMER_BASIC_INFORMATION
} nt_timer_info_class;


typedef enum _nt_event_type {
	NT_NOTIFICATION_EVENT,
	NT_SYNCHRONIZATION_EVENT
} nt_event_type;


typedef enum _nt_event_states {
	NT_EVENT_NOT_SIGNALED,
	NT_EVENT_SIGNALED
} nt_event_states;


typedef enum _nt_event_info_class {
	NT_EVENT_BASIC_INFORMATION
} nt_event_info_class;


typedef enum _nt_semaphore_info_class {
	NT_SEMAPHORE_BASIC_INFORMATION
} nt_semaphore_info_class;


typedef enum _nt_mutant_info_class {
	NT_MUTANT_BASIC_INFORMATION
} nt_mutant_info_class;


typedef enum _nt_io_completion_info_class {
	NT_IO_COMPLETION_BASIC_INFORMATION
} nt_io_completion_info_class;


/* cache block size */
#define	NT_SYNC_BLOCK_SIZE		64

/* timer access bits */
#define NT_TIMER_QUERY_STATE		0x00000001U
#define NT_TIMER_MODIFY_STATE		0x00000002U
#define NT_TIMER_ALL_ACCESS 		0x001F0003U


/* event access bits */
#define NT_EVENT_QUERY_STATE		0x00000001U
#define NT_EVENT_MODIFY_STATE		0x00000002U
#define NT_EVENT_ALL_ACCESS 		0x001F0003U


/* semaphore access bits */
#define NT_SEMAPHORE_QUERY_STATE	0x00000001U
#define NT_SEMAPHORE_MODIFY_STATE	0x00000002U
#define NT_SEMAPHORE_ALL_ACCESS 	0x001F0003U


/* mutant access bits */
#define NT_MUTANT_QUERY_STATE		0x00000001U
#define NT_MUTANT_ALL_ACCESS 		0x001F0001U


/* io completion access bits */
#define NT_IO_COMPLETION_QUERY_STATE	0x00000001U
#define NT_IO_COMPLETION_MODIFY_STATE	0x00000002U
#define NT_IO_COMPLETION_ALL_ACCESS 	0x001F0003U

/* alertable threads */
#define NT_SYNC_NON_ALERTABLE		0x00000000U
#define NT_SYNC_ALERTABLE		0x00000001U

/* sync block flag bits */
#define NT_SYNC_BLOCK_YIELD_TO_SERVER	0x00000001U

typedef struct _nt_timer_basic_information {
	nt_large_integer	timer_remaining;
	int32_t			signal_state;
} nt_timer_basic_information;


typedef struct _nt_event_basic_information {
	nt_event_type		event_type;
	int32_t			signal_state;
} nt_event_basic_information, nt_ebi;


typedef struct _nt_semaphore_basic_information {
	int32_t		current_count;
	int32_t		max_count;
} nt_semaphore_basic_information;


typedef struct _nt_mutant_basic_information {
	int32_t		signal_state;
	int32_t		owned;
	int32_t		abandoned;
} nt_mutant_basic_information;


typedef struct _nt_io_completion_basic_information {
	int32_t		signal_state;
} nt_io_completion_basic_information;


typedef union __attr_aligned__(NT_SYNC_BLOCK_SIZE) _nt_sync_block {
	char		cache_line[NT_SYNC_BLOCK_SIZE];
	struct {
		int32_t		tid;
		int32_t		pid;
		uint32_t	flags;
		uint32_t	srvtid;
		uint32_t	lock_tries;
		uint32_t	ref_cnt;
		uint32_t	busy;
		int32_t		invalid;
		nt_timeout	lock_wait;
		void *		hwait;
		void *		hsignal;
		void *		hserver;
	};
} nt_sync_block;


typedef void	__stdcall nt_timer_apc_routine(
	void *		timer_context,
	uint32_t	timer_low_value,
	uint32_t	timer_high_value);


typedef int32_t __stdcall ntapi_zw_wait_for_single_object(
	__in	void *			handle,
	__in 	int32_t			alertable,
	__in	nt_large_integer *	timeout	__optional);


typedef int32_t __stdcall ntapi_zw_signal_and_wait_for_single_object(
	__in	void *			handle_to_signal,
	__in	void *			handle_to_wait,
	__in 	int32_t			alertable,
	__in	nt_large_integer *	timeout	__optional);


typedef int32_t __stdcall ntapi_zw_wait_for_multiple_objects(
	__in	uint32_t		handle_count,
	__in	void **			handles,
	__in	nt_wait_type		wait_type,
	__in 	int32_t			alertable,
	__in	nt_large_integer *	timeout	__optional);


typedef int32_t __stdcall ntapi_zw_create_timer(
	__out	void **			htimer,
	__in 	uint32_t 		desired_access,
	__in 	nt_object_attributes * 	obj_attr,
	__in 	nt_timer_type		timer_type);


typedef int32_t __stdcall ntapi_zw_open_timer(
	__out	void **			htimer,
	__in 	uint32_t 		desired_access,
	__in 	nt_object_attributes * 	obj_attr);


typedef int32_t __stdcall ntapi_zw_cancel_timer(
	__in	void *			htimer,
	__out 	int32_t *		previous_state	__optional);


typedef int32_t __stdcall ntapi_zw_set_timer(
	__in	void *			htimer,
	__in	nt_large_integer *	due_time,
	__in	nt_timer_apc_routine *	timer_apc_routine	__optional,
	__in	void *			timer_context,
	__in	int32_t			resume,
	__in	int32_t			period,
	__out	int32_t *		previous_state		__optional);


typedef int32_t __stdcall ntapi_zw_query_timer(
	__in	void *			htimer,
	__in	nt_timer_info_class	timer_info_class,
	__out	void *			timer_info,
	__in	size_t			timer_info_length,
	__out	size_t *		returned_length		__optional);


typedef int32_t __stdcall ntapi_zw_create_event(
	__out	void **			hevent,
	__in 	uint32_t 		desired_access,
	__in 	nt_object_attributes * 	obj_attr,
	__in 	nt_event_type		event_type,
	__in 	int32_t			initial_state);


typedef int32_t __stdcall ntapi_zw_open_event(
	__out	void **			hevent,
	__in 	uint32_t 		desired_access,
	__in 	nt_object_attributes * 	obj_attr);


typedef int32_t __stdcall ntapi_zw_set_event(
	__in	void *		hevent,
	__out	int32_t *	previous_state);


typedef int32_t __stdcall ntapi_zw_pulse_event(
	__in	void *		hevent,
	__out	int32_t *	previous_state);


typedef int32_t __stdcall ntapi_zw_reset_event(
	__in	void *		hevent,
	__out	int32_t *	previous_state);


typedef int32_t __stdcall ntapi_zw_clear_event(
	__in	void *		hevent);


typedef int32_t __stdcall ntapi_zw_query_event(
	__in	void *			hevent,
	__in	nt_event_info_class	event_info_class,
	__out	void *			event_info,
	__in	size_t			event_info_length,
	__out	size_t *		returned_length		__optional);


typedef int32_t __stdcall ntapi_zw_create_semaphore(
	__out	void **			hsemaphore,
	__in 	uint32_t 		desired_access,
	__in 	nt_object_attributes * 	obj_attr,
	__in 	int32_t			initial_count,
	__in 	int32_t			max_count);


typedef int32_t __stdcall ntapi_zw_open_semaphore(
	__out	void **			hsemaphore,
	__in 	uint32_t 		desired_access,
	__in 	nt_object_attributes * 	obj_attr);


typedef int32_t __stdcall ntapi_zw_release_semaphore(
	__in	void *		hsemaphore,
	__in	int32_t		release_count,
	__out	int32_t *	previous_count);


typedef int32_t __stdcall ntapi_zw_query_semaphore(
	__in	void *			hsemaphore,
	__in	nt_semaphore_info_class	semaphore_info_class,
	__out	void *			semaphore_info,
	__in	size_t			semaphore_info_length,
	__out	size_t *		returned_length		__optional);


typedef int32_t __stdcall ntapi_zw_create_mutant(
	__out	void **			hmutant,
	__in 	uint32_t 		desired_access,
	__in 	nt_object_attributes * 	obj_attr,
	__in	int32_t			initial_owner);


typedef int32_t __stdcall ntapi_zw_open_mutant(
	__out	void **			hmutant,
	__in 	uint32_t 		desired_access,
	__in 	nt_object_attributes * 	obj_attr);


typedef int32_t __stdcall ntapi_zw_release_mutant(
	__in	void *		hmutant,
	__out	int32_t *	previous_state);


typedef int32_t __stdcall ntapi_zw_query_mutant(
	__in	void *			hmutant,
	__in	nt_mutant_info_class	mutant_info_class,
	__out	void *			mutant_info,
	__in	size_t			mutant_info_length,
	__out	size_t *		returned_length		__optional);


typedef int32_t __stdcall ntapi_zw_create_io_completion(
	__out	void **			hio_completion,
	__in 	uint32_t 		desired_access,
	__in 	nt_object_attributes * 	obj_attr,
	__in	uint32_t		max_concurrent_threads);


typedef int32_t __stdcall ntapi_zw_open_io_completion(
	__out	void **			hio_completion,
	__in 	uint32_t 		desired_access,
	__in 	nt_object_attributes * 	obj_attr);


typedef int32_t __stdcall ntapi_zw_set_io_completion(
	__in	void *		hio_completion,
	__in	uint32_t	completion_key,
	__in	uint32_t	completion_value,
	__in	int32_t		status,
	__in	uint32_t	information);


typedef int32_t __stdcall ntapi_zw_remove_io_completion(
	__in	void *			hio_completion,
	__out	uint32_t *		completion_key,
	__out	uint32_t *		completion_value,
	__out	nt_io_status_block *	io_status_block,
	__in	nt_large_integer *	timeout);


typedef int32_t __stdcall ntapi_zw_query_io_completion(
	__in	void *			hio_completion,
	__in	nt_io_completion_info_class	io_completion_info_class,
	__out	void *			io_completion_info,
	__in	size_t			io_completion_info_length,
	__out	size_t *		returned_length		__optional);


typedef int32_t __stdcall ntapi_zw_create_event_pair(
	__out	void **			hevent_pair,
	__in 	uint32_t 		desired_access,
	__in 	nt_object_attributes * 	obj_attr);


typedef int32_t __stdcall ntapi_zw_open_event_pair(
	__out	void **			hevent_pair,
	__in 	uint32_t 		desired_access,
	__in 	nt_object_attributes * 	obj_attr);


typedef int32_t __stdcall ntapi_zw_wait_low_event_pair(
	__in	void *	hevent_pair);


typedef int32_t __stdcall ntapi_zw_set_low_event_pair(
	__in	void *	hevent_pair);


typedef int32_t __stdcall ntapi_zw_wait_high_event_pair(
	__in	void *	hevent_pair);


typedef int32_t __stdcall ntapi_zw_set_high_event_pair(
	__in	void *	hevent_pair);


typedef int32_t __stdcall ntapi_zw_set_low_wait_high_event_pair(
	__in	void *	hevent_pair);


typedef int32_t __stdcall ntapi_zw_set_high_wait_low_event_pair(
	__in	void *	hevent_pair);


/* extensions */
typedef int32_t __stdcall ntapi_tt_create_inheritable_event(
	__out	void **		hevent,
	__in 	nt_event_type	event_type,
	__in 	int32_t		initial_state);


typedef int32_t __stdcall ntapi_tt_create_private_event(
	__out	void **		hevent,
	__in 	nt_event_type	event_type,
	__in 	int32_t		initial_state);


typedef void __stdcall ntapi_tt_sync_block_init(
	__in	nt_sync_block *	sync_block,
	__in	uint32_t	flags			__optional,
	__in	int32_t		srvtid			__optional,
	__in	int32_t		default_lock_tries	__optional,
	__in	int64_t		default_lock_wait	__optional,
	__in	void *		hsignal			__optional);


typedef int32_t __stdcall ntapi_tt_sync_block_lock(
	__in	nt_sync_block *	sync_block,
	__in	int32_t		lock_tries	__optional,
	__in	int64_t		lock_wait	__optional,
	__in	uint32_t *	sig_flag	__optional);


typedef int32_t __stdcall ntapi_tt_sync_block_server_lock(
	__in	nt_sync_block *	sync_block,
	__in	int32_t		lock_tries	__optional,
	__in	int64_t		lock_wait	__optional,
	__in	uint32_t *	sig_flag	__optional);


typedef int32_t __stdcall ntapi_tt_sync_block_unlock(
	__in	nt_sync_block *	sync_block);


typedef void __stdcall ntapi_tt_sync_block_validate(
	__in	nt_sync_block *	sync_block);


typedef int32_t __stdcall ntapi_tt_sync_block_invalidate(
	__in	nt_sync_block *	sync_block);


typedef int32_t __stdcall ntapi_tt_sync_block_discard(
	__in	nt_sync_block *	sync_block);


typedef int32_t __stdcall ntapi_tt_wait_for_dummy_event(void);

#endif
