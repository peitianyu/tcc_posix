/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_device.h"
#include "psx.h"

typedef enum __dos_drive_handle_type {
	__DOS_DRIVE_DEVICE_HANDLE,
	__DOS_DRIVE_ROOT_HANDLE
} _dos_drive_handle_type;

static int32_t __fastcall	__psx_get_dos_drive_device_or_root_handle(
	void **			hdrive,
	unsigned char *		drive_letter,
	_dos_drive_handle_type	handle_type)
{
	int32_t			status;
	wchar16_t		wletter;
	int			index;
	struct __dos_drive *	drive;
	void *			hfile;

	*hdrive = (void *)0;
	wletter = *drive_letter;

	if ((wletter >= 'A') && (wletter <= 'Z'))
		index = wletter - 'A';
	else if ((wletter >= 'a') && (wletter <= 'z'))
		index = wletter - 'a';
	else
		return NT_STATUS_INVALID_PARAMETER;

	drive = &(dos_drives[index]);

	/* handle type */
	switch (handle_type) {
		case __DOS_DRIVE_DEVICE_HANDLE:
			if (drive->hdevice) {
				*hdrive = drive->hdevice;
				status = NT_STATUS_SUCCESS;
			} else {
				status = __ntapi->tt_get_dos_drive_device_handle(
					&hfile,
					&wletter);

				if (status == NT_STATUS_SUCCESS)
					drive->hdevice = *hdrive = hfile;
			}
			break;

		case __DOS_DRIVE_ROOT_HANDLE:
			if (drive->hroot) {
				*hdrive = drive->hroot;
				status = NT_STATUS_SUCCESS;
			} else {
				status = __ntapi->tt_get_dos_drive_root_handle(
					&hfile,
					&wletter);

				if (status == NT_STATUS_SUCCESS)
					drive->hroot = *hdrive = hfile;
			}
			break;

		default:
			status = NT_STATUS_INVALID_PARAMETER;
			break;
	}

	return status;
}


int32_t __fastcall	__psx_get_dos_drive_device_handle(
	void **		hdrive,
	unsigned char *	drive_letter)
{
	return __psx_get_dos_drive_device_or_root_handle(
		hdrive,
		drive_letter,
		__DOS_DRIVE_DEVICE_HANDLE);
}


int32_t __fastcall	__psx_get_dos_drive_root_handle(
	void **		hroot,
	unsigned char *	drive_letter)
{
	return __psx_get_dos_drive_device_or_root_handle(
		hroot,
		drive_letter,
		__DOS_DRIVE_ROOT_HANDLE);
}
