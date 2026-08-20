/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#ifndef _PSX_H_
#define _PSX_H_

#include <ntapi/ntapi.h>
#include <psxscl/psxscl.h>

#include "psx_systypes.h"
#include "psx_syscalls.h"
#include "psx_impl.h"

#include "psx_limits.h"
#include "psx_ptrace.h"
#include "psx_resource.h"
#include "psx_sysinfo.h"
#include "psx_signal.h"
#include "psx_socket.h"
#include "psx_stat.h"
#include "psx_profile.h"
#include "psx_dirent.h"

#define __sys_routine(routine) \
	__sys_routine__##routine

#define __sys_interface(routine) \
	__psx_api __sys_routine(routine) __sys_##routine

/* brk */
typedef intptr_t	__sys_routine(brk)(uintptr_t brk);
typedef intptr_t	__sys_routine(madvise)(void * addr, size_t length, int advice);

/* fcntl */
typedef intptr_t	__sys_routine(dup)(int);
typedef intptr_t	__sys_routine(dup2)(int, int);
typedef intptr_t	__sys_routine(dup3)(int, int, int flags);
typedef intptr_t	__sys_routine(fcntl)(int, int cmd, void * arg);

/* fd,ofd */
typedef intptr_t	__sys_routine(close)(int);
typedef intptr_t	__sys_routine(creat)(const unsigned char *, mode_t);
typedef intptr_t	__sys_routine(open)(const unsigned char *, int, mode_t);
typedef intptr_t	__sys_routine(openat)(int, const unsigned char *, int, mode_t);

/* fs */
typedef intptr_t	__sys_routine(access)(const unsigned char * path, int amode);
typedef intptr_t	__sys_routine(faccessat)(int, const unsigned char * path, int amode, int flag);
typedef intptr_t	__sys_routine(chdir)(const unsigned char *);
typedef intptr_t	__sys_routine(fchdir)(int);
typedef intptr_t	__sys_routine(chmod)(const unsigned char * path, mode_t mode);
typedef intptr_t	__sys_routine(fchmodat)(int, const unsigned char * path, mode_t mode, int flag);
typedef intptr_t	__sys_routine(getcwd)(char * buf, size_t size);
typedef intptr_t	__sys_routine(getdents)(int, struct __dirent *, unsigned int);
typedef intptr_t	__sys_routine(fstat)(int, struct __stat * xstat);
typedef intptr_t	__sys_routine(fstatat)(int, const unsigned char * path, struct __stat * xstat, int flag);
typedef intptr_t	__sys_routine(lstat)(const unsigned char * path, struct __stat * xstat);
typedef intptr_t	__sys_routine(stat)(const unsigned char * path, struct __stat * xstat);
typedef intptr_t	__sys_routine(mkdir)(const unsigned char * path, mode_t mode);
typedef intptr_t	__sys_routine(mkdirat)(int, const unsigned char * path, mode_t mode);
typedef ssize_t		__sys_routine(readlink)(const unsigned char * path, const unsigned char * buf, size_t);
typedef ssize_t		__sys_routine(readlinkat)(int, const unsigned char * path, const unsigned char * buf, size_t);
typedef intptr_t	__sys_routine(rename)(const unsigned char * src, const unsigned char * dst);
typedef intptr_t	__sys_routine(renameat)(int, const unsigned char * src, int, const unsigned char * dst);
typedef intptr_t	__sys_routine(rmdir)(const unsigned char * path);
typedef intptr_t	__sys_routine(unlink)(const unsigned char *path);
typedef intptr_t	__sys_routine(unlinkat)(int, const unsigned char * path, int flag);

/* ioctl */
typedef intptr_t	__sys_routine(ioctl)(int, unsigned long, void *, void *, void *, void *);

/* ipc */
typedef intptr_t	__sys_routine(pipe)(int[2]);

/* kernel */
typedef intptr_t	__sys_routine(clock_getres)(clockid_t clock_id, struct timespec * res);
typedef intptr_t	__sys_routine(clock_gettime)(clockid_t clock_id, struct timespec * tp);
typedef intptr_t	__sys_routine(clock_settime)(clockid_t clock_id, const struct timespec * tp);
typedef intptr_t	__sys_routine(gettimeofday)(struct timeval * tp, void * tzp);
typedef intptr_t	__sys_routine(nanosleep)(const struct timespec * req, struct timespec * rem);
typedef intptr_t	__sys_routine(clock_nanosleep)(int, int, const struct timespec *, struct timespec *);
typedef intptr_t	__sys_routine(time)(int *);
typedef intptr_t	__sys_routine(sched_setscheduler)(pid_t, int, const struct sched_param *);
typedef intptr_t	__sys_routine(sysinfo)(struct __sysinfo * info);
typedef intptr_t	__sys_routine(uname)(struct __utsname *);

/* mman */
typedef void *		__sys_routine(mmap)(void * addr, size_t length, int prot, int flags, int,off_t offset);
typedef void *		__sys_routine(mremap)(void * mapaddr, size_t mapsize, size_t newsize, int flags);
typedef intptr_t	__sys_routine(munmap)(void * addr, size_t length);
typedef intptr_t	__sys_routine(mprotect)(void * addr, size_t length, int prot);
typedef intptr_t	__sys_routine(msync)(void * addr, size_t length, int flags);

/* mount */
typedef intptr_t	__sys_routine(mount)(const char * source, const char * target, const char * fstype, uintptr_t mntflags, const void * data);

 /* process */
typedef intptr_t	__sys_routine(execve)(const unsigned char * path, const char ** argv, const char ** envp);
typedef void		__sys_routine(exit)(int);
typedef void		__sys_routine(exit_group)(int);
typedef intptr_t	__sys_routine(fork)(void);

/* profile */
typedef clock_t		__sys_routine(times)(struct tms * buf);

/* resource */
typedef intptr_t	__sys_routine(getrlimit)(int resource, struct __rlimit * rlim);
typedef intptr_t	__sys_routine(getrusage)(int who, struct __rusage * r_usage);
typedef intptr_t	__sys_routine(prlimit)(pid_t pid, int resource, const struct __rlimit * new_limit, struct __rlimit * old_limit);
typedef intptr_t	__sys_routine(setrlimit)(int resource, const struct __rlimit * rlim);

/* session */
typedef intptr_t	__sys_routine(chroot)(const unsigned char *);
typedef intptr_t	__sys_routine(getpid)(void);
typedef intptr_t	__sys_routine(getppid)(void);
typedef intptr_t	__sys_routine(getpgid)(pid_t);
typedef intptr_t	__sys_routine(getpgrp)(pid_t pid);
typedef intptr_t	__sys_routine(setpgid)(pid_t pid, pid_t pgid);
typedef intptr_t	__sys_routine(wait4)(pid_t, int * status, int options, struct __rusage *);
typedef intptr_t	__sys_routine(waitid)(int idtype, id_t id, siginfo_t *, int options);

/* signal */
typedef intptr_t	__sys_routine(rt_sigaction)(int signum, const struct __sigaction * act, struct __sigaction * oldact, size_t sigsetsize);
typedef intptr_t	__sys_routine(rt_sigprocmask)(int,const sigset_t *,sigset_t *);
typedef intptr_t	__sys_routine(getitimer)(enum __psx_timer_type which, struct itimerval * curr_value);
typedef intptr_t	__sys_routine(setitimer)(enum __psx_timer_type which, const struct itimerval * new_value, struct itimerval * old_value);
/* tcc_posix: tkill/kill (musl raise 依赖) */
typedef intptr_t	__sys_routine(tkill)(int tid, int signum);
typedef intptr_t	__sys_routine(kill)(int pid, int signum);
__sys_interface(tkill);
__sys_interface(kill);

/* socket */
typedef intptr_t	__sys_routine(accept)(int, struct __sockaddr *, socklen_t *);
typedef intptr_t	__sys_routine(bind)(int, const struct __sockaddr *, socklen_t);
typedef intptr_t	__sys_routine(connect)(int, const struct __sockaddr *, socklen_t);
typedef intptr_t	__sys_routine(getpeername)(int, struct __sockaddr *, socklen_t *);
typedef intptr_t	__sys_routine(getsockname)(int, struct __sockaddr *, socklen_t *);
typedef intptr_t	__sys_routine(getsockopt)(int, int, int, void *, socklen_t *);
typedef intptr_t	__sys_routine(listen)(int, int);
typedef ssize_t		__sys_routine(recvfrom)(int, void *, size_t, int, struct __sockaddr *, socklen_t *);
typedef ssize_t		__sys_routine(recvmsg)(int, struct __msghdr *, int);
typedef ssize_t		__sys_routine(sendmsg)(int, const struct __msghdr *, int);
typedef ssize_t		__sys_routine(sendto)(int, const void *, size_t, int, const struct __sockaddr *, socklen_t);
typedef intptr_t	__sys_routine(setsockopt)(int, int, int, const void *, socklen_t);
typedef intptr_t	__sys_routine(shutdown)(int, int);
typedef intptr_t	__sys_routine(socket)(int, int, int);
typedef intptr_t	__sys_routine(socketpair)(int, int, int, int[2]);

/* stdio */
typedef intptr_t	__sys_routine(fsync)(int);
typedef off_t		__sys_routine(lseek)(int,off_t,int);
typedef ssize_t		__sys_routine(pread)(int,void *,size_t,off_t);
typedef ssize_t		__sys_routine(pwrite)(int,const void *,size_t,off_t);
typedef ssize_t		__sys_routine(read)(int,void *,size_t);
typedef ssize_t		__sys_routine(readv)(int,const struct iovec *,int);
typedef ssize_t		__sys_routine(write)(int,const void *,size_t);
typedef ssize_t		__sys_routine(writev)(int,const struct iovec *,int);

/* thread */
typedef long		__sys_routine(clone)(uintptr_t,void *,void *,void *,struct pt_regs *);
typedef intptr_t	__sys_routine(gettid)(void);
typedef intptr_t	__sys_routine(set_tid_address)(int *);
typedef intptr_t	__sys_routine(futex)(int *, int, int, void *, void *, int);
typedef intptr_t	__sys_routine(set_robust_list)(void *, size_t);

/* token */
typedef gid_t		__sys_routine(getegid)(void);
typedef uid_t		__sys_routine(geteuid)(void);
typedef gid_t		__sys_routine(getgid)(void);
typedef uid_t		__sys_routine(getuid)(void);
typedef intptr_t	__sys_routine(setgid)(gid_t);
typedef intptr_t	__sys_routine(setuid)(uid_t);


/* brk */
__sys_interface(brk);
__sys_interface(madvise);

/* fcntl */
__sys_interface(dup);
__sys_interface(dup2);
__sys_interface(dup3);
__sys_interface(fcntl);

/* fd,ofd */
__sys_interface(close);
__sys_interface(creat);
__sys_interface(open);
__sys_interface(openat);

/* fs */
__sys_interface(access);
__sys_interface(faccessat);
__sys_interface(chdir);
__sys_interface(fchdir);
__sys_interface(chmod);
__sys_interface(fchmodat);
__sys_interface(getcwd);
__sys_interface(getdents);
__sys_interface(fstat);
__sys_interface(fstatat);
__sys_interface(lstat);
__sys_interface(stat);
__sys_interface(mkdir);
__sys_interface(mkdirat);
__sys_interface(readlink);
__sys_interface(readlinkat);
__sys_interface(rename);
__sys_interface(renameat);
__sys_interface(rmdir);
__sys_interface(unlink);
__sys_interface(unlinkat);

/* ioctl */
__sys_interface(ioctl);

/* ipc */
__sys_interface(pipe);

/* kernel */
__sys_interface(clock_getres);
__sys_interface(clock_gettime);
__sys_interface(clock_settime);
__sys_interface(gettimeofday);
__sys_interface(nanosleep);
__sys_interface(clock_nanosleep);
__sys_interface(time);
__sys_interface(sched_setscheduler);
__sys_interface(sysinfo);
__sys_interface(uname);

/* mman */
__sys_interface(mmap);
__sys_interface(mremap);
__sys_interface(munmap);
__sys_interface(mprotect);
__sys_interface(msync);

/* mount */
__sys_interface(mount);

 /* process */
__sys_interface(execve);
__sys_interface(exit);
__sys_interface(exit_group);
__sys_interface(fork);

/* profile */
__sys_interface(times);

/* resource */
__sys_interface(getrlimit);
__sys_interface(getrusage);
__sys_interface(prlimit);
__sys_interface(setrlimit);

/* session */
__sys_interface(chroot);
__sys_interface(getpid);
__sys_interface(getppid);
__sys_interface(getpgid);
__sys_interface(getpgrp);
__sys_interface(setpgid);
__sys_interface(wait4);
__sys_interface(waitid);

/* signal */
__sys_interface(rt_sigaction);
__sys_interface(rt_sigprocmask);
__sys_interface(getitimer);
__sys_interface(setitimer);

/* socket */
__sys_interface(accept);
__sys_interface(bind);
__sys_interface(connect);
__sys_interface(getpeername);
__sys_interface(getsockname);
__sys_interface(getsockopt);
__sys_interface(listen);
__sys_interface(recvfrom);
__sys_interface(recvmsg);
__sys_interface(sendmsg);
__sys_interface(sendto);
__sys_interface(setsockopt);
__sys_interface(shutdown);
__sys_interface(socket);
__sys_interface(socketpair);

/* stdio */
__sys_interface(fsync);
__sys_interface(lseek);
__sys_interface(pread);
__sys_interface(pwrite);
__sys_interface(read);
__sys_interface(readv);
__sys_interface(write);
__sys_interface(writev);

/* thread */
__sys_interface(clone);
__sys_interface(gettid);
__sys_interface(set_tid_address);
__sys_interface(futex);
__sys_interface(set_robust_list);

/* token */
__sys_interface(getegid);
__sys_interface(geteuid);
__sys_interface(getgid);
__sys_interface(getuid);
__sys_interface(setgid);
__sys_interface(setuid);


/* work in progress */
void __psx_populate_syscall_vtbl(uint32_t flags);

#endif
