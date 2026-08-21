/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_syscalls.h"
#include "psx_helper.h"
#include "psx_errno.h"
#include "psx_strace.h"
#include "psx_daemon.h"
#include "psx_timer.h"
#include "psx.h"

/**************************************/
/* SYSCALL VTABLE -- WORK IN PROGRESS */
/**************************************/

static mode_t umask = 2;
mode_t __sys_umask(mode_t mask){mode_t tmp=umask; umask = mask; return tmp;}

static void __populate_syscall_vtbl(void)
{
	#define set_syscall_pointer(routine) \
		__sysvtbl[SYS_##routine] = (uintptr_t *)__sys_##routine

	/* brk */
	set_syscall_pointer(brk);
	set_syscall_pointer(madvise);

	/* fcntl */
	set_syscall_pointer(dup);
	set_syscall_pointer(dup2);
	set_syscall_pointer(dup3);
	set_syscall_pointer(fcntl);

	/* fd,ofd */
	set_syscall_pointer(close);
	set_syscall_pointer(creat);
	set_syscall_pointer(open);
	set_syscall_pointer(openat);

	/* fs */
	set_syscall_pointer(access);
	set_syscall_pointer(statfs);
	set_syscall_pointer(fstatfs);
	set_syscall_pointer(faccessat);
	set_syscall_pointer(chdir);
	set_syscall_pointer(fchdir);
	set_syscall_pointer(chmod);
	set_syscall_pointer(fchmodat);
	set_syscall_pointer(getcwd);
	set_syscall_pointer(getdents);
	set_syscall_pointer(fstat);
	set_syscall_pointer(fstatat);
	set_syscall_pointer(lstat);
	set_syscall_pointer(stat);
	set_syscall_pointer(mkdir);
	set_syscall_pointer(mkdirat);
	set_syscall_pointer(readlink);
	set_syscall_pointer(readlinkat);
	set_syscall_pointer(rename);
	set_syscall_pointer(renameat);
	set_syscall_pointer(rmdir);
	set_syscall_pointer(unlink);
	set_syscall_pointer(unlinkat);

	/* ioctl */
	set_syscall_pointer(ioctl);

	/* ipc */
	set_syscall_pointer(pipe);

	/* kernel */
	set_syscall_pointer(clock_getres);
	set_syscall_pointer(time);
	set_syscall_pointer(clock_gettime);
	set_syscall_pointer(clock_settime);
	set_syscall_pointer(gettimeofday);
	set_syscall_pointer(sched_setscheduler);
	set_syscall_pointer(sched_yield);
	set_syscall_pointer(sysinfo);
	set_syscall_pointer(uname);

	/* mman */
	set_syscall_pointer(mmap);
	set_syscall_pointer(mprotect);
	set_syscall_pointer(msync);
	set_syscall_pointer(mremap);
	set_syscall_pointer(munmap);

	/* mount */
	set_syscall_pointer(mount);

	 /* process */
	set_syscall_pointer(execve);
	set_syscall_pointer(exit);
	set_syscall_pointer(exit_group);
	set_syscall_pointer(fork);

	/* profile */
	set_syscall_pointer(times);

	/* timer (R4) -- musl timer 库层恢复: src/time/timer_*.c */
	set_syscall_pointer(timer_create);
	set_syscall_pointer(timer_settime);
	set_syscall_pointer(timer_gettime);
	set_syscall_pointer(timer_getoverrun);
	set_syscall_pointer(timer_delete);

	/* mq (R10a) -- musl mq 库层补全: src/mq/mq_*.c */
	set_syscall_pointer(mq_open);
	set_syscall_pointer(mq_unlink);
	set_syscall_pointer(mq_timedsend);
	set_syscall_pointer(mq_timedreceive);
	set_syscall_pointer(mq_notify);
	set_syscall_pointer(mq_getsetattr);

	/* ipc (R10b) -- musl ipc 库层补全: src/ipc/{msg,sem,shm}_*.c */
	set_syscall_pointer(msgget);
	set_syscall_pointer(msgsnd);
	set_syscall_pointer(msgrcv);
	set_syscall_pointer(msgctl);
	set_syscall_pointer(semget);
	set_syscall_pointer(semop);
	set_syscall_pointer(semtimedop);
	set_syscall_pointer(semctl);
	set_syscall_pointer(shmget);
	set_syscall_pointer(shmat);
	set_syscall_pointer(shmdt);
	set_syscall_pointer(shmctl);

	/* resource */
	set_syscall_pointer(getrlimit);
	set_syscall_pointer(getrusage);
	set_syscall_pointer(prlimit);
	set_syscall_pointer(setrlimit);

	/* session */
	set_syscall_pointer(chroot);
	set_syscall_pointer(getpid);
	set_syscall_pointer(getppid);
	set_syscall_pointer(getpgid);
	set_syscall_pointer(getpgrp);
	set_syscall_pointer(setpgid);
	set_syscall_pointer(wait4);
	set_syscall_pointer(waitid);

	/* select/poll (R5) -- musl poll()/select() 走 SYS_poll(7)/SYS_select(23);
	   pselect6/ppoll 一并注册供直调 */
	set_syscall_pointer(poll);
	set_syscall_pointer(select);
	set_syscall_pointer(pselect6);
	set_syscall_pointer(ppoll);

	/* signal */
	set_syscall_pointer(rt_sigaction);
	set_syscall_pointer(rt_sigprocmask);
	set_syscall_pointer(getitimer);
	set_syscall_pointer(setitimer);
	/* tcc_posix: 2015 pre-alpha 缺 tkill/kill, musl raise() 依赖 */
	set_syscall_pointer(tkill);
	set_syscall_pointer(kill);

	/* socket */
	set_syscall_pointer(accept);
	set_syscall_pointer(bind);
	set_syscall_pointer(connect);
	set_syscall_pointer(getpeername);
	set_syscall_pointer(getsockname);
	set_syscall_pointer(getsockopt);
	set_syscall_pointer(listen);
	set_syscall_pointer(recvfrom);
	set_syscall_pointer(recvmsg);
	set_syscall_pointer(sendmsg);
	set_syscall_pointer(sendto);
	set_syscall_pointer(setsockopt);
	set_syscall_pointer(shutdown);
	set_syscall_pointer(socket);
	set_syscall_pointer(socketpair);

	/* stdio */
	set_syscall_pointer(fsync);
	set_syscall_pointer(lseek);
	set_syscall_pointer(pread);
	set_syscall_pointer(pwrite);
	set_syscall_pointer(read);
	set_syscall_pointer(readv);
	set_syscall_pointer(write);
	set_syscall_pointer(writev);

	/* thread */
	set_syscall_pointer(clone);
	set_syscall_pointer(gettid);
	set_syscall_pointer(set_tid_address);
	set_syscall_pointer(futex);
	set_syscall_pointer(set_robust_list);

	/* token */
	set_syscall_pointer(getegid);
	set_syscall_pointer(geteuid);
	set_syscall_pointer(getgid);
	set_syscall_pointer(getuid);
	set_syscall_pointer(setgid);
	set_syscall_pointer(setuid);

	return;
}



void __psx_populate_syscall_vtbl(uint32_t flags)
{
	(void)flags;
	__populate_syscall_vtbl();

		/* pid (implemented but unregistered in 2015 pre-alpha) */
	set_syscall_pointer(getpid);
	set_syscall_pointer(getppid);

	/* time (2015 pre-alpha: nanosleep was unregistered) */
	set_syscall_pointer(nanosleep);
	set_syscall_pointer(clock_nanosleep);

	set_syscall_pointer(umask);
}