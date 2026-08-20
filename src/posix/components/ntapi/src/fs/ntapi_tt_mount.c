/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <ntapi/nt_object.h>
#include <ntapi/nt_file.h>
#include <ntapi/nt_mount.h>
#include <ntapi/nt_atomic.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

typedef enum __dos_drive_handle_type {
	__DOS_DRIVE_DEVICE_HANDLE,
	__DOS_DRIVE_ROOT_HANDLE
} _dos_drive_handle_type;

typedef struct __dos_name_buffer {
	wchar16_t	global_prefix[4];
	wchar16_t	dos_letter;
	wchar16_t	colon;
	wchar16_t	root;
	wchar16_t	null_termination;
} _dos_name_buffer;


static int32_t __stdcall __tt_connect_to_mount_point_manager(void)
{
	int32_t			status;

	void *			hdev;
	void *			hdev_prev;
	nt_oa			oa;
	nt_iosb			iosb;
	nt_unicode_string	dev_name;
	uint16_t		dev_name_buffer[] = {
				'\\','?','?','\\',
				'M','o','u','n','t',
				'P','o','i','n','t',
				'M','a','n','a','g','e','r',0};

	dev_name.strlen = sizeof(wchar16_t) * (4+5+5+7);
	dev_name.maxlen = 0;
	dev_name.buffer = dev_name_buffer;

	oa.len		= sizeof(nt_oa);
	oa.root_dir	= (void *)0;
	oa.obj_name	= &dev_name;
	oa.obj_attr	= NT_OBJ_CASE_INSENSITIVE;
	oa.sec_desc	= (nt_sd *)0;
	oa.sec_qos	= (nt_sqos *)0;

	status = __ntapi->zw_create_file(
		&hdev,
		NT_SEC_SYNCHRONIZE | NT_FILE_READ_ATTRIBUTES,
		&oa,
		&iosb,
		0,
		NT_FILE_ATTRIBUTE_NORMAL,
		NT_FILE_SHARE_READ | NT_FILE_SHARE_WRITE,
		NT_FILE_OPEN,
		NT_FILE_NON_DIRECTORY_FILE | NT_FILE_SYNCHRONOUS_IO_NONALERT,
		(void *)0,
		0);

	if (status != NT_STATUS_SUCCESS)
		return status;

	hdev_prev = (void *)at_locked_cas(
		(intptr_t *)&__ntapi_internals()->hdev_mount_point_mgr,
		0,(intptr_t)hdev);

	if (hdev_prev)
		__ntapi->zw_close(hdev);

	return status;
}


static int32_t __stdcall __tt_get_dos_drive_device_or_root_handle(
	__out	void **			hdrive,
	__in	wchar16_t *		drive_letter,
	__in	_dos_drive_handle_type	handle_type)
{
	#define		__common_mode	(NT_FILE_SYNCHRONOUS_IO_ALERT)
	#define		__common_access	(NT_SEC_SYNCHRONIZE \
					| NT_FILE_READ_ATTRIBUTES)

	int32_t			status;

	nt_oa			oa;
	nt_iosb			iosb;
	uint32_t		open_flags;
	uint32_t		access_flags;
	nt_unicode_string	dos_name;
	_dos_name_buffer	dos_name_buffer = {
					{'\\','?','?','\\'},
					'_',':',0,0};

	if (!hdrive || !drive_letter)
		return NT_STATUS_INVALID_PARAMETER;

	if ((*drive_letter>='A') && (*drive_letter<='Z'))
		dos_name_buffer.dos_letter = *drive_letter;
	else if ((*drive_letter>='a') && (*drive_letter<='z'))
		dos_name_buffer.dos_letter = *drive_letter + 'A' - 'a';
	else
		return NT_STATUS_INVALID_PARAMETER_2;

	dos_name.strlen = ((size_t)(&((_dos_name_buffer *)0)->root));
	dos_name.maxlen = 0;
	dos_name.buffer = &(dos_name_buffer.global_prefix[0]);

	switch (handle_type) {
		case __DOS_DRIVE_DEVICE_HANDLE:
			open_flags	= __common_mode;
			access_flags	= __common_access;
			break;

		case __DOS_DRIVE_ROOT_HANDLE:
			open_flags	= __common_mode   | NT_FILE_DIRECTORY_FILE;
			access_flags	= __common_access | NT_FILE_READ_ACCESS;
			dos_name_buffer.root = '\\';
			dos_name.strlen += sizeof(wchar16_t);
			break;
		default:
			open_flags	= 0;
			access_flags	= 0;
			break;
	}

	oa.len		= sizeof(nt_oa);
	oa.root_dir	= (void *)0;
	oa.obj_name	= &dos_name;
	oa.obj_attr	= NT_OBJ_INHERIT;
	oa.sec_desc	= (nt_sd *)0;
	oa.sec_qos	= (nt_sqos *)0;

	status = __ntapi->zw_open_file(
		hdrive,
		access_flags,
		&oa,
		&iosb,
		NT_FILE_SHARE_READ | NT_FILE_SHARE_WRITE,
		open_flags);

	return status;
}


int32_t __stdcall	__ntapi_tt_get_dos_drive_device_handle(
	__out	void **			hdevice,
	__in	wchar16_t *		drive_letter)
{
	return __tt_get_dos_drive_device_or_root_handle(
		hdevice,
		drive_letter,
		__DOS_DRIVE_DEVICE_HANDLE);
}


int32_t __stdcall	__ntapi_tt_get_dos_drive_root_handle(
	__out	void **			hroot,
	__in	wchar16_t *		drive_letter)
{
	return __tt_get_dos_drive_device_or_root_handle(
		hroot,
		drive_letter,
		__DOS_DRIVE_ROOT_HANDLE);
}



int32_t __stdcall	__ntapi_tt_get_dos_drive_device_name(
	__in	void *			hdevice		__optional,
	__in	wchar16_t *		drive_letter	__optional,
	__out	nt_mount_dev_name *	buffer,
	__in	uint32_t		buffer_size)
{
	int32_t			status;
	nt_iosb			iosb;

	if (!hdevice && (status = __tt_get_dos_drive_device_or_root_handle(
			&hdevice,
			drive_letter,
			__DOS_DRIVE_DEVICE_HANDLE)))
		return status;

	return __ntapi->zw_device_io_control_file(
		hdevice,
		(void *)0,
		(nt_io_apc_routine *)0,
		(void *)0,
		&iosb,
		NT_IOCTL_MOUNTDEV_QUERY_DEVICE_NAME,
		(void *)0,
		0,
		buffer,
		buffer_size);
}


int32_t __stdcall	__ntapi_tt_get_dos_drive_mount_points(
	__in	void *			hdevice		__optional,
	__in	wchar16_t *		drive_letter	__optional,
	__in	nt_mount_dev_name *	dev_name	__optional,
	__out	void *			buffer,
	__in	uint32_t		buffer_size)
{
	int32_t			status;
	nt_iosb			iosb;
	wchar16_t		dev_name_buffer[64];
	nt_mount_point_param *	dev_mount_point;
	nt_mount_points *	dev_mount_points;
	uintptr_t		addr;

	if (!dev_name) {
		dev_name = (nt_mount_dev_name *)&dev_name_buffer;
		if ((status = __ntapi_tt_get_dos_drive_device_name(
				hdevice,
				drive_letter,
				dev_name,
				sizeof(dev_name_buffer))))
			return status;
	}

	if (buffer_size < sizeof(nt_mount_mgr_mount_point) \
				+ sizeof(nt_mount_dev_name) \
				+ sizeof(dev_name->name_length))
		return NT_STATUS_BUFFER_TOO_SMALL;

	dev_mount_point = (nt_mount_point_param *)buffer;
	dev_mount_point->symlink_name_offset = 0;
	dev_mount_point->symlink_name_length = 0;
	dev_mount_point->unique_id_offset    = 0;
	dev_mount_point->unique_id_length    = 0;
	dev_mount_point->device_name_offset  = ((size_t)(&((nt_mount_point_param *)0)->device_name));
	dev_mount_point->device_name_length  = dev_name->name_length;
	dev_mount_point->mount_points_offset = 0;

	__ntapi->tt_memcpy_utf16(
		dev_mount_point->device_name,
		dev_name->name,
		dev_name->name_length);

	addr = (uintptr_t)(dev_mount_point->device_name) + dev_name->name_length;
	addr += sizeof(uintptr_t) - 1;
	addr /= sizeof(uintptr_t);
	addr *= sizeof(uintptr_t);
	dev_mount_points = (nt_mount_points *)addr;


	if (!__ntapi_internals()->hdev_mount_point_mgr)
		status = __tt_connect_to_mount_point_manager();

	if (!__ntapi_internals()->hdev_mount_point_mgr)
		return status;


	status = __ntapi->zw_device_io_control_file(
		__ntapi_internals()->hdev_mount_point_mgr,
		(void *)0,
		(nt_io_apc_routine *)0,
		(void *)0,
		&iosb,
		NT_IOCTL_MOUNTMGR_QUERY_POINTS,
		dev_mount_point,
		(uint32_t)(uintptr_t)&(((nt_mount_point_param *)0)->device_name) + dev_name->name_length,
		dev_mount_points,
		(uint32_t)((uintptr_t)buffer + buffer_size - addr));

	dev_mount_point->mount_points_offset = (uint16_t)((uintptr_t)addr - (uintptr_t)buffer);

	return status;
}


int32_t __stdcall	__ntapi_tt_dev_mount_points_to_statfs(
	__in		nt_mount_points *	mount_points,
	__in_out	nt_statfs *		statfs)
{
	int32_t				status;
	uint32_t			hash;
	uint32_t			i;

	nt_mount_mgr_mount_point *	mount_point;
	char *				symlink;

	mount_point = mount_points->mount_points;
	statfs->nt_drive_letter = 0;


	for (i = 0; i < mount_points->number; i++, mount_point++) {
		symlink = (char *)mount_points + mount_point->symlink_name_offset;

		/* both prefixes of interest happen to be of the same length */
		hash = __ntapi->tt_buffer_crc32(
			0, symlink, __DOS_DEVICES_PREFIX_LEN);

		if (hash == __DOS_DEVICES_PREFIX_HASH)
			statfs->nt_drive_letter = ((nt_dos_devices_name *)(symlink))->letter;
		else if (hash == __VOLUME_PATH_PREFIX_HASH) {
			status = __ntapi_tt_utf16_string_to_guid(
				(nt_guid_str_utf16 *)(symlink \
					+ __VOLUME_PATH_PREFIX_LEN \
					- sizeof(wchar16_t)),
				&statfs->nt_volume_guid);

			if (status != NT_STATUS_SUCCESS)
				return status;
		}
	}

	return 0;
}


int32_t __stdcall	__ntapi_tt_get_dos_drive_letter_from_device(
	__in	void *			hdevice		__optional,
	__out	wchar16_t *		drive_letter,
	__in	nt_mount_dev_name *	dev_name	__optional,
	__out	void *			buffer,
	__in	uint32_t		buffer_size)
{
	int32_t			status;
	wchar16_t		dev_name_buffer[128];
	nt_statfs		statfs;
	uint32_t		offset;
	nt_mount_points *	mnt_points;

	if (!dev_name) {
		dev_name = (nt_mount_dev_name *)&dev_name_buffer;
		status = __ntapi_tt_get_dos_drive_device_name(
			hdevice,
			(wchar16_t *)0,
			dev_name,
			sizeof(dev_name_buffer));

		if (status != NT_STATUS_SUCCESS)
			return status;
	}


	offset = ((nt_mount_point_param *)buffer)->mount_points_offset;
	mnt_points = (nt_mount_points *)((uintptr_t)buffer + offset);

	status = __ntapi_tt_dev_mount_points_to_statfs(
		mnt_points,
		&statfs);

	if (status != NT_STATUS_SUCCESS)
		return status;

	*drive_letter = statfs.nt_drive_letter;

	return status;
}
