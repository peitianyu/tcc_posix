#ifndef _NT_PORT_H_
#define _NT_PORT_H_

#include <psxtypes/psxtypes.h>
#include "nt_object.h"
#include "nt_process.h"

typedef enum _nt_lpc_type {
	NT_LPC_NEW_MESSAGE		= 0,
	NT_LPC_REQUEST			= 1,
	NT_LPC_REPLY			= 2,
	NT_LPC_DATAGRAM			= 3,
	NT_LPC_LOST_REPLY		= 4,
	NT_LPC_PORT_CLOSED		= 5,
	NT_LPC_CLIENT_DIED		= 6,
	NT_LPC_EXCEPTION		= 7,
	NT_LPC_DEBUG_EVENT		= 8,
	NT_LPC_ERROR_EVENT		= 9,
	NT_LPC_CONNECTION_REQUEST	= 10,
	NT_ALPC_REQUEST			= 0x2000 | NT_LPC_REQUEST,
	NT_ALPC_CONNECTION_REQUEST	= 0x2000 | NT_LPC_CONNECTION_REQUEST,
} nt_lpc_type;


typedef enum _nt_port_info_class {
	NT_PORT_BASIC_INFORMATION
} nt_port_info_class;


/* friendly port types */
typedef enum _nt_port_type {
	NT_PORT_TYPE_DEFAULT,	/* {'s','v','c','a','n','y'} */
	NT_PORT_TYPE_SUBSYSTEM,	/* {'n','t','c','t','t','y'} */
	NT_PORT_TYPE_VMOUNT,	/* {'v','m','o','u','n','t'} */
	NT_PORT_TYPE_DAEMON,	/* {'d','a','e','m','o','n'} */
	NT_PORT_TYPE_CAP
} nt_port_type;


typedef enum _nt_port_subtype {
	NT_PORT_SUBTYPE_DEFAULT,
	NT_PORT_SUBTYPE_PRIVATE,
	NT_PORT_SUBTYPE_CAP
} nt_port_subtype;


/* friendly port guids */
#define NT_PORT_GUID_DEFAULT	{0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}}
#define NT_PORT_GUID_SUBSYSTEM	{0xce7f8d40,0x81cd,0x41c6,{0xa4,0xb7,0xb8,0x35,0x67,0xdf,0x15,0xd9}}
#define NT_PORT_GUID_VMOUNT	{0x893d63d2,0x23e8,0x4caa,{0xa8,0x41,0x7f,0x6e,0x77,0x6b,0xd5,0x70}}
#define NT_PORT_GUID_DAEMON	{0xcf765d9e,0x6bd8,0x4a8d,{0x8a,0x21,0x17,0x34,0xcd,0x3a,0x8d,0xa7}}

/* lpc messages */
#define NT_LPC_REFUSE_CONNECTION	0x0000
#define NT_LPC_ACCEPT_CONNECTION	0x0001
#define NT_LPC_MAX_MSG_DATA_SIZE	0x0104


typedef struct _nt_port_basic_information {
	void *		dummy_invalid;
} nt_port_basic_information;


typedef struct _nt_port_message {
	uint16_t 	data_size;
	uint16_t 	msg_size;
	uint16_t 	msg_type;
	uint16_t 	virtual_ranges_offset;
	nt_client_id	client_id;
	uint32_t	msg_id;
	size_t		section_size;
} nt_port_message;


/* csrss port message structure: new process, first thread */
typedef struct _nt_port_message_csrss_process {
	nt_port_message		header;
	uintptr_t		unknown_1st;
	uint32_t		opcode;
	int32_t			status;
	uintptr_t		unknown_2nd;
	void *			hprocess;
	void *			hthread;
	uintptr_t		unique_process_id;
	uintptr_t		unique_thread_id;
	void *			reserved[8];
} nt_port_message_csrss_process;

/* csrss port message structure: existing process, new thread */
typedef struct _nt_port_message_csrss_thread {
	nt_port_message		header;
	uintptr_t		unknown_1st;
	uint32_t		opcode;
	int32_t			status;
	uintptr_t		unknown_2nd;
	void *			hthread;
	uintptr_t		unique_process_id;
	uintptr_t		unique_thread_id;
	void *			reserved[8];
} nt_port_message_csrss_thread;


typedef struct _nt_port_section_write {
	uint32_t	length;
	void * 		hsection;
	uint32_t	offset;
	size_t		view_size;
	void *		view_base;
	void *		target_vew_base;
} nt_port_section_write;


typedef struct _nt_port_section_read {
	uint32_t	length;
	size_t		view_size;
	void *		view_base;
} nt_port_section_read;


/* attributes of a friendly port */
typedef struct _nt_port_keys {
	uint32_t	reserved;
	uint32_t	key[6];
	uint32_t	padding;
} nt_port_keys;

typedef struct _nt_port_attr {
	nt_guid		guid;
	nt_port_type	type;
	nt_port_subtype	subtype;
	int32_t		ver_major;
	int32_t		ver_minor;
	uint32_t	options;
	uint32_t	flags;
	nt_port_keys	keys;
} nt_port_attr;


/* guid component of a friendly port name */
typedef struct _nt_port_guid {
	wchar16_t	uscore_guid;
	wchar16_t	port_guid[36];
	wchar16_t	uscore_keys;
} nt_port_guid;

/* keys component of a friendly port name */
typedef struct _nt_port_name_keys {
	wchar16_t	key_1st[8];
	wchar16_t	uscore_1st;
	wchar16_t	key_2nd[8];
	wchar16_t	uscore_2nd;
	wchar16_t	key_3rd[8];
	wchar16_t	uscore_3rd;
	wchar16_t	key_4th[8];
	wchar16_t	uscore_4th;
	wchar16_t	key_5th[8];
	wchar16_t	uscore_5th;
	wchar16_t	key_6th[8];
} nt_port_name_keys;


/* friendly port name */
typedef struct _nt_port_name {
	wchar16_t		base_named_objects[17];
	wchar16_t		backslash;
	wchar16_t		svc_prefix[6];
	nt_port_guid		port_guid;
	nt_port_name_keys	port_name_keys;
	wchar16_t 		null_termination;
} nt_port_name;


typedef int32_t __stdcall ntapi_zw_create_port(
	__out		void **			hport,
	__in 	 	nt_object_attributes *	obj_attr,
	__out	 	uint32_t		max_data_size,
	__out	 	uint32_t		max_msg_size,
	__in_out 	uint32_t		reserved);


typedef int32_t __stdcall ntapi_zw_create_waitable_port(
	__out		void **			hport,
	__in 	 	nt_object_attributes *	obj_attr,
	__out	 	uint32_t		max_data_size,
	__out	 	uint32_t		max_msg_size,
	__in_out 	uint32_t		reserved);


typedef int32_t __stdcall ntapi_zw_connect_port(
	__out 	 	void **					hport,
	__in 	 	nt_unicode_string *			port_name,
	__in 	 	nt_security_quality_of_service *	sec_qos,
	__in_out 	nt_port_section_write *			write_section	__optional,
	__in_out 	nt_port_section_read *			read_section	__optional,
	__out	 	uint32_t *				max_msg_size	__optional,
	__in_out 	void *					msg_data	__optional,
	__in_out 	uint32_t *				msg_data_length __optional);


typedef int32_t __stdcall ntapi_zw_secure_connect_port(
	__out 	 	void **					hport,
	__in 	 	nt_unicode_string *			port_name,
	__in 	 	nt_security_quality_of_service *	sec_qos,
	__in_out 	nt_port_section_write *			write_section	__optional,
	__in		nt_sid *				server_dis	__optional,
	__in_out 	nt_port_section_read *			read_section	__optional,
	__out	 	uint32_t *				max_msg_size	__optional,
	__in_out 	void *					msg_data	__optional,
	__in_out 	uint32_t *				msg_data_length __optional);


typedef int32_t __stdcall ntapi_zw_listen_port(
	__in 	 void *			hport,
	__in 	 nt_port_message *	port_message);


typedef int32_t __stdcall ntapi_zw_accept_connect_port(
	__out 	void **			hport,
	__in	intptr_t		port_id,
	__in 	nt_port_message *	port_message,
	__in	int32_t			response,
	__out	nt_port_section_write *	write_section	__optional,
	__out	nt_port_section_read *	read_section	__optional);


typedef int32_t __stdcall ntapi_zw_complete_connect_port(
	__in 	void *	hport);


typedef int32_t __stdcall ntapi_zw_request_port(
	__in 	 void *		hport,
	__in 	 void *		request_msg);


typedef int32_t __stdcall ntapi_zw_request_wait_reply_port(
	__in 	 void *		hport,
	__in 	 void *		request_msg,
	__out 	 void *		reply_msg);


typedef int32_t __stdcall ntapi_zw_reply_port(
	__in 	 void *			hport,
	__in 	nt_port_message *	reply_message);


typedef int32_t __stdcall ntapi_zw_reply_wait_reply_port(
	__in 	 	void *			hport,
	__in_out 	nt_port_message *	reply_message);


typedef int32_t __stdcall ntapi_zw_reply_wait_receive_port(
	__in	void *			hport,
	__out	intptr_t *		port_id		__optional,
	__in	nt_port_message *	reply_message	__optional,
	__out	nt_port_message *	receive_message);


typedef int32_t __stdcall ntapi_zw_reply_wait_receive_port_ex(
	__in	void *			hport,
	__out	intptr_t *		port_id		__optional,
	__in	nt_port_message *	reply_message	__optional,
	__out	nt_port_message *	receive_message,
	__in	nt_large_integer *	timeout);

typedef int32_t __stdcall ntapi_zw_read_request_data(
	__in	void *			hport,
	__in	nt_port_message *	message,
	__in	uint32_t		index,
	__out	void *			buffer,
	__in	size_t			buffer_length,
	__out	size_t *		returned_length	__optional);


typedef int32_t __stdcall ntapi_zw_write_request_data(
	__in	void *			hport,
	__in	nt_port_message *	message,
	__in	uint32_t		index,
	__in	void *			buffer,
	__in	size_t			buffer_length,
	__out	size_t *		returned_length	__optional);


typedef int32_t __stdcall ntapi_zw_query_information_port(
	__in	void *			hport,
	__in	nt_port_info_class	port_info_class,
	__out	void *			port_info,
	__in	size_t			port_info_length,
	__out	size_t *		returned_length	__optional);


typedef int32_t __stdcall ntapi_zw_impersonate_client_of_port(
	__in	void *			hport,
	__in	nt_port_message *	message);


typedef int32_t __stdcall ntapi_csr_client_call_server(
	__in	void *		msg_csrss,
	__in	void *		msg_unknown,
	__in	uint32_t	msg_opcode,
	__in	uint32_t	msg_size);


typedef void * __cdecl ntapi_csr_port_handle(int32_t * pstatus);


/* extensions */
typedef int32_t __stdcall ntapi_tt_port_guid_from_type(
	__out	nt_guid *		guid,
	__in	nt_port_type		type,
	__in	nt_port_subtype		subtype);


typedef int32_t __stdcall ntapi_tt_port_type_from_guid(
	__out	nt_port_type *		type,
	__out	nt_port_subtype *	subtype,
	__in	nt_guid *		guid);


typedef int32_t __stdcall	ntapi_tt_port_generate_keys(
	__out	nt_port_keys *		keys);


typedef void __stdcall	ntapi_tt_port_format_keys(
	__in	nt_port_keys *		keys,
	__out	nt_port_name_keys *	name_keys);


typedef void __stdcall	ntapi_tt_port_name_from_attributes(
	__out	nt_port_name *		name,
	__in	nt_port_attr *		attr);

#endif
