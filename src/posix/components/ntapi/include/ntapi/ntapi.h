#ifndef _NTAPI_H_
#define _NTAPI_H_

#if   defined (NTAPI_BUILD)
#define	__ntapi_api __attr_export__
#elif defined (NTAPI_SHARED)
#define	__ntapi_api __attr_import__
#elif defined (NTAPI_STATIC)
#define	__ntapi_api
#else
#define	__ntapi_api
#endif

#include <psxtypes/psxtypes.h>
#include "nt_status.h"
#include "nt_crc32.h"
#include "nt_object.h"
#include "nt_sysinfo.h"
#include "nt_memory.h"
#include "nt_section.h"
#include "nt_thread.h"
#include "nt_process.h"
#include "nt_job.h"
#include "nt_token.h"
#include "nt_sync.h"
#include "nt_time.h"
#include "nt_profiling.h"
#include "nt_port.h"
#include "nt_device.h"
#include "nt_file.h"
#include "nt_registry.h"
#include "nt_security.h"
#include "nt_pnp.h"
#include "nt_exception.h"
#include "nt_locale.h"
#include "nt_uuid.h"
#include "nt_atom.h"
#include "nt_os.h"
#include "nt_ipc.h"
#include "nt_ldr.h"
#include "nt_string.h"
#include "nt_guid.h"
#include "nt_argv.h"
#include "nt_blitter.h"
#include "nt_unicode.h"
#include "nt_socket.h"
#include "nt_mount.h"
#include "nt_istat.h"
#include "nt_stat.h"
#include "nt_statfs.h"
#include "nt_daemon.h"
#include "nt_vfd.h"
#include "nt_tty.h"
#include "nt_vmount.h"
#include "nt_hash.h"
#include "nt_debug.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef struct _ntapi_vtbl {
	/* imported symbols: head */
	/* nt_object.h */
	ntapi_zw_query_object *				zw_query_object;
	ntapi_zw_set_information_object *		zw_set_information_object;
	ntapi_zw_duplicate_object *			zw_duplicate_object;
	ntapi_zw_make_temporary_object *		zw_make_temporary_object;
	ntapi_zw_close *				zw_close;
	ntapi_zw_query_security_object *		zw_query_security_object;
	ntapi_zw_set_security_object *			zw_set_security_object;
	ntapi_zw_create_directory_object *		zw_create_directory_object;
	ntapi_zw_open_directory_object *		zw_open_directory_object;
	ntapi_zw_query_directory_object *		zw_query_directory_object;
	ntapi_zw_create_symbolic_link_object *		zw_create_symbolic_link_object;
	ntapi_zw_open_symbolic_link_object *		zw_open_symbolic_link_object;
	ntapi_zw_query_symbolic_link_object *		zw_query_symbolic_link_object;

	/* nt_sysinfo.h */
	ntapi_zw_query_system_information *		zw_query_system_information;
	ntapi_zw_set_system_information *		zw_set_system_information;
	ntapi_zw_query_system_environment_value *	zw_query_system_environment_value;
	ntapi_zw_set_system_environment_value *		zw_set_system_environment_value;
	ntapi_zw_shutdown_system *			zw_shutdown_system;
	ntapi_zw_system_debug_control *			zw_system_debug_control;

	/* nt_memory.h */
	ntapi_zw_allocate_virtual_memory *		zw_allocate_virtual_memory;
	ntapi_zw_free_virtual_memory *			zw_free_virtual_memory;
	ntapi_zw_query_virtual_memory *			zw_query_virtual_memory;
	ntapi_zw_protect_virtual_memory *		zw_protect_virtual_memory;
	ntapi_zw_read_virtual_memory *			zw_read_virtual_memory;
	ntapi_zw_write_virtual_memory *			zw_write_virtual_memory;
	ntapi_zw_lock_virtual_memory *			zw_lock_virtual_memory;
	ntapi_zw_unlock_virtual_memory *		zw_unlock_virtual_memory;
	ntapi_zw_flush_virtual_memory *			zw_flush_virtual_memory;
	ntapi_zw_allocate_user_physical_pages *		zw_allocate_user_physical_pages;
	ntapi_zw_free_user_physical_pages *		zw_free_user_physical_pages;
	ntapi_zw_map_user_physical_pages *		zw_map_user_physical_pages;
	ntapi_zw_get_write_watch *			zw_get_write_watch;
	ntapi_zw_reset_write_watch *			zw_reset_write_watch;

	/* nt_section.h */
	ntapi_zw_create_section *			zw_create_section;
	ntapi_zw_open_section *				zw_open_section;
	ntapi_zw_query_section *			zw_query_section;
	ntapi_zw_extend_section *			zw_extend_section;
	ntapi_zw_map_view_of_section *			zw_map_view_of_section;
	ntapi_zw_unmap_view_of_section *		zw_unmap_view_of_section;
	ntapi_zw_are_mapped_files_the_same *		zw_are_mapped_files_the_same;

	/* nt_thread.h */
	ntapi_zw_create_thread *			zw_create_thread;
	ntapi_zw_open_thread *				zw_open_thread;
	ntapi_zw_terminate_thread *			zw_terminate_thread;
	ntapi_zw_query_information_thread *		zw_query_information_thread;
	ntapi_zw_set_information_thread *		zw_set_information_thread;
	ntapi_zw_suspend_thread *			zw_suspend_thread;
	ntapi_zw_resume_thread *			zw_resume_thread;
	ntapi_zw_get_context_thread *			zw_get_context_thread;
	ntapi_zw_set_context_thread *			zw_set_context_thread;
	ntapi_zw_queue_apc_thread *			zw_queue_apc_thread;
	ntapi_zw_test_alert *				zw_test_alert;
	ntapi_zw_alert_thread *				zw_alert_thread;
	ntapi_zw_alert_resume_thread *			zw_alert_resume_thread;
	ntapi_zw_register_thread_terminate_port *	zw_register_thread_terminate_port;
	ntapi_zw_impersonate_thread *			zw_impersonate_thread;
	ntapi_zw_impersonate_anonymous_token *		zw_impersonate_anonymous_token;

	/* nt_process.h */
	ntapi_zw_create_process *			zw_create_process;
	ntapi_zw_create_user_process *			zw_create_user_process;
	ntapi_zw_open_process *				zw_open_process;
	ntapi_zw_terminate_process *			zw_terminate_process;
	ntapi_zw_query_information_process *		zw_query_information_process;
	ntapi_zw_set_information_process *		zw_set_information_process;
	ntapi_zw_flush_instruction_cache *		zw_flush_instruction_cache;
	ntapi_rtl_create_process_parameters *		rtl_create_process_parameters;
	ntapi_rtl_destroy_process_parameters *		rtl_destroy_process_parameters;
	ntapi_rtl_normalize_process_params *		rtl_normalize_process_params;
	ntapi_rtl_create_query_debug_buffer *		rtl_create_query_debug_buffer;
	ntapi_rtl_destroy_query_debug_buffer *		rtl_destroy_query_debug_buffer;
	ntapi_rtl_query_process_debug_information *	rtl_query_process_debug_information;

	/* nt_job.h */
	ntapi_zw_create_job_object *			zw_create_job_object;
	ntapi_zw_open_job_object *			zw_open_job_object;
	ntapi_zw_terminate_job_object *			zw_terminate_job_object;
	ntapi_zw_assign_process_to_job_object *		zw_assign_process_to_job_object;
	ntapi_zw_query_information_job_object *		zw_query_information_job_object;
	ntapi_zw_set_information_job_object *		zw_set_information_job_object;

	/* nt_token.h */
	ntapi_zw_create_token *				zw_create_token;
	ntapi_zw_open_process_token *			zw_open_process_token;
	ntapi_zw_open_thread_token *			zw_open_thread_token;
	ntapi_zw_duplicate_token *			zw_duplicate_token;
	ntapi_zw_filter_token *				zw_filter_token;
	ntapi_zw_adjust_privileges_token *		zw_adjust_privileges_token;
	ntapi_zw_adjust_groups_token *			zw_adjust_groups_token;
	ntapi_zw_query_information_token *		zw_query_information_token;
	ntapi_zw_set_information_token *		zw_set_information_token;

	/* nt_sync.h */
	ntapi_zw_wait_for_single_object *		zw_wait_for_single_object;
	ntapi_zw_signal_and_wait_for_single_object *	zw_signal_and_wait_for_single_object;
	ntapi_zw_wait_for_multiple_objects *		zw_wait_for_multiple_objects;
	ntapi_zw_create_timer *				zw_create_timer;
	ntapi_zw_open_timer *				zw_open_timer;
	ntapi_zw_cancel_timer *				zw_cancel_timer;
	ntapi_zw_set_timer *				zw_set_timer;
	ntapi_zw_query_timer *				zw_query_timer;
	ntapi_zw_create_event *				zw_create_event;
	ntapi_zw_open_event *				zw_open_event;
	ntapi_zw_set_event *				zw_set_event;
	ntapi_zw_pulse_event *				zw_pulse_event;
	ntapi_zw_reset_event *				zw_reset_event;
	ntapi_zw_clear_event *				zw_clear_event;
	ntapi_zw_query_event *				zw_query_event;
	ntapi_zw_create_semaphore *			zw_create_semaphore;
	ntapi_zw_open_semaphore *			zw_open_semaphore;
	ntapi_zw_release_semaphore *			zw_release_semaphore;
	ntapi_zw_query_semaphore *			zw_query_semaphore;
	ntapi_zw_create_mutant *			zw_create_mutant;
	ntapi_zw_open_mutant *				zw_open_mutant;
	ntapi_zw_release_mutant *			zw_release_mutant;
	ntapi_zw_query_mutant *				zw_query_mutant;
	ntapi_zw_create_io_completion *			zw_create_io_completion;
	ntapi_zw_open_io_completion *			zw_open_io_completion;
	ntapi_zw_set_io_completion *			zw_set_io_completion;
	ntapi_zw_remove_io_completion *			zw_remove_io_completion;
	ntapi_zw_query_io_completion *			zw_query_io_completion;
	ntapi_zw_create_event_pair *			zw_create_event_pair;
	ntapi_zw_open_event_pair *			zw_open_event_pair;
	ntapi_zw_wait_low_event_pair *			zw_wait_low_event_pair;
	ntapi_zw_set_low_event_pair *			zw_set_low_event_pair;
	ntapi_zw_wait_high_event_pair *			zw_wait_high_event_pair;
	ntapi_zw_set_high_event_pair *			zw_set_high_event_pair;
	ntapi_zw_set_low_wait_high_event_pair *		zw_set_low_wait_high_event_pair;
	ntapi_zw_set_high_wait_low_event_pair *		zw_set_high_wait_low_event_pair;

	/* nt_time.h */
	ntapi_zw_query_system_time *			zw_query_system_time;
	ntapi_zw_set_system_time *			zw_set_system_time;
	ntapi_zw_query_performance_counter *		zw_query_performance_counter;
	ntapi_zw_set_timer_resolution *			zw_set_timer_resolution;
	ntapi_zw_query_timer_resolution *		zw_query_timer_resolution;
	ntapi_zw_delay_execution *			zw_delay_execution;
	ntapi_zw_yield_execution *			zw_yield_execution;

	/* nt_profiling.h */
	ntapi_zw_create_profile *			zw_create_profile;
	ntapi_zw_set_interval_profile *			zw_set_interval_profile;
	ntapi_zw_query_interval_profile *		zw_query_interval_profile;
	ntapi_zw_start_profile *			zw_start_profile;
	ntapi_zw_stop_profile *				zw_stop_profile;

	/* nt_port.h */
	ntapi_zw_create_port *				zw_create_port;
	ntapi_zw_create_waitable_port *			zw_create_waitable_port;
	ntapi_zw_connect_port *				zw_connect_port;
	ntapi_zw_secure_connect_port *			zw_secure_connect_port;
	ntapi_zw_listen_port *				zw_listen_port;
	ntapi_zw_accept_connect_port *			zw_accept_connect_port;
	ntapi_zw_complete_connect_port *		zw_complete_connect_port;
	ntapi_zw_request_port *				zw_request_port;
	ntapi_zw_request_wait_reply_port *		zw_request_wait_reply_port;
	ntapi_zw_reply_port *				zw_reply_port;
	ntapi_zw_reply_wait_reply_port *		zw_reply_wait_reply_port;
	ntapi_zw_reply_wait_receive_port *		zw_reply_wait_receive_port;
	ntapi_zw_reply_wait_receive_port_ex *		zw_reply_wait_receive_port_ex;
	ntapi_zw_read_request_data *			zw_read_request_data;
	ntapi_zw_write_request_data *			zw_write_request_data;
	ntapi_zw_query_information_port *		zw_query_information_port;
	ntapi_zw_impersonate_client_of_port *		zw_impersonate_client_of_port;
	ntapi_csr_client_call_server *			csr_client_call_server;
	ntapi_csr_port_handle *				csr_port_handle;

	/* nt_device.h */
	ntapi_zw_load_driver *				zw_load_driver;
	ntapi_zw_unload_driver *			zw_unload_driver;

	/* nt_file.h */
	ntapi_zw_create_file *				zw_create_file;
	ntapi_zw_open_file *				zw_open_file;
	ntapi_zw_delete_file *				zw_delete_file;
	ntapi_zw_flush_buffers_file *			zw_flush_buffers_file;
	ntapi_zw_cancel_io_file *			zw_cancel_io_file;
	ntapi_zw_cancel_io_file_ex *			zw_cancel_io_file_ex;
	ntapi_zw_read_file *				zw_read_file;
	ntapi_zw_write_file *				zw_write_file;
	ntapi_zw_read_file_scatter *			zw_read_file_scatter;
	ntapi_zw_write_file_gather *			zw_write_file_gather;
	ntapi_zw_lock_file *				zw_lock_file;
	ntapi_zw_unlock_file *				zw_unlock_file;
	ntapi_zw_device_io_control_file *		zw_device_io_control_file;
	ntapi_zw_fs_control_file *			zw_fs_control_file;
	ntapi_zw_notify_change_directory_file *		zw_notify_change_directory_file;
	ntapi_zw_query_ea_file *			zw_query_ea_file;
	ntapi_zw_set_ea_file *				zw_set_ea_file;
	ntapi_zw_create_named_pipe_file *		zw_create_named_pipe_file;
	ntapi_zw_create_mailslot_file *			zw_create_mailslot_file;
	ntapi_zw_query_volume_information_file *	zw_query_volume_information_file;
	ntapi_zw_set_volume_information_file *		zw_set_volume_information_file;
	ntapi_zw_query_quota_information_file *		zw_query_quota_information_file;
	ntapi_zw_set_quota_information_file *		zw_set_quota_information_file;
	ntapi_zw_query_attributes_file *		zw_query_attributes_file;
	ntapi_zw_query_full_attributes_file *		zw_query_full_attributes_file;
	ntapi_zw_query_directory_file *			zw_query_directory_file;
	ntapi_zw_query_information_file *		zw_query_information_file;
	ntapi_zw_set_information_file *			zw_set_information_file;

	/* nt_resistry.h */
	ntapi_zw_create_key *				zw_create_key;
	ntapi_zw_open_key *				zw_open_key;
	ntapi_zw_delete_key *				zw_delete_key;
	ntapi_zw_flush_key *				zw_flush_key;
	ntapi_zw_save_key *				zw_save_key;
	ntapi_zw_save_merged_keys *			zw_save_merged_keys;
	ntapi_zw_restore_key *				zw_restore_key;
	ntapi_zw_load_key *				zw_load_key;
	ntapi_zw_load_key2 *				zw_load_key2;
	ntapi_zw_unload_key *				zw_unload_key;
	ntapi_zw_query_open_sub_keys *			zw_query_open_sub_keys;
	ntapi_zw_replace_key *				zw_replace_key;
	ntapi_zw_set_information_key *			zw_set_information_key;
	ntapi_zw_query_key *				zw_query_key;
	ntapi_zw_enumerate_key *			zw_enumerate_key;
	ntapi_zw_notify_change_key *			zw_notify_change_key;
	ntapi_zw_notify_change_multiple_keys *		zw_notify_change_multiple_keys;
	ntapi_zw_delete_value_key *			zw_delete_value_key;
	ntapi_zw_set_value_key *			zw_set_value_key;
	ntapi_zw_query_value_key *			zw_query_value_key;
	ntapi_zw_enumerate_value_key *			zw_enumerate_value_key;
	ntapi_zw_query_multiple_value_key *		zw_query_multiple_value_key;
	ntapi_zw_initialize_registry *			zw_initialize_registry;

	/* nt_security.h */
	ntapi_zw_privilege_check *			zw_privilege_check;
	ntapi_zw_privilege_object_audit_alarm *		zw_privilege_object_audit_alarm;
	ntapi_zw_privileged_service_audit_alarm *	zw_privileged_service_audit_alarm;
	ntapi_zw_access_check *				zw_access_check;
	ntapi_zw_access_check_and_audit_alarm *		zw_access_check_and_audit_alarm;
	ntapi_zw_access_check_by_type *			zw_access_check_by_type;
	ntapi_zw_access_check_by_type_result_list *	zw_access_check_by_type_result_list;
	ntapi_zw_open_object_audit_alarm *		zw_open_object_audit_alarm;
	ntapi_zw_close_object_audit_alarm *		zw_close_object_audit_alarm;
	ntapi_zw_delete_object_audit_alarm *		zw_delete_object_audit_alarm;

	ntapi_zw_access_check_by_type_and_audit_alarm *	zw_access_check_by_type_and_audit_alarm;

	ntapi_zw_access_check_by_type_result_list_and_audit_alarm *		zw_access_check_by_type_result_list_and_audit_alarm;
	ntapi_zw_access_check_by_type_result_list_and_audit_alarm_by_handle *	zw_access_check_by_type_result_list_and_audit_alarm_by_handle;

	/* nt_pnp.h */
	ntapi_zw_is_system_resume_automatic *		zw_is_system_resume_automatic;
	ntapi_zw_set_thread_execution_state *		zw_set_thread_execution_state;
	ntapi_zw_get_device_power_state *		zw_get_device_power_state;
	ntapi_zw_set_system_power_state *		zw_set_system_power_state;
	ntapi_zw_initiate_power_action *		zw_initiate_power_action;
	ntapi_zw_power_information *			zw_power_information;
	ntapi_zw_plug_play_control *			zw_plug_play_control;
	ntapi_zw_get_plug_play_event *			zw_get_plug_play_event;

	/* nt_exception */
	ntapi_zw_raise_exception *			zw_raise_exception;
	ntapi_zw_continue *				zw_continue;

	/* nt_locale */
	ntapi_zw_query_default_locale *			zw_query_default_locale;
	ntapi_zw_set_default_locale *			zw_set_default_locale;
	ntapi_zw_query_default_ui_language *		zw_query_default_ui_language;
	ntapi_zw_set_default_ui_language *		zw_set_default_ui_language;
	ntapi_zw_query_install_ui_language *		zw_query_install_ui_language;

	/* nt_uuid.h */
	ntapi_zw_allocate_locally_unique_id *		zw_allocate_locally_unique_id;
	ntapi_zw_allocate_uuids *			zw_allocate_uuids;
	ntapi_zw_set_uuid_seed *			zw_set_uuid_seed;

	/* nt_atom.h */
	ntapi_zw_add_atom *				zw_add_atom;
	ntapi_zw_find_atom *				zw_find_atom;
	ntapi_zw_delete_atom *				zw_delete_atom;
	ntapi_zw_query_information_atom *		zw_query_information_atom;

	/* nt_os.h */
	ntapi_zw_flush_write_buffer *			zw_flush_write_buffer;
	ntapi_zw_raise_hard_error *			zw_raise_hard_error;
	ntapi_zw_set_default_hard_error_port *		zw_set_default_hard_error_port;
	ntapi_zw_display_string *			zw_display_string;
	ntapi_zw_create_paging_file *			zw_create_paging_file;
	ntapi_zw_set_ldt_entries *			zw_set_ldt_entries;
	ntapi_zw_vdm_control *				zw_vdm_control;

	/* nt_ldr.h */
	ntapi_ldr_load_dll *				ldr_load_dll;
	ntapi_ldr_unload_dll *				ldr_unload_dll;

	/* nt_string.h */
	ntapi_memset *					memset;
	ntapi_sprintf *					sprintf;
	ntapi_strlen *					strlen;
	/* imported symbols: tail */

	/* alternate implementation */
	/* nt_string.h */
	ntapi_wcslen *					wcslen;
	ntapi_rtl_init_unicode_string *			rtl_init_unicode_string;

	/* extension functions */
	/* nt_object.h */
	ntapi_tt_create_keyed_object_directory *	tt_create_keyed_object_directory;
	ntapi_tt_open_keyed_object_directory *		tt_open_keyed_object_directory;
	ntapi_tt_create_keyed_object_directory_entry *	tt_create_keyed_object_directory_entry;

	/* nt_crc32.h */
	ntapi_tt_buffer_crc32 *				tt_buffer_crc32;
	ntapi_tt_mbstr_crc32 *				tt_mbstr_crc32;
	ntapi_tt_crc32_table *				tt_crc32_table;

	/* nt_file.h */
	ntapi_tt_get_file_handle_type *			tt_get_file_handle_type;
	ntapi_tt_open_logical_parent_directory *	tt_open_logical_parent_directory;
	ntapi_tt_open_physical_parent_directory	*	tt_open_physical_parent_directory;

	/* nt_ipc.h */
	ntapi_ipc_create_pipe *				ipc_create_pipe;

	/* nt_ldr.h */
	ntapi_ldr_load_system_dll *			ldr_load_system_dll;
	ntapi_ldr_create_state_snapshot *		ldr_create_state_snapshot;
	ntapi_ldr_revert_state_to_snapshot *		ldr_revert_state_to_snapshot;

	/* nt_string.h */
	ntapi_tt_string_null_offset_multibyte *		tt_string_null_offset_multibyte;
	ntapi_tt_string_null_offset_short *		tt_string_null_offset_short;
	ntapi_tt_string_null_offset_dword *		tt_string_null_offset_dword;
	ntapi_tt_string_null_offset_qword *		tt_string_null_offset_qword;
	ntapi_tt_string_null_offset_ptrsize *		tt_string_null_offset_ptrsize;
	ntapi_tt_aligned_block_memset *			tt_aligned_block_memset;
	ntapi_tt_aligned_block_memcpy *			tt_aligned_block_memcpy;
	ntapi_tt_aligned_memcpy_utf16 *			tt_aligned_memcpy_utf16;
	ntapi_tt_memcpy_utf16 *				tt_memcpy_utf16;
	ntapi_tt_generic_memset *			tt_generic_memset;
	ntapi_tt_generic_memcpy *			tt_generic_memcpy;
	ntapi_tt_uint16_to_hex_utf16 *			tt_uint16_to_hex_utf16;
	ntapi_tt_uint32_to_hex_utf16 *			tt_uint32_to_hex_utf16;
	ntapi_tt_uint64_to_hex_utf16 *			tt_uint64_to_hex_utf16;
	ntapi_tt_uintptr_to_hex_utf16 *			tt_uintptr_to_hex_utf16;
	ntapi_tt_hex_utf16_to_uint16 *			tt_hex_utf16_to_uint16;
	ntapi_tt_hex_utf16_to_uint32 *			tt_hex_utf16_to_uint32;
	ntapi_tt_hex_utf16_to_uint64 *			tt_hex_utf16_to_uint64;
	ntapi_tt_hex_utf16_to_uintptr *			tt_hex_utf16_to_uintptr;
	ntapi_tt_uint16_to_hex_utf8 *			tt_uint16_to_hex_utf8;
	ntapi_tt_uint32_to_hex_utf8 *			tt_uint32_to_hex_utf8;
	ntapi_tt_uint64_to_hex_utf8 *			tt_uint64_to_hex_utf8;
	ntapi_tt_uintptr_to_hex_utf8 *			tt_uintptr_to_hex_utf8;
	ntapi_tt_init_unicode_string_from_utf16*	tt_init_unicode_string_from_utf16;

	/* nt_guid.h */
	ntapi_tt_guid_copy *				tt_guid_copy;
	ntapi_tt_guid_compare *				tt_guid_compare;
	ntapi_tt_guid_to_utf16_string *			tt_guid_to_utf16_string;
	ntapi_tt_utf16_string_to_guid *			tt_utf16_string_to_guid;

	/* nt_sysinfo.h */
	ntapi_tt_get_system_directory_native_path *	tt_get_system_directory_native_path;
	ntapi_tt_get_system_directory_dos_path *	tt_get_system_directory_dos_path;
	ntapi_tt_get_system_directory_handle *		tt_get_system_directory_handle;
	ntapi_tt_get_system_info_snapshot *		tt_get_system_info_snapshot;

	/* nt_thread.h */
	ntapi_tt_create_thread *			tt_create_thread;
	ntapi_tt_create_local_thread *			tt_create_local_thread;
	ntapi_tt_create_remote_thread *			tt_create_remote_thread;

	/* nt_process.h */
	ntapi_tt_fork *					tt_fork;
	ntapi_tt_create_remote_process_params *		tt_create_remote_process_params;
	ntapi_tt_create_native_process *		tt_create_native_process;
	ntapi_tt_get_runtime_data *			tt_get_runtime_data;
	ntapi_tt_init_runtime_data *			tt_init_runtime_data;
	ntapi_tt_update_runtime_data *			tt_update_runtime_data;
	ntapi_tt_exec_map_image_as_data *		tt_exec_map_image_as_data;
	ntapi_tt_exec_unmap_image *			tt_exec_unmap_image;

	/* nt_section.h */
	ntapi_tt_get_section_name *			tt_get_section_name;

	/* nt_sync.h */
	ntapi_tt_create_inheritable_event *		tt_create_inheritable_event;
	ntapi_tt_create_private_event *			tt_create_private_event;
	ntapi_tt_sync_block_init *			tt_sync_block_init;
	ntapi_tt_sync_block_lock *			tt_sync_block_lock;
	ntapi_tt_sync_block_server_lock *		tt_sync_block_server_lock;
	ntapi_tt_sync_block_unlock *			tt_sync_block_unlock;
	ntapi_tt_sync_block_invalidate *		tt_sync_block_invalidate;
	ntapi_tt_wait_for_dummy_event *			tt_wait_for_dummy_event;

	/* nt_port.h */
	ntapi_tt_port_guid_from_type *			tt_port_guid_from_type;
	ntapi_tt_port_type_from_guid *			tt_port_type_from_guid;
	ntapi_tt_port_generate_keys *			tt_port_generate_keys;
	ntapi_tt_port_format_keys *			tt_port_format_keys;
	ntapi_tt_port_name_from_attributes *		tt_port_name_from_attributes;

	/* nt_argv.h */
	ntapi_tt_get_cmd_line_utf16 *			tt_get_cmd_line_utf16;
	ntapi_tt_get_peb_env_block_utf16 *		tt_get_peb_env_block_utf16;
	ntapi_tt_parse_cmd_line_args_utf16 *		tt_parse_cmd_line_args_utf16;
	ntapi_tt_get_argv_envp_utf8 *			tt_get_argv_envp_utf8;
	ntapi_tt_get_argv_envp_utf16 *			tt_get_argv_envp_utf16;
	ntapi_tt_get_env_var_meta_utf16 *		tt_get_env_var_meta_utf16;
	ntapi_tt_get_short_option_meta_utf16 *		tt_get_short_option_meta_utf16;
	ntapi_tt_get_long_option_meta_utf16 *		tt_get_long_option_meta_utf16;
	ntapi_tt_array_copy_utf8 *			tt_array_copy_utf8;
	ntapi_tt_array_copy_utf16 *			tt_array_copy_utf16;
	ntapi_tt_array_convert_utf8_to_utf16 *		tt_array_convert_utf8_to_utf16;
	ntapi_tt_array_convert_utf16_to_utf8 *		tt_array_convert_utf16_to_utf8;

	/* nt_blitter.h */
	ntapi_blt_alloc *				blt_alloc;
	ntapi_blt_free *				blt_free;
	ntapi_blt_acquire *				blt_acquire;
	ntapi_blt_obtain *				blt_obtain;
	ntapi_blt_possess *				blt_possess;
	ntapi_blt_release *				blt_release;
	ntapi_blt_get *					blt_get;
	ntapi_blt_set *					blt_set;

	/* nt_unicode.h */
	ntapi_uc_validate_unicode_stream_utf8 *		uc_validate_unicode_stream_utf8;
	ntapi_uc_validate_unicode_stream_utf16 *	uc_validate_unicode_stream_utf16;
	ntapi_uc_get_code_point_byte_count_utf8 *	uc_get_code_point_byte_count_utf8;
	ntapi_uc_get_code_point_byte_count_utf16 *	uc_get_code_point_byte_count_utf16;
	ntapi_uc_convert_unicode_stream_utf8_to_utf16 *	uc_convert_unicode_stream_utf8_to_utf16;
	ntapi_uc_convert_unicode_stream_utf8_to_utf32 *	uc_convert_unicode_stream_utf8_to_utf32;
	ntapi_uc_convert_unicode_stream_utf16_to_utf8 *	uc_convert_unicode_stream_utf16_to_utf8;
	ntapi_uc_convert_unicode_stream_utf16_to_utf32 *uc_convert_unicode_stream_utf16_to_utf32;

	/* nt_daemon.h */
	ntapi_dsr_init *				dsr_init;
	ntapi_dsr_start *				dsr_start;
	ntapi_dsr_create_port *				dsr_create_port;
	ntapi_dsr_connect_internal_client *		dsr_connect_internal_client;
	ntapi_dsr_internal_client_connect *		dsr_internal_client_connect;

	/* nt_vfd.h */
	ntapi_vfd_dev_name_init *			vfd_dev_name_init;

	/* nt_tty.h */
	ntapi_tty_create_session *			tty_create_session;
	ntapi_tty_join_session *			tty_join_session;
	ntapi_tty_connect *				tty_connect;
	ntapi_tty_client_session_query *		tty_client_session_query;
	ntapi_tty_client_session_set *			tty_client_session_set;
	ntapi_tty_client_process_register *		tty_client_process_register;
	ntapi_tty_query_information_server *		tty_query_information_server;
	ntapi_tty_request_peer *			tty_request_peer;
	ntapi_tty_vms_query *				tty_vms_query;
	ntapi_tty_vms_request *				tty_vms_request;
	ntapi_pty_open *				pty_open;
	ntapi_pty_reopen *				pty_reopen;
	ntapi_pty_close *				pty_close;
	ntapi_pty_read *				pty_read;
	ntapi_pty_write *				pty_write;
	ntapi_pty_fcntl *				pty_fcntl;
	ntapi_pty_ioctl *				pty_ioctl;
	ntapi_pty_query *				pty_query;
	ntapi_pty_set *					pty_set;
	ntapi_pty_cancel *				pty_cancel;

	/* nt_socket.h */
	ntapi_sc_socket *				sc_socket;
	ntapi_sc_bind *					sc_bind;
	ntapi_sc_listen *				sc_listen;
	ntapi_sc_accept *				sc_accept;
	ntapi_sc_connect *				sc_connect;
	ntapi_sc_send *					sc_send;
	ntapi_sc_recv *					sc_recv;
	ntapi_sc_shutdown *				sc_shutdown;
	ntapi_sc_getsockname *				sc_getsockname;
	ntapi_sc_server_accept_connection *		sc_server_accept_connection;
	ntapi_sc_server_duplicate_socket *		sc_server_duplicate_socket;
	ntapi_sc_wait *					sc_wait;

	/* nt_mount.h */
	ntapi_tt_get_dos_drive_device_handle *		tt_get_dos_drive_device_handle;
	ntapi_tt_get_dos_drive_root_handle *		tt_get_dos_drive_root_handle;
	ntapi_tt_get_dos_drive_device_name *		tt_get_dos_drive_device_name;
	ntapi_tt_get_dos_drive_mount_points *		tt_get_dos_drive_mount_points;
	ntapi_tt_dev_mount_points_to_statfs *		tt_dev_mount_points_to_statfs;
	ntapi_tt_get_dos_drive_letter_from_device *	tt_get_dos_drive_letter_from_device;

	/* nt_istat.h */
	ntapi_tt_istat *				tt_istat;
	ntapi_tt_validate_fs_handle *			tt_validate_fs_handle;

	/* nt_stat.h */
	ntapi_tt_stat *					tt_stat;

	/* nt_statfs.h */
	ntapi_tt_statfs *				tt_statfs;

	/* nt_vmount.h */
	ntapi_vms_get_node_by_dev_name *		vms_get_node_by_dev_name;
	ntapi_vms_get_node_by_end_component *		vms_get_node_by_end_component;
	ntapi_vms_cache_alloc *				vms_cache_alloc;
	ntapi_vms_cache_free *				vms_cache_free;
	ntapi_vms_client_connect *			vms_client_connect;
	ntapi_vms_client_disconnect *			vms_client_disconnect;
	ntapi_vms_point_attach *			vms_point_attach;
	ntapi_vms_point_get_handles *			vms_point_get_handles;
	ntapi_vms_ref_count_inc *			vms_ref_count_inc;
	ntapi_vms_ref_count_dec *			vms_ref_count_dec;
	ntapi_vms_table_query *				vms_table_query;

	/* nt_debug.h */
	ntapi_dbg_write *				dbg_write;
	ntapi_dbg_fn_call *				dbg_fn_call;
	ntapi_dbg_msg *					dbg_msg;
} ntapi_vtbl;


__ntapi_api
int32_t __fastcall ntapi_init(ntapi_vtbl ** pvtbl);


#ifdef __cplusplus
}
#endif

#endif
