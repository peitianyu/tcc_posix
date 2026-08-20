#ifndef _PSX_DEVICE_H_
#define _PSX_DEVICE_H_

#include <ntapi/ntapi.h>

struct __dos_drive {
	void *	hdevice;
	void *	hroot;
};

int32_t __fastcall	__psx_get_dos_drive_device_handle(
	void **		hdrive,
	unsigned char *	drive_letter);

int32_t __fastcall	__psx_get_dos_drive_root_handle(
	void **		hroot,
	unsigned char *	drive_letter);

#endif
