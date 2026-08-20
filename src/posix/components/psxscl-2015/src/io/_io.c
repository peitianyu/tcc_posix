/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_tlca.h"
#include "psx_signal.h"
#include "psx_errno.h"
#include "psx_fcntl.h"
#include "psx_ofd.h"
#include "psx_io.h"
#include "psx.h"

static ssize_t __io(int fd, void * buf, size_t bytes, off_t offset, int whence, int mode)
{
	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;
	struct __ofd *		ctxofd;
	struct __ofd *		ofd;
	nt_iosb			iosb;
	uint32_t		nbytes;
	nt_large_integer *	poffset;
	ntapi_zw_read_file *	iofn[2];

	tlca = __tlca_self();
	ctx  = tlca->ctx;
	__psx_io_prolog(tlca);

	if (!(ctxofd  = __psx_ofd_ref_inc(ctx,fd)))
		return __psx_io_epilog(tlca,-EBADF,EPSXONLY);

	else if ((iosb.status = __iovtbl[ctxofd->info.fdtype].prolog(tlca,ctxofd,&ofd)))
		return __psx_io_epilog(tlca,-ENOMEM,iosb.status);

	nbytes  = (uint32_t)bytes;
	poffset = (whence == SEEK_SET) ? (nt_large_integer *)&offset : 0;

	iofn[0] = __iovtbl[ofd->info.fdtype].read;
	iofn[1] = __iovtbl[ofd->info.fdtype].write;

	ofd->info.reserved = mode;
	ofd->info.iostatus = iofn[mode](
		ofd->info.hfile,
		ofd->info.hevent,0,0,
		&iosb,buf,nbytes,
		poffset,0);

	__psx_io_set_status(tlca,ofd,&iosb);
	__iovtbl[ctxofd->info.fdtype].epilog(ctxofd,ofd);
	__psx_ofd_ref_dec(ctx,ctxofd);

	return __psx_io_epilog(tlca,iosb.info,tlca->ntstatus);
}

__psx_api
ssize_t __sys_read(int fd, void * buf, size_t bytes)
{
	return __io(fd,buf,bytes,0,SEEK_CUR,__IO_READ);
}

__psx_api
ssize_t __sys_pread(int fd, void * buf, size_t bytes, off_t offset)
{
	/* tcc_posix: 2015 的 NT ReadFile 在同步句柄下忽略 poffset,
	   用当前指针并推进 → pread 语义错误 (不应改偏移).
	   用 保存指针 + 定位 + 读 + 恢复 模拟. */
	ssize_t ret;
	off_t cur = __sys_lseek(fd, 0, SEEK_CUR);
	if (cur < 0)
		return __sys_read(fd, buf, bytes); /* 非定位文件退化为 read */
	if (__sys_lseek(fd, offset, SEEK_SET) < 0)
		return -1;
	ret = __sys_read(fd, buf, bytes);
	__sys_lseek(fd, cur, SEEK_SET);
	return ret;
}

__psx_api
ssize_t __sys_write(int fd, const void * buf, size_t bytes)
{
	return __io(fd,(void *)buf,bytes,0,SEEK_CUR,__IO_WRITE);
}

__psx_api
ssize_t __sys_pwrite(int fd, const void * buf, size_t bytes, off_t offset)
{
	return __io(fd,(void *)buf,bytes,offset,SEEK_SET,__IO_WRITE);
}
