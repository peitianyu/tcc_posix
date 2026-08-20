/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#ifndef ___NTAPI_FNAPI_H_
#define ___NTAPI_FNAPI_H_

#include <psxtypes/psxtypes.h>
#include <ntapi/ntapi.h>
#include "ntapi_hash_table.h"

#ifdef __cplusplus
extern "C" {
#endif

/* internal prototypes */
typedef int32_t __stdcall ntapi_tt_create_remote_runtime_data(
	__in		void *				hprocess,
	__in_out	nt_runtime_data_block *		runtime_data);

typedef void ** __cdecl ntapi_tt_get_csr_port_handle_addr_by_logic(void);

/* nt_object.h */
ntapi_tt_create_keyed_object_directory		__ntapi_tt_create_keyed_object_directory;
ntapi_tt_open_keyed_object_directory		__ntapi_tt_open_keyed_object_directory;
ntapi_tt_create_keyed_object_directory_entry	__ntapi_tt_create_keyed_object_directory_entry;

/* nt_crc32.h */
ntapi_tt_buffer_crc32				__ntapi_tt_buffer_crc32;
ntapi_tt_mbstr_crc32				__ntapi_tt_mbstr_crc32;
ntapi_tt_crc32_table				__ntapi_tt_crc32_table;

/* nt_file.h */
ntapi_tt_get_file_handle_type			__ntapi_tt_get_file_handle_type;
ntapi_tt_open_logical_parent_directory		__ntapi_tt_open_logical_parent_directory;
ntapi_tt_open_physical_parent_directory		__ntapi_tt_open_physical_parent_directory;


/* nt_ipc.h */
ntapi_ipc_create_pipe				__ntapi_ipc_create_pipe_v1;
ntapi_ipc_create_pipe				__ntapi_ipc_create_pipe_v2;

/* nt_ldr */
ntapi_ldr_load_system_dll			__ntapi_ldr_load_system_dll;
ntapi_ldr_create_state_snapshot			__ntapi_ldr_create_state_snapshot;
ntapi_ldr_revert_state_to_snapshot		__ntapi_ldr_revert_state_to_snapshot;

/* nt_string.h */
ntapi_tt_string_null_offset_multibyte		__ntapi_tt_string_null_offset_multibyte;
ntapi_tt_string_null_offset_short		__ntapi_tt_string_null_offset_short;
ntapi_tt_string_null_offset_dword		__ntapi_tt_string_null_offset_dword;
ntapi_tt_string_null_offset_qword		__ntapi_tt_string_null_offset_qword;
ntapi_tt_string_null_offset_ptrsize		__ntapi_tt_string_null_offset_ptrsize;
ntapi_wcslen					__ntapi_wcslen;
ntapi_tt_aligned_block_memset			__ntapi_tt_aligned_block_memset;
ntapi_tt_aligned_block_memcpy			__ntapi_tt_aligned_block_memcpy;
ntapi_tt_init_unicode_string_from_utf16		__ntapi_tt_init_unicode_string_from_utf16;
ntapi_tt_memcpy_utf16				__ntapi_tt_memcpy_utf16;
ntapi_tt_aligned_memcpy_utf16			__ntapi_tt_aligned_memcpy_utf16;
ntapi_tt_generic_memset				__ntapi_tt_generic_memset;
ntapi_tt_generic_memcpy				__ntapi_tt_generic_memcpy;
ntapi_tt_uint16_to_hex_utf16			__ntapi_tt_uint16_to_hex_utf16;
ntapi_tt_uint32_to_hex_utf16			__ntapi_tt_uint32_to_hex_utf16;
ntapi_tt_uint64_to_hex_utf16			__ntapi_tt_uint64_to_hex_utf16;
ntapi_tt_uintptr_to_hex_utf16			__ntapi_tt_uintptr_to_hex_utf16;
ntapi_tt_hex_utf16_to_uint16			__ntapi_tt_hex_utf16_to_uint16;
ntapi_tt_hex_utf16_to_uint32			__ntapi_tt_hex_utf16_to_uint32;
ntapi_tt_hex_utf16_to_uint64			__ntapi_tt_hex_utf16_to_uint64;
ntapi_tt_hex_utf16_to_uintptr			__ntapi_tt_hex_utf16_to_uintptr;
ntapi_tt_uint16_to_hex_utf8			__ntapi_tt_uint16_to_hex_utf8;
ntapi_tt_uint32_to_hex_utf8			__ntapi_tt_uint32_to_hex_utf8;
ntapi_tt_uint64_to_hex_utf8			__ntapi_tt_uint64_to_hex_utf8;
ntapi_tt_uintptr_to_hex_utf8			__ntapi_tt_uintptr_to_hex_utf8;

/* nt_guid.h */
ntapi_tt_guid_to_utf16_string			__ntapi_tt_guid_to_utf16_string;
ntapi_tt_utf16_string_to_guid			__ntapi_tt_utf16_string_to_guid;

/* nt_sysinfo.h */
ntapi_tt_get_system_directory_native_path	__ntapi_tt_get_system_directory_native_path;
ntapi_tt_get_system_directory_dos_path		__ntapi_tt_get_system_directory_dos_path;
ntapi_tt_get_system_directory_handle		__ntapi_tt_get_system_directory_handle;
ntapi_tt_get_system_info_snapshot		__ntapi_tt_get_system_info_snapshot;

/* nt_thread.h */
ntapi_tt_create_thread		__ntapi_tt_create_thread;
ntapi_tt_create_local_thread			__ntapi_tt_create_local_thread;
ntapi_tt_create_remote_thread			__ntapi_tt_create_remote_thread;

/* nt_process.h */
ntapi_tt_fork					__ntapi_tt_fork_v1;
ntapi_tt_fork					__ntapi_tt_fork_v2;
ntapi_tt_create_remote_process_params		__ntapi_tt_create_remote_process_params;
ntapi_tt_create_remote_runtime_data		__ntapi_tt_create_remote_runtime_data;
ntapi_tt_create_native_process			__ntapi_tt_create_native_process_v1;
ntapi_tt_create_native_process			__ntapi_tt_create_native_process_v2;
ntapi_tt_get_runtime_data			__ntapi_tt_get_runtime_data;
ntapi_tt_init_runtime_data			__ntapi_tt_init_runtime_data;
ntapi_tt_update_runtime_data			__ntapi_tt_update_runtime_data;
ntapi_tt_exec_map_image_as_data			__ntapi_tt_exec_map_image_as_data;
ntapi_tt_exec_unmap_image			__ntapi_tt_exec_unmap_image;

/* nt_section.h */
ntapi_tt_get_section_name			__ntapi_tt_get_section_name;

/* nt_sync.h */
ntapi_tt_create_inheritable_event		__ntapi_tt_create_inheritable_event;
ntapi_tt_create_private_event			__ntapi_tt_create_private_event;
ntapi_tt_wait_for_dummy_event			__ntapi_tt_wait_for_dummy_event;
ntapi_tt_sync_block_init			__ntapi_tt_sync_block_init;
ntapi_tt_sync_block_lock			__ntapi_tt_sync_block_lock;
ntapi_tt_sync_block_server_lock			__ntapi_tt_sync_block_server_lock;
ntapi_tt_sync_block_unlock			__ntapi_tt_sync_block_unlock;
ntapi_tt_sync_block_invalidate			__ntapi_tt_sync_block_invalidate;

/* nt_port.h */
ntapi_tt_port_guid_from_type			__ntapi_tt_port_guid_from_type;
ntapi_tt_port_type_from_guid			__ntapi_tt_port_type_from_guid;
ntapi_tt_port_generate_keys			__ntapi_tt_port_generate_keys;
ntapi_tt_port_format_keys			__ntapi_tt_port_format_keys;
ntapi_tt_port_name_from_attributes		__ntapi_tt_port_name_from_attributes;

/* nt_argv.h */
ntapi_tt_get_cmd_line_utf16			__ntapi_tt_get_cmd_line_utf16;
ntapi_tt_get_peb_env_block_utf16		__ntapi_tt_get_peb_env_block_utf16;
ntapi_tt_parse_cmd_line_args_utf16		__ntapi_tt_parse_cmd_line_args_utf16;
ntapi_tt_get_argv_envp_utf8			__ntapi_tt_get_argv_envp_utf8;
ntapi_tt_get_argv_envp_utf16			__ntapi_tt_get_argv_envp_utf16;
ntapi_tt_get_env_var_meta_utf16			__ntapi_tt_get_env_var_meta_utf16;
ntapi_tt_get_short_option_meta_utf16		__ntapi_tt_get_short_option_meta_utf16;
ntapi_tt_get_long_option_meta_utf16		__ntapi_tt_get_long_option_meta_utf16;
ntapi_tt_array_copy_utf8			__ntapi_tt_array_copy_utf8;
ntapi_tt_array_copy_utf16			__ntapi_tt_array_copy_utf16;
ntapi_tt_array_convert_utf8_to_utf16		__ntapi_tt_array_convert_utf8_to_utf16;
ntapi_tt_array_convert_utf16_to_utf8		__ntapi_tt_array_convert_utf16_to_utf8;

/* nt_blitter.h */
ntapi_blt_alloc					__ntapi_blt_alloc;
ntapi_blt_free					__ntapi_blt_free;
ntapi_blt_acquire				__ntapi_blt_acquire;
ntapi_blt_obtain				__ntapi_blt_obtain;
ntapi_blt_possess				__ntapi_blt_possess;
ntapi_blt_release				__ntapi_blt_release;
ntapi_blt_get					__ntapi_blt_get;
ntapi_blt_set					__ntapi_blt_set;

/* nt_unicode.h */
ntapi_uc_validate_unicode_stream_utf8		__ntapi_uc_validate_unicode_stream_utf8;
ntapi_uc_validate_unicode_stream_utf16		__ntapi_uc_validate_unicode_stream_utf16;
ntapi_uc_get_code_point_byte_count_utf8		__ntapi_uc_get_code_point_byte_count_utf8;
ntapi_uc_get_code_point_byte_count_utf16	__ntapi_uc_get_code_point_byte_count_utf16;
ntapi_uc_convert_unicode_stream_utf8_to_utf16	__ntapi_uc_convert_unicode_stream_utf8_to_utf16;
ntapi_uc_convert_unicode_stream_utf8_to_utf32	__ntapi_uc_convert_unicode_stream_utf8_to_utf32;
ntapi_uc_convert_unicode_stream_utf16_to_utf8	__ntapi_uc_convert_unicode_stream_utf16_to_utf8;
ntapi_uc_convert_unicode_stream_utf16_to_utf32	__ntapi_uc_convert_unicode_stream_utf16_to_utf32;


/* nt_daemon.h */
ntapi_dsr_init					__ntapi_dsr_init;
ntapi_dsr_start					__ntapi_dsr_start;
ntapi_dsr_create_port				__ntapi_dsr_create_port;
ntapi_dsr_connect_internal_client		__ntapi_dsr_connect_internal_client;
ntapi_dsr_internal_client_connect		__ntapi_dsr_internal_client_connect;

/* nt_vfd.h */
ntapi_vfd_dev_name_init				__ntapi_vfd_dev_name_init;

/* nt_tty.h */
ntapi_tty_create_session			__ntapi_tty_create_session;
ntapi_tty_join_session				__ntapi_tty_join_session;
ntapi_tty_connect				__ntapi_tty_connect;
ntapi_tty_client_session_query			__ntapi_tty_client_session_query;
ntapi_tty_client_session_set			__ntapi_tty_client_session_set;
ntapi_tty_client_process_register		__ntapi_tty_client_process_register;
ntapi_tty_query_information_server		__ntapi_tty_query_information_server;
ntapi_tty_request_peer				__ntapi_tty_request_peer;
ntapi_tty_vms_query				__ntapi_tty_vms_query;
ntapi_tty_vms_request				__ntapi_tty_vms_request;
ntapi_pty_open					__ntapi_pty_open;
ntapi_pty_reopen				__ntapi_pty_reopen;
ntapi_pty_close					__ntapi_pty_close;
ntapi_pty_read					__ntapi_pty_read;
ntapi_pty_write					__ntapi_pty_write;
ntapi_pty_ioctl					__ntapi_pty_ioctl;
ntapi_pty_query					__ntapi_pty_query;
ntapi_pty_set					__ntapi_pty_set;
ntapi_pty_cancel				__ntapi_pty_cancel;

/* nt_socket.h */
ntapi_sc_socket					__ntapi_sc_socket_v1;
ntapi_sc_socket					__ntapi_sc_socket_v2;
ntapi_sc_bind					__ntapi_sc_bind_v1;
ntapi_sc_bind					__ntapi_sc_bind_v2;
ntapi_sc_connect				__ntapi_sc_connect_v1;
ntapi_sc_connect				__ntapi_sc_connect_v2;
ntapi_sc_getsockname				__ntapi_sc_getsockname_v1;
ntapi_sc_getsockname				__ntapi_sc_getsockname_v2;
ntapi_sc_server_accept_connection		__ntapi_sc_server_accept_connection_v1;
ntapi_sc_server_accept_connection		__ntapi_sc_server_accept_connection_v2;
ntapi_sc_server_duplicate_socket		__ntapi_sc_server_duplicate_socket;
ntapi_sc_listen					__ntapi_sc_listen;
ntapi_sc_accept					__ntapi_sc_accept;
ntapi_sc_send					__ntapi_sc_send;
ntapi_sc_recv					__ntapi_sc_recv;
ntapi_sc_shutdown				__ntapi_sc_shutdown;
ntapi_sc_wait					__ntapi_sc_wait;

/* nt_mount.h */
ntapi_tt_get_dos_drive_device_handle		__ntapi_tt_get_dos_drive_device_handle;
ntapi_tt_get_dos_drive_root_handle		__ntapi_tt_get_dos_drive_root_handle;
ntapi_tt_get_dos_drive_device_name		__ntapi_tt_get_dos_drive_device_name;
ntapi_tt_get_dos_drive_mount_points		__ntapi_tt_get_dos_drive_mount_points;
ntapi_tt_dev_mount_points_to_statfs		__ntapi_tt_dev_mount_points_to_statfs;
ntapi_tt_get_dos_drive_letter_from_device	__ntapi_tt_get_dos_drive_letter_from_device;

/* nt_istat.h */
ntapi_tt_istat					__ntapi_tt_istat;
ntapi_tt_validate_fs_handle			__ntapi_tt_validate_fs_handle;

/* nt_stat.h */
ntapi_tt_stat					__ntapi_tt_stat;

/* nt_statfs.h */
ntapi_tt_statfs					__ntapi_tt_statfs;

/* nt_vmount.h */
ntapi_vms_get_node_by_dev_name			__ntapi_vms_get_node_by_dev_name;
ntapi_vms_get_node_by_end_component		__ntapi_vms_get_node_by_end_component;
ntapi_vms_cache_alloc				__ntapi_vms_cache_alloc;
ntapi_vms_cache_free				__ntapi_vms_cache_free;
ntapi_vms_client_connect			__ntapi_vms_client_connect;
ntapi_vms_client_disconnect			__ntapi_vms_client_disconnect;
ntapi_vms_point_attach				__ntapi_vms_point_attach;
ntapi_vms_point_get_handles			__ntapi_vms_point_get_handles;
ntapi_vms_ref_count_inc				__ntapi_vms_ref_count_inc;
ntapi_vms_ref_count_dec				__ntapi_vms_ref_count_dec;
ntapi_vms_table_query				__ntapi_vms_table_query;

/* nt_hashes.h */
ntapi_tt_populate_hashed_import_table		__ntapi_tt_populate_hashed_import_table;

/* nt_guid.h */
ntapi_tt_guid_copy				__ntapi_tt_guid_copy;
ntapi_tt_guid_compare				__ntapi_tt_guid_compare;
ntapi_tt_guid_to_utf16_string			__ntapi_tt_guid_to_utf16_string;
ntapi_tt_utf16_string_to_guid			__ntapi_tt_utf16_string_to_guid;

/* debug */
ntapi_dbg_write					__dbg_write;
ntapi_dbg_fn_call				__dbg_fn_call;
ntapi_dbg_msg					__dbg_msg;

/* csrss */
ntapi_tt_get_csr_port_handle_addr_by_logic	__GET_CSR_PORT_HANDLE_BY_LOGIC;
ntapi_csr_port_handle				__ntapi_csr_port_handle;

#ifdef __cplusplus
}
#endif
#endif
