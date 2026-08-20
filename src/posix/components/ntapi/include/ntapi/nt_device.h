#ifndef _NT_DEVICE_H_
#define _NT_DEVICE_H_

#include <psxtypes/psxtypes.h>
#include "nt_object.h"

typedef enum _nt_device_type {
	NT_FILE_DEVICE_8042_PORT           = 0x00000027,
	NT_FILE_DEVICE_ACPI                = 0x00000032,
	NT_FILE_DEVICE_BATTERY             = 0x00000029,
	NT_FILE_DEVICE_BEEP                = 0x00000001,
	NT_FILE_DEVICE_BUS_EXTENDER        = 0x0000002a,
	NT_FILE_DEVICE_CD_ROM              = 0x00000002,
	NT_FILE_DEVICE_CD_ROM_FILE_SYSTEM  = 0x00000003,
	NT_FILE_DEVICE_CHANGER             = 0x00000030,
	NT_FILE_DEVICE_CONTROLLER          = 0x00000004,
	NT_FILE_DEVICE_DATALINK            = 0x00000005,
	NT_FILE_DEVICE_DFS                 = 0x00000006,
	NT_FILE_DEVICE_DFS_FILE_SYSTEM     = 0x00000035,
	NT_FILE_DEVICE_DFS_VOLUME          = 0x00000036,
	NT_FILE_DEVICE_DISK                = 0x00000007,
	NT_FILE_DEVICE_DISK_FILE_SYSTEM    = 0x00000008,
	NT_FILE_DEVICE_DVD                 = 0x00000033,
	NT_FILE_DEVICE_FILE_SYSTEM         = 0x00000009,
	NT_FILE_DEVICE_FIPS                = 0x0000003a,
	NT_FILE_DEVICE_FULLSCREEN_VIDEO    = 0x00000034,
	NT_FILE_DEVICE_INPORT_PORT         = 0x0000000a,
	NT_FILE_DEVICE_KEYBOARD            = 0x0000000b,
	NT_FILE_DEVICE_KS                  = 0x0000002f,
	NT_FILE_DEVICE_KSEC                = 0x00000039,
	NT_FILE_DEVICE_MAILSLOT            = 0x0000000c,
	NT_FILE_DEVICE_MASS_STORAGE        = 0x0000002d,
	NT_FILE_DEVICE_MIDI_IN             = 0x0000000d,
	NT_FILE_DEVICE_MIDI_OUT            = 0x0000000e,
	NT_FILE_DEVICE_MODEM               = 0x0000002b,
	NT_FILE_DEVICE_MOUSE               = 0x0000000f,
	NT_FILE_DEVICE_MULTI_UNC_PROVIDER  = 0x00000010,
	NT_FILE_DEVICE_NAMED_PIPE          = 0x00000011,
	NT_FILE_DEVICE_NETWORK             = 0x00000012,
	NT_FILE_DEVICE_NETWORK_BROWSER     = 0x00000013,
	NT_FILE_DEVICE_NETWORK_FILE_SYSTEM = 0x00000014,
	NT_FILE_DEVICE_NETWORK_REDIRECTOR  = 0x00000028,
	NT_FILE_DEVICE_NULL                = 0x00000015,
	NT_FILE_DEVICE_PARALLEL_PORT       = 0x00000016,
	NT_FILE_DEVICE_PHYSICAL_NETCARD    = 0x00000017,
	NT_FILE_DEVICE_PRINTER             = 0x00000018,
	NT_FILE_DEVICE_SCANNER             = 0x00000019,
	NT_FILE_DEVICE_SCREEN              = 0x0000001c,
	NT_FILE_DEVICE_SERENUM             = 0x00000037,
	NT_FILE_DEVICE_SERIAL_MOUSE_PORT   = 0x0000001a,
	NT_FILE_DEVICE_SERIAL_PORT         = 0x0000001b,
	NT_FILE_DEVICE_SMARTCARD           = 0x00000031,
	NT_FILE_DEVICE_SMB                 = 0x0000002e,
	NT_FILE_DEVICE_SOUND               = 0x0000001d,
	NT_FILE_DEVICE_STREAMS             = 0x0000001e,
	NT_FILE_DEVICE_TAPE                = 0x0000001f,
	NT_FILE_DEVICE_TAPE_FILE_SYSTEM    = 0x00000020,
	NT_FILE_DEVICE_TERMSRV             = 0x00000038,
	NT_FILE_DEVICE_TRANSPORT           = 0x00000021,
	NT_FILE_DEVICE_UNKNOWN             = 0x00000022,
	NT_FILE_DEVICE_VDM                 = 0x0000002c,
	NT_FILE_DEVICE_VIDEO               = 0x00000023,
	NT_FILE_DEVICE_VIRTUAL_DISK        = 0x00000024,
	NT_FILE_DEVICE_WAVE_IN             = 0x00000025,
	NT_FILE_DEVICE_WAVE_OUT            = 0x00000026,
} nt_device_type;


/* forward declaration of structures */
struct _nt_device_object;
struct _nt_driver_object;

typedef struct _nt_list_entry {
	struct _nt_list_entry *		flink;
	struct _nt_list_entry *		blink;
} nt_list_entry;


typedef struct _nt_dispatcher_header {
	int32_t		lock;		/* context-specific interpretations */
	int32_t		signal_state;	/* context-specific interpretations */
	nt_list_entry	wait_list_head;
} nt_dispatcher_header;


typedef struct _nt_io_completion_context {
	void *	port;
	void *	key;
} nt_io_completion_context;


typedef struct _nt_fast_io_dispatch {
	uint32_t	size_of_fast_io_dispatch;
	unsigned char * fast_io_check_if_possible;
	unsigned char * fast_io_read;
	unsigned char *	fast_io_write;
	unsigned char *	fast_io_query_basic_info;
	unsigned char *	fast_io_query_standard_info;
	unsigned char *	fast_io_lock;
	unsigned char *	fast_io_unlock_single;
	unsigned char *	fast_io_unlock_all;
	unsigned char *	fast_io_unlock_all_by_key;
	unsigned char *	fast_io_device_control;
	void *		acquire_file_for_nt_create_section;
	void *		release_file_for_nt_create_section;
	void *		fast_io_detach_device;
	unsigned char *	fast_io_query_network_open_info;
	int32_t		acquire_for_mod_write;
	unsigned char *	mdl_read;
	unsigned char *	mdl_read_complete;
	unsigned char *	prepare_mdl_write;
	unsigned char *	mdl_write_complete;
	unsigned char *	fast_io_read_compressed;
	unsigned char *	fast_io_write_compressed;
	unsigned char * mdl_read_complete_compressed;
	unsigned char *	mdl_write_complete_compressed;
	unsigned char *	fast_io_query_open;
	int32_t *	release_for_mod_write;
	int32_t *	acquire_for_cc_flush;
	int32_t *	release_for_cc_flush;
} nt_fast_io_dispatch;


typedef struct _nt_io_timer {
	int16_t		type;
	int16_t		timer_flag;
	nt_list_entry	timer_listj;
	void *		timer_routine;
	void *		context;
	void *		device_object;
} nt_io_timer;


typedef struct _nt_ecp_list {
	char	opaque[1];
} nt_ecp_list;


typedef struct _nt_txn_parameter_block {
	uint16_t	length;
	uint16_t	tx_fs_context;
	void *		transaction_object;
} nt_txn_parameter_block;


typedef struct _nt_io_driver_create_context {
	uint16_t			size;
	struct _nt_ecp_list *		extra_create_parameters;
	void *				device_object_hint;
	nt_txn_parameter_block *	txn_parameters;
} nt_io_driver_create_context;


typedef struct _nt_irp {
	int16_t			type;
	uint16_t		size;
	struct _nt_mdl *	mdl_address;
	uint32_t		flags;
	uintptr_t		associated_irp;
	nt_list_entry		thread_list_entry;
	char			requestor_mode;
	unsigned char		pending_returned;
	char			stack_count;
	char			current_location;
	unsigned char		cancel;
	unsigned char		cancel_irql;
	char			apc_environment;
	unsigned char		allocation_flags;
	nt_io_status_block *	user_iosb;
	struct _nt_kevent *	user_event;
	void *			overlay[2];
	void *			cancel_routine;
	void *			user_buffer;
	void *			tail;
} nt_irp;


typedef struct _nt_kdevice_queue {
	int16_t			type;
	int16_t			size;
	struct _nt_list_entry	device_list_head;
	uint64_t		lock;
	unsigned char		busy_hint[8];
} nt_kdevice_queue;


typedef struct _nt_kdevice_queue_entry {
	nt_list_entry	device_list_entry;
	uint32_t	sort_key;
	unsigned char	inserted;
} nt_kdevice_queue_entry;


typedef struct _nt_kevent {
	struct _nt_dispatcher_header	header;
} nt_kevent;


typedef struct _nt_kdpc {
	unsigned char		type;
	unsigned char		importance;
	uint16_t		number;
	nt_list_entry		dpc_list_entry;
	void *			deferred_routine;
	void *			deferred_context;
	void *			system_argument_1st;
	void *			system_argument_2nd;
	void *			dpc_data;
} nt_kdpc;


typedef struct _nt_mdl {
	struct _nt_mdl *	next;
	int16_t			size;
	int16_t			mdl_flags;
	void *			process;
	void *			mapped_system_va;
	void *			start_va;
	uint32_t		byte_count;
	uint32_t		byte_offset;
} nt_mdl;


typedef struct _nt_vpb {
	int16_t				type;
	int16_t				size;
	uint16_t			flags;
	uint16_t			volume_label_length;
	struct _nt_device_object *	device_object;
	struct _nt_device_object *	real_device;
	uint32_t			serial_number;
	uint32_t			reference_count;
	wchar16_t			volume_label[32];
} nt_vpb;


typedef struct _nt_wait_context_block {
	struct _nt_kdevice_queue_entry		wait_queue_entry;
	void *					device_routine;
	void *					device_context;
	uint32_t				number_of_map_registers;
	void *					device_object;
	void *					current_irp;
	struct _kdpc *				buffer_chaining_dpc;
} nt_wait_context_block;


typedef struct _nt_device_object {
	int16_t				type;
	uint16_t			size;
	int32_t				ref_count;
	struct _nt_driver_object *	driver_obj;
	struct _nt_device_object *	next_device;
	struct _nt_device_object *	attached_device;
	struct _nt_irp *		current_irp;
	struct _nt_io_timer *		timer;
	uint32_t			flags;
	uint32_t			characteristics;
	struct _nt_vpb *		vpb;
	void *				dev_ext;
	nt_device_type			dev_type;
	char				stack_size;

	union {
		struct _nt_list_entry		list_entry;
		struct _nt_wait_context_block	wcb;
	} queue;

	uint32_t			alignment_requirement;
	struct _nt_kdevice_queue	dev_queue;
	struct _nt_kdpc			dpc;
	uint32_t			active_thread_count;
	nt_security_descriptor *	sec_desc;
	struct _nt_kevent		dev_lock;
	uint16_t			sector_size;
	uint16_t			spare1;
	void *				device_object_extension;
	void *				reserved;
} nt_device_object;


typedef struct _nt_driver_object {
	int16_t				type;
	int16_t				size;
	struct _nt_device_object *	dev_obj;
	uint32_t			flags;
	void *				driver_start;
	uint32_t			driver_size;
	void *				driver_section;
	void *				driver_extension; /* TODO: define struct _nt_driver_extension (tedious) */
	nt_unicode_string		driver_name;
	nt_unicode_string *		hardware_database;
	struct _nt_fast_io_dispatch *	fast_io_dispatch;
	int32_t *			driver_init;
	void *				driver_start_io;
	void *				driver_unload;
	void *				major_function[28];
} nt_driver_object;


typedef int32_t __stdcall ntapi_zw_load_driver(
	__in	nt_unicode_string *	driver_service_name);


typedef int32_t __stdcall ntapi_zw_unload_driver(
	__in	nt_unicode_string *	driver_service_name);

#endif
