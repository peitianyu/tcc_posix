/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_fsctl.h>
#include <ntapi/nt_mount.h>
#include <ntapi/nt_statfs.h>
#include "ntapi_impl.h"

int32_t __stdcall __ntapi_tt_statfs(
	__in	void *			hfile,
	__in	void *			hroot	__optional,
	__in	nt_unicode_string *	path,
	__out	nt_statfs *		statfs,
	__out	uintptr_t *		buffer,
	__in	uint32_t		buffer_size,
	__in	uint32_t		flags)
{
	int32_t			status;
	nt_oa			oa;
	nt_iosb			iosb;
	nt_unicode_string *	sdev;
	uint32_t		hash;
	wchar16_t *		wch;
	wchar16_t *		wch_mark;
	uint32_t		offset;
	void *			mnt_points_buffer;
	nt_mount_points *	mnt_points;
	nt_fsai *		fsai;
	nt_fsfsi *		fsfsi;
	uint32_t *		fsid;
	uint64_t *		pguid;

	/* validation */
	if (!hfile && !path)
		return NT_STATUS_INVALID_HANDLE;

	/* hfile */
	if (!hfile) {
		/* oa */
		oa.len = sizeof(nt_oa);
		oa.root_dir = hroot;
		oa.obj_name = path;
		oa.obj_attr = 0;
		oa.sec_desc = 0;
		oa.sec_qos = 0;

		/* open file/folder */
		status = __ntapi->zw_open_file(
			&hfile,
			NT_SEC_SYNCHRONIZE | NT_FILE_READ_ATTRIBUTES | NT_FILE_READ_ACCESS,
			&oa,
			&iosb,
			NT_FILE_SHARE_READ | NT_FILE_SHARE_WRITE,
			NT_FILE_SYNCHRONOUS_IO_ALERT);

		if (status != NT_STATUS_SUCCESS)
			return status;

		statfs->flags_out = NT_STATFS_NEW_HANDLE;
	}

	statfs->hfile    = hfile;
	statfs->flags_in = flags;

	/* maximum component length, file system type */
	status = __ntapi->zw_query_volume_information_file(
		hfile,
		&iosb,
		buffer,
		buffer_size,
		NT_FILE_FS_ATTRIBUTE_INFORMATION);

	if (status != NT_STATUS_SUCCESS)
		return status;

	fsai = (nt_fsai *)buffer;
	statfs->f_type = 0;
	statfs->f_namelen = fsai->maximum_component_name_length;
	statfs->nt_fstype_hash = __ntapi->tt_buffer_crc32(
		0,
		&fsai->file_system_name,
		fsai->file_system_name_length);

	/* max files per volume */
	switch (statfs->nt_fstype_hash) {
		case NT_FS_TYPE_HPFS_NAME_HASH:
		case NT_FS_TYPE_NTFS_NAME_HASH:
		case NT_FS_TYPE_SMB_NAME_HASH:
		case NT_FS_TYPE_UDF_NAME_HASH:
			statfs->f_files = 0xFFFFFFFF;
			break;

		case NT_FS_TYPE_FAT16_NAME_HASH:
			statfs->f_files = 0x10000;
			break;

		case NT_FS_TYPE_FAT32_NAME_HASH:
			statfs->f_files = 0x400000;
			break;

		default:
			/* pretend there is no limitation */
			statfs->f_files = (-1);
			break;
	}

	/* number of free file records on volume */
	/* (skip, yet indicate that the volume is not empty) */
	statfs->f_ffree = (size_t)statfs->f_files >> 4 << 3;

	/* file system size information */
	status = __ntapi->zw_query_volume_information_file(
		hfile,
		&iosb,
		buffer,
		buffer_size,
		NT_FILE_FS_FULL_SIZE_INFORMATION);

	if (status != NT_STATUS_SUCCESS)
		return status;

	fsfsi = (nt_fsfsi *)buffer;
	statfs->f_blocks = fsfsi->total_allocation_units.quad;
	statfs->f_bfree  = fsfsi->actual_available_allocation_units.quad;
	statfs->f_bavail = fsfsi->caller_available_allocation_units.quad;
	statfs->f_bsize  = fsfsi->sectors_per_allocation_unit * fsfsi->bytes_per_sector;
	statfs->f_frsize = fsfsi->bytes_per_sector;

	/* TODO: consolidate with istat */
	/* system-unique device name */
	iosb.info = 0;
	status = __ntapi->zw_query_object(
		hfile,
		NT_OBJECT_NAME_INFORMATION,
		buffer,
		buffer_size,
		(uint32_t *)&iosb.info);

	if (status != NT_STATUS_SUCCESS)
		return status;

	sdev = (nt_unicode_string *)buffer;

	if (sdev->strlen < __DEVICE_PATH_PREFIX_LEN)
		return NT_STATUS_INVALID_HANDLE;

	hash = __ntapi->tt_buffer_crc32(
		0,
		sdev->buffer,
		__DEVICE_PATH_PREFIX_LEN);

	if (hash != __DEVICE_PATH_PREFIX_HASH)
		return NT_STATUS_INVALID_HANDLE;

	wch_mark = sdev->buffer + __DEVICE_PATH_PREFIX_LEN/sizeof(wchar16_t);
	wch = wch_mark;
	while (*wch != '\\') wch++;
	statfs->dev_name_strlen = (uint16_t)((wch - sdev->buffer) * sizeof(uint16_t));
	statfs->record_name_strlen = sdev->strlen - statfs->dev_name_strlen;

	statfs->dev_name_hash = __ntapi->tt_buffer_crc32(
		hash,wch_mark,
		sizeof(wchar16_t) * (wch - wch_mark));

	/* copy device name (optional, no failure) */
	if (flags & NT_STATFS_DEV_NAME_COPY) {
		if (statfs->dev_name_maxlen < sdev->strlen)
			*statfs->dev_name = 0;
		else
			__ntapi->tt_memcpy_utf16(
				(wchar16_t *)statfs->dev_name,
				(wchar16_t *)sdev->buffer,
				sdev->strlen);
	} else
		*statfs->dev_name = 0;

	/* f_fsid: hash of the system-unique device name */
	/* (never use the volume serial number) */
	fsid = (uint32_t *)&(statfs->f_fsid);
	fsid[0] = statfs->dev_name_hash;
	fsid[1] = 0;

	/* f_flags, nt_attr, nt_control_flags (todo?) */
	statfs->f_flags = 0;
	statfs->nt_attr = 0;
	statfs->nt_control_flags = 0;
	statfs->nt_padding = 0;

	if (!(flags & NT_STATFS_VOLUME_GUID)) {
		statfs->nt_drive_letter = 0;
		pguid = (uint64_t *)&(statfs->nt_volume_guid);
		*pguid = 0; *(++pguid) = 0;
		return NT_STATUS_SUCCESS;
	}

	/* dos device letter and volume guid */
	wch = (wchar16_t *)sdev->buffer;
	mnt_points_buffer = (void *)((uintptr_t)wch + statfs->dev_name_strlen);

	*(--wch) = statfs->dev_name_strlen;
	offset   = sizeof(nt_unicode_string) + statfs->dev_name_strlen;

	status = __ntapi->tt_get_dos_drive_mount_points(
		(void *)0,
		(wchar16_t *)0,
		(nt_mount_dev_name *)wch,
		mnt_points_buffer,
		buffer_size - offset);

	if (status != NT_STATUS_SUCCESS)
		return status;

	offset = ((nt_mount_point_param *)mnt_points_buffer)->mount_points_offset;
	mnt_points = (nt_mount_points *)((uintptr_t)mnt_points_buffer + offset);

	status = __ntapi->tt_dev_mount_points_to_statfs(
		mnt_points,
		statfs);

	return status;
}
