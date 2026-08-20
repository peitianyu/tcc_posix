#ifndef _NT_STATFS_H_
#define _NT_STATFS_H_

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_object.h>

/* ntapi_tt_statfs info flags bits */
#define NT_STATFS_DEFAULT		(0x00000000)
#define NT_STATFS_COMMON		(0x00000001)
#define NT_STATFS_DEV_NAME_COPY		(0x00000002)
#define NT_STATFS_VOLUME_GUID		(0x00000004)
#define NT_STATFS_DOS_DRIVE_LETTER	NT_STATFS_VOLUME_GUID
#define NT_STATFS_NEW_HANDLE		(0x80000000)


#define NT_FS_TYPE_FAT16_NAME_HASH	(0x00000000)
#define NT_FS_TYPE_FAT32_NAME_HASH	(0x00000001)
#define NT_FS_TYPE_HPFS_NAME_HASH	(0x00000002)
#define NT_FS_TYPE_MSDOS_NAME_HASH	(0x00000003)
#define NT_FS_TYPE_NTFS_NAME_HASH	(0xbfbc5fdb)
#define NT_FS_TYPE_SMB_NAME_HASH	(0x00000004)
#define NT_FS_TYPE_UDF_NAME_HASH	(0x00000005)

typedef struct _nt_fsid_t {
	uint32_t	__val[2];
} nt_fsid_t;

typedef struct _nt_statfs {
	uintptr_t	f_type;
	uintptr_t	f_bsize;
	uint64_t	f_blocks;
	uint64_t	f_bfree;
	uint64_t	f_bavail;
	uint64_t	f_files;
	uint64_t	f_ffree;
	nt_fsid_t	f_fsid;
	uintptr_t	f_namelen;
	uintptr_t	f_frsize;
	uintptr_t	f_flags;
	uintptr_t	f_spare[4];
	uint32_t	nt_fstype_hash;
	uint32_t	nt_attr;
	uint32_t	nt_control_flags;
	wchar16_t	nt_drive_letter;
	wchar16_t	nt_padding;
	nt_guid		nt_volume_guid;
	void *		hfile;
	uint32_t	flags_in;
	uint32_t	flags_out;
	uint16_t	record_name_strlen;
	uint16_t	dev_name_strlen;
	uint16_t	dev_name_maxlen;
	uint32_t	dev_name_hash;
	wchar16_t	dev_name[];
} nt_statfs;


typedef int32_t __stdcall ntapi_tt_statfs(
	__in	void *			hfile	__optional,
	__in	void *			hroot	__optional,
	__in	nt_unicode_string *	path	__optional,
	__out	nt_statfs *		statfs,
	__out	uintptr_t *		buffer,
	__in	uint32_t		buffer_size,
	__in	uint32_t		flags);


#endif
