/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_fcntl.h"
#include "psx_sigfn.h"
#include "psx_errno.h"
#include "psx.h"

typedef intptr_t __fcntl_handler(
	int			fdidx,
	int			cmd,
	void *			arg,
	struct __fd *		fd,
	struct __ofd *		ofd);


static __fcntl_handler	__fcntl_dupfd;
static __fcntl_handler	__fcntl_getfd;
static __fcntl_handler	__fcntl_setfd;
static __fcntl_handler	__fcntl_getfl;
static __fcntl_handler	__fcntl_setfl;

static __fcntl_handler	__fcntl_getlk;
static __fcntl_handler	__fcntl_setlk;
static __fcntl_handler	__fcntl_setlkw;

static __fcntl_handler	__fcntl_setown;
static __fcntl_handler	__fcntl_getown;
static __fcntl_handler	__fcntl_setsig;
static __fcntl_handler	__fcntl_getsig;

static __fcntl_handler	__fcntl_setownex;
static __fcntl_handler	__fcntl_getownex;
static __fcntl_handler	__fcntl_getowneruids;

static __fcntl_handler	__fcntl_unused;


static __fcntl_handler * __fcntl_vtbl[F_CAP] = {
	__fcntl_dupfd,
	__fcntl_getfd,
	__fcntl_setfd,
	__fcntl_getfl,
	__fcntl_setfl,

	__fcntl_getlk,
	__fcntl_setlk,
	__fcntl_setlkw,

	__fcntl_setown,
	__fcntl_getown,
	__fcntl_setsig,
	__fcntl_getsig,

	__fcntl_unused,
	__fcntl_unused,
	__fcntl_unused,

	__fcntl_setownex,
	__fcntl_getownex,
	__fcntl_getowneruids
};


__psx_api
intptr_t __sys_fcntl(int fdidx, int cmd, void * arg)
{
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	intptr_t		ret;
	struct __fd *		fd;
	struct __ofd *		ofd;

	tlca = __tlca_self();
	ctx  = tlca->ctx;
	__psx_sig_prolog(tlca);

	if ((unsigned)cmd >= F_CAP)
		return __psx_sig_epilog(tlca,-EINVAL,EPSXONLY);

	if (!(fd = __psx_fd_ref_inc(ctx,fdidx)))
		return __psx_sig_epilog(tlca,-EBADF,EPSXONLY);

	tlca->ntstatus = EPSXONLY;
	ofd = __psx_ofd_ref_inc(ctx,fdidx);
	ret = __fcntl_vtbl[cmd](fdidx,cmd,arg,fd,ofd);

	__psx_ofd_ref_dec(ctx,ofd);
	__psx_fd_ref_dec(ctx,fd);

	tlca->ntstatus = (ret<0) ? EPSXONLY : NT_STATUS_SUCCESS;
	return __psx_sig_epilog(tlca,ret,tlca->ntstatus);
}


static intptr_t __fcntl_dupfd(int fdidx, int cmd, void * arg, struct __fd * fd, struct __ofd * ofd)
{
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __fd *		dupfd;
	intptr_t		dupidx;

	tlca = __tlca_self();
	ctx  = tlca->ctx;
	dupidx = (int32_t)(intptr_t)arg;

	if (!(dupfd = __psx_fd_obtain(ctx,&dupidx)))
		return -EMFILE;

	dupfd->ofdidx  = fd->ofdidx;
	dupfd->flags   = fd->flags & ~O_CLOEXEC;
	dupfd->refcnt  = 0;

	at_locked_inc_32(&ofd->info.refcnt);
	at_store_32(&dupfd->invalid,0);

	return dupidx;
}

static intptr_t __fcntl_getfd(int fdidx, int cmd, void * arg, struct __fd * fd, struct __ofd * ofd)
{
	return fd->flags;
}

static intptr_t __fcntl_setfd(int fdidx, int cmd, void * arg, struct __fd * fd, struct __ofd * ofd)
{
	fd->flags = (uint32_t)(uintptr_t)arg;
	return 0;
}

static intptr_t __fcntl_getfl(int fdidx, int cmd, void * arg, struct __fd * fd, struct __ofd * ofd)
{
	return ofd->info.psxflags;
}

static intptr_t __fcntl_setfl(int fdidx, int cmd, void * arg, struct __fd * fd, struct __ofd * ofd)
{
	ofd->info.psxflags = (uint32_t)(uintptr_t)arg;
	ofd->info.ntflags &= ((ofd->info.psxflags & O_NONBLOCK)
				? ~NT_FILE_SYNCHRONOUS_IO_ALERT
				: ofd->info.ntflags);
	return 0;
}


static intptr_t __fcntl_getlk(int fdidx, int cmd, void * arg, struct __fd * fd, struct __ofd * ofd)
{
	/* todo */
	return -EINVAL;
}

static intptr_t __fcntl_setlk(int fdidx, int cmd, void * arg, struct __fd * fd, struct __ofd * ofd)
{
	/* todo */
	return -EINVAL;
}

static intptr_t __fcntl_setlkw(int fdidx, int cmd, void * arg, struct __fd * fd, struct __ofd * ofd)
{
	/* todo */
	return -EINVAL;
}


static intptr_t __fcntl_setown(int fdidx, int cmd, void * arg, struct __fd * fd, struct __ofd * ofd)
{
	/* todo */
	return -EINVAL;
}

static intptr_t __fcntl_getown(int fdidx, int cmd, void * arg, struct __fd * fd, struct __ofd * ofd)
{
	/* todo */
	return -EINVAL;
}

static intptr_t __fcntl_setsig(int fdidx, int cmd, void * arg, struct __fd * fd, struct __ofd * ofd)
{
	/* todo */
	return -EINVAL;
}

static intptr_t __fcntl_getsig(int fdidx, int cmd, void * arg, struct __fd * fd, struct __ofd * ofd)
{
	return PSX_SIGIO;
}


static intptr_t __fcntl_setownex(int fdidx, int cmd, void * arg, struct __fd * fd, struct __ofd * ofd)
{
	/* todo */
	return -EINVAL;
}

static intptr_t __fcntl_getownex(int fdidx, int cmd, void * arg, struct __fd * fd, struct __ofd * ofd)
{
	/* todo */
	return -EINVAL;
}


static intptr_t __fcntl_getowneruids(int fdidx, int cmd, void * arg, struct __fd * fd, struct __ofd * ofd)
{
	/* todo */
	return -EINVAL;
}


static intptr_t __fcntl_unused(int fdidx, int cmd, void * arg, struct __fd * fd, struct __ofd * ofd)
{
	return -EINVAL;
}
