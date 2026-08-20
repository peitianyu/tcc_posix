#ifndef _NT_FILE_H_
#define _NT_FILE_H_

#include <psxtypes/psxtypes.h>
#include "nt_object.h"
#include "nt_device.h"

typedef enum _nt_file_info_class {
	NT_FILE_DIRECTORY_INFORMATION			= 1,
	NT_FILE_FULL_DIRECTORY_INFORMATION		= 2,
	NT_FILE_BOTH_DIRECTORY_INFORMATION		= 3,
	NT_FILE_BASIC_INFORMATION			= 4,
	NT_FILE_STANDARD_INFORMATION			= 5,
	NT_FILE_INTERNAL_INFORMATION			= 6,
	NT_FILE_EA_INFORMATION				= 7,
	NT_FILE_ACCESS_INFORMATION			= 8,
	NT_FILE_NAME_INFORMATION			= 9,
	NT_FILE_RENAME_INFORMATION			= 10,
	NT_FILE_LINK_INFORMATION			= 11,
	NT_FILE_NAMES_INFORMATION			= 12,
	NT_FILE_DISPOSITION_INFORMATION			= 13,
	NT_FILE_POSITION_INFORMATION			= 14,
	NT_FILE_FULL_EA_INFORMATION			= 15,
	NT_FILE_MODE_INFORMATION			= 16,
	NT_FILE_ALIGNMENT_INFORMATION			= 17,
	NT_FILE_ALL_INFORMATION				= 18,
	NT_FILE_ALLOCATION_INFORMATION			= 19,
	NT_FILE_END_OF_FILE_INFORMATION			= 20,
	NT_FILE_ALTERNATE_NAME_INFORMATION		= 21,
	NT_FILE_STREAM_INFORMATION			= 22,
	NT_FILE_PIPE_INFORMATION			= 23,
	NT_FILE_PIPE_LOCAL_INFORMATION			= 24,
	NT_FILE_PIPE_REMOTE_INFORMATION			= 25,
	NT_FILE_MAILSLOT_QUERY_INFORMATION		= 26,
	NT_FILE_MAILSLOT_SET_INFORMATION		= 27,
	NT_FILE_COMPRESSION_INFORMATION			= 28,
	NT_FILE_OBJECT_ID_INFORMATION			= 29,
	NT_FILE_COMPLETION_INFORMATION			= 30,
	NT_FILE_MOVE_CLUSTER_INFORMATION		= 31,
	NT_FILE_QUOTA_INFORMATION			= 32,
	NT_FILE_REPARSE_POINT_INFORMATION		= 33,
	NT_FILE_NETWORK_OPEN_INFORMATION		= 34,
	NT_FILE_ATTRIBUTE_TAG_INFORMATION		= 35,
	NT_FILE_TRACKING_INFORMATION			= 36,
	NT_FILE_ID_BOTH_DIRECTORY_INFORMATION		= 37,
	NT_FILE_ID_FULL_DIRECTORY_INFORMATION		= 38,
	NT_FILE_VALID_DATA_LENGTH_INFORMATION		= 39,
	NT_FILE_SHORT_NAME_INFORMATION			= 40,
	NT_FILE_IO_COMPLETION_NOTIFICATION_INFORMATION	= 41,
	NT_FILE_IO_STATUS_BLOCK_RANGE_INFORMATION	= 42,
	NT_FILE_IO_PRIORITY_HINT_INFORMATION		= 43,
	NT_FILE_SFIO_RESERVE_INFORMATION		= 44,
	NT_FILE_SFIO_VOLUME_INFORMATION			= 45,
	NT_FILE_HARD_LINK_INFORMATION			= 46,
	NT_FILE_PROCESS_IDS_USING_FILE_INFORMATION	= 47,
	NT_FILE_NORMALIZED_NAME_INFORMATION		= 48,
	NT_FILE_NETWORK_PHYSICAL_NAME_INFORMATION	= 49,
	NT_FILE_ID_GLOBAL_TX_DIRECTORY_INFORMATION	= 50,
	NT_FILE_IS_REMOTE_DEVICE_INFORMATION		= 51,
	NT_FILE_ATTRIBUTE_CACHE_INFORMATION		= 52,
	NT_FILE_NUMA_NODE_INFORMATION			= 53,
	NT_FILE_STANDARD_LINK_INFORMATION		= 54,
	NT_FILE_REMOTE_PROTOCOL_INFORMATION		= 55,
	NT_FILE_REPLACE_COMPLETION_INFORMATION		= 56,
	NT_FILE_INFORMATION_CAP				= 57
} nt_file_info_class;


typedef enum _nt_file_create_disposition {
	NT_FILE_SUPERSEDE	= 0x00000000,
	NT_FILE_OPEN		= 0x00000001,
	NT_FILE_CREATE		= 0x00000002,
	NT_FILE_OPEN_IF		= 0x00000003,
	NT_FILE_OVERWRITE	= 0x00000004,
	NT_FILE_OVERWRITE_IF	= 0x00000005
} nt_file_create_disposition;


typedef enum _nt_file_status {
	NT_FILE_SUPERSEDED	= 0x00000000,
	NT_FILE_OPENED		= 0x00000001,
	NT_FILE_CREATED		= 0x00000002,
	NT_FILE_OVERWRITTEN	= 0x00000003,
	NT_FILE_EXISTS		= 0x00000004,
	NT_FILE_DOES_NOT_EXIST	= 0x00000005
} nt_file_status;


typedef enum _nt_file_action {
	NT_FILE_ACTION_ADDED			= 0x00000001,
	NT_FILE_ACTION_REMOVED			= 0x00000002,
	NT_FILE_ACTION_MODIFIED			= 0x00000003,
	NT_FILE_ACTION_RENAMED_OLD_NAME		= 0x00000004,
	NT_FILE_ACTION_RENAMED_NEW_NAME		= 0x00000005,
	NT_FILE_ACTION_ADDED_STREAM		= 0x00000006,
	NT_FILE_ACTION_REMOVED_STREAM		= 0x00000007,
	NT_FILE_ACTION_MODIFIED_STREAM		= 0x00000008,
	NT_FILE_ACTION_REMOVED_BY_DELETE	= 0x00000009,
} nt_file_action;


typedef enum _nt_file_pipe_flags {
	NT_FILE_PIPE_BYTE_STREAM_MODE	= 0x00000000,
	NT_FILE_PIPE_MESSAGE_MODE	= 0x00000001,

	NT_FILE_PIPE_QUEUE_OPERATION	= 0x00000000,
	NT_FILE_PIPE_COMPLETE_OPERATION	= 0x00000001,

	NT_FILE_PIPE_BYTE_STREAM_TYPE	= 0x00000000,
	NT_FILE_PIPE_MESSAGE_TYPE	= 0x00000001,

	NT_FILE_PIPE_INBOUND		= 0x00000000,
	NT_FILE_PIPE_OUTBOUND		= 0x00000001,
	NT_FILE_PIPE_FULL_DUPLEX	= 0x00000002,

	NT_FILE_PIPE_DISCONNECTED_STATE	= 0x00000001,
	NT_FILE_PIPE_LISTENING_STATE	= 0x00000002,
	NT_FILE_PIPE_CONNECTED_STATE	= 0x00000003,
	NT_FILE_PIPE_CLOSING_STATE	= 0x00000004,

	NT_FILE_PIPE_CLIENT_END		= 0x00000000,
	NT_FILE_PIPE_SERVER_END		= 0x00000001,
} nt_file_pipe_flags;


typedef enum _nt_io_priority_hint {
	NT_IO_PRIORITY_VERY_LOW		= 0,
	NT_IO_PRIORITY_LOW		= 1,
	NT_IO_PRIORITY_NORMAL		= 2,
	NT_IO_PRIORITY_HIGH		= 3,
	NT_IO_PRIORITY_CRITICAL		= 4,
	NT_MAX_IO_PRIORITY_TYPES	= 5
} nt_io_priority_hint;


typedef enum _nt_fs_info_class {
	NT_FILE_FS_VOLUME_INFORMATION		= 1,
	NT_FILE_FS_LABEL_INFORMATION		= 2,
	NT_FILE_FS_SIZE_INFORMATION		= 3,
	NT_FILE_FS_DEVICE_INFORMATION		= 4,
	NT_FILE_FS_ATTRIBUTE_INFORMATION	= 5,
	NT_FILE_FS_CONTROL_INFORMATION		= 6,
	NT_FILE_FS_FULL_SIZE_INFORMATION	= 7,
	NT_FILE_FS_OBJECT_ID_INFORMATION	= 8,
	NT_FILE_FS_DRIVER_PATH_INFORMATION	= 9,
	NT_FILE_FS_VOLUME_FLAGS_INFORMATION	= 10,
	NT_FILE_FS_SECTOR_SIZE_INFORMATION	= 11,
} nt_fs_info_class;


/* file attributes */
#define NT_FILE_ATTRIBUTE_READONLY		0x00000001
#define NT_FILE_ATTRIBUTE_HIDDEN		0x00000002
#define NT_FILE_ATTRIBUTE_SYSTEM		0x00000004
#define NT_FILE_ATTRIBUTE_DIRECTORY		0x00000010
#define NT_FILE_ATTRIBUTE_ARCHIVE		0x00000020
#define NT_FILE_ATTRIBUTE_DEVICE		0x00000040
#define NT_FILE_ATTRIBUTE_NORMAL		0x00000080
#define NT_FILE_ATTRIBUTE_TEMPORARY		0x00000100
#define NT_FILE_ATTRIBUTE_SPARSE_FILE		0x00000200
#define NT_FILE_ATTRIBUTE_REPARSE_POINT		0x00000400
#define NT_FILE_ATTRIBUTE_COMPRESSED		0x00000800
#define NT_FILE_ATTRIBUTE_OFFLINE		0x00001000
#define NT_FILE_ATTRIBUTE_NOT_CONTENT_INDEXED	0x00002000
#define NT_FILE_ATTRIBUTE_ENCRYPTED		0x00004000
#define NT_FILE_ATTRIBUTE_INTEGRITY_STREAM	0x00008000
#define NT_FILE_ATTRIBUTE_VIRTUAL		0x00010000
#define NT_FILE_ATTRIBUTE_NO_SCRUB_DATA		0x00020000


/* file create options */
#define NT_FILE_ASYNCHRONOUS_IO			0x00000000
#define NT_FILE_DIRECTORY_FILE			0x00000001
#define NT_FILE_WRITE_THROUGH			0x00000002
#define NT_FILE_SEQUENTIAL_ONLY			0x00000004
#define NT_FILE_NO_INTERMEDIATE_BUFFERING	0x00000008
#define NT_FILE_SYNCHRONOUS_IO_ALERT		0x00000010
#define NT_FILE_SYNCHRONOUS_IO_NONALERT		0x00000020
#define NT_FILE_NON_DIRECTORY_FILE		0x00000040
#define NT_FILE_CREATE_TREE_CONNECTION		0x00000080
#define NT_FILE_COMPLETE_IF_OPLOCKED		0x00000100
#define NT_FILE_NO_EA_KNOWLEDGE			0x00000200
#define NT_FILE_OPEN_REMOTE_INSTANCE		0x00000400
#define NT_FILE_RANDOM_ACCESS			0x00000800
#define NT_FILE_DELETE_ON_CLOSE			0x00001000
#define NT_FILE_OPEN_BY_FILE_ID			0x00002000
#define NT_FILE_OPEN_FOR_BACKUP_INTENT		0x00004000
#define NT_FILE_NO_COMPRESSION			0x00008000
#define NT_FILE_SESSION_AWARE			0x00040000
#define NT_FILE_RESERVE_OPFILTER		0x00100000
#define NT_FILE_OPEN_REPARSE_POINT		0x00200000
#define NT_FILE_OPEN_NO_RECALL			0x00400000
#define NT_FILE_OPEN_FOR_FREE_SPACE_QUERY	0x00800000


/* file share access */
#define NT_FILE_SHARE_READ			0x00000001
#define NT_FILE_SHARE_WRITE			0x00000002
#define NT_FILE_SHARE_DELETE			0x00000004


/* file notify */
#define NT_FILE_NOTIFY_CHANGE_FILE_NAME		0x00000001
#define NT_FILE_NOTIFY_CHANGE_DIR_NAME		0x00000002
#define NT_FILE_NOTIFY_CHANGE_ATTRIBUTES	0x00000004
#define NT_FILE_NOTIFY_CHANGE_SIZE		0x00000008
#define NT_FILE_NOTIFY_CHANGE_LAST_WRITE	0x00000010
#define NT_FILE_NOTIFY_CHANGE_LAST_ACCESS	0x00000020
#define NT_FILE_NOTIFY_CHANGE_CREATION		0x00000040
#define NT_FILE_NOTIFY_CHANGE_EA		0x00000040
#define NT_FILE_NOTIFY_CHANGE_SECURITY		0x00000100
#define NT_FILE_NOTIFY_CHANGE_STREAM_NAME	0x00000100
#define NT_FILE_NOTIFY_CHANGE_STREAM_SIZE	0x00000100
#define NT_FILE_NOTIFY_CHANGE_STREAM_WRITE	0x00000100


/* file system flags */
#define NT_FILE_CASE_SENSITIVE_SEARCH		0x00000001
#define NT_FILE_CASE_PRESERVED_NAMES		0x00000002
#define NT_FILE_UNICODE_ON_DISK			0x00000004
#define NT_FILE_PERSISTENT_ACLS			0x00000008
#define NT_FILE_FILE_COMPRESSION		0x00000010
#define NT_FILE_VOLUME_QUOTAS			0x00000020
#define NT_FILE_SUPPORTS_SPARSE_FILES		0x00000040
#define NT_FILE_SUPPORTS_REPARSE_POINTS		0x00000080
#define NT_FILE_SUPPORTS_REMOTE_STORAGE		0x00000100
#define NT_FILE_VOLUME_IS_COMPRESSED		0x00008000
#define NT_FILE_SUPPORTS_OBJECT_IDS		0x00010000
#define NT_FILE_SUPPORTS_ENCRYPTION		0x00020000
#define NT_FILE_NAMED_STREAMS			0x00040000
#define NT_FILE_READ_ONLY_VOLUME		0x00080000
#define NT_FILE_SEQUENTIAL_WRITE_ONCE		0x00100000
#define NT_FILE_SUPPORTS_TRANSACTIONS		0x00200000
#define NT_FILE_SUPPORTS_HARD_LINKS		0x00400000
#define NT_FILE_SUPPORTS_EXTENDED_ATTRIBUTES	0x00800000
#define NT_FILE_SUPPORTS_OPEN_BY_FILE_ID	0x01000000
#define NT_FILE_SUPPORTS_USN_JOURNAL		0x02000000
#define NT_FILE_SUPPORT_INTEGRITY_STREAMS	0x04000000


/* file sector size flags */
#define NT_SSINFO_FLAGS_ALIGNED_DEVICE			0x00000001
#define NT_SSINFO_FLAGS_PARTITION_ALIGNED_ON_DEVICE	0x00000002
#define NT_SSINFO_FLAGS_NO_SEEK_PENALTY			0x00000004
#define NT_SSINFO_FLAGS_TRIM_ENABLED			0x00000008


/* file alignment flags */
#define NT_FILE_BYTE_ALIGNMENT		0x00000000
#define NT_FILE_WORD_ALIGNMENT		0x00000001
#define NT_FILE_LONG_ALIGNMENT		0x00000003
#define NT_FILE_QUAD_ALIGNMENT		0x00000007
#define NT_FILE_OCTA_ALIGNMENT		0x0000000f
#define NT_FILE_32_BYTE_ALIGNMENT	0x0000001f
#define NT_FILE_64_BYTE_ALIGNMENT	0x0000003f
#define NT_FILE_128_BYTE_ALIGNMENT	0x0000007f
#define NT_FILE_256_BYTE_ALIGNMENT	0x000000ff
#define NT_FILE_512_BYTE_ALIGNMENT	0x000001ff


/* file tx info flags */
#define NT_FILE_ID_GLOBAL_TX_DIR_INFO_FLAG_WRITELOCKED		0x00000001
#define NT_FILE_ID_GLOBAL_TX_DIR_INFO_FLAG_VISIBLE_TO_TX	0x00000002
#define NT_FILE_ID_GLOBAL_TX_DIR_INFO_FLAG_VISIBLE_OUTSIDE_TX	0x00000004


/* friendly file type bits */
#define NT_FILE_TYPE_UNKNOWN		(0x0000u)
#define NT_FILE_TYPE_FILE		(0x0001u)
#define NT_FILE_TYPE_DIRECTORY		(0x0002u)
#define NT_FILE_TYPE_PIPE		(0x0004u)
#define NT_FILE_TYPE_SOCKET		(0x0008u)
#define NT_FILE_TYPE_MAILSLOT		(0x0010u)
#define NT_FILE_TYPE_CSRSS		(0x0020u)
#define NT_FILE_TYPE_PTY		(0x0040u)
#define NT_FILE_TYPE_VFD		(0x0080u)


/* file access bits */
#define	NT_FILE_ANY_ACCESS		(0x0000u)
#define	NT_FILE_READ_ACCESS		(0x0001u)
#define	NT_FILE_READ_DATA		(0x0001u)
#define	NT_FILE_LIST_DIRECTORY		(0x0001u)
#define	NT_FILE_WRITE_ACCESS		(0x0002u)
#define	NT_FILE_WRITE_DATA		(0x0002u)
#define	NT_FILE_ADD_FILE		(0x0002u)
#define	NT_FILE_APPEND_DATA		(0x0004u)
#define	NT_FILE_ADD_SUBDIRECTORY	(0x0004u)
#define	NT_FILE_CREATE_PIPE_INSTANCE	(0x0004u)
#define	NT_FILE_READ_EA			(0x0008u)
#define	NT_FILE_WRITE_EA		(0x0010u)
#define	NT_FILE_EXECUTE			(0x0020u)
#define	NT_FILE_TRAVERSE		(0x0020u)
#define	NT_FILE_DELETE_CHILD		(0x0040u)
#define	NT_FILE_READ_ATTRIBUTES		(0x0080u)
#define	NT_FILE_WRITE_ATTRIBUTES	(0x0100u)

#define NT_FILE_ALL_ACCESS		NT_FILE_ANY_ACCESS \
					| NT_FILE_READ_ACCESS \
					| NT_FILE_WRITE_ACCESS \
					| NT_FILE_APPEND_DATA \
					| NT_FILE_READ_EA \
					| NT_FILE_WRITE_EA \
					| NT_FILE_EXECUTE \
					| NT_FILE_TRAVERSE \
					| NT_FILE_DELETE_CHILD \
					| NT_FILE_READ_ATTRIBUTES \
					| NT_FILE_WRITE_ATTRIBUTES \
					| NT_SEC_SYNCHRONIZE \
					| NT_SEC_STANDARD_RIGHTS_ALL


/* structures related to nt_fs_info_class */
typedef struct _nt_file_fs_volume_information {
	nt_large_integer	volume_creation_time;
	uint32_t		volume_serial_number;
	uint32_t		volume_label_length;
	unsigned char		supports_objects ;
	unsigned char		reserved;
	wchar16_t		volume_label[];
} nt_file_fs_volume_information, nt_fsvi;


typedef struct _nt_file_fs_label_information {
	uint32_t		volume_label_length;
	wchar16_t		volume_label[];
} nt_file_fs_label_information;


typedef struct _nt_file_fs_size_information {
	nt_large_integer	total_allocation_units;
	nt_large_integer	available_allocation_units;
	uint32_t		sectors_per_allocation_unit;
	uint32_t		bytes_per_sector;
} nt_file_fs_size_information, nt_fssi;


typedef struct _nt_file_fs_device_information {
	nt_device_type	device_type;
	uint32_t	characteristics;
} nt_file_fs_device_information, nt_fsdi;


typedef struct _nt_file_fs_attribute_information {
	uint32_t	file_system_attributes;
	uint32_t	maximum_component_name_length;
	uint32_t	file_system_name_length;
	wchar16_t	file_system_name[];
} nt_file_fs_attribute_information, nt_fsai;


typedef struct _nt_file_fs_control_information {
	nt_large_integer	free_space_start_filtering;
	nt_large_integer	free_space_threshold;
	nt_large_integer	free_space_stop_filtering;
	nt_large_integer	default_quota_threshold;
	nt_large_integer	default_quota_limit;
	uint32_t		file_system_control_flags;
	uint32_t		padding;
} nt_file_fs_control_information, nt_fsci;


typedef struct _nt_file_fs_full_size_information {
	nt_large_integer	total_allocation_units;
	nt_large_integer	caller_available_allocation_units;
	nt_large_integer	actual_available_allocation_units;
	uint32_t		sectors_per_allocation_unit;
	uint32_t		bytes_per_sector;
} nt_file_fs_full_size_information, nt_fsfsi;


typedef struct _nt_file_fs_object_id_information {
	nt_uuid		volume_object_id;
	uint32_t	volume_object_id_extended_info;
} nt_file_fs_object_id_information, nt_fsoii;


typedef struct _nt_file_fs_driver_path_information {
	unsigned char 		driver_in_path;
	unsigned char		reserved[3];
	uint32_t		driver_name_length;
	wchar16_t		driver_name[];
} nt_file_fs_driver_path_information, nt_fsdpi;


typedef struct _nt_file_fs_sector_size_information {
	uint32_t	logical_bytes_per_sector;
	uint32_t	physical_bytes_per_sector_for_atomicity;
	uint32_t	physical_bytes_per_sector_for_performance;
	uint32_t	effective_physical_bytes_per_sector_for_atomicity;
	uint32_t	flags;
	uint32_t	byte_offset_for_sector_alignment;
	uint32_t	byte_offset_for_partition_alignment;
} nt_file_fs_sector_size_information, nt_fsssi;


typedef struct _nt_file_fs_persistent_volume_information {
	uint32_t	volume_flags;
	uint32_t	flag_mask;
	uint32_t	version;
	uint32_t	reserved;
} nt_file_fs_persistent_volume_information;


/* structures related to zw_query_quota_information_file */
typedef struct _nt_file_user_quota_information {
	uint32_t		next_entry_offset;
	uint32_t		sid_length;
	nt_large_integer	change_time;
	nt_large_integer	quota_used;
	nt_large_integer	quota_threshold;
	nt_large_integer	quota_limit;
	nt_sid			sid[];
} nt_file_user_quota_information;


typedef struct _nt_file_quota_list_information {
	uint32_t	next_entry_offset;
	uint32_t	sid_length;
	nt_sid		sid[];
} nt_file_quota_list_information;



/* structures included in nt_file_all_information */
typedef struct _nt_file_basic_information {
	nt_large_integer	creation_time;
	nt_large_integer	last_access_time;
	nt_large_integer	last_write_time;
	nt_large_integer	change_time;
	uint32_t		file_attr;
} nt_file_basic_information, nt_fbi;


typedef struct _nt_file_standard_information {
	nt_large_integer	allocation_size;
	nt_large_integer	end_of_file;
	uint32_t		number_of_links;
	unsigned char		delete_pending;
	unsigned char		directory;
	unsigned char		reserved[2];
} nt_file_standard_information, nt_fsi;


typedef struct _nt_file_internal_information {
	nt_large_integer	index_number;
} nt_file_internal_information, nt_fii;


typedef struct _nt_file_ea_information {
	uint32_t	ea_size;
} nt_file_ea_information, nt_fei;


typedef struct _nt_file_access_information {
	uint32_t	access_flags;
} nt_file_access_information, nt_facci;


typedef struct _nt_file_position_information {
	nt_large_integer	current_byte_offset;
} nt_file_position_information, nt_fpi;


typedef struct _nt_file_mode_information {
	uint32_t	mode;
} nt_file_mode_information, nt_fmi;


typedef struct _nt_file_alignment_information {
	uint32_t	alignment_requirement;
} nt_file_alignment_information, nt_falii;


typedef struct _nt_file_name_information {
	uint32_t	file_name_length;
	wchar16_t	file_name[];
} nt_file_name_information, nt_fni;


typedef struct _nt_file_all_information {
	nt_file_basic_information	basic_info;
	nt_file_standard_information	standard_info;
	nt_file_internal_information	internal_info;
	nt_file_ea_information		ea_info;
	nt_file_access_information	access_info;
	nt_file_position_information	position_info;
	nt_file_mode_information	mode_info;
	nt_file_alignment_information	alignment_info;
	nt_file_name_information	name_info;
} nt_file_all_information, nt_fai;



/* structures related to nt_file_info_class */
typedef struct _nt_file_directory_information {
	uint32_t		next_entry_offset;
	uint32_t		file_index;
	nt_large_integer	creation_time;
	nt_large_integer	last_access_time;
	nt_large_integer	last_write_time;
	nt_large_integer	change_time;
	nt_large_integer	end_of_file;
	nt_large_integer	allocation_size;
	uint32_t		file_attributes;
	uint32_t		file_name_length;
	wchar16_t		file_name[];
} nt_file_directory_information, nt_fdiri;


typedef struct _nt_file_full_dir_information {
	uint32_t		next_entry_offset;
	uint32_t		file_index;
	nt_large_integer	creation_time;
	nt_large_integer	last_access_time;
	nt_large_integer	last_write_time;
	nt_large_integer	change_time;
	nt_large_integer	end_of_file;
	nt_large_integer	allocation_size;
	uint32_t		file_attributes;
	uint32_t		file_name_length;
	uint32_t		ea_size;
	wchar16_t		file_name[];
} nt_file_full_dir_information;


typedef struct _nt_file_both_dir_information {
	uint32_t		next_entry_offset;
	uint32_t		file_index;
	nt_large_integer	creation_time;
	nt_large_integer	last_access_time;
	nt_large_integer	last_write_time;
	nt_large_integer	change_time;
	nt_large_integer	end_of_file;
	nt_large_integer	allocation_size;
	uint32_t		file_attributes;
	uint32_t		file_name_length;
	uint32_t		ea_size;
	char			short_name_length;
	char			reserved;
	wchar16_t		short_name[12];
	wchar16_t		file_name[];
} nt_file_both_dir_information;


typedef struct _nt_file_rename_information {
	unsigned char		replace_if_exists;
	void *			root_directory;
	uint32_t		file_name_length;
	wchar16_t		file_name[];
} nt_file_rename_information, nt_frni;


typedef struct _nt_file_link_information {
	unsigned char		replace_if_exists;
	void *			root_directory;
	uint32_t		file_name_length;
	wchar16_t		file_name[];
} nt_file_link_information;


typedef struct _nt_file_names_information {
	uint32_t		next_entry_offset;
	uint32_t		file_index;
	uint32_t		file_name_length;
	wchar16_t		file_name[];
} nt_file_names_information, nt_fnamesi;


typedef struct _nt_file_disposition_information {
	unsigned char	delete_file;
} nt_file_disposition_information, nt_fdi;


typedef struct _nt_file_full_ea_information {
	uint32_t	next_entry_offset;
	unsigned char	flags;
	unsigned char	ea_name_length;
	uint16_t	ea_value_length;
	char		ea_name[];
} nt_file_full_ea_information;


typedef struct _nt_file_allocation_information {
	nt_large_integer	allocation_size;
} nt_file_allocation_information;


typedef struct _nt_file_end_of_file_information {
	nt_large_integer	end_of_file;
} nt_file_end_of_file_information, nt_eof;


typedef struct _nt_file_stream_information {
	uint32_t		next_entry_offset;
	uint32_t		stream_name_length;
	nt_large_integer	stream_size;
	nt_large_integer	stream_allocation_size;
	wchar16_t		stream_name[];
} nt_file_stream_information;


typedef struct _nt_file_pipe_information {
	uint32_t	read_mode;
	uint32_t	completion_mode;
} nt_file_pipe_information;


typedef struct _nt_file_pipe_local_information {
	uint32_t	named_pipe_type;
	uint32_t	named_pipe_configuration;
	uint32_t	maximum_instances;
	uint32_t	current_instances;
	uint32_t	inbound_quota;
	uint32_t	read_data_available;
	uint32_t	outbound_quota;
	uint32_t	write_quota_available;
	uint32_t	named_pipe_state;
	uint32_t	named_pipe_end;
} nt_file_pipe_local_information;


typedef struct _nt_file_pipe_remote_information {
	nt_large_integer	collect_data_time;
	uint32_t		maximum_collection_count;
} nt_file_pipe_remote_information;


typedef struct _nt_file_mailslot_query_information {
	uint32_t		maximum_message_size;
	uint32_t		mailslot_quota;
	uint32_t		next_message_size;
	uint32_t		messages_available;
	nt_large_integer	read_timeout;
} nt_file_mailslot_query_information;


typedef struct _nt_file_mailslot_set_information {
	nt_large_integer *	read_timeout;
} nt_file_mailslot_set_information;


typedef struct _nt_file_compression_information {
	nt_large_integer		CompressedFileSize;
	uint16_t			CompressionFormat;
	unsigned char			CompressionUnitShift;
	unsigned char			ChunkShift;
	unsigned char			ClusterShift;
	unsigned char			Reserved[3];
} nt_file_compression_information;


typedef struct _nt_file_object_id_buffer {
	unsigned char	obj_id[16];
	unsigned char	birth_volume_id[16];
	unsigned char	birth_object_id[16];
	unsigned char	domain_id[16];
} nt_file_object_id_buffer;


typedef struct _nt_file_object_id_information {
	int64_t		file_reference;
	unsigned char	object_id[16];

	union {
		struct {
			unsigned char	birth_volume_id[16];
			unsigned char	birth_object_id[16];
			unsigned char	domain_id[16];
		};
		unsigned char	extended_info[48];
	};
} nt_file_object_id_information;


typedef struct _nt_file_quota_information {
	uint32_t		next_entry_offset;
	uint32_t		sid_length;
	nt_large_integer	change_time;
	nt_large_integer	quota_used;
	nt_large_integer	quota_threshold;
	nt_large_integer	quota_limit;
	nt_sid			sid;
} nt_file_quota_information;


typedef struct _nt_file_reparse_point_information {
	int64_t		file_reference;
	uint32_t	tag;
} nt_file_reparse_point_information, nt_frpi;


typedef struct _nt_file_network_open_information {
	nt_large_integer	creation_time;
	nt_large_integer	last_access_time;
	nt_large_integer	last_write_time;
	nt_large_integer	change_time;
	nt_large_integer	allocation_size;
	nt_large_integer	end_of_file;
	uint32_t		file_attributes;
} nt_file_network_open_information;


typedef struct _nt_file_attribute_tag_information {
	uint32_t	file_attr;
	uint32_t	reparse_tag;
} nt_file_attribute_tag_information, nt_ftagi;


typedef struct _nt_file_id_both_dir_information {
	uint32_t		next_entry_offset;
	uint32_t		file_index;
	nt_large_integer	creation_time;
	nt_large_integer	last_access_time;
	nt_large_integer	last_write_time;
	nt_large_integer	change_time;
	nt_large_integer	end_of_file;
	nt_large_integer	allocation_size;
	uint32_t		file_attributes;
	uint32_t		file_name_length;
	uint32_t		ea_size;
	char			short_name_length;
	char			reserved;
	wchar16_t		short_name[12];
	nt_large_integer	file_id;
	wchar16_t		file_name[];
} nt_file_id_both_dir_information, nt_fsdirent;


typedef struct _nt_file_id_full_dir_information {
	uint32_t		next_entry_offset;
	uint32_t		file_index;
	nt_large_integer	creation_time;
	nt_large_integer	last_access_time;
	nt_large_integer	last_write_time;
	nt_large_integer	change_time;
	nt_large_integer	end_of_file;
	nt_large_integer	allocation_size;
	uint32_t		file_attributes;
	uint32_t		file_name_length;
	uint32_t		ea_size;
	nt_large_integer	FileId;
	wchar16_t		file_name[];
} nt_file_id_full_dir_information;


typedef struct _nt_file_valid_data_length_information {
	nt_large_integer	valid_data_length;
} nt_file_valid_data_length_information;


typedef struct _nt_file_io_priority_hint_information {
	nt_io_priority_hint	priority_hint;
} nt_file_io_priority_hint_information;


typedef struct _nt_file_link_entry_information {
	uint32_t	next_entry_offset;
	int64_t		parent_file_id;
	uint32_t	file_name_length;
	wchar16_t	file_name[];
} nt_file_link_entry_information;


typedef struct _nt_file_links_information {
	uint32_t	bytes_needed;
	uint32_t	entries_returned;
	nt_file_link_entry_information	entry;
} nt_file_links_information;


typedef struct _nt_file_id_global_tx_dir_information {
	uint32_t		next_entry_offset;
	uint32_t		file_index;
	nt_large_integer	creation_time;
	nt_large_integer	last_access_time;
	nt_large_integer	last_write_time;
	nt_large_integer	change_time;
	nt_large_integer	end_of_file;
	nt_large_integer	allocation_size;
	uint32_t		file_attributes;
	uint32_t		file_name_length;
	nt_large_integer	file_id;
	nt_guid			locking_transaction_id;
	uint32_t		tx_info_flags;
	wchar16_t		file_name[];
} nt_file_id_global_tx_dir_information;


typedef struct _nt_file_completion_information {
	void *	port;
	void *	key;
} nt_file_completion_information;



/* file extended attributes */
typedef struct _nt_file_get_ea_information {
	uint32_t	next_entry_offset;
	unsigned char	ea_name_length;
	char		ea_name[];
} nt_file_get_ea_information;


/* quota information */
typedef struct _nt_file_get_quota_information {
	uint32_t	next_entry_offset;
	uint32_t	sid_length;
	nt_sid		sid;
} nt_file_get_quota_information;


/* file change notification */
typedef struct _nt_file_notify_information {
	uint32_t	next_entry_offset;
	uint32_t	action;
	uint32_t	file_name_length;
	wchar16_t	file_name[];
} nt_file_notify_information;


/* file segment element */
typedef union _nt_file_segment_element {
  uint64_t *	buffer;
  uint64_t	alignment;
} nt_file_segment_element;


/* section object pointers */
typedef struct _nt_section_object_pointers {
	void *	data_section_object;
	void *	shared_cache_map;
	void *	image_section_object;
} nt_section_object_pointers;


/* reparse guid data buffer */
typedef struct _nt_reparse_guid_data_buffer {
	uint32_t	reparse_tag;
	uint16_t	reparse_data_length;
	uint16_t	reserved;
	nt_guid		reparse_guid;
	unsigned char	generic_reparse_buffer[];
} nt_reparse_guid_data_buffer;


/* reparse data buffer */
typedef struct _nt_reparse_data_buffer {
	uint32_t	reparse_tag;
	uint16_t	reparse_data_length;
	uint16_t	reserved;

	union {
		struct {
			uint16_t	substitute_name_offset;
			uint16_t	substitute_name_length;
			uint16_t	print_name_offset;
			uint16_t	print_name_length;
			uint32_t	flags;
		} symbolic_link_reparse_buffer;

		struct {
			uint16_t	substitute_name_offset;
			uint16_t	substitute_name_length;
			uint16_t	print_name_offset;
			uint16_t	print_name_length;
		} mount_point_reparse_buffer;
	};

	wchar16_t	path_buffer[];
} nt_reparse_data_buffer;


/* file object */
typedef struct _nt_file_object {
	int16_t				type;
	int16_t				size;
	nt_device_object *		dev_obj;
	nt_vpb				vpb;
	void *				fs_context;
	void *				fs_context_2nd;
	nt_section_object_pointers *	sec_obj_ptr;
	void *				private_cache_map;
	int32_t				final_status;
	struct _nt_file_object *	related_file_obj;
	unsigned char			lock_operation;
	unsigned char			delete_pending;
	unsigned char			read_access;
	unsigned char			write_access;
	unsigned char			delete_access;
	unsigned char			shared_read;
	unsigned char			shared_write;
	unsigned char			shared_delete;
	uint32_t			flags;
	nt_unicode_string		file_name;
	nt_large_integer		current_byte_offset;
	uint32_t			waiters;
	uint32_t			busy;
	void *				last_lock;
	nt_kevent			lock;
	nt_kevent			event;
	nt_io_completion_context *	completion_context;
	uint32_t			irp_list_lock;
	nt_list_entry			irp_list;
	void *				file_obj_ext;
} nt_file_object;




/* file-related function signatures */
typedef int32_t __stdcall ntapi_zw_create_file(
	__out	void **			hfile,
	__in	uint32_t		desired_access,
	__in	nt_object_attributes *	obj_attr,
	__out	nt_io_status_block *	io_status_block,
	__in	nt_large_integer *	allocation_size		__optional,
	__in	uint32_t		file_attr,
	__in	uint32_t		share_access,
	__in	uint32_t		create_disposition,
	__in	uint32_t		create_options,
	__in	void *			ea_buffer		__optional,
	__in	uint32_t		ea_length);


typedef int32_t	__stdcall ntapi_zw_open_file(
	__out	void **			hfile,
	__in	uint32_t		desired_access,
	__in	nt_object_attributes *	obj_attr,
	__out	nt_io_status_block *	io_status_block,
	__in	uint32_t		share_access,
	__in	uint32_t		open_options);


typedef int32_t	__stdcall ntapi_zw_delete_file(
	__in	nt_object_attributes *	obj_attr);


typedef int32_t	__stdcall ntapi_zw_flush_buffers_file(
	__in	void *			hfile,
	__out	nt_io_status_block *	io_status_block);


typedef int32_t	__stdcall ntapi_zw_cancel_io_file(
	__in	void *			hfile,
	__out	nt_io_status_block *	io_status_block);


typedef int32_t	__stdcall ntapi_zw_cancel_io_file_ex(
	__in	void *			hfile,
	__in	nt_io_status_block *	request_iosb	__optional,
	__out	nt_io_status_block *	cancel_iosb);


typedef int32_t	__stdcall ntapi_zw_read_file(
	__in	void *			hfile,
	__in	void *			hevent		__optional,
	__in	nt_io_apc_routine *	apc_routine	__optional,
	__in	void *			apc_context	__optional,
	__out	nt_io_status_block *	io_status_block,
	__out	void *			buffer,
	__in	uint32_t		bytes_to_read,
	__in	nt_large_integer *	byte_offset	__optional,
	__in	uint32_t *		key		__optional);


typedef int32_t __stdcall ntapi_zw_write_file(
	__in	void *			hfile,
	__in 	void *			hevent		__optional,
	__in	nt_io_apc_routine *	apc_routine	__optional,
	__in	void * 			apc_context	__optional,
	__out	nt_io_status_block * 	io_status_block,
	__in	void * 			buffer,
	__in	uint32_t		bytes_sent,
	__in	nt_large_integer *	byte_offset	__optional,
	__in	uint32_t *		key		__optional);


typedef int32_t	__stdcall ntapi_zw_read_file_scatter(
	__in	void *				hfile,
	__in	void *				hevent		__optional,
	__in	nt_io_apc_routine *		apc_routine	__optional,
	__in	void *				apc_context	__optional,
	__out	nt_io_status_block *		io_status_block,
	__in	nt_file_segment_element *	buffer,
	__in	uint32_t			bytes_to_read,
	__in	nt_large_integer *		byte_offset	__optional,
	__in	uint32_t *			key		__optional);


typedef int32_t	__stdcall ntapi_zw_write_file_gather(
	__in	void *				hfile,
	__in	void *				hevent		__optional,
	__in	nt_io_apc_routine *		apc_routine	__optional,
	__in	void *				apc_context	__optional,
	__out	nt_io_status_block *		io_status_block,
	__in	nt_file_segment_element *	buffer,
	__in	uint32_t			bytes_to_read,
	__in	nt_large_integer *		byte_offset	__optional,
	__in	uint32_t *			key		__optional);


typedef int32_t	__stdcall ntapi_zw_lock_file(
	__in	void *				hfile,
	__in	void *				hevent		__optional,
	__in	nt_io_apc_routine *		apc_routine	__optional,
	__in	void *				apc_context	__optional,
	__out	nt_io_status_block *		io_status_block,
	__in	nt_large_integer *		lock_offset,
	__in	nt_large_integer *		lock_length,
	__in	uint32_t *			key,
	__in	unsigned char			fail_immediately,
	__in	unsigned char			exclusive_lock);


typedef int32_t	__stdcall ntapi_zw_unlock_file(
	__in	void *				hfile,
	__out	nt_io_status_block *		io_status_block,
	__in	nt_large_integer *		lock_offset,
	__in	nt_large_integer *		lock_length,
	__in	uint32_t *			key);


typedef int32_t	__stdcall ntapi_zw_device_io_control_file(
	__in	void *				hfile,
	__in	void *				hevent			__optional,
	__in	nt_io_apc_routine *		apc_routine		__optional,
	__in	void *				apc_context		__optional,
	__out	nt_io_status_block *		io_status_block,
	__in	uint32_t			io_control_code,
	__in	void *				input_buffer		__optional,
	__in	uint32_t			input_buffer_length,
	__out	void *				output_buffer		__optional,
	__in	uint32_t			output_buffer_length);


typedef int32_t	__stdcall ntapi_zw_fs_control_file(
	__in	void *				hfile,
	__in	void *				hevent			__optional,
	__in	nt_io_apc_routine *		apc_routine		__optional,
	__in	void *				apc_context		__optional,
	__out	nt_io_status_block *		io_status_block,
	__in	uint32_t			fs_control_code,
	__in	void *				input_buffer		__optional,
	__in	uint32_t			input_buffer_length,
	__out	void *				output_buffer		__optional,
	__in	uint32_t			output_buffer_length);


typedef int32_t	__stdcall ntapi_zw_notify_change_directory_file(
	__in	void *				hfile	,
	__in	void *				hevent			__optional,
	__in	nt_io_apc_routine *		apc_routine		__optional,
	__in	void *				apc_context		__optional,
	__out	nt_io_status_block *		io_status_block,
	__out	nt_file_notify_information *	buffer,
	__in	uint32_t			buffer_length,
	__in	uint32_t			notify_filter,
	__in	unsigned char			watch_subtree);


typedef int32_t	__stdcall ntapi_zw_query_ea_file(
	__in	void *				hfile	,
	__out	nt_io_status_block *		io_status_block,
	__out	nt_file_full_ea_information *	buffer,
	__in	uint32_t			buffer_length,
	__in	unsigned char			return_single_entry,
	__in	nt_file_get_ea_information *	ea_list			__optional,
	__in	uint32_t			ea_list_length,
	__in	uint32_t *			ea_index		__optional,
	__in	unsigned char			restart_scan);


typedef int32_t	__stdcall ntapi_zw_set_ea_file(
	__in	void *				hfile	,
	__out	nt_io_status_block *		io_status_block,
	__in	nt_file_full_ea_information *	buffer,
	__in	uint32_t			buffer_length);


typedef int32_t	__stdcall ntapi_zw_create_named_pipe_file(
	__out	void **			hfile,
	__in	uint32_t		desired_access,
	__in	nt_object_attributes *	obj_attr,
	__out	nt_io_status_block *	io_status_block,
	__in	uint32_t		share_access,
	__in	uint32_t		create_disposition,
	__in	uint32_t		create_options,
	__in	uint32_t		type_message,
	__in	uint32_t		readmode_message,
	__in	uint32_t		non_blocking,
	__in	uint32_t		max_instances,
	__in	size_t			in_buffer_size,
	__in	size_t			out_buffer_size,
	__in	nt_large_integer *	default_timeout	__optional);


typedef int32_t	__stdcall ntapi_zw_create_mailslot_file(
	__out	void **			hfile,
	__in	uint32_t		desired_access,
	__in	nt_object_attributes *	obj_attr,
	__out	nt_io_status_block *	io_status_block,
	__in	uint32_t		create_options,
	__in	uint32_t		in_buffer_size,
	__in	uint32_t		max_message_size,
	__in	nt_large_integer *	read_timeout	__optional);


typedef int32_t	__stdcall ntapi_zw_query_volume_information_file(
	__in	void *				hfile	,
	__out	nt_io_status_block *		io_status_block,
	__out	void *				vol_info,
	__in	uint32_t			vol_info_length,
	__in	nt_fs_info_class		vol_info_class);


typedef int32_t	__stdcall ntapi_zw_set_volume_information_file(
	__in	void *				hfile	,
	__out	nt_io_status_block *		io_status_block,
	__in	void *				vol_info,
	__in	uint32_t			vol_info_length,
	__in	nt_fs_info_class		vol_info_class);


typedef int32_t	__stdcall ntapi_zw_query_quota_information_file(
	__in	void *					hfile	,
	__out	nt_io_status_block *			io_status_block,
	__out	nt_file_user_quota_information *	buffer,
	__in	uint32_t				buffer_length,
	__in	unsigned char				returning_single_entry,
	__in	nt_file_quota_list_information *	sid_list		__optional,
	__in	uint32_t				sid_list_length,
	__in	nt_sid *				start_sid		__optional,
	__in	unsigned char				restart_scan);


typedef int32_t	__stdcall ntapi_zw_set_quota_information_file(
	__in	void *					hfile	,
	__out	nt_io_status_block *			io_status_block,
	__in	nt_file_user_quota_information *	buffer,
	__in	uint32_t				buffer_length);


typedef int32_t	__stdcall ntapi_zw_query_attributes_file(
	__in	nt_object_attributes *		obj_attr,
	__out	nt_file_basic_information *	file_info);


typedef int32_t	__stdcall ntapi_zw_query_full_attributes_file(
	__in	nt_object_attributes *			obj_attr,
	__out	nt_file_network_open_information *	file_info);


typedef int32_t	__stdcall ntapi_zw_query_directory_file(
	__in	void *			hfile,
	__in	void *			hevent		__optional,
	__in	nt_io_apc_routine *	apc_routine	__optional,
	__in	void *			apc_context	__optional,
	__out	nt_io_status_block *	io_status_block,
	__out	void *			file_info,
	__in	uint32_t		file_info_length,
	__in	nt_file_info_class	file_info_class,
	__in	unsigned char		return_single_entry,
	__in	nt_unicode_string *	file_name	__optional,
	__in	unsigned char		restart_scan);


typedef int32_t	__stdcall ntapi_zw_query_information_file(
	__in	void *			hfile,
	__out	nt_io_status_block *	io_status_block,
	__out	void *			file_info,
	__in	uint32_t		file_info_length,
	__in	nt_file_info_class	file_info_class);


typedef int32_t	__stdcall ntapi_zw_set_information_file(
	__in	void *			hfile,
	__out	nt_io_status_block *	io_status_block,
	__in	void *			file_info,
	__in	uint32_t		file_info_length,
	__in	nt_file_info_class	file_info_class);

/* extension functions */
typedef int32_t __stdcall ntapi_tt_get_file_handle_type(
	__in	void *		handle,
	__out	int32_t *	type);

typedef int32_t __stdcall ntapi_tt_open_logical_parent_directory(
	__out	void **		hparent,
	__in	void *		hdir,
	__out	uintptr_t *	buffer,
	__in	uint32_t	buffer_size,
	__in	uint32_t	desired_access,
	__in	uint32_t	open_options,
	__out	int32_t *	type);

typedef int32_t __stdcall ntapi_tt_open_physical_parent_directory(
	__out	void **		hparent,
	__in	void *		hdir,
	__out	uintptr_t *	buffer,
	__in	uint32_t	buffer_size,
	__in	uint32_t	desired_access,
	__in	uint32_t	open_options,
	__out	int32_t *	type);

#endif
