#ifndef _NT_TOKEN_H_
#define _NT_TOKEN_H_

#include <psxtypes/psxtypes.h>
#include "nt_object.h"

typedef enum _nt_token_type {
	NT_TOKEN_PRIMARY	= 1,
	NT_TOKEN_IMPERSONATION	= 2,
} nt_token_type;


typedef enum _nt_token_info_class {
	NT_TOKEN_USER 			= 1,
	NT_TOKEN_GROUPS			= 2,
	NT_TOKEN_PRIVILEGES		= 3,
	NT_TOKEN_OWNER			= 4,
	NT_TOKEN_PRIMARY_GROUP		= 5,
	NT_TOKEN_DEFAULT_DACL		= 6,
	NT_TOKEN_SOURCE			= 7,
	NT_TOKEN_TYPE			= 8,
	NT_TOKEN_IMPERSONATION_LEVEL	= 9,
	NT_TOKEN_STATISTICS		= 10,
	NT_TOKEN_RESTRICTED_SIDS	= 11,
	NT_TOKEN_SESSION_ID		= 12,
} nt_token_info_class;


/* token access bits */
#define NT_TOKEN_ASSIGN_PRIMARY		0x00000001U
#define NT_TOKEN_DUPLICATE		0x00000002U
#define NT_TOKEN_IMPERSONATE		0x00000004U
#define NT_TOKEN_QUERY			0x00000008U
#define NT_TOKEN_QUERY_SOURCE		0x00000010U
#define NT_TOKEN_ADJUST_PRIVILEGES	0x00000020U
#define NT_TOKEN_ADJUST_GROUPS		0x00000040U
#define NT_TOKEN_ADJUST_DEFAULT		0x00000080U
#define NT_TOKEN_ADJUST_SESSIONID	0x00000100U

#define NT_TOKEN_ALL_ACCESS	NT_SEC_STANDARD_RIGHTS_REQUIRED \
					| NT_TOKEN_ASSIGN_PRIMARY \
					| NT_TOKEN_DUPLICATE \
					| NT_TOKEN_IMPERSONATE \
					| NT_TOKEN_QUERY \
					| NT_TOKEN_QUERY_SOURCE \
					| NT_TOKEN_ADJUST_PRIVILEGES \
					| NT_TOKEN_ADJUST_GROUPS \
					| NT_TOKEN_ADJUST_SESSIONID \
					| NT_TOKEN_ADJUST_DEFAULT


#define NT_TOKEN_READ		NT_SEC_STANDARD_RIGHTS_READ \
					| NT_TOKEN_QUERY


#define NT_TOKEN_WRITE		NT_SEC_STANDARD_RIGHTS_WRITE \
					| TOKEN_ADJUST_PRIVILEGES \
					| NT_OKEN_ADJUST_GROUPS \
					| NT_TOKEN_ADJUST_DEFAULT

#define NT_TOKEN_EXECUTE	NT_SEC_STANDARD_RIGHTS_EXECUTE


/* filtered token flags */
#define NT_DISABLE_MAX_PRIVILEGE	0x01


typedef struct _nt_token_statistics {
	nt_luid					token_id;
	nt_luid					authentication_id;
	nt_large_integer			expiration_time;
	nt_token_type				token_type;
	nt_security_impersonation_level		impersonation_level;
	uint32_t				dynamic_charged;
	uint32_t				dynamic_available;
	uint32_t				group_count;
	uint32_t				privilege_count;
	nt_luid					modified_id;
} nt_token_statistics;


typedef int32_t __stdcall ntapi_zw_create_token(
	__out	void **				htoken,
	__in	uint32_t			desired_access,
	__in	nt_object_attributes *		obj_attr,
	__in	nt_token_type			type,
	__in	nt_luid *			authentication_id,
	__in	nt_large_integer *		expiration_time,
	__in	nt_token_user *			user,
	__in	nt_token_groups *		groups,
	__in	nt_token_privileges *		privileges,
	__in	nt_token_owner *		owner,
	__in	nt_token_primary_group *	primary_group,
	__in	nt_token_default_dacl *		default_dacl,
	__in	nt_token_source *		source);


typedef int32_t __stdcall ntapi_zw_open_process_token(
	__in	void *			hprocess,
	__in	uint32_t		desired_access,
	__out	void **			htoken);


typedef int32_t __stdcall ntapi_zw_open_thread_token(
	__in	void *			hthread,
	__in	uint32_t		desired_access,
	__in	int32_t			open_as_self,
	__out	void **			htoken);


typedef int32_t __stdcall ntapi_zw_duplicate_token(
	__in	void *				htoken_existing,
	__in	uint32_t			desired_access,
	__in	nt_object_attributes *		obj_attr,
	__in	int32_t				effective_only,
	__in	nt_token_type			token_type,
	__out	void **				htoken_new);


typedef int32_t __stdcall ntapi_zw_filter_token(
	__in	void *				htoken_existing,
	__in	uint32_t			flags,
	__in	nt_token_groups *		sids_to_disable,
	__in	nt_token_privileges *		privileges_to_delete,
	__in	nt_token_groups *		sids_to_restrict,
	__out	void **				htoken_new);


typedef int32_t __stdcall ntapi_zw_adjust_privileges_token(
	__in	void *				htoken,
	__in	int32_t				disable_all_privileges,
	__in	nt_token_privileges *		new_state,
	__in	size_t				buffer_length,
	__in	nt_token_privileges *		prev_state	__optional,
	__out	size_t *			returned_length);


typedef int32_t __stdcall ntapi_zw_adjust_groups_token(
	__in	void *				htoken,
	__in	int32_t				reset_to_default,
	__in	nt_token_groups *		new_state,
	__in	size_t				buffer_length,
	__in	nt_token_groups *		prev_state	__optional,
	__out	size_t *			returned_length);


typedef int32_t __stdcall ntapi_zw_query_information_token(
	__in	void *			htoken,
	__in	nt_token_info_class	token_info_class,
	__out	void *			token_info,
	__in	size_t			token_info_length,
	__out	size_t *		returned_length);


typedef int32_t __stdcall ntapi_zw_set_information_token(
	__in	void *			htoken,
	__in	nt_token_info_class	token_info_class,
	__in	void *			token_info,
	__in	size_t			token_info_length);

#endif
