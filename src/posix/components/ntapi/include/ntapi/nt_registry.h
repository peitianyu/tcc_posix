#ifndef _NT_REGISTRY_H_
#define _NT_REGISTRY_H_

#include <psxtypes/psxtypes.h>
#include "nt_object.h"

typedef enum _nt_registry_types {
	NT_REG_NONE				= 0x00,
	NT_REG_SZ				= 0x01,
	NT_REG_EXPAND_SZ			= 0x02,
	NT_REG_BINARY				= 0x03,
	NT_REG_DWORD				= 0x04,
	NT_REG_DWORD_LITTLE_ENDIAN		= 0x04,
	NT_REG_DWORD_BIG_ENDIAN			= 0x05,
	NT_REG_LINK				= 0x06,
	NT_REG_MULTI_SZ				= 0x07,
	NT_REG_RESOURCE_LIST			= 0x08,
	NT_REG_FULL_RESOURCE_DESCRIPTOR		= 0x09,
	NT_REG_RESOURCE_REQUIREMENTS_LIST	= 0x0A,
	NT_REG_QWORD				= 0x0B,
	NT_REG_QWORD_LITTLE_ENDIAN		= 0x0B,
} nt_registry_types;


typedef enum _nt_key_info_class {
	NT_KEY_BASIC_INFORMATION,
	NT_KEY_NODE_INFORMATION,
	NT_KEY_FULL_INFORMATION,
	NT_KEY_NAME_INFORMATION,
} nt_key_info_class;


typedef enum _nt_key_value_info_class {
	NT_KEY_VALUE_BASIC_INFORMATION,
	NT_KEY_VALUE_FULL_INFORMATION,
	NT_KEY_VALUE_PARTIAL_INFORMATION,
	NT_KEY_VALUE_FULL_INFORMATION_ALIGN64,
} nt_key_value_info_class;


typedef enum _nt_key_set_info_class {
	NT_KEY_LAST_WRITE_TIME_INFORMATION	= 0
} nt_key_set_info_class;


/* registry key access bits */
#define NT_KEY_QUERY_VALUE		0x00000001
#define NT_KEY_SET_VALUE		0x00000002
#define NT_KEY_CREATE_SUB_NT_KEY	0x00000004
#define NT_KEY_ENUMERATE_SUB_NT_KEYS	0x00000008
#define NT_KEY_NOTIFY			0x00000010
#define NT_KEY_CREATE_LINK		0x00000020
#define NT_KEY_WOW64_64NT_KEY		0x00000100
#define NT_KEY_WOW64_32NT_KEY		0x00000200
#define NT_KEY_WRITE			0x00020006
#define NT_KEY_READ			0x00020019
#define NT_KEY_EXECUTE			0x00020019
#define NT_KEY_ALL_ACCESS		0x000F003F


/* registry option bits */
#define NT_REG_OPTION_NON_VOLATILE	0x00000000L
#define NT_REG_OPTION_VOLATILE		0x00000001L
#define NT_REG_OPTION_CREATE_LINK	0x00000002L
#define NT_REG_OPTION_BACKUP_RESTORE	0x00000004L
#define NT_REG_OPTION_OPEN_LINK		0x00000008L


/* registry hive option bits */
#define NT_REG_WHOLE_HIVE_VOLATILE	0x00000001L
#define NT_REG_REFRESH_HIVE		0x00000002L
#define NT_REG_NO_LAZY_FLUSH		0x00000004L
#define NT_REG_FORCE_RESTORE		0x00000008L


/* registry disposition bits */
#define NT_REG_CREATED_NEW_KEY		0x00000000L
#define NT_REG_OPENED_EXISTING_KEY	0x00000001L


/* registry monitor bits */
#define NT_REG_MONITOR_SINGLE_KEY	0x0000
#define NT_REG_MONITOR_SECOND_KEY	0x0001


/* registry key notification bits */
#define NT_REG_NOTIFY_CHANGE_NAME	0x00000001L
#define NT_REG_NOTIFY_CHANGE_ATTRIBUTES	0x00000002L
#define NT_REG_NOTIFY_CHANGE_LAST_SET	0x00000004L
#define NT_REG_NOTIFY_CHANGE_SECURITY	0x00000008L

#define NT_REG_LEGAL_CHANGE_FILTER	NT_REG_NOTIFY_CHANGE_NAME \
					| NT_REG_NOTIFY_CHANGE_ATTRIBUTES \
					| NT_REG_NOTIFY_CHANGE_LAST_SET \
					| NT_REG_NOTIFY_CHANGE_SECURITY


typedef struct _nt_key_basic_information {
	nt_large_integer	last_write_time;
	uint32_t		title_index;
	uint32_t		name_length;
	wchar16_t		name[];
} nt_key_basic_information;


typedef struct _nt_key_node_information {
	nt_large_integer	last_write_time;
	uint32_t		title_index;
	uint32_t		class_offset;
	uint32_t		class_length;
	uint32_t		name_length;
	wchar16_t		name[];
} nt_key_node_information;


typedef struct _nt_key_full_information {
	nt_large_integer	last_write_time;
	uint32_t		title_index;
	uint32_t		class_offset;
	uint32_t		class_length;
	uint32_t		sub_keys;
	uint32_t		max_name_len;
	uint32_t		max_class_len;
	uint32_t		values;
	uint32_t		max_value_name_len;
	uint32_t		max_value_data_len;
	wchar16_t		kclass[];
} nt_key_full_information;


typedef struct _nt_key_name_information {
	uint32_t	name_length;
	wchar16_t	name[];
} nt_key_name_information;


typedef struct _nt_key_value_basic_information {
	uint32_t	title_index;
	uint32_t	type;
	uint32_t	name_length;
	wchar16_t	name[];
} _nt_key_value_basic_information;


typedef struct _nt_key_value_full_information {
	uint32_t	title_index;
	uint32_t	type;
	uint32_t	data_offset;
	uint32_t	data_length;
	uint32_t	name_length;
	wchar16_t	name[];
} nt_key_value_full_information;


typedef struct _nt_key_value_partial_information {
	uint32_t	title_index;
	uint32_t	type;
	uint32_t	data_length;
	unsigned char	data[];
} nt_key_value_partial_information;


typedef struct _nt_key_value_entry {
	nt_unicode_string *	value_name;
	uint32_t		data_length;
	uint32_t		data_offset;
	uint32_t		type;
} nt_key_value_entry;


typedef struct _nt_key_last_write_time_information {
	nt_large_integer	last_write_time;
} nt_key_last_write_time_information;


typedef int32_t	__stdcall ntapi_zw_create_key(
	__out	void **			hkey,
	__in	uint32_t		desired_access,
	__in	nt_object_attributes *	obj_attr,
	__in	uint32_t		title_index,
	__in	nt_unicode_string *	reg_class	__optional,
	__in	uint32_t		create_options,
	__out	uint32_t *		disposition	__optional);


typedef int32_t	__stdcall ntapi_zw_open_key(
	__out	void **			hkey,
	__in	uint32_t		desired_access,
	__in	nt_object_attributes *	obj_attr);


typedef int32_t	__stdcall ntapi_zw_delete_key(
	__in	void *	hkey);


typedef int32_t	__stdcall ntapi_zw_flush_key(
	__in	void *	hkey);


typedef int32_t	__stdcall ntapi_zw_save_key(
	__in	void *	hkey,
	__in	void *	hfile);


typedef int32_t	__stdcall ntapi_zw_save_merged_keys(
	__in	void *	hkey_1st,
	__in	void *	hkey_2nd,
	__in	void *	hfile);


typedef int32_t	__stdcall ntapi_zw_restore_key(
	__in	void *		hkey,
	__in	void *		hfile,
	__in	uint32_t	flags);


typedef int32_t	__stdcall ntapi_zw_load_key(
	__in	nt_object_attributes	key_obj_attr,
	__in	nt_object_attributes	file_obj_attr);


typedef int32_t	__stdcall ntapi_zw_load_key2(
	__in	nt_object_attributes	key_obj_attr,
	__in	nt_object_attributes	file_obj_attr,
	__in	uint32_t		flags);


typedef int32_t	__stdcall ntapi_zw_unload_key(
	__in	nt_object_attributes	key_obj_attr);


typedef int32_t	__stdcall ntapi_zw_query_open_sub_keys(
	__in	nt_object_attributes	key_obj_attr,
	__out	uint32_t *		number_of_keys);


typedef int32_t	__stdcall ntapi_zw_replace_key(
	__in	nt_object_attributes	new_file_obj_attr,
	__in	void *			hkey,
	__in	nt_object_attributes	old_file_obj_attr);


typedef int32_t	__stdcall ntapi_zw_set_information_key(
	__in	void *			hkey,
	__in	nt_key_set_info_class	key_info_class,
	__in	void *			key_info,
	__in	uint32_t		key_info_length);


typedef int32_t	__stdcall ntapi_zw_query_key(
	__in	void *			hkey,
	__in	nt_key_info_class	key_info_class,
	__out	void *			key_info,
	__in	uint32_t		key_info_length,
	__out	uint32_t *		result_length);


typedef int32_t	__stdcall ntapi_zw_enumerate_key(
	__in	void *			hkey,
	__in	uint32_t		index,
	__in	nt_key_info_class	key_info_class,
	__out	void *			key_info,
	__in	uint32_t		key_info_length,
	__out	uint32_t *		result_length);


typedef int32_t	__stdcall ntapi_zw_notify_change_key(
	__in	void *			hkey,
	__in	void *			hevent		__optional,
	__in	nt_io_apc_routine *	apc_routine	__optional,
	__in	void *			apc_context	__optional,
	__out	nt_io_status_block *	io_status_block,
	__in	uint32_t		notify_filter,
	__in	unsigned char		watch_subtree,
	__in	void *			buffer,
	__in	uint32_t		buffer_length,
	__in	unsigned char		asynchronous);


typedef int32_t	__stdcall ntapi_zw_notify_change_multiple_keys(
	__in	void *			hkey,
	__in	uint32_t		flags,
	__in	nt_object_attributes *	key_obj_attr,
	__in	void *			hevent		__optional,
	__in	nt_io_apc_routine *	apc_routine	__optional,
	__in	void *			apc_context	__optional,
	__out	nt_io_status_block *	io_status_block,
	__in	uint32_t		notify_filter,
	__in	unsigned char		watch_subtree,
	__in	void *			buffer,
	__in	uint32_t		buffer_length,
	__in	unsigned char		asynchronous);


typedef int32_t	__stdcall ntapi_zw_delete_value_key(
	__in	void *			hkey,
	__in	nt_unicode_string *	value_name);


typedef int32_t	__stdcall ntapi_zw_set_value_key(
	__in	void *			hkey,
	__in	nt_unicode_string *	value_name,
	__in	uint32_t		title_index,
	__in	uint32_t		type,
	__in	void *			data,
	__in	uint32_t		data_size);


typedef int32_t	__stdcall ntapi_zw_query_value_key(
	__in	void *			hkey,
	__in	nt_unicode_string *	value_name,
	__in	nt_key_value_info_class	key_value_info_class,
	__out	void *			key_value_info,
	__in	uint32_t		key_value_info_length,
	__out	uint32_t *		result_length);


typedef int32_t	__stdcall ntapi_zw_enumerate_value_key(
	__in	void *			hkey,
	__in	uint32_t		index,
	__in	nt_key_value_info_class	key_value_info_class,
	__out	void *			key_value_info,
	__in	uint32_t		key_value_info_length,
	__out	uint32_t *		result_length);


typedef int32_t	__stdcall ntapi_zw_query_multiple_value_key(
	__in		void *			hkey,
	__in_out	nt_key_value_entry *	value_list,
	__in		uint32_t		number_of_values,
	__out		void *			buffer,
	__in_out	uint32_t *		buffer_length,
	__out		uint32_t *		buffer_nedded);


typedef int32_t	__stdcall ntapi_zw_initialize_registry(
	__in	unsigned char	setup);

#endif
