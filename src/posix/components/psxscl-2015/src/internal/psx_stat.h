#ifndef _PSX_STAT_H_
#define _PSX_STAT_H_

#include "psx_systypes.h"
#include "psx_ofd.h"

#define S_IFDIR		0040000
#define S_IFCHR		0020000
#define S_IFBLK		0060000
#define S_IFREG		0100000
#define S_IFIFO		0010000
#define S_IFLNK		0120000
#define S_IFSOCK	0140000

#define S_ISUID		04000
#define S_ISGID		02000
#define S_ISVTX		01000
#define S_IRUSR		0400
#define S_IWUSR		0200
#define S_IXUSR		0100
#define S_IRWXU		0700
#define S_IRGRP		0040
#define S_IWGRP		0020
#define S_IXGRP		0010
#define S_IRWXG		0070
#define S_IROTH		0004
#define S_IWOTH		0002
#define S_IXOTH		0001
#define S_IRWXO		0007

struct __stat {
	dev_t		st_dev;
	ino_t		st_ino;
	nlink_t		st_nlink;

	mode_t		st_mode;
	uid_t		st_uid;
	gid_t		st_gid;
	uint32_t	__labi;
	dev_t		st_rdev;
	off_t		st_size;
	blksize_t	st_blksize;
	blkcnt_t	st_blocks;

	struct timespec st_atim;
	struct timespec st_mtim;
	struct timespec st_ctim;
	struct timespec st_birthtime;
};

/* 卷信息 (statfs/fstatfs), 布局对齐 musl x86_64 struct statfs
   (f_type..f_ffree 各 u64, f_fsid[2], 余下各 u64) */
struct __statfs {
	uint64_t	f_type;
	uint64_t	f_bsize;
	uint64_t	f_blocks;
	uint64_t	f_bfree;
	uint64_t	f_bavail;
	uint64_t	f_files;
	uint64_t	f_ffree;
	int32_t		f_fsid[2];
	uint64_t	f_namelen;
	uint64_t	f_frsize;
	uint64_t	f_flags;
	uint64_t	f_spare[4];
};

struct __psx_tlca;

int32_t	__fastcall __psx_stat(struct __psx_tlca *, struct __ofd *, struct __stat *);

#endif
