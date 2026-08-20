#ifndef _PSX_SYSCALLS_H
#define _PSX_SYSCALLS_H

#if defined(__X86_MODEL)
#include "bits/i386/psx_syscalls.h"
#elif defined(__X86_64_MODEL)
#include "bits/x86_64/psx_syscalls.h"
#endif

/* (add some elegance while using matching identifiers) */
#undef  SYS_getdents
#undef  SYS_prlimit
#undef  SYS_fstatat
#undef  SYS_pread
#undef  SYS_pwrite

#define SYS_getdents	SYS_getdents64
#define SYS_prlimit	SYS_prlimit64
#define SYS_fstatat	SYS_newfstatat
#define SYS_pread	SYS_pread64
#define SYS_pwrite	SYS_pwrite64

#endif
