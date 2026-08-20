#ifndef _NT_ACL_H_
#define _NT_ACL_H_

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_object.h>

typedef enum _nt_sid_name_use {
	NT_SID_TYPE_USER = 1,
	NT_SID_TYPE_GROUP,
	NT_SID_TYPE_DOMAIN,
	NT_SID_TYPE_ALIAS,
	NT_SID_TYPE_WELL_KNOWN_GROUP,
	NT_SID_TYPE_DELETED_ACCOUNT,
	NT_SID_TYPE_INVALID,
	NT_SID_TYPE_UNKNOWN,
	NT_SID_TYPE_COMPUTER,
	NT_SID_TYPE_LABEL
} nt_sid_name_use;


/* access control entry types */
#define NT_ACE_TYPE_ACCESS_ALLOWED			(0x00)
#define NT_ACE_TYPE_ACCESS_DENIED			(0x01)
#define NT_ACE_TYPE_SYSTEM_AUDIT			(0x02)
#define NT_ACE_TYPE_SYSTEM_ALARM			(0x03)
#define NT_ACE_TYPE_ACCESS_ALLOWED_COMPOUND		(0x04)
#define NT_ACE_TYPE_ACCESS_ALLOWED_OBJECT		(0x05)
#define NT_ACE_TYPE_ACCESS_DENIED_OBJECT		(0x06)
#define NT_ACE_TYPE_SYSTEM_AUDIT_OBJECT			(0x07)
#define NT_ACE_TYPE_SYSTEM_ALARM_OBJECT			(0x08)
#define NT_ACE_TYPE_ACCESS_ALLOWED_CALLBACK		(0x09)
#define NT_ACE_TYPE_ACCESS_DENIED_CALLBACK		(0x0A)
#define NT_ACE_TYPE_ACCESS_ALLOWED_CALLBACK_OBJECT	(0x0B)
#define NT_ACE_TYPE_ACCESS_DENIED_CALLBACK_OBJECT	(0x0C)
#define NT_ACE_TYPE_SYSTEM_AUDIT_CALLBACK		(0x0D)
#define NT_ACE_TYPE_SYSTEM_ALARM_CALLBACK		(0x0E)
#define NT_ACE_TYPE_SYSTEM_AUDIT_CALLBACK_OBJECT	(0x0F)
#define NT_ACE_TYPE_SYSTEM_ALARM_CALLBACK_OBJECT	(0x10)
#define NT_ACE_TYPE_SYSTEM_MANDATORY_LABEL		(0x11)
#define NT_ACE_TYPE_SYSTEM_RESOURCE_ATTRIBUTE		(0x12)
#define NT_ACE_TYPE_SYSTEM_SCOPED_POLICY_ID		(0x13)


/* acceess control entry flags */
#define NT_ACE_OBJECT_INHERIT                		(0x01)
#define NT_ACE_CONTAINER_INHERIT             		(0x02)
#define NT_ACE_NO_PROPAGATE_INHERIT          		(0x04)
#define NT_ACE_INHERIT_ONLY                  		(0x08)
#define NT_ACE_INHERITED                     		(0x10)
#define NT_ACE_VALID_INHERIT_FLAGS			(0x1F)
#define NT_ACE_SUCCESSFUL_ACCESS_ACE_FLAG		(0x40)
#define NT_ACE_FAILED_ACCESS_ACE_FLAG			(0x80)

typedef struct _nt_ace_header {
	unsigned char	ace_type;
	unsigned char	ace_flags;
	uint16_t	ace_size;
} nt_ace_header;


typedef struct _nt_access_allowed_ace {
	nt_ace_header	header;
	uint32_t	mask;
	uint32_t	sid_start;
} nt_access_allowed_ace;


typedef struct _nt_access_denied_ace {
	nt_ace_header	header;
	uint32_t	mask;
	uint32_t	sid_start;
} nt_access_denied_ace;


typedef struct _nt_system_audit_ace {
	nt_ace_header	header;
	uint32_t	mask;
	uint32_t	sid_start;
} nt_system_audit_ace;


typedef struct _nt_system_alarm_ace {
	nt_ace_header	header;
	uint32_t	mask;
	uint32_t	sid_start;
} nt_system_alarm_ace;


typedef struct _nt_system_resource_attribute_ace {
	nt_ace_header	header;
	uint32_t	mask;
	uint32_t	sid_start;
} nt_system_resource_attribute_ace;


typedef struct _nt_system_scoped_policy_id_ace {
	nt_ace_header	header;
	uint32_t	mask;
	uint32_t	sid_start;
} nt_system_scoped_policy_id_ace;


typedef struct _nt_system_mandatory_label_ace {
	nt_ace_header	header;
	uint32_t	mask;
	uint32_t	sid_start;
} nt_system_mandatory_label_ace;

#endif
