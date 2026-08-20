#ifndef _PSX_SIGFN_H
#define _PSX_SIGFN_H

#include "psx_systypes.h"
#include "psx_limits.h"

enum __psx_signal_type {
	PSX_SIGARG,
	PSX_SIGHUP,
	PSX_SIGINT,
	PSX_SIGQUIT,
	PSX_SIGILL,
	PSX_SIGTRAP,
	PSX_SIGABRT,
	PSX_SIGBUS,
	PSX_SIGFPE,
	PSX_SIGKILL,
	PSX_SIGUSR1,
	PSX_SIGSEGV,
	PSX_SIGUSR2,
	PSX_SIGPIPE,
	PSX_SIGALRM,
	PSX_SIGTERM,
	PSX_SIGSTKFLT,
	PSX_SIGCHLD,
	PSX_SIGCONT,
	PSX_SIGSTOP,
	PSX_SIGTSTP,
	PSX_SIGTTIN,
	PSX_SIGTTOU,
	PSX_SIGURG,
	PSX_SIGXCPU,
	PSX_SIGXFSZ,
	PSX_SIGVTALRM,
	PSX_SIGPROF,
	PSX_SIGWINCH,
	PSX_SIGIO,
	PSX_SIGPWR,
	PSX_SIGSYS,
	PSX_SIGRT0,
	PSX_SIGRT1,
	PSX_SIGRT2,
	PSX_SIGRT3,
	PSX_SIGRT4,
	PSX_SIGRT5,
	PSX_SIGRT6,
	PSX_SIGRT7,
	PSX_SIGRT8,
	PSX_SIGRT9,
	PSX_SIGRT10,
	PSX_SIGRT11,
	PSX_SIGRT12,
	PSX_SIGRT13,
	PSX_SIGRT14,
	PSX_SIGRT15,
	PSX_SIGRT16,
	PSX_SIGRT17,
	PSX_SIGRT18,
	PSX_SIGRT19,
	PSX_SIGRT20,
	PSX_SIGRT21,
	PSX_SIGRT22,
	PSX_SIGRT23,
	PSX_SIGRT24,
	PSX_SIGRT25,
	PSX_SIGRT26,
	PSX_SIGRT27,
	PSX_SIGRT28,
	PSX_SIGRT29,
	PSX_SIGRT30,
	PSX_SIGRT31
};

#define SIG_ERR  ((void (*)(int))-1)
#define SIG_DFL  ((void (*)(int)) 0)
#define SIG_IGN  ((void (*)(int)) 1)

union sigval_t {
	int32_t	sival_int;
	void *	sival_ptr;
};

typedef struct __siginfo_t {
	int32_t		si_signo;
	int32_t		si_errno;
	int32_t		si_code;

	union {
		char __pad[128 - 2*sizeof(int) - sizeof(intptr_t)];

		struct {
			union {
				struct {
					pid_t si_pid;
					uid_t si_uid;
				} __piduid;
				struct {
					int si_timerid;
					int si_overrun;
				} __timer;
			} __first;
			union {
				union sigval_t si_value;
				struct {
					int	si_status;
					clock_t si_utime, si_stime;
				} __sigchld;
			} __second;
		} __si_common;
		struct {
			void *	si_addr;
			short	si_addr_lsb;
			struct {
				void *	si_lower;
				void *	si_upper;
			} __addr_bnd;
		} __sigfault;
		struct {
			long	si_band;
			int	si_fd;
		} __sigpoll;
		struct {
			void *	 si_call_addr;
			int      si_syscall;
			unsigned si_arch;
		} __sigsys;
	} __si_fields;
} siginfo_t;

#define si_pid		__si_fields.__si_common.__first.__piduid.si_pid
#define si_uid		__si_fields.__si_common.__first.__piduid.si_uid
#define si_status	__si_fields.__si_common.__second.__sigchld.si_status
#define si_utime	__si_fields.__si_common.__second.__sigchld.si_utime
#define si_stime	__si_fields.__si_common.__second.__sigchld.si_stime
#define si_value	__si_fields.__si_common.__second.si_value
#define si_addr		__si_fields.__sigfault.si_addr
#define si_addr_lsb	__si_fields.__sigfault.si_addr_lsb
#define si_lower	__si_fields.__sigfault.__addr_bnd.si_lower
#define si_upper	__si_fields.__sigfault.__addr_bnd.si_upper
#define si_band		__si_fields.__sigpoll.si_band
#define si_fd		__si_fields.__sigpoll.si_fd
#define si_timerid	__si_fields.__si_common.__first.__timer.si_timerid
#define si_overrun	__si_fields.__si_common.__first.__timer.si_overrun
#define si_ptr		si_value.sival_ptr
#define si_int		si_value.sival_int
#define si_call_addr	__si_fields.__sigsys.si_call_addr
#define si_syscall	__si_fields.__sigsys.si_syscall
#define si_arch		__si_fields.__sigsys.si_arch


typedef struct __sigset_t {
	uintptr_t __bits[128/sizeof(intptr_t)];
} sigset_t;

struct __stack {
	void *	ss_sp;
	int32_t	ss_flags;
	size_t	ss_size;
};

struct __attr_aligned__(__PSX_UCONTEXT_ALIGN) __ucontext {
	uint32_t		uc_csize;
	uint32_t		uc_msize;
	uint32_t		uc_pad[2];
	uintptr_t		uc_flags;
	uintptr_t		uc_ntflags;
	uintptr_t		uc_psxflags;
	uintptr_t		uc_usrflags;
	uint32_t		uc_cpuid[4];
	uint32_t		uc_cpucaps[4];
	uintptr_t		uc_reserved[32];
	uintptr_t		uc_align[2];
	struct __stack		uc_stack;
	struct __ucontext *	uc_link;
	sigset_t		uc_sigmask;
	nt_thread_context	uc_mcontext;
};

typedef void	(*sighfn_t)(int);
typedef void	(*sigafn_t)(int, siginfo_t *, void *);
typedef void	(*sigrfn_t)(void);

struct __sigaction {
	sigafn_t	sa_handler;
	sigset_t	sa_mask;
	int		sa_flags;
	sigrfn_t	sa_restorer;
};

#endif
