/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>

#include <ntapi/nt_status.h>
#include <ntapi/nt_crc32.h>
#include <ntapi/nt_object.h>
#include <ntapi/nt_sysinfo.h>
#include <ntapi/nt_memory.h>
#include <ntapi/nt_section.h>
#include <ntapi/nt_thread.h>
#include <ntapi/nt_process.h>
#include <ntapi/nt_job.h>
#include <ntapi/nt_token.h>
#include <ntapi/nt_sync.h>
#include <ntapi/nt_time.h>
#include <ntapi/nt_profiling.h>
#include <ntapi/nt_port.h>
#include <ntapi/nt_device.h>
#include <ntapi/nt_file.h>
#include <ntapi/nt_registry.h>
#include <ntapi/nt_security.h>
#include <ntapi/nt_pnp.h>
#include <ntapi/nt_exception.h>
#include <ntapi/nt_locale.h>
#include <ntapi/nt_uuid.h>
#include <ntapi/nt_atom.h>
#include <ntapi/nt_os.h>
#include <ntapi/nt_ldr.h>
#include <ntapi/nt_string.h>
#include <ntapi/nt_guid.h>
#include <ntapi/nt_argv.h>
#include <ntapi/nt_blitter.h>
#include <ntapi/nt_unicode.h>
#include <ntapi/nt_socket.h>
#include <ntapi/nt_mount.h>
#include <ntapi/nt_istat.h>
#include <ntapi/nt_stat.h>
#include <ntapi/nt_statfs.h>
#include <ntapi/nt_daemon.h>
#include <ntapi/nt_tty.h>
#include <ntapi/nt_vmount.h>
#include <ntapi/nt_hash.h>
#include <ntapi/nt_debug.h>
#include <ntapi/nt_atomic.h>
#include <ntapi/ntapi.h>

#include "ntapi_impl.h"
#include "ntapi_hash_table.h"

/* simplified once mechanism for free-standing applications */
typedef int32_t __fastcall __ntapi_init_fn(ntapi_vtbl ** pvtbl);

static __ntapi_init_fn __ntapi_init_once;
static __ntapi_init_fn __ntapi_init_pending;
static __ntapi_init_fn __ntapi_init_completed;

static intptr_t		 __ntapi_init_idx = 0;
static __ntapi_init_fn * __ntapi_init_vtbl[3] = {
	__ntapi_init_once,
	__ntapi_init_pending,
	__ntapi_init_completed};

/* accessor */
ntapi_vtbl ___ntapi		= {0};
ntapi_vtbl ___ntapi_shadow	= {0};

/* .bss */
static __ntapi_img_sec_bss __ntapi_img_bss;

/* .rdata */
static union __ntapi_img_rdata __ntapi_rdata = {{
		{__NTAPI_HASH_TABLE},		/* __ntapi_import_table */
		0,				/* __ntapi */
		{{0}},				/* __session_name */
		0}};				/* __internals */

#define internals	__ntapi_rdata.img_sec_data.__internals
#define import_table	__ntapi_rdata.img_sec_data.__ntapi_import_table


static int32_t __fastcall __ntapi_init_once(ntapi_vtbl ** pvtbl)
{
	int32_t					status;
	void * 					hntdll;
	size_t					block_size;
	ntapi_zw_allocate_virtual_memory *	pfn_zw_allocate_virtual_memory;
	char					fname_allocate_virtual_memory[] = 
						"ZwAllocateVirtualMemory";
	/* once */
	at_locked_inc(&__ntapi_init_idx);

	/* pvtbl */
	if (!(pvtbl))
		return NT_STATUS_INVALID_PARAMETER;
	else
		*pvtbl = (ntapi_vtbl *)0;

	/* ntdll */
	if (!(hntdll = pe_get_ntdll_module_handle()))
		return NT_STATUS_DLL_INIT_FAILED;

	pfn_zw_allocate_virtual_memory	= (ntapi_zw_allocate_virtual_memory *)
		pe_get_procedure_address(
			hntdll,
			fname_allocate_virtual_memory);

	if (!pfn_zw_allocate_virtual_memory)
		return NT_STATUS_DLL_INIT_FAILED;

	/* ntapi_internals: alloc */
	block_size = sizeof(ntapi_internals);
	status = pfn_zw_allocate_virtual_memory(
		NT_CURRENT_PROCESS_HANDLE,
		(void **)&internals,
		0,
		&block_size,
		NT_MEM_COMMIT,
		NT_PAGE_READWRITE);

	if (status != NT_STATUS_SUCCESS)
		return status;

	/* hashed import table */
	__ntapi_tt_populate_hashed_import_table(
		pe_get_ntdll_module_handle(),
		__ntapi,
		import_table,
		__NT_IMPORTED_SYMBOLS_ARRAY_SIZE);

	/* alternate implementation */
	__ntapi->rtl_init_unicode_string 			= __ntapi_tt_init_unicode_string_from_utf16;

	/* extension functions */
	/* nt_object.h */
	__ntapi->tt_create_keyed_object_directory		= __ntapi_tt_create_keyed_object_directory;
	__ntapi->tt_open_keyed_object_directory			= __ntapi_tt_open_keyed_object_directory;
	__ntapi->tt_create_keyed_object_directory_entry		= __ntapi_tt_create_keyed_object_directory_entry;

	/* nt_crc32.h */
	__ntapi->tt_buffer_crc32 				= __ntapi_tt_buffer_crc32;
	__ntapi->tt_mbstr_crc32					= __ntapi_tt_mbstr_crc32;
	__ntapi->tt_crc32_table					= __ntapi_tt_crc32_table;

	/* nt_file.h */
	__ntapi->tt_get_file_handle_type				= __ntapi_tt_get_file_handle_type;
	__ntapi->tt_open_logical_parent_directory		= __ntapi_tt_open_logical_parent_directory;
	__ntapi->tt_open_physical_parent_directory		= __ntapi_tt_open_physical_parent_directory;

	/* nt_ldr.h */
	__ntapi->ldr_load_system_dll				= __ntapi_ldr_load_system_dll;
	__ntapi->ldr_create_state_snapshot			= __ntapi_ldr_create_state_snapshot;
	__ntapi->ldr_revert_state_to_snapshot			= __ntapi_ldr_revert_state_to_snapshot;

	/* nt_string.h */
	__ntapi->tt_string_null_offset_multibyte 		= __ntapi_tt_string_null_offset_multibyte;
	__ntapi->tt_string_null_offset_short			= __ntapi_tt_string_null_offset_short;
	__ntapi->tt_string_null_offset_dword			= __ntapi_tt_string_null_offset_dword;
	__ntapi->tt_string_null_offset_qword			= __ntapi_tt_string_null_offset_qword;
	__ntapi->tt_string_null_offset_ptrsize			= __ntapi_tt_string_null_offset_ptrsize;
	__ntapi->strlen						= __ntapi_tt_string_null_offset_multibyte;
	__ntapi->wcslen						= __ntapi_wcslen;
	__ntapi->tt_aligned_block_memset 			= __ntapi_tt_aligned_block_memset;
	__ntapi->tt_aligned_block_memcpy 			= __ntapi_tt_aligned_block_memcpy;
	__ntapi->tt_memcpy_utf16 				= __ntapi_tt_memcpy_utf16;
	__ntapi->tt_aligned_memcpy_utf16 			= __ntapi_tt_aligned_memcpy_utf16;
	__ntapi->tt_generic_memset				= __ntapi_tt_generic_memset;
	__ntapi->tt_generic_memcpy 				= __ntapi_tt_generic_memcpy;
	__ntapi->tt_uint16_to_hex_utf16				= __ntapi_tt_uint16_to_hex_utf16;
	__ntapi->tt_uint32_to_hex_utf16				= __ntapi_tt_uint32_to_hex_utf16;
	__ntapi->tt_uint64_to_hex_utf16				= __ntapi_tt_uint64_to_hex_utf16;
	__ntapi->tt_uintptr_to_hex_utf16 			= __ntapi_tt_uintptr_to_hex_utf16;
	__ntapi->tt_hex_utf16_to_uint16				= __ntapi_tt_hex_utf16_to_uint16;
	__ntapi->tt_hex_utf16_to_uint32				= __ntapi_tt_hex_utf16_to_uint32;
	__ntapi->tt_hex_utf16_to_uint64				= __ntapi_tt_hex_utf16_to_uint64;
	__ntapi->tt_hex_utf16_to_uintptr 			= __ntapi_tt_hex_utf16_to_uintptr;
	__ntapi->tt_init_unicode_string_from_utf16 		= __ntapi_tt_init_unicode_string_from_utf16;
	__ntapi->tt_uint16_to_hex_utf8				= __ntapi_tt_uint16_to_hex_utf8;
	__ntapi->tt_uint32_to_hex_utf8				= __ntapi_tt_uint32_to_hex_utf8;
	__ntapi->tt_uint64_to_hex_utf8				= __ntapi_tt_uint64_to_hex_utf8;
	__ntapi->tt_uintptr_to_hex_utf8				= __ntapi_tt_uintptr_to_hex_utf8;

	/* nt_guid.h */
	__ntapi->tt_guid_copy 					= __ntapi_tt_guid_copy;
	__ntapi->tt_guid_compare 				= __ntapi_tt_guid_compare;
	__ntapi->tt_guid_to_utf16_string 			= __ntapi_tt_guid_to_utf16_string;
	__ntapi->tt_utf16_string_to_guid 			= __ntapi_tt_utf16_string_to_guid;

	/* nt_sysinfo.h */
	__ntapi->tt_get_system_directory_native_path 		= __ntapi_tt_get_system_directory_native_path;
	__ntapi->tt_get_system_directory_dos_path 		= __ntapi_tt_get_system_directory_dos_path;
	__ntapi->tt_get_system_directory_handle			= __ntapi_tt_get_system_directory_handle;
	__ntapi->tt_get_system_info_snapshot  			= __ntapi_tt_get_system_info_snapshot;

	/* nt_thread.h */
	__ntapi->tt_create_local_thread				= __ntapi_tt_create_local_thread;
	__ntapi->tt_create_remote_thread 			= __ntapi_tt_create_remote_thread;
	__ntapi->tt_create_thread 				= __ntapi_tt_create_thread;

	/* nt_process.h */
	__ntapi->tt_create_remote_process_params 		= __ntapi_tt_create_remote_process_params;
	__ntapi->tt_get_runtime_data 				= __ntapi_tt_get_runtime_data;
	__ntapi->tt_init_runtime_data				= __ntapi_tt_init_runtime_data;
	__ntapi->tt_update_runtime_data				= __ntapi_tt_update_runtime_data;
	__ntapi->tt_exec_map_image_as_data			= __ntapi_tt_exec_map_image_as_data;
	__ntapi->tt_exec_unmap_image				= __ntapi_tt_exec_unmap_image;

	/* nt_section.h */
	__ntapi->tt_get_section_name 				= __ntapi_tt_get_section_name;

	/* nt_sync.h */
	__ntapi->tt_create_inheritable_event 			= __ntapi_tt_create_inheritable_event;
	__ntapi->tt_create_private_event 			= __ntapi_tt_create_private_event;
	__ntapi->tt_wait_for_dummy_event 			= __ntapi_tt_wait_for_dummy_event;
	__ntapi->tt_sync_block_init 				= __ntapi_tt_sync_block_init;
	__ntapi->tt_sync_block_lock 				= __ntapi_tt_sync_block_lock;
	__ntapi->tt_sync_block_server_lock 			= __ntapi_tt_sync_block_server_lock;
	__ntapi->tt_sync_block_unlock 				= __ntapi_tt_sync_block_unlock;
	__ntapi->tt_sync_block_invalidate 			= __ntapi_tt_sync_block_invalidate;

	/* nt_port.h */
	__ntapi->csr_port_handle 				= __ntapi_csr_port_handle;
	__ntapi->tt_port_guid_from_type				= __ntapi_tt_port_guid_from_type;
	__ntapi->tt_port_type_from_guid				= __ntapi_tt_port_type_from_guid;
	__ntapi->tt_port_generate_keys 				= __ntapi_tt_port_generate_keys;
	__ntapi->tt_port_format_keys 				= __ntapi_tt_port_format_keys;
	__ntapi->tt_port_name_from_attributes 			= __ntapi_tt_port_name_from_attributes;

	/* nt_argv.h */
	__ntapi->tt_get_cmd_line_utf16 				= __ntapi_tt_get_cmd_line_utf16;
	__ntapi->tt_get_peb_env_block_utf16 			= __ntapi_tt_get_peb_env_block_utf16;
	__ntapi->tt_parse_cmd_line_args_utf16 			= __ntapi_tt_parse_cmd_line_args_utf16;
	__ntapi->tt_get_argv_envp_utf8 				= __ntapi_tt_get_argv_envp_utf8;
	__ntapi->tt_get_argv_envp_utf16				= __ntapi_tt_get_argv_envp_utf16;
	__ntapi->tt_get_env_var_meta_utf16 			= __ntapi_tt_get_env_var_meta_utf16;
	__ntapi->tt_get_short_option_meta_utf16			= __ntapi_tt_get_short_option_meta_utf16;
	__ntapi->tt_get_long_option_meta_utf16 			= __ntapi_tt_get_long_option_meta_utf16;
	__ntapi->tt_array_copy_utf16				= __ntapi_tt_array_copy_utf16;
	__ntapi->tt_array_copy_utf8				= __ntapi_tt_array_copy_utf8;
	__ntapi->tt_array_convert_utf8_to_utf16			= __ntapi_tt_array_convert_utf8_to_utf16;
	__ntapi->tt_array_convert_utf16_to_utf8			= __ntapi_tt_array_convert_utf16_to_utf8;

	/* nt_blitter.h */
	__ntapi->blt_alloc					= __ntapi_blt_alloc;
	__ntapi->blt_free					= __ntapi_blt_free;
	__ntapi->blt_acquire					= __ntapi_blt_acquire;
	__ntapi->blt_obtain					= __ntapi_blt_obtain;
	__ntapi->blt_possess					= __ntapi_blt_possess;
	__ntapi->blt_release					= __ntapi_blt_release;
	__ntapi->blt_get 					= __ntapi_blt_get;
	__ntapi->blt_set 					= __ntapi_blt_set;

	/* nt_unicode.h */
	__ntapi->uc_validate_unicode_stream_utf8 		= __ntapi_uc_validate_unicode_stream_utf8;
	__ntapi->uc_validate_unicode_stream_utf16 		= __ntapi_uc_validate_unicode_stream_utf16;
	__ntapi->uc_get_code_point_byte_count_utf8		= __ntapi_uc_get_code_point_byte_count_utf8;
	__ntapi->uc_get_code_point_byte_count_utf16		= __ntapi_uc_get_code_point_byte_count_utf16;
	__ntapi->uc_convert_unicode_stream_utf8_to_utf16 	= __ntapi_uc_convert_unicode_stream_utf8_to_utf16;
	__ntapi->uc_convert_unicode_stream_utf8_to_utf32 	= __ntapi_uc_convert_unicode_stream_utf8_to_utf32;
	__ntapi->uc_convert_unicode_stream_utf16_to_utf8 	= __ntapi_uc_convert_unicode_stream_utf16_to_utf8;
	__ntapi->uc_convert_unicode_stream_utf16_to_utf32 	= __ntapi_uc_convert_unicode_stream_utf16_to_utf32;

	/* nt_daemon.h */
	__ntapi->dsr_init					= __ntapi_dsr_init;
	__ntapi->dsr_start					= __ntapi_dsr_start;
	__ntapi->dsr_create_port 				= __ntapi_dsr_create_port;
	__ntapi->dsr_connect_internal_client			= __ntapi_dsr_connect_internal_client;
	__ntapi->dsr_internal_client_connect			= __ntapi_dsr_internal_client_connect;

	/* nt_vfd.h */
	__ntapi->vfd_dev_name_init				= __ntapi_vfd_dev_name_init;

	/* nt_tty.h */
	__ntapi->tty_create_session				= __ntapi_tty_create_session;
	__ntapi->tty_join_session				= __ntapi_tty_join_session;
	__ntapi->tty_connect					= __ntapi_tty_connect;
	__ntapi->tty_client_session_query			= __ntapi_tty_client_session_query;
	__ntapi->tty_client_session_set				= __ntapi_tty_client_session_set;
	__ntapi->tty_client_process_register			= __ntapi_tty_client_process_register;
	__ntapi->tty_query_information_server			= __ntapi_tty_query_information_server;
	__ntapi->tty_request_peer				= __ntapi_tty_request_peer;
	__ntapi->tty_vms_query					= __ntapi_tty_vms_query;
	__ntapi->tty_vms_request 				= __ntapi_tty_vms_request;
	__ntapi->pty_open					= __ntapi_pty_open;
	__ntapi->pty_reopen					= __ntapi_pty_reopen;
	__ntapi->pty_close					= __ntapi_pty_close;
	__ntapi->pty_read					= __ntapi_pty_read;
	__ntapi->pty_write					= __ntapi_pty_write;
	__ntapi->pty_ioctl					= __ntapi_pty_ioctl;
	__ntapi->pty_query					= __ntapi_pty_query;
	__ntapi->pty_set						= __ntapi_pty_set;
	__ntapi->pty_cancel					= __ntapi_pty_cancel;

	/* nt_socket.h */
	__ntapi->sc_listen					= __ntapi_sc_listen;
	__ntapi->sc_accept					= __ntapi_sc_accept;
	__ntapi->sc_send 					= __ntapi_sc_send;
	__ntapi->sc_recv 					= __ntapi_sc_recv;
	__ntapi->sc_shutdown					= __ntapi_sc_shutdown;
	__ntapi->sc_server_duplicate_socket			= __ntapi_sc_server_duplicate_socket;
	__ntapi->sc_wait = __ntapi_sc_wait;

	/* nt_mount.h */
	__ntapi->tt_get_dos_drive_device_handle			= __ntapi_tt_get_dos_drive_device_handle;
	__ntapi->tt_get_dos_drive_root_handle			= __ntapi_tt_get_dos_drive_root_handle;
	__ntapi->tt_get_dos_drive_device_name			= __ntapi_tt_get_dos_drive_device_name;
	__ntapi->tt_get_dos_drive_mount_points			= __ntapi_tt_get_dos_drive_mount_points;
	__ntapi->tt_dev_mount_points_to_statfs			= __ntapi_tt_dev_mount_points_to_statfs;
	__ntapi->tt_get_dos_drive_letter_from_device		= __ntapi_tt_get_dos_drive_letter_from_device;

	/* nt_istat.h */
	__ntapi->tt_istat					= __ntapi_tt_istat;
	__ntapi->tt_validate_fs_handle				= __ntapi_tt_validate_fs_handle;

	/* nt_stat.h */
	__ntapi->tt_stat 					= __ntapi_tt_stat;

	/* nt_statfs.h */
	__ntapi->tt_statfs					= __ntapi_tt_statfs;

	/* nt_vmount.h */
	__ntapi->vms_get_node_by_dev_name			= __ntapi_vms_get_node_by_dev_name;
	__ntapi->vms_get_node_by_end_component			= __ntapi_vms_get_node_by_end_component;
	__ntapi->vms_cache_alloc 				= __ntapi_vms_cache_alloc;
	__ntapi->vms_cache_free					= __ntapi_vms_cache_free;
	__ntapi->vms_client_connect				= __ntapi_vms_client_connect;
	__ntapi->vms_client_disconnect				= __ntapi_vms_client_disconnect;
	__ntapi->vms_point_attach				= __ntapi_vms_point_attach;
	__ntapi->vms_point_get_handles				= __ntapi_vms_point_get_handles;
	__ntapi->vms_ref_count_inc				= __ntapi_vms_ref_count_inc;
	__ntapi->vms_ref_count_dec				= __ntapi_vms_ref_count_dec;
	__ntapi->vms_table_query 				= __ntapi_vms_table_query;

	/* nt_debug.h */
	#ifdef __DEBUG
	__ntapi->dbg_write					= __dbg_write;
	__ntapi->dbg_fn_call					= __dbg_fn_call;
	__ntapi->dbg_msg 					= __dbg_msg;
	#endif


	/* OS version dependent functions */
	if (__ntapi->zw_create_user_process) {
		__ntapi->tt_fork 				= __ntapi_tt_fork_v2;
		__ntapi->tt_create_native_process		= __ntapi_tt_create_native_process_v2;
		__ntapi->ipc_create_pipe 			= __ntapi_ipc_create_pipe_v2;
		__ntapi->sc_socket				= __ntapi_sc_socket_v2;
		__ntapi->sc_bind 				= __ntapi_sc_bind_v2;
		__ntapi->sc_connect				= __ntapi_sc_connect_v2;
		__ntapi->sc_server_accept_connection		= __ntapi_sc_server_accept_connection_v2;
		__ntapi->sc_getsockname				= __ntapi_sc_getsockname_v2;
	} else {
		__ntapi->tt_fork = __ntapi_tt_fork_v1;
		__ntapi->tt_create_native_process		= __ntapi_tt_create_native_process_v1;
		__ntapi->ipc_create_pipe 			= __ntapi_ipc_create_pipe_v1;
		__ntapi->sc_socket				= __ntapi_sc_socket_v1;
		__ntapi->sc_bind 				= __ntapi_sc_bind_v1;
		__ntapi->sc_connect				= __ntapi_sc_connect_v1;
		__ntapi->sc_server_accept_connection		= __ntapi_sc_server_accept_connection_v1;
		__ntapi->sc_getsockname				= __ntapi_sc_getsockname_v1;
	}

	/* internals */
	internals->ntapi_img_sec_bss				= &__ntapi_img_bss;
	internals->subsystem					= &__ntapi_rdata.img_sec_data.__session_name;

	internals->tt_get_csr_port_handle_addr_by_logic		= __GET_CSR_PORT_HANDLE_BY_LOGIC;
	internals->csr_port_handle_addr				= __GET_CSR_PORT_HANDLE_BY_LOGIC();

	/* shadow copy for client libraries */
	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)&___ntapi_shadow,
		(uintptr_t *)&___ntapi,
		sizeof(ntapi_vtbl));

	/* done */
	*pvtbl	= &___ntapi_shadow;
	at_locked_inc(&__ntapi_init_idx);

	return NT_STATUS_SUCCESS;
}


static int32_t __fastcall __ntapi_init_pending(ntapi_vtbl ** pvtbl)
{
	return NT_STATUS_PENDING;
}

static int32_t __fastcall __ntapi_init_completed(ntapi_vtbl ** pvtbl)
{
	*pvtbl = __ntapi;
	return NT_STATUS_SUCCESS;
};


__ntapi_api
int32_t __fastcall ntapi_init(ntapi_vtbl ** pvtbl)
{
	return __ntapi_init_vtbl[__ntapi_init_idx](pvtbl);
}


ntapi_internals * __cdecl __ntapi_internals(void)
{
	return internals;
}
