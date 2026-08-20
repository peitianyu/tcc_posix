#ifndef _PSX_RESOURCE_H_
#define _PSX_RESOURCE_H_

enum __psx_resource_type {
	RLIMIT_CPU,
	RLIMIT_FSIZE,
	RLIMIT_DATA,
	RLIMIT_STACK,
	RLIMIT_CORE,
	RLIMIT_RSS,
	RLIMIT_NPROC,
	RLIMIT_NOFILE,
	RLIMIT_MEMLOCK,
	RLIMIT_AS,
	RLIMIT_LOCKS,
	RLIMIT_SIGPENDING,
	RLIMIT_MSGQUEUE,
	RLIMIT_NICE,
	RLIMIT_RTPRIO,
	RLIMIT_NLIMITS
};

typedef unsigned long long rlim_t;

struct __rlimit {
	rlim_t	rlim_cur;
	rlim_t	rlim_max;
};

struct __rusage
{
	struct timeval	ru_utime;
	struct timeval	ru_stime;

	intptr_t	ru_maxrss;
	intptr_t	ru_ixrss;
	intptr_t	ru_idrss;
	intptr_t	ru_isrss;
	intptr_t	ru_minflt;
	intptr_t	ru_majflt;
	intptr_t	ru_nswap;
	intptr_t	ru_inblock;
	intptr_t	ru_oublock;
	intptr_t	ru_msgsnd;
	intptr_t	ru_msgrcv;
	intptr_t	ru_nsignals;
	intptr_t	ru_nvcsw;
	intptr_t	ru_nivcsw;

	intptr_t	ru_reserved[16];
};

#endif
