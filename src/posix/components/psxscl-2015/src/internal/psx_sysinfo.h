#ifndef _PSX_SYSINFO_H_
#define _PSX_SYSINFO_H_

#include "psx_systypes.h"

struct __sysinfo {
	uintptr_t	uptime;
	uintptr_t	loads[3];
	uintptr_t	totalram;
	uintptr_t	freeram;
	uintptr_t	sharedram;
	uintptr_t	bufferram;
	uintptr_t	totalswap;
	uintptr_t	freeswap;
	uint16_t	procs;
	uint16_t	pad;
	uintptr_t	totalhigh;
	uintptr_t	freehigh;
	unsigned 	mem_unit;
	char		__labi[256];
};


struct __utsname {
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
	char domainname[65];
};

#endif
