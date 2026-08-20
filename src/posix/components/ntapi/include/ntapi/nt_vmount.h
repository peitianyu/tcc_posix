#ifndef _NT_VMOUNT_H_
#define _NT_VMOUNT_H_

#include <psxtypes/psxtypes.h>
#include <dalist/dalist.h>
#include "nt_port.h"
#include "nt_file.h"
#include "nt_statfs.h"
#include "nt_tty.h"

/**
 *  vmount api:
 *  -----------
 *  this header provides an api that can be used to
 *  implement a virtual mount system.  note, however,
 *  that this library does not (and should not)
 *  provide the server-side implementation of the
 *  system, but only the client-side functionality.
 *  in the larger-scale midipix project, for
 *  instance, a session-wide virtual mount system
 *  is provided by the subsystem ntctty; if you
 *  are writing a free-standing midipix application,
 *  then you may either use the interfaces provided
 *  by ntctty, or roll your own.
**/


/** virtual mount system: concepts
 *  ------------------------------
 *  elements of the virtual mount system are exposed to the
 *  client in terms of offsets from the virtual mount system
 *  base address, rather than absolute pointers.  when using
 *  a shared memory section, this allows each client to map
 *  the virtual mount system at an address that is independent
 *  of all other clients. most importantly, clients are only
 *  granted read-only access to the section, and it is hence
 *  impossible for a malformed client to trash the contents
 *  of the virtual mount system.
 *
 *  to locate information regarding a particular mount point,
 *  the client traverses the virtual mount system using several
 *  numeric keys.
 *
 *  the server_key member of the virtual mount system structure,
 *  in combination with the node-specific server keys, allow clients
 *  to efficiently use a single ref_counter server call at the end of
 *  the path resolution process.
 *
 *  in the larger-scale midipix project, the above call fails only when
 *  a referenced mount point has in the meantime become invalid; in other
 *  words, the call shall succeed even if the queried mount point is
 *  no longer found at the top of the target directory stack.
**/


typedef intptr_t			nt_vms_offset;
typedef struct nt_vms_cache_interface *	nt_vms_cache;


typedef struct _nt_vms_flags {
	uint16_t	rel_depth;
	uint16_t	attr;
} nt_vms_flags;

typedef struct __attr_ptr_size_aligned__ _nt_vms_point {
	intptr_t	target;
	intptr_t	source;
	nt_vms_offset	parent;
	nt_vms_offset	prev;
	nt_vms_offset	next;
	int32_t		fstype;
	nt_vms_flags	flags;
	nt_luid		luid;
	intptr_t	ref_count;
	intptr_t	server_key;
	int32_t		stack_index;
	int32_t		status;
	nt_fii		fii;
	uint32_t	dev_name_hash;
	uint16_t	dev_name_strlen;
	uint16_t	dev_name_maxlen;
	nt_vms_offset	dev_name;
	uint32_t	end_component_hash;
	uint16_t	end_component_strlen;
	uint16_t	end_component_maxlen;
	nt_vms_offset	end_component;
	uint32_t	src_fstype_hash;
	uint32_t	src_attr;
	uint32_t	src_control_flags;
	wchar16_t	src_drive_letter;
	wchar16_t	src_padding;
	nt_guid		src_volume_guid;
	nt_fii		src_fii;
	uint32_t	src_dev_name_hash;
	uint32_t	reserved;
} nt_vms_point;


typedef struct _nt_vms_node {
	nt_vms_offset		prev;
	nt_vms_offset		next;
	uint32_t		end_component_hash;
	uint32_t		dev_name_hash;
	nt_large_integer	index_number;
	nt_vms_offset		stack;
} nt_vms_node;


typedef struct __attr_ptr_size_aligned__ _nt_vms_system {
	intptr_t		client_key;
	intptr_t		server_key;
	void *			hroot;
	intptr_t		vms_points_cap;
	nt_vms_offset		dev_name_head_node;		/* dev_name_hash, fii.index_number */
	nt_vms_offset		end_component_head_node;	/* end_component_hash, dev_name_hash, fii.index_number */
} nt_vms_system;


typedef enum _nt_vms_opcodes {
	NT_VMS_CLIENT_OPCODE_BASE	= 0x80000,
	/* virtual mount system daemon opcodes */
	NT_VMS_CLIENT_CONNECT		= NT_VMS_CLIENT_OPCODE_BASE,
	NT_VMS_CLIENT_DISCONNECT,
	NT_VMS_CLIENT_UNSHARE,
	NT_VMS_CLIENT_CONFIG,
	NT_VMS_POINT_ATTACH,
	NT_VMS_POINT_DETACH,
	NT_VMS_POINT_GET_HANDLES,
	NT_VMS_POINT_GET_VOLINFO,
	NT_VMS_REF_COUNT_INC,
	NT_VMS_REF_COUNT_DEC,
	NT_VMS_TABLE_QUERY,
	NT_VMS_TABLE_CLONE,
	/* client opcodes: exclusive upper limit */
	NT_VMS_CLIENT_OPCODE_CAP
} nt_vms_opcodes;

typedef struct __attr_ptr_size_aligned__ _nt_vms_msg_info {
	uintptr_t	msg_id;
	uint32_t	opcode;
	int32_t		status;
	uintptr_t	msg_key;
} nt_vms_msg_info;


typedef struct __attr_ptr_size_aligned__ _nt_vms_daemon_info {
	nt_guid		daemon_guid;
	uint32_t	srv_pid;
	uint32_t	srv_tid;
	uint32_t	session;
	uint32_t	instance;
	uint32_t	ver_maj;
	uint32_t	ver_min;
	uint32_t	section_size;
	uint32_t	advisory_size;
	uint32_t	points_mounted;
	uint32_t	points_available;
	void *		section_handle;
} nt_vms_daemon_info;


/* attach/detach */
typedef struct __attr_ptr_size_aligned__ _nt_vms_point_info {
	nt_vms_point	point;
	nt_fii		src_fii;
	uint32_t	src_dev_name_hash;
	uint32_t	src_flags;
} nt_vms_point_info;


/* inc/dec */
typedef struct __attr_ptr_size_aligned__ _nt_vms_ref_count_info {
	nt_vms_offset	ref_point;
	nt_luid		luid;
	intptr_t	server_key;
	intptr_t	ref_count;
	nt_fii		fii;
	uint32_t	dev_name_hash;
	int32_t		stack_index;
	void *		hsource;
	void *		htarget;
} nt_vms_ref_count_info;


/* query/clone */
typedef struct __attr_ptr_size_aligned__ _nt_vms_table_info {
	intptr_t	client_key;
	intptr_t	client_section_addr;
	intptr_t	client_section_size;
	intptr_t	reserved;
} nt_vms_table_info;


typedef struct __attr_ptr_size_aligned__ _nt_vms_daemon_msg {
	nt_port_message			header;
	struct {
		nt_vms_msg_info		msginfo;

		union {
			nt_vms_daemon_info	vmsinfo;
			nt_vms_point_info	pointinfo;
			nt_vms_ref_count_info	refcntinfo;
		};
	} data;
} nt_vms_daemon_msg;


typedef struct __attr_ptr_size_aligned__ _nt_vms_port_msg {
	nt_port_message			header;
	nt_vms_msg_info			msginfo;

	union {
		nt_vms_daemon_info	vmsinfo;
		nt_vms_point_info	pointinfo;
		nt_vms_ref_count_info	refcntinfo;
	};
} nt_vms_port_msg;


/* vms helper functions */
typedef nt_vms_node * __stdcall		ntapi_vms_get_end_component_first_node(
	__in	nt_vms_system *		pvms_sys,
	__in	uint32_t		end_component_hash);


typedef nt_vms_node * __stdcall		ntapi_vms_get_node_by_dev_name(
	__in	nt_vms_system *		pvms_sys,
	__in	uint32_t		dev_name_hash,
	__in	nt_large_integer	index_number);


typedef nt_vms_node * __stdcall		ntapi_vms_get_node_by_end_component(
	__in	nt_vms_system *		pvms_sys,
	__in	uint32_t		end_component_hash,
	__in	uint32_t		dev_name_hash,
	__in	nt_large_integer	index_number);


typedef nt_vms_point * __stdcall	ntapi_vms_get_top_of_stack_mount_point(
	__in	nt_vms_system *		pvms_sys,
	__in	nt_vms_node *		node);


/* vms optional cache functions */
typedef nt_vms_cache __stdcall	ntapi_vms_cache_alloc(
	__in	nt_vms_system *		vms_sys,
	__in	uint32_t		flags	__reserved,
	__in	void *			options	__reserved,
	__out	int32_t *		status	__optional);


typedef int32_t __stdcall	ntapi_vms_cache_free(
	__in	nt_vms_cache		cache);


typedef int32_t __stdcall	ntapi_vms_cache_record_append(
	__in	nt_vms_cache		cache,
	__in	void *			hfile,
	__in	uint32_t		dev_name_hash,
	__in	nt_large_integer	index_number,
	__in	intptr_t		client_key,
	__in	intptr_t		server_key);


typedef int32_t __stdcall	ntapi_vms_cache_record_remove(
	__in	nt_vms_cache		cache,
	__in	void *			hfile);


/* vms server calls */
typedef int32_t __stdcall	ntapi_vms_client_connect(
	__out	void **			hvms,
	__in	nt_tty_vms_info *	vmsinfo);


typedef int32_t __stdcall	ntapi_vms_client_disconnect(
	__in	void *			hvms);


typedef int32_t __stdcall	ntapi_vms_client_unshare(
	__in	void *			hvms,
	__out	nt_tty_vms_info *	vmsinfo);


typedef int32_t __stdcall	ntapi_vms_client_config(
	__in	void *			hvms);


typedef int32_t __stdcall	ntapi_vms_point_attach(
	__in	void *			hvms,
	__in	nt_vms_point_info *	point_info);


typedef int32_t __stdcall	ntapi_vms_point_detach(
	__in	void *			hvms,
	__in	nt_vms_ref_count_info *	ref_cnt_info);


typedef int32_t __stdcall	ntapi_vms_point_get_handles(
	__in	void *			hvms,
	__in	nt_vms_ref_count_info *	ref_cnt_info);


typedef int32_t __stdcall	ntapi_vms_point_get_volinfo(
	__in	void *			hvms,
	__in	nt_vms_point_info *	point_info);


typedef int32_t __stdcall	ntapi_vms_ref_count_inc(
	__in	void *			hvms,
	__in	nt_vms_ref_count_info *	ref_cnt_info);


typedef int32_t __stdcall	ntapi_vms_ref_count_dec(
	__in	void *			hvms,
	__in	nt_vms_ref_count_info *	ref_cnt_info);


typedef int32_t __stdcall	ntapi_vms_table_query(
	__in	void *			hvms,
	__in	nt_vms_daemon_info *	vms_info);


typedef int32_t __stdcall	ntapi_vms_table_clone(
	__in	void *			hvms,
	__in	nt_vms_daemon_info *	vms_info);


#endif
