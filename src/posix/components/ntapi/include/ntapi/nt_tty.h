#ifndef _NT_TTY_H_
#define _NT_TTY_H_

/**
 *  tty api:
 *  -----------
 *  this header describes the tty interfaces that are used
 *  by native applications and libraries to communicate with
 *  ntctty, the project's native subsystem and pty server.
**/

#include <psxtypes/psxtypes.h>
#include "nt_object.h"
#include "nt_port.h"
#include "nt_termios.h"

typedef enum _nt_tty_opcode {
	NT_TTY_CLIENT_OPCODE_BASE	= 0x40000,
	/* primary connection */
	NT_TTY_CLIENT_SESSION_CONNECT	= NT_TTY_CLIENT_OPCODE_BASE,
	NT_TTY_CLIENT_SESSION_DISCONNECT,
	NT_TTY_CLIENT_SESSION_QUERY,
	NT_TTY_CLIENT_SESSION_SET,
	/* process registration */
	NT_TTY_CLIENT_PROCESS_REGISTER,
	NT_TTY_CLIENT_PROCESS_UNREGISTER,
	/* session information */
	NT_TTY_QUERY_INFORMATION_SERVER,
	NT_TTY_QUERY_INFORMATION_SESSION,
	NT_TTY_QUERY_INFORMATION_PROCESS,
	NT_TTY_QUERY_INFORMATION_THREAD,
	NT_TTY_QUERY_INFORMATION_SECTION,
	NT_TTY_QUERY_INFORMATION_PTY,
	/* peer daemon calls */
	NT_TTY_REQUEST_PEER,
	NT_TTY_SIGNAL_PEER,
	/* pty */
	NT_TTY_PTY_OPEN,
	NT_TTY_PTY_CLOSE,
	NT_TTY_PTY_READ,
	NT_TTY_PTY_WRITE,
	NT_TTY_PTY_QUERY,
	NT_TTY_PTY_SET,
	NT_TTY_PTY_FCNTL,
	NT_TTY_PTY_IOCTL,
	NT_TTY_PTY_CANCEL,
	NT_TTY_PTY_PEEK,
	/* virtual mount system */
	NT_TTY_VMS_QUERY,
	NT_TTY_VMS_REQUEST,
	/* exclusive upper limit */
	NT_TTY_CLIENT_OPCODE_CAP
} nt_tty_opcode;


typedef enum _nt_tty_session_type {
	NT_TTY_SESSION_PRIMARY,
	NT_TTY_SESSION_PRIVATE
} nt_tty_session_type;


typedef enum _nt_tty_info_class {
	NT_TTY_SESSION_INFORMATION,
	NT_TTY_INFORMATION_CAP
} nt_tty_info_class;


typedef enum _nt_pty_info_class {
	NT_PTY_BASIC_INFORMATION,
	NT_PTY_CLIENT_INFORMATION,
	NT_PTY_INFORMATION_CAP
} nt_pty_info_class;


typedef struct __attr_ptr_size_aligned__ _nt_tty_msg_info {
	uintptr_t	msg_id;
	uint32_t	opcode;
	int32_t		status;
	void *		reserved;
} nt_tty_msg_info;


typedef struct __attr_ptr_size_aligned__ _nt_tty_register_info {
	uintptr_t	process_id;
	uintptr_t	thread_id;
	uintptr_t	flags;
	uintptr_t	reserved;
} nt_tty_register_info;


typedef struct __attr_ptr_size_aligned__ _nt_tty_server_info {
	nt_port_attr	attr;
	intptr_t	pid;
	intptr_t	tid;
} nt_tty_server_info;


typedef struct __attr_ptr_size_aligned__ _nt_tty_vms_info {
	void *		hroot;
	uint32_t	hash;
	uint32_t	flags;
	int64_t		key;
	nt_guid		vms_guid;
	nt_port_keys	vms_keys;
} nt_tty_vms_info;


typedef struct __attr_ptr_size_aligned__ _nt_tty_peer_info {
	uint32_t	opcode;
	uint32_t	flags;
	nt_guid		service;
	nt_port_attr	peer;
} nt_tty_peer_info;


typedef struct __attr_ptr_size_aligned__ _nt_tty_sigctl_info {
	void *			hpty;
	nt_guid			guid;
	nt_luid			luid;
	uint32_t		ctlcode;
	int32_t			cccode;
	nt_client_id		cid;
	uintptr_t		ctxarg[4];
	struct tty_termios	terminfo;
	struct tty_winsize	winsize;
	nt_iosb			iosb;
} nt_tty_sigctl_info;


typedef struct __attr_ptr_size_aligned__ _nt_pty_fd_info {
	void *		hpty;
	void *		section;
	void *		section_addr;
	size_t		section_size;
	nt_guid		guid;
	nt_luid		luid;
	uint32_t	access;
	uint32_t	flags;
	uint32_t	share;
	uint32_t	options;
	intptr_t	state;
	void *		hevent[2];
} nt_pty_fd_info;


typedef struct __attr_ptr_size_aligned__ _nt_pty_io_info {
	void *		hpty;
	void *		hevent;
	void *		reserved;
	void *		apc_routine;
	void *		apc_context;
	nt_iosb *	riosb;
	void *		raddr;
	nt_iosb		iosb;
	off_t		offset;
	size_t		nbytes;
	uint32_t	key;
	uint32_t	npending;
	nt_guid		guid;
	nt_luid		luid;
} nt_pty_io_info;


typedef struct __attr_ptr_size_aligned__ _nt_pty_client_info {
	uintptr_t	any[4];
} nt_pty_client_info;


typedef struct __attr_ptr_size_aligned__ _nt_tty_session_info {
	int32_t		pid;
	int32_t		pgid;
	int32_t		sid;
	int32_t		reserved;
} nt_tty_session_info;


typedef struct __attr_ptr_size_aligned__ _nt_tty_register_msg {
	nt_port_message			header;
	struct {
		nt_tty_msg_info		ttyinfo;
		nt_tty_register_info	reginfo;
	} data;
} nt_tty_register_msg;


typedef struct __attr_ptr_size_aligned__ _nt_tty_server_msg {
	nt_port_message			header;
	struct {
		nt_tty_msg_info		ttyinfo;
		nt_tty_server_info	srvinfo;
	} data;
} nt_tty_server_msg;


typedef struct __attr_ptr_size_aligned__ _nt_tty_vms_msg {
	nt_port_message			header;
	struct {
		nt_tty_msg_info		ttyinfo;
		nt_tty_vms_info		vmsinfo;
	} data;
} nt_tty_vms_msg;


typedef struct __attr_ptr_size_aligned__ _nt_tty_peer_msg {
	nt_port_message			header;
	struct {
		nt_tty_msg_info		ttyinfo;
		nt_tty_peer_info	peerinfo;
	} data;
} nt_tty_peer_msg;


typedef struct __attr_ptr_size_aligned__ _nt_pty_fd_msg {
	nt_port_message			header;
	struct {
		nt_tty_msg_info		ttyinfo;
		nt_pty_fd_info		fdinfo;
	} data;
} nt_pty_fd_msg;


typedef struct __attr_ptr_size_aligned__ _nt_pty_io_msg {
	nt_port_message			header;
	struct {
		nt_tty_msg_info		ttyinfo;
		nt_pty_io_info		ioinfo;
	} data;
} nt_pty_io_msg;


typedef struct __attr_ptr_size_aligned__ _nt_pty_sigctl_msg {
	nt_port_message			header;
	struct {
		nt_tty_msg_info		ttyinfo;
		nt_tty_sigctl_info	ctlinfo;
	} data;
} nt_pty_sigctl_msg;


typedef struct __attr_ptr_size_aligned__ _nt_tty_session_msg {
	nt_port_message			header;
	struct {
		nt_tty_msg_info		ttyinfo;
		nt_tty_session_info	sessioninfo;
	} data;
} nt_tty_session_msg;


typedef struct __attr_ptr_size_aligned__ _nt_tty_port_msg {
	nt_port_message			header;
	nt_tty_msg_info			ttyinfo;
	union {
		nt_tty_register_info	reginfo;
		nt_tty_vms_info		vmsinfo;
		nt_tty_peer_info	peerinfo;
		nt_tty_sigctl_info	ctlinfo;
		nt_pty_fd_info		fdinfo;
		nt_pty_io_info		ioinfo;
		nt_pty_client_info	clientinfo;
		nt_tty_session_info	sessioninfo;
	};
} nt_tty_port_msg;


__assert_aligned_size(nt_tty_msg_info,		__SIZEOF_POINTER__);
__assert_aligned_size(nt_tty_register_info,	__SIZEOF_POINTER__);
__assert_aligned_size(nt_tty_vms_info,		__SIZEOF_POINTER__);
__assert_aligned_size(nt_tty_peer_info,		__SIZEOF_POINTER__);
__assert_aligned_size(nt_tty_sigctl_info,	__SIZEOF_POINTER__);
__assert_aligned_size(nt_pty_fd_info,		__SIZEOF_POINTER__);
__assert_aligned_size(nt_pty_io_info,		__SIZEOF_POINTER__);
__assert_aligned_size(nt_tty_register_msg,	__SIZEOF_POINTER__);
__assert_aligned_size(nt_tty_vms_msg,		__SIZEOF_POINTER__);
__assert_aligned_size(nt_tty_peer_msg,		__SIZEOF_POINTER__);
__assert_aligned_size(nt_pty_sigctl_msg,	__SIZEOF_POINTER__);
__assert_aligned_size(nt_pty_fd_msg,		__SIZEOF_POINTER__);
__assert_aligned_size(nt_pty_io_msg,		__SIZEOF_POINTER__);
__assert_aligned_size(nt_tty_port_msg,		__SIZEOF_POINTER__);


/* tty session */
typedef int32_t __stdcall	ntapi_tty_create_session(
	__out	void **			hport,
	__out	nt_port_name *		port_name,
	__in	nt_tty_session_type	type,
	__in	const nt_guid *		guid		__optional,
	__in	wchar16_t *		image_name	__optional);


typedef int32_t __stdcall	ntapi_tty_join_session(
	__out	void **			hport,
	__out	nt_port_name *		port_name,
	__in	nt_port_attr *		port_attr,
	__in	nt_tty_session_type	type);


typedef int32_t __stdcall	ntapi_tty_connect(
	__out	void **			hport,
	__in	wchar16_t *		tty_port_name,
	__in	int32_t			impersonation_level);


typedef int32_t __stdcall	ntapi_tty_client_session_query(
	__in	void *			hport,
	__out	nt_tty_session_info *	sessioninfo);


typedef int32_t __stdcall	ntapi_tty_client_session_set(
	__in	void *			hport,
	__in	nt_tty_session_info *	sessioninfo);


typedef int32_t __stdcall	ntapi_tty_client_process_register(
	__in	void *			hport,
	__in	uintptr_t		process_id,
	__in	uintptr_t		thread_id,
	__in	uintptr_t		flags,
	__in	nt_large_integer *	reserved);


typedef int32_t __stdcall ntapi_tty_query_information_server(
	__in	void *			hport,
	__out	nt_tty_server_info *	srvinfo);


/* pty api */
typedef struct nt_pty_context nt_pty;

typedef int32_t	__stdcall ntapi_pty_open(
	__in	void *			hport,
	__out	nt_pty **		pty,
	__in	uint32_t		desired_access,
	__in	nt_object_attributes*	obj_attr,
	__out	nt_iosb *		iosb,
	__in	uint32_t		share_access,
	__in	uint32_t		open_options);


typedef int32_t	__stdcall ntapi_pty_reopen(
	__in	void *			hport,
	__in	nt_pty *		pty);


typedef int32_t __stdcall ntapi_pty_close(
	__in	nt_pty *		pty);


typedef int32_t	__stdcall ntapi_pty_read(
	__in	nt_pty *		pty,
	__in	void *			hevent		__optional,
	__in	nt_io_apc_routine *	apc_routine	__optional,
	__in	void *			apc_context	__optional,
	__out	nt_iosb *		iosb,
	__out	void *			buffer,
	__in	uint32_t		nbytes,
	__in	nt_large_integer *	offset		__optional,
	__in	uint32_t *		key		__optional);


typedef int32_t __stdcall ntapi_pty_write(
	__in	nt_pty *		pty,
	__in 	void *			hevent		__optional,
	__in	nt_io_apc_routine *	apc_routine	__optional,
	__in	void * 			apc_context	__optional,
	__out	nt_iosb *		iosb,
	__in	void * 			buffer,
	__in	uint32_t		nbytes,
	__in	nt_large_integer *	offset		__optional,
	__in	uint32_t *		key		__optional);


typedef int32_t	__stdcall ntapi_pty_fcntl(
	__in	nt_pty *		pty,
	__in	void *			hevent			__optional,
	__in	nt_io_apc_routine *	apc_routine		__optional,
	__in	void *			apc_context		__optional,
	__out	nt_iosb *		iosb,
	__in	uint32_t		fs_control_code,
	__in	void *			input_buffer		__optional,
	__in	uint32_t		input_buffer_length,
	__out	void *			output_buffer		__optional,
	__in	uint32_t		output_buffer_length);


typedef int32_t	__stdcall ntapi_pty_ioctl(
	__in	nt_pty *		pty,
	__in	void *			hevent			__optional,
	__in	nt_io_apc_routine *	apc_routine		__optional,
	__in	void *			apc_context		__optional,
	__out	nt_iosb *		iosb,
	__in	uint32_t		io_control_code,
	__in	void *			input_buffer		__optional,
	__in	uint32_t		input_buffer_length,
	__out	void *			output_buffer		__optional,
	__in	uint32_t		output_buffer_length);


typedef int32_t	__stdcall ntapi_pty_query(
	__in	nt_pty *		pty,
	__out	nt_io_status_block *	iosb,
	__out	void *			pty_info,
	__in	uint32_t		pty_info_length,
	__in	nt_pty_info_class	pty_info_class);


typedef int32_t	__stdcall ntapi_pty_set(
	__in	nt_pty *		hfile,
	__out	nt_io_status_block *	iosb,
	__in	void *			pty_info,
	__in	uint32_t		pty_info_length,
	__in	nt_pty_info_class	pty_info_class);


typedef int32_t	__stdcall ntapi_pty_cancel(
	__in	nt_pty *		pty,
	__out	nt_iosb *		iosb);


/* peer daemon calls */
typedef int32_t __stdcall ntapi_tty_request_peer(
	__in	void *		hport,
	__in	int32_t		opcode,
	__in	uint32_t	flags,
	__in	const nt_guid *	service,
	__in	nt_port_attr *	peer);


/* virtual mount system */
typedef int32_t __stdcall	ntapi_tty_vms_query(
	__in	void *			hport,
	__in	nt_tty_vms_info *	vmsinfo);


typedef int32_t __stdcall	ntapi_tty_vms_request(
	__in	void *			hport,
	__in	nt_tty_vms_info *	vmsinfo);

#endif
