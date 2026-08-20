#ifndef _PSX_SYSTYPES_H_
#define _PSX_SYSTYPES_H_

#include <psxtypes/psxtypes.h>

#define	false		0
#define	true		1

typedef	int64_t		off_t;
typedef	int32_t		pid_t;
typedef uint32_t	uid_t;
typedef uint32_t	gid_t;

typedef uint32_t	mode_t;
typedef uint64_t	ino_t;
typedef uint64_t	dev_t;
typedef uintptr_t	nlink_t;
typedef intptr_t	blksize_t;
typedef int64_t		blkcnt_t;

typedef uint32_t	id_t;
typedef int32_t		key_t;
typedef uint32_t	useconds_t;

typedef intptr_t	clock_t;
typedef int64_t		time_t;
typedef intptr_t	suseconds_t;

typedef int32_t		clockid_t;
typedef intptr_t	clock_t;

struct timespec {
	time_t		tv_sec;
	intptr_t	tv_nsec;
};

struct timeval {
	intptr_t tv_sec;
	intptr_t tv_usec;
};

struct itimerval {
	struct timeval	it_interval;
	struct timeval	it_value;
};

struct iovec {
     void *	iov_base;
     size_t	iov_len;
 };

struct sched_param {
	int x;
};

#endif
