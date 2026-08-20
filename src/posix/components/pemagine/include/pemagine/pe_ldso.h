#ifndef PE_LDSO_H
#define PE_LDSO_H

#ifdef  PE_LDSO

#define pe_enum_image_exports               __ldso_enum_image_exports
#define pe_enum_image_import_hdrs           __ldso_enum_image_import_hdrs
#define pe_enum_modules_in_init_order       __ldso_enum_modules_in_init_order
#define pe_enum_modules_in_load_order       __ldso_enum_modules_in_load_order
#define pe_enum_modules_in_memory_order     __ldso_enum_modules_in_memory_order
#define pe_find_framework_loader            __ldso_find_framework_loader
#define pe_get_export_symbol_info           __ldso_get_export_symbol_info
#define pe_get_first_module_handle          __ldso_get_first_module_handle
#define pe_get_framework_runtime_data       __ldso_get_framework_runtime_data
#define pe_get_ldr_entry_from_addr          __ldso_get_ldr_entry_from_addr
#define pe_get_image_coff_hdr_addr          __ldso_get_image_coff_hdr_addr
#define pe_get_image_data_dirs_addr         __ldso_get_image_data_dirs_addr
#define pe_get_image_dos_hdr_addr           __ldso_get_image_dos_hdr_addr
#define pe_get_image_entry_point_addr       __ldso_get_image_entry_point_addr
#define pe_get_image_export_hdr_addr        __ldso_get_image_export_hdr_addr
#define pe_get_image_import_dir_addr        __ldso_get_image_import_dir_addr
#define pe_get_image_named_section_addr     __ldso_get_image_named_section_addr
#define pe_get_image_opt_hdr_addr           __ldso_get_image_opt_hdr_addr
#define pe_get_image_section_tbl_addr       __ldso_get_image_section_tbl_addr
#define pe_get_image_special_hdr_addr       __ldso_get_image_special_hdr_addr
#define pe_get_image_stack_heap_info        __ldso_get_image_stack_heap_info
#define pe_get_import_symbol_info           __ldso_get_import_symbol_info
#define pe_get_image_block_section_addr     __ldso_get_image_block_section_addr
#define pe_get_kernel32_module_handle       __ldso_get_kernel32_module_handle
#define pe_get_module_handle                __ldso_get_module_handle
#define pe_get_ntdll_module_handle          __ldso_get_ntdll_module_handle
#define pe_get_peb_command_line             __ldso_get_peb_command_line
#define pe_get_peb_environment_block        __ldso_get_peb_environment_block
#define pe_get_procedure_address            __ldso_get_procedure_address
#define pe_get_symbol_module_info           __ldso_get_symbol_module_info
#define pe_get_symbol_name                  __ldso_get_symbol_name
#define pe_load_framework_library           __ldso_load_framework_library
#define pe_load_framework_loader            __ldso_load_framework_loader
#define pe_load_framework_loader_ex         __ldso_load_framework_loader_ex
#define pe_open_image_from_addr             __ldso_open_image_from_addr
#define pe_open_physical_parent_directory   __ldso_open_physical_parent_directory
#define pe_terminate_current_process        __ldso_terminate_current_process

#endif

#endif
