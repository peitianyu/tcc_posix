#ifndef _NT_OBJECT_H_
#define _NT_OBJECT_H_

#include <psxtypes/psxtypes.h>

typedef enum _nt_object_info_class {
	NT_OBJECT_BASIC_INFORMATION	= 0,
	NT_OBJECT_NAME_INFORMATION	= 1,
	NT_OBJECT_TYPE_INFORMATION	= 2,
	NT_OBJECT_ALL_TYPES_INFORMATION	= 3,
	NT_OBJECT_HANDLE_INFORMATION	= 4
} nt_object_info_class;


typedef enum _nt_security_impersonation_level {
	NT_SECURITY_ANONYMOUS		= 0,
	NT_SECURITY_IDENTIFICATION	= 1,
	NT_SECURITY_IMPERSONATION	= 2,
	NT_SECURITY_DELEGATION		= 3
} nt_security_impersonation_level;


typedef enum _nt_security_information {
	NT_OWNER_SECURITY_INFORMATION	= 0x01,
	NT_GROUP_SECURITY_INFORMATION	= 0x02,
	NT_DACL_SECURITY_INFORMATION	= 0x04,
	NT_SACL_SECURITY_INFORMATION	= 0x08
} nt_security_information;



/* generic access rights */
#define NT_SEC_DELETE				(0x00010000u)
#define NT_SEC_READ_CONTROL			(0x00020000u)
#define NT_SEC_WRITE_DAC			(0x00040000u)
#define NT_SEC_WRITE_OWNER			(0x00080000u)
#define NT_SEC_SYNCHRONIZE			(0x00100000u)
#define NT_SEC_STANDARD_RIGHTS_REQUIRED		(0x000F0000u)
#define NT_SEC_STANDARD_RIGHTS_READ		NT_SEC_READ_CONTROL
#define NT_SEC_STANDARD_RIGHTS_WRITE		NT_SEC_READ_CONTROL
#define NT_SEC_STANDARD_RIGHTS_EXECUTE		NT_SEC_READ_CONTROL
#define NT_SEC_STANDARD_RIGHTS_ALL		(0x001F0000u)
#define NT_SEC_SPECIFIC_RIGHTS_ALL		(0x0000FFFFu)

#define NT_GENERIC_ALL				(0x10000000u)
#define NT_GENERIC_EXECUTE			(0x20000000u)
#define NT_GENERIC_WRITE			(0x40000000u)
#define NT_GENERIC_READ				(0x80000000u)


/* zw_open_directory access rights */
#define NT_DIRECTORY_QUERY			(0x0001u)
#define NT_DIRECTORY_TRAVERSE			(0x0002u)
#define NT_DIRECTORY_CREATE_OBJECT		(0x0004u)
#define NT_DIRECTORY_CREATE_SUBDIRECTORY	(0x0008u)
#define NT_DIRECTORY_ALL_ACCESS			NT_DIRECTORY_QUERY \
							| NT_DIRECTORY_TRAVERSE \
							| NT_DIRECTORY_CREATE_OBJECT \
							| NT_DIRECTORY_CREATE_SUBDIRECTORY \
							| NT_SEC_STANDARD_RIGHTS_REQUIRED

/* zw_open_symbolic_link_object access rights */
#define NT_SYMBOLIC_LINK_QUERY			(0x0001u)
#define NT_SYMBOLIC_LINK_ALL_ACCESS		NT_SYMBOLIC_LINK_QUERY \
							| NT_SEC_STANDARD_RIGHTS_REQUIRED

/* object handles */
#define NT_HANDLE_FLAG_INHERIT			(0x0001u)
#define NT_HANDLE_FLAG_PROTECT_FROM_CLOSE	(0x0002u)
#define NT_HANDLE_PERMANENT			(0x0010u)
#define NT_HANDLE_EXCLUSIVE			(0x0020u)
#define NT_INVALID_HANDLE_VALUE 		((void *)(intptr_t)-1)

/* object attribute bits */
#define NT_OBJ_INHERIT	 			(0x0002u)
#define NT_OBJ_PERMANENT 			(0x0010u)
#define NT_OBJ_EXCLUSIVE 			(0x0020u)
#define NT_OBJ_CASE_INSENSITIVE			(0x0040u)
#define NT_OBJ_OPENIF	 			(0x0080u)
#define NT_OBJ_OPENLINK	 			(0x0100u)
#define NT_OBJ_KERNEL_HANDLE 			(0x0200u)

/* duplicate object bits */
#define NT_DUPLICATE_CLOSE_SOURCE		(0x0001u)
#define NT_DUPLICATE_SAME_ACCESS		(0x0002u)
#define NT_DUPLICATE_SAME_ATTRIBUTES		(0x0004u)

/* nt_security_descriptor constants (IFS open specification) */
#define NT_SE_OWNER_DEFAULTED		(int16_t)0x0001
#define NT_SE_GROUP_DEFAULTED		(int16_t)0x0002
#define NT_SE_DACL_PRESENT		(int16_t)0x0004
#define NT_SE_DACL_DEFAULTED		(int16_t)0x0008
#define NT_SE_SACL_PRESENT		(int16_t)0x0010
#define NT_SE_SACL_DEFAULTED		(int16_t)0x0020
#define NT_SE_DACL_AUTO_INHERIT_REQ	(int16_t)0x0100
#define NT_SE_SACL_AUTO_INHERIT_REQ	(int16_t)0x0200
#define NT_SE_DACL_AUTO_INHERITED	(int16_t)0x0400
#define NT_SE_SACL_AUTO_INHERITED	(int16_t)0x0800
#define NT_SE_DACL_PROTECTED		(int16_t)0x1000
#define NT_SE_SACL_PROTECTED		(int16_t)0x2000
#define NT_SE_RM_CONTROL_VALID		(int16_t)0x4000
#define NT_SE_SELF_RELATIVE		(int16_t)0x8000

/* security tracking */
#define NT_SECURITY_TRACKING_STATIC	0
#define NT_SECURITY_TRACKING_DYNAMIC	1

/* predefined security authorities */
#define NT_SECURITY_NULL_SID_AUTHORITY		0
#define NT_SECURITY_WORLD_SID_AUTHORITY		1
#define NT_SECURITY_LOCAL_SID_AUTHORITY		2
#define NT_SECURITY_CREATOR_SID_AUTHORITY	3
#define NT_SECURITY_NON_UNIQUE_AUTHORITY	4
#define NT_SECURITY_NT_AUTHORITY		5

/* token source length */
#define NT_TOKEN_SOURCE_LENGTH			8


typedef struct _nt_unicode_string {
	uint16_t	strlen;
	uint16_t	maxlen;
	uint16_t *	buffer;
} nt_unicode_string;


typedef union _nt_large_integer {
	struct {
		uint32_t	ulow;
		int32_t		ihigh;
	};
	long long		quad;
} nt_large_integer, nt_timeout, nt_filetime, nt_sec_size;


typedef struct _nt_io_status_block {
	union {
		int32_t		status;
		void *		pointer;
	};
	intptr_t		info;
} nt_io_status_block, nt_iosb;


typedef struct _nt_quota_limits {
	size_t			paged_pool_limit;
	size_t			non_paged_pool_limit;
	size_t			minimum_working_set_size;
	size_t			maximum_working_set_size;
	size_t			pagefile_limit;
	nt_large_integer	time_limit;
} nt_quota_limits, nt_ql;


typedef struct _nt_kernel_user_times {
	nt_large_integer	create_time;
	nt_large_integer	exit_time;
	nt_large_integer	kernel_time;
	nt_large_integer	user_time;
} nt_kernel_user_times, nt_kut;


typedef struct _nt_io_counters {
	nt_large_integer	read_operation_count;
	nt_large_integer	write_operation_count;
	nt_large_integer	other_operation_count;
	nt_large_integer	read_transfer_count;
	nt_large_integer	write_transfer_count;
	nt_large_integer	other_transfer_count;
} nt_io_counters;


typedef struct _nt_vm_counters {
	size_t		peak_virtual_size;
	size_t		virtual_size;
	size_t		page_fault_count;
	size_t		peak_working_set_size;
	size_t		working_set_size;
	size_t		quota_peak_paged_pool_usage;
	size_t		quota_paged_pool_usage;
	size_t		quota_peak_non_paged_pool_usage;
	size_t		quota_non_paged_pool_usage;
	size_t		pagefile_usage;
	size_t		peak_pagefile_usage;
} nt_vm_counters;


typedef struct _nt_pooled_usage_and_limits {
	size_t		peak_paged_pool_usage;
	size_t		paged_pool_usage;
	size_t		paged_pool_limit;
	size_t		peak_non_paged_pool_usage;
	size_t		non_paged_pool_usage;
	size_t		non_paged_pool_limit;
	size_t		peak_pagefile_usage;
	size_t		pagefile_usage;
	size_t		pagefile_limit;
} nt_pooled_usage_and_limits, nt_pual;


typedef struct _nt_client_id {
	uintptr_t	process_id;
	uintptr_t	thread_id;
} nt_client_id, nt_cid;


typedef struct _nt_generic_mapping {
	uint32_t	generic_read;
	uint32_t	generic_write;
	uint32_t	generic_execute;
	uint32_t	generic_all;
} nt_generic_mapping, nt_gmap;


typedef struct _nt_security_attributes {
	uint32_t	length;
	void *		security_descriptor;
	int32_t		inherit_handle;
} nt_security_attributes, nt_sa;


typedef struct _nt_guid {
	uint32_t	data1;
	uint16_t	data2;
	uint16_t	data3;
	unsigned char	data4[8];
} nt_guid, nt_uuid;


typedef struct _nt_uuid_vector {
	uint32_t	count;
	nt_uuid *	uuid[];
} nt_uuid_vector;


typedef struct _nt_acl {
	unsigned char	acl_revision;
	unsigned char	sbz_1st;
	uint16_t	acl_size;
	uint16_t	ace_count;
	uint16_t	sbz_2nd;
} nt_acl;


typedef struct _nt_security_descriptor {
	unsigned char	revision;
	unsigned char	sbz_1st;
	uint16_t	control;
	uint32_t	offset_owner;
	uint32_t	offset_group;
	uint32_t	offset_sacl;
	uint32_t	offset_dacl;
} nt_security_descriptor, nt_sd;


typedef struct _nt_security_quality_of_service {
  uint32_t	length;
  int32_t	impersonation_level;
  int32_t 	context_tracking_mode;
  int32_t	effective_only;
} nt_security_quality_of_service, nt_sqos;


typedef struct _nt_sid_identifier_authority {
	unsigned char	value[6];
} nt_sid_identifier_authority;


typedef struct _nt_sid {
	unsigned char			revision;
	unsigned char			sub_authority_count;
	nt_sid_identifier_authority	identifier_authority;
	uint32_t			sub_authority[1];
} nt_sid;


typedef struct _nt_sid_and_attributes {
	nt_sid *	sid;
	uint32_t	attributes;
} nt_sid_and_attributes;


typedef struct _nt_token_user {
	nt_sid_and_attributes	user;
} nt_token_user;


typedef struct _nt_token_owner {
	nt_sid *	owner;
} nt_token_owner;


typedef struct _nt_token_primary_group {
	nt_sid *	primary_group;
} nt_token_primary_group;


typedef struct _nt_token_groups {
	uint32_t		group_count;
	nt_sid_and_attributes	groups[];
} nt_token_groups;


typedef struct _nt_token_default_dacl {
	nt_acl *	default_dacl;
} nt_token_default_dacl;


typedef struct _nt_luid {
	uint32_t	low;
	int32_t		high;
} nt_luid;


typedef struct _nt_token_origin {
	nt_luid		originating_logon_session;
} nt_token_origin;


typedef struct _nt_token_source {
	char		source_name[NT_TOKEN_SOURCE_LENGTH];
	nt_luid		source_identifier;
} nt_token_source;


typedef struct _nt_luid_and_attributes {
	nt_luid		luid;
	uint32_t	attributes;
} nt_luid_and_attributes;


typedef struct _nt_token_privileges {
	uint32_t		privilege_count;
	nt_luid_and_attributes	privileges[];
} nt_token_privileges;


typedef struct _nt_object_attributes {
	uint32_t		len;
	void *			root_dir;
	nt_unicode_string *	obj_name;
	uint32_t		obj_attr;
	nt_security_descriptor *sec_desc;
	nt_sqos *		sec_qos;
} nt_object_attributes, nt_oa;


typedef struct _nt_object_basic_information {
	uint32_t		attributes;
	uint32_t		granted_access;
	uint32_t		handle_count;
	uint32_t		pointer_count;
	uint32_t		paged_pool_usage;
	uint32_t		non_paged_pool_usage;
	uint32_t		reserved[3];
	uint32_t		name_information_length;
	uint32_t		type_information_length;
	uint32_t		security_descriptor_length;
	nt_large_integer	create_time;
} nt_object_basic_information;


typedef struct _nt_object_name_information {
	nt_unicode_string	name;
} nt_object_name_information;



typedef struct _nt_object_handle_information {
	unsigned char	inherit;
	unsigned char	protect_from_close;
} nt_object_handle_information, nt_ohio;


typedef struct _nt_directory_basic_information {
	nt_unicode_string	object_name;
	nt_unicode_string	object_type_name;
} nt_directory_basic_information;


typedef struct _nt_keyed_object_directory_guid {
	wchar16_t		uscore_guid;
	wchar16_t		pgrp_guid[36];
	wchar16_t		uscore_key;
} nt_keyed_object_directory_guid, nt_keyed_objdir_guid;

typedef struct _nt_keyed_object_directory_name {
	wchar16_t		base_named_objects[17];
	wchar16_t		backslash;
	wchar16_t		prefix[6];
	nt_keyed_objdir_guid	objdir_guid;
	wchar16_t		key[8];
} nt_keyed_object_directory_name, nt_keyed_objdir_name;


typedef void nt_io_apc_routine(
	void *			apc_context,
	nt_io_status_block *	io_status_block,
	uint32_t		reserved);


typedef int32_t __stdcall ntapi_zw_query_object(
	__in	void *			handle,
	__in	nt_object_info_class	obj_info_class,
	__out	void *			obj_info,
	__in	size_t			obj_info_length,
	__out	uint32_t *		returned_length		__optional);


typedef int32_t __stdcall ntapi_zw_set_information_object(
	__in	void *			handle,
	__in	nt_object_info_class	obj_info_class,
	__in	void *			obj_info,
	__in	size_t			obj_info_length);


typedef int32_t __stdcall ntapi_zw_duplicate_object(
	__in	void *		hprocess_src,
	__in	void *		handle_src,
	__in	void *		hprocess_dst,
	__out	void **		handle_dst	__optional,
	__in	uint32_t	desired_access,
	__in	uint32_t	attributes,
	__in	uint32_t	options);


typedef int32_t __stdcall ntapi_zw_make_temporary_object(
	__in	void *	handle);


typedef int32_t __stdcall ntapi_zw_close(
	__in	void *	handle);



typedef int32_t __stdcall ntapi_zw_query_security_object(
	__in	void *				handle,
	__in	nt_security_information		security_info,
	__out	nt_security_descriptor *	security_descriptor,
	__in	size_t				security_descriptor_length,
	__out	size_t *			returned_length);


typedef int32_t __stdcall ntapi_zw_set_security_object(
	__in	void *				handle,
	__in	nt_security_information		security_info,
	__out	nt_security_descriptor *	security_descriptor);



typedef int32_t __stdcall ntapi_zw_create_directory_object(
	__out	void **			directory_handle,
	__in	uint32_t		desired_access,
	__in	nt_object_attributes *	obj_attr);


typedef int32_t __stdcall ntapi_zw_open_directory_object(
	__out	void **			directory_handle,
	__in	uint32_t		desired_access,
	__in	nt_object_attributes *	obj_attr);


typedef int32_t __stdcall ntapi_zw_query_directory_object(
	__in		void *		directory_handle,
	__out		void *		buffer,
	__in		size_t		buffer_length,
	__in		int32_t		return_single_entry,
	__in		int32_t		return_scan,
	__in_out	uint32_t *	context,
	__out		size_t *	returned_length);


typedef int32_t __stdcall ntapi_zw_create_symbolic_link_object(
	__out	void **			symbolic_link_handle,
	__in	uint32_t		desired_access,
	__in	nt_object_attributes *	obj_attr,
	__in	nt_unicode_string *	target_name);


typedef int32_t __stdcall ntapi_zw_open_symbolic_link_object(
	__out	void **			symbolic_link_handle,
	__in	uint32_t		desired_access,
	__in	nt_object_attributes *	obj_attr);


typedef int32_t __stdcall ntapi_zw_query_symbolic_link_object(
	__in		void *			symbolic_link_handle,
	__in_out	nt_unicode_string *	target_name,
	__out		size_t *		returned_length);

/* extension functions */
typedef int32_t __stdcall ntapi_tt_create_keyed_object_directory(
	__out	void **			hdir,
	__in	uint32_t		desired_access,
	__in	const wchar16_t		prefix[6],
	__in	nt_guid *		guid,
	__in	uint32_t		key);

typedef int32_t __stdcall ntapi_tt_open_keyed_object_directory(
	__out	void **			hdir,
	__in	uint32_t		desired_access,
	__in	const wchar16_t		prefix[6],
	__in	nt_guid *		guid,
	__in	uint32_t		key);

typedef int32_t __stdcall ntapi_tt_create_keyed_object_directory_entry(
	__out	void **			hentry,
	__in	uint32_t		desired_access,
	__in	void *			hdir,
	__in	void *			htarget,
	__in	nt_unicode_string *	target_name,
	__in	uint32_t		key);

#endif
