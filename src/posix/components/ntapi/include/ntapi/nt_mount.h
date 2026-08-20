#ifndef _NT_MOUNT_H_
#define _NT_MOUNT_H_

#include <psxtypes/psxtypes.h>
#include "nt_ioctl.h"
#include "nt_statfs.h"

/* hash mark */
/* {'\\','D','e','v','i','c','e','\\'} */
#define __DEVICE_PATH_PREFIX_LEN	(8 * sizeof(wchar16_t))
#define __DEVICE_PATH_PREFIX_HASH	(0xDA6FA40B)

/* {'\\','?','?','\\','V','o','l','u','m','e','{'} */
#define __VOLUME_PATH_PREFIX_LEN	(11 * sizeof(wchar16_t))
#define __VOLUME_PATH_PREFIX_HASH	(0xFEBA8529)

/* {'\\','D','o','s','D','e','v','i','c','e','s'} */
#define __DOS_DEVICES_PREFIX_LEN	(11 * sizeof(wchar16_t))
#define __DOS_DEVICES_PREFIX_HASH	(0x565D0514)


typedef enum _nt_mount_moint_type {
	NT_MOUNT_POINT_NONE,
	NT_MOUNT_POINT_DEVICE,
	NT_MOUNT_POINT_VOLUME,
	NT_MOUNT_POINT_DIRECTORY
} nt_mount_moint_type;

/* reparse tag values */
#define NT_IO_REPARSE_TAG_RESERVED_ZERO		(0x00000000)
#define NT_IO_REPARSE_TAG_RESERVED_ONE		(0x00000001)
#define NT_IO_REPARSE_TAG_MOUNT_POINT		(0xA0000003)
#define NT_IO_REPARSE_TAG_HSM			(0xC0000004)
#define NT_IO_REPARSE_TAG_HSM2			(0x80000006)
#define NT_IO_REPARSE_TAG_DRIVER_EXTENDER	(0x80000005)
#define NT_IO_REPARSE_TAG_SIS			(0x80000007)
#define NT_IO_REPARSE_TAG_DFS			(0x8000000A)
#define NT_IO_REPARSE_TAG_DFSR			(0x80000012)
#define NT_IO_REPARSE_TAG_FILTER_MANAGER	(0x8000000B)
#define NT_IO_REPARSE_TAG_SYMLINK		(0xA000000C)


typedef struct _nt_mount_dev_name {
	uint16_t	name_length;
	wchar16_t	name[];
} nt_mount_dev_name;


typedef struct _nt_mount_mgr_mount_point {
	uint32_t 	symlink_name_offset;
	uint16_t	symlink_name_length;
	uint32_t	unique_id_offset;
	uint16_t	unique_id_length;
	uint32_t	device_name_offset;
	uint16_t	device_name_length;
} nt_mount_mgr_mount_point, nt_mount_point;


typedef struct _nt_mount_mgr_mount_point_param {
	uint32_t 	symlink_name_offset;
	uint16_t	symlink_name_length;
	uint32_t	unique_id_offset;
	uint16_t	unique_id_length;
	uint32_t	device_name_offset;
	uint16_t	device_name_length;
	uint16_t	mount_points_offset;
	wchar16_t	device_name[];
} nt_mount_mgr_mount_point_param, nt_mount_point_param;


typedef struct _nt_mount_mgr_mount_points {
	uint32_t			size;
	uint32_t			number;
	nt_mount_mgr_mount_point	mount_points[];
} nt_mount_mgr_mount_points, nt_mount_points;


typedef struct _nt_mount_point_reparse_buffer {
	uint32_t	reparse_tag;
	uint16_t	reparse_data_length;
	uint16_t	reserved;
	uint16_t	substitute_name_offset;
	uint16_t	substitute_name_length;
	uint16_t	print_name_offset;
	uint16_t	print_name_length;
	uintptr_t	path_buffer[];
} _nt_mount_point_reparse_buffer, nt_mprb;


typedef struct _nt_dos_devices_name {
	wchar16_t	dos_devices_prefix[11];
	wchar16_t	slash;
	wchar16_t	letter;
	wchar16_t	colon;
} nt_dos_devices_name;


typedef int32_t __stdcall	ntapi_tt_get_dos_drive_device_handle(
	__out	void **			hdevice,
	__in	wchar16_t *		drive_letter);


typedef int32_t __stdcall	ntapi_tt_get_dos_drive_root_handle(
	__out	void **			hroot,
	__in	wchar16_t *		drive_letter);


typedef int32_t __stdcall	ntapi_tt_get_dos_drive_device_name(
	__in	void *			hdevice		__optional,
	__in	wchar16_t *		drive_letter	__optional,
	__out	nt_mount_dev_name *	buffer,
	__in	uint32_t		buffer_size);


typedef int32_t __stdcall	ntapi_tt_get_dos_drive_mount_points(
	__in	void *			hdevice		__optional,
	__in	wchar16_t *		drive_letter	__optional,
	__in	nt_mount_dev_name *	dev_name	__optional,
	__out	void *			buffer,
	__in	uint32_t		buffer_size);


typedef int32_t __stdcall	ntapi_tt_dev_mount_points_to_statfs(
	__in		nt_mount_points *	mount_points,
	__in_out	nt_statfs *		statfs);


typedef int32_t __stdcall	ntapi_tt_get_dos_drive_letter_from_device(
	__in	void *			hdevice		__optional,
	__out	wchar16_t *		drive_letter,
	__in	nt_mount_dev_name *	dev_name	__optional,
	__out	void *			buffer,
	__in	uint32_t		buffer_size);

#endif
