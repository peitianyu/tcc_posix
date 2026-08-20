#ifndef _PSX_STRACE_H_
#define _PSX_STRACE_H_

/*****************************************/
/* POOR MAN'S STRACE -- WORK IN PROGRESS */
/*****************************************/

#include "psx.h"

enum __strace_sysids {
	__STRACE_SIGNAL_TRAMPOLINE_PROLOG,
	__STRACE_SIGNAL_TRAMPOLINE_EPILOG,
	__STRACE_SIGNAL_HANDLER_INVOCATION,
	__STRACE_BRK,
	__STRACE_MADVISE,
	__STRACE_DUP,
	__STRACE_DUP2,
	__STRACE_DUP3,
	__STRACE_FCNTL,
	__STRACE_CLOSE,
	__STRACE_CREAT,
	__STRACE_OPEN,
	__STRACE_OPENAT,
	__STRACE_ACCESS,
	__STRACE_FACCESSAT,
	__STRACE_CHDIR,
	__STRACE_FCHDIR,
	__STRACE_CHMOD,
	__STRACE_FCHMODAT,
	__STRACE_GETCWD,
	__STRACE_GETDENTS,
	__STRACE_FSTAT,
	__STRACE_FSTATAT,
	__STRACE_LSTAT,
	__STRACE_STAT,
	__STRACE_MKDIR,
	__STRACE_MKDIRAT,
	__STRACE_READLINK,
	__STRACE_READLINKAT,
	__STRACE_RENAME,
	__STRACE_RENAMEAT,
	__STRACE_RMDIR,
	__STRACE_UNLINK,
	__STRACE_UNLINKAT,
	__STRACE_IOCTL,
	__STRACE_PIPE,
	__STRACE_CLOCK_GETRES,
	__STRACE_CLOCK_GETTIME,
	__STRACE_CLOCK_SETTIME,
	__STRACE_GETTIMEOFDAY,
	__STRACE_SCHED_SETSCHEDULER,
	__STRACE_SYSINFO,
	__STRACE_UNAME,
	__STRACE_MMAP,
	__STRACE_MREMAP,
	__STRACE_MUNMAP,
	__STRACE_MOUNT,
	__STRACE_EXECVE,
	__STRACE_EXIT,
	__STRACE_EXIT_GROUP,
	__STRACE_FORK,
	__STRACE_TIMES,
	__STRACE_GETRLIMIT,
	__STRACE_GETRUSAGE,
	__STRACE_PRLIMIT,
	__STRACE_SETRLIMIT,
	__STRACE_CHROOT,
	__STRACE_GETPID,
	__STRACE_GETPPID,
	__STRACE_GETPGID,
	__STRACE_GETPGRP,
	__STRACE_SETPGID,
	__STRACE_WAIT4,
	__STRACE_WAITID,
	__STRACE_RT_SIGACTION,
	__STRACE_RT_SIGPROCMASK,
	__STRACE_RT_GETITIMER,
	__STRACE_RT_SETITIMER,
	__STRACE_ACCEPT,
	__STRACE_BIND,
	__STRACE_CONNECT,
	__STRACE_GETPEERNAME,
	__STRACE_GETSOCKNAME,
	__STRACE_GETSOCKOPT,
	__STRACE_LISTEN,
	__STRACE_RECVFROM,
	__STRACE_RECVMSG,
	__STRACE_SENDMSG,
	__STRACE_SENDTO,
	__STRACE_SETSOCKOPT,
	__STRACE_SHUTDOWN,
	__STRACE_SOCKET,
	__STRACE_SOCKETPAIR,
	__STRACE_FSYNC,
	__STRACE_LSEEK,
	__STRACE_PREAD,
	__STRACE_PWRITE,
	__STRACE_READ,
	__STRACE_READV,
	__STRACE_WRITE,
	__STRACE_WRITEV,
	__STRACE_CLONE,
	__STRACE_GETTID,
	__STRACE_SET_TID_ADDRESS,
	__STRACE_GETEGID,
	__STRACE_GETEUID,
	__STRACE_GETGID,
	__STRACE_GETUID,
	__STRACE_SETGID,
	__STRACE_SETUID,
};

#define __STRACE_INTERFACE_NAMES	\
	"[signal_trampoline_prolog]",	\
	"[signal_trampoline_epilog]",	\
	"[signal_handler_invocation]",	\
	"brk",				\
	"madvise",			\
	"dup",				\
	"dup2",				\
	"dup3",				\
	"fcntl",			\
	"close",			\
	"creat",			\
	"open",				\
	"openat",			\
	"access",			\
	"faccessat",			\
	"chdir",			\
	"fchdir",			\
	"chmod",			\
	"fchmodat",			\
	"getcwd",			\
	"getdents",			\
	"fstat",			\
	"fstatat",			\
	"lstat",			\
	"stat",				\
	"mkdir",			\
	"mkdirat",			\
	"readlink",			\
	"readlinkat",			\
	"rename",			\
	"renameat",			\
	"rmdir",			\
	"unlink",			\
	"unlinkat",			\
	"ioctl",			\
	"pipe",				\
	"clock_getres",			\
	"clock_gettime",		\
	"clock_settime",		\
	"gettimeofday",			\
	"sched_setscheduler",		\
	"sysinfo",			\
	"uname",			\
	"mmap",				\
	"mremap",			\
	"munmap",			\
	"mount",			\
	"execve",			\
	"exit",				\
	"exit_group",			\
	"fork",				\
	"times",			\
	"getrlimit",			\
	"getrusage",			\
	"prlimit",			\
	"setrlimit",			\
	"chroot",			\
	"getpid",			\
	"getppid",			\
	"getpgid",			\
	"getpgrp",			\
	"setpgid",			\
	"wait4",			\
	"waitid",			\
	"rt_sigaction",			\
	"rt_sigprocmask",		\
	"getitimer",			\
	"setitimer",			\
	"accept",			\
	"bind",				\
	"connect",			\
	"getpeername",			\
	"getsockname",			\
	"getsockopt",			\
	"listen",			\
	"recvfrom",			\
	"recvmsg",			\
	"sendmsg",			\
	"sendto",			\
	"setsockopt",			\
	"shutdown",			\
	"socket",			\
	"socketpair",			\
	"fsync",			\
	"lseek",			\
	"pread",			\
	"pwrite",			\
	"read",				\
	"readv",			\
	"write",			\
	"writev",			\
	"clone",			\
	"gettid",			\
	"set_tid_address",		\
	"getegid",			\
	"geteuid",			\
	"getgid",			\
	"getuid",			\
	"setgid",			\
	"setuid"

#define __strace_interface(routine) \
	__sys_routine(routine) __strace_##routine

/* brk */
__strace_interface(brk);
__strace_interface(madvise);

/* fcntl */
__strace_interface(dup);
__strace_interface(dup2);
__strace_interface(dup3);
__strace_interface(fcntl);

/* fd,ofd */
__strace_interface(close);
__strace_interface(creat);
__strace_interface(open);
__strace_interface(openat);

/* fs */
__strace_interface(access);
__strace_interface(faccessat);
__strace_interface(chdir);
__strace_interface(fchdir);
__strace_interface(chmod);
__strace_interface(fchmodat);
__strace_interface(getcwd);
__strace_interface(getdents);
__strace_interface(fstat);
__strace_interface(fstatat);
__strace_interface(lstat);
__strace_interface(stat);
__strace_interface(mkdir);
__strace_interface(mkdirat);
__strace_interface(readlink);
__strace_interface(readlinkat);
__strace_interface(rename);
__strace_interface(renameat);
__strace_interface(rmdir);
__strace_interface(unlink);
__strace_interface(unlinkat);

/* ioctl */
__strace_interface(ioctl);

/* ipc */
__strace_interface(pipe);

/* kernel */
__strace_interface(clock_getres);
__strace_interface(clock_gettime);
__strace_interface(clock_settime);
__strace_interface(gettimeofday);
__strace_interface(sched_setscheduler);
__strace_interface(sysinfo);
__strace_interface(uname);

/* mman */
__strace_interface(mmap);
__strace_interface(mremap);
__strace_interface(munmap);

/* mount */
__strace_interface(mount);

 /* process */
__strace_interface(execve);
__strace_interface(exit);
__strace_interface(exit_group);
__strace_interface(fork);

/* profile */
__strace_interface(times);

/* resource */
__strace_interface(getrlimit);
__strace_interface(getrusage);
__strace_interface(prlimit);
__strace_interface(setrlimit);

/* session */
__strace_interface(chroot);
__strace_interface(getpid);
__strace_interface(getppid);
__strace_interface(getpgid);
__strace_interface(getpgrp);
__strace_interface(setpgid);
__strace_interface(wait4);
__strace_interface(waitid);

/* signal */
__strace_interface(rt_sigaction);
__strace_interface(rt_sigprocmask);
__strace_interface(getitimer);
__strace_interface(setitimer);

/* socket */
__strace_interface(accept);
__strace_interface(bind);
__strace_interface(connect);
__strace_interface(getpeername);
__strace_interface(getsockname);
__strace_interface(getsockopt);
__strace_interface(listen);
__strace_interface(recvfrom);
__strace_interface(recvmsg);
__strace_interface(sendmsg);
__strace_interface(sendto);
__strace_interface(setsockopt);
__strace_interface(shutdown);
__strace_interface(socket);
__strace_interface(socketpair);

/* stdio */
__strace_interface(fsync);
__strace_interface(lseek);
__strace_interface(pread);
__strace_interface(pwrite);
__strace_interface(read);
__strace_interface(readv);
__strace_interface(write);
__strace_interface(writev);

/* thread */
__strace_interface(clone);
__strace_interface(gettid);
__strace_interface(set_tid_address);

/* token */
__strace_interface(getegid);
__strace_interface(geteuid);
__strace_interface(getgid);
__strace_interface(getuid);
__strace_interface(setgid);
__strace_interface(setuid);

#endif
