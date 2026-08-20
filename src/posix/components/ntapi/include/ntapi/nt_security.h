#ifndef _NT_SECURITY_H_
#define _NT_SECURITY_H_

#include <psxtypes/psxtypes.h>
#include "nt_object.h"

typedef enum _nt_audit_event_type {
	NT_AUDIT_EVENT_OBJECT_ACCESS,
	NT_AUDIT_EVENT_DIRECTORY_SERVICE_ACCESS
} nt_audit_event_type;


/* audit flag bits */
#define NT_AUDIT_ALLOW_NO_PRIVILEGE 	0x01


typedef struct _nt_privilege_set {
	uint32_t	privilege_count;
	uint32_t	control;
	nt_luid_and_attributes	privilege[];
} nt_privilege_set;


typedef struct _nt_object_type_list {
	int32_t		level;
	int32_t		sbz;
	nt_guid *	object_type;
} nt_object_type_list;


typedef int32_t __stdcall ntapi_zw_privilege_check(
	__in	void *			htoken,
	__in	nt_privilege_set *	required_privileges,
	__out	unsigned char *		result);


typedef int32_t __stdcall ntapi_zw_privilege_object_audit_alarm(
	__in	nt_unicode_string *	subsystem_name,
	__in	void *			handle_id,
	__in	void *			htoken,
	__in	uint32_t		desired_access,
	__in	nt_privilege_set *	privileges,
	__in	unsigned char		access_granted);


typedef int32_t __stdcall ntapi_zw_privileged_service_audit_alarm(
	__in	nt_unicode_string *	subsystem_name,
	__in	nt_unicode_string *	service_name,
	__in	void *			htoken,
	__in	nt_privilege_set *	privileges,
	__in	unsigned char		access_granted);


typedef int32_t __stdcall ntapi_zw_access_check(
	__in	nt_security_descriptor *	sec_desc,
	__in	void *				htoken,
	__in	uint32_t			desired_access,
	__in	nt_generic_mapping *		generic_mapping,
	__in	nt_privilege_set *		privilege_set,
	__in	uint32_t *			privilege_set_length,
	__out	uint32_t *			granted_access,
	__out	unsigned char *			access_status);


typedef int32_t __stdcall ntapi_zw_access_check_and_audit_alarm(
	__in	nt_unicode_string *		subsystem_name,
	__in	void *				handle_id,
	__in	nt_unicode_string *		object_type_name,
	__in	nt_unicode_string *		object_name,
	__in	nt_security_descriptor *	sec_desc,
	__in	uint32_t			desired_access,
	__in	nt_generic_mapping *		generic_mapping,
	__in	unsigned char			object_creation,
	__out	uint32_t *			granted_access,
	__out	unsigned char *			access_status,
	__out	unsigned char *			generate_on_close);


typedef int32_t __stdcall ntapi_zw_access_check_by_type(
	__in	nt_security_descriptor *	sec_desc,
	__in	nt_sid *			principal_self_sid,
	__in	void *				htoken,
	__in	uint32_t			desired_access,
	__in	nt_object_type_list *		obj_type_list,
	__in	uint32_t			obj_type_list_length,
	__in	nt_generic_mapping *		generic_mapping,
	__in	nt_privilege_set *		privilege_set,
	__in	uint32_t *			privilege_set_length,
	__out	uint32_t *			granted_access,
	__out	unsigned char *			access_status);


typedef int32_t __stdcall ntapi_zw_access_check_by_type_and_audit_alarm(
	__in	nt_unicode_string *		subsystem_name,
	__in	void *				handle_id,
	__in	nt_unicode_string *		object_type_name,
	__in	nt_unicode_string *		object_name,
	__in	nt_security_descriptor *	sec_desc,
	__in	nt_sid *			principal_self_sid,
	__in	uint32_t			desired_access,
	__in	nt_audit_event_type		audit_type,
	__in	uint32_t			augid_flags,
	__in	nt_object_type_list *		obj_type_list,
	__in	uint32_t			obj_type_list_length,
	__in	nt_generic_mapping *		generic_mapping,
	__in	unsigned char			object_creation,
	__out	uint32_t *			granted_access,
	__out	uint32_t *			access_status,
	__out	unsigned char *			generate_on_close);


typedef int32_t __stdcall ntapi_zw_access_check_by_type_result_list(
	__in	nt_security_descriptor *	sec_desc,
	__in	nt_sid *			principal_self_sid,
	__in	void *				htoken,
	__in	uint32_t			desired_access,
	__in	nt_object_type_list *		obj_type_list,
	__in	uint32_t			obj_type_list_length,
	__in	nt_generic_mapping *		generic_mapping,
	__in	nt_privilege_set *		privilege_set,
	__in	uint32_t *			privilege_set_length,
	__out	uint32_t *			granted_access_list,
	__out	uint32_t *			access_status_list);


typedef int32_t __stdcall ntapi_zw_access_check_by_type_result_list_and_audit_alarm(
	__in	nt_unicode_string *		subsystem_name,
	__in	void *				handle_id,
	__in	nt_unicode_string *		object_type_name,
	__in	nt_unicode_string *		object_name,
	__in	nt_security_descriptor *	sec_desc,
	__in	nt_sid *			principal_self_sid,
	__in	uint32_t			desired_access,
	__in	nt_audit_event_type		audit_type,
	__in	uint32_t			augid_flags,
	__in	nt_object_type_list *		obj_type_list,
	__in	uint32_t			obj_type_list_length,
	__in	nt_generic_mapping *		generic_mapping,
	__in	unsigned char			object_creation,
	__out	uint32_t *			granted_access_list,
	__out	uint32_t *			access_status_list,
	__out	uint32_t *			generate_on_close);


typedef int32_t __stdcall ntapi_zw_access_check_by_type_result_list_and_audit_alarm_by_handle(
	__in	nt_unicode_string *		subsystem_name,
	__in	void *				handle_id,
	__in	void *				htoken,
	__in	nt_unicode_string *		object_type_name,
	__in	nt_unicode_string *		object_name,
	__in	nt_security_descriptor *	sec_desc,
	__in	nt_sid *			principal_self_sid,
	__in	uint32_t			desired_access,
	__in	nt_audit_event_type		audit_type,
	__in	uint32_t			augid_flags,
	__in	nt_object_type_list *		obj_type_list,
	__in	uint32_t			obj_type_list_length,
	__in	nt_generic_mapping *		generic_mapping,
	__in	unsigned char			object_creation,
	__out	uint32_t *			granted_access_list,
	__out	uint32_t *			access_status_list,
	__out	uint32_t *			generate_on_close);


typedef int32_t __stdcall ntapi_zw_open_object_audit_alarm(
	__in	nt_unicode_string *		subsystem_name,
	__in	void **				handle_id,
	__in	nt_unicode_string *		object_type_name,
	__in	nt_unicode_string *		object_name,
	__in	nt_security_descriptor *	sec_desc,
	__in	void *				htoken,
	__in	uint32_t			desired_access,
	__in	uint32_t			granted_access,
	__in	nt_privilege_set *		privileges	__optional,
	__in	unsigned char			object_creation,
	__in	unsigned char			access_granted,
	__out	unsigned char *			generate_on_close);

typedef int32_t __stdcall ntapi_zw_close_object_audit_alarm(
	__in	nt_unicode_string *		subsystem_name,
	__in	void *				handle_id,
	__out	unsigned char *			generate_on_close);


typedef int32_t __stdcall ntapi_zw_delete_object_audit_alarm(
	__in	nt_unicode_string *		subsystem_name,
	__in	void *				handle_id,
	__out	unsigned char *			generate_on_close);

#endif
