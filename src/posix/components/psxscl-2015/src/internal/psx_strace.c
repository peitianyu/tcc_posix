/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx.h"
#include "psx_systypes.h"
#include "psx_syscalls.h"
#include "psx_errno.h"
#include "psx_errstr.h"
#include "psx_strace.h"
#include "psx_systypes.h"
#include "psx_limits.h"
#include "psx_sysinfo.h"
#include "psx_stat.h"
#include "psx_signal.h"
#include "psx_debug.h"
#include "psx_impl.h"

/*****************************************/
/* POOR MAN'S STRACE -- WORK IN PROGRESS */
/* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
/* this module will eventually be moved  */
/* out of the system call library and    */
/* into its own stand-alone utility.     */
/*****************************************/

enum __strace_types {
	__STRACE_SINTEGER,
	__STRACE_UINTEGER,
	__STRACE_POINTER,
	__STRACE_STRING,
	__STRACE_BUFFER,
	__STRACE_BINARY
};

struct __strace_param {
	enum __strace_types	type;
	intptr_t		value;
	size_t			len;
};

struct __strace_params {
	int			sysid;
	int			cparams;
	struct __strace_param	ret;
	struct __strace_param	params[6];
	uint32_t		flags;
};

#define __STRACE_ABBR_STRINGS	0x0
#define __STRACE_FULL_STRINGS	0x1

static char * __strace_sysnames[]	= {__STRACE_INTERFACE_NAMES};
static char * __psx_error_strings[]	= {__PSX_ERROR_STRINGS};
static char * __strace_nonprinting[]	= {
	"\\0","\\001","\\002","\\003","\\004","\\005","\\006","\\007",
	"\\010","\\011","\\n","\\013","\\014","\\015","\\016","\\017",
	"\\020","\\021","\\022","\\023","\\024","\\025","\\026","\\027",
	"\\030","\\031","\\032","\\033","\\034","\\035","\\036","\\037"};

static int __strace_uintptr_to_utf8(uintptr_t value,unsigned char * buf)
{
	int i,len;
	uintptr_t val;

	if (!value) {
		*buf = '0';
		len = 1;
	} else {
		for (len=0,val=value; val; val=val/10,len++);
		for (i=len,buf+=len-1; i; i--,buf--,value=value/10)
			*buf = '0' + (value % 10);
	}

	return len;
}

static int __strace_sprintf(unsigned char * buf,struct __strace_param * param)
{
	size_t len;
	char * sub;
	unsigned char *ch, *ch_cap, *dst;

	switch (param->type) {
		case __STRACE_SINTEGER:
			if (param->value >= 0)
				len = __strace_uintptr_to_utf8((uintptr_t)param->value,buf);
			else {
				buf[0]='-';
				len = 1 + __strace_uintptr_to_utf8((uintptr_t)(-param->value),buf+1);
			}
			break;

		case __STRACE_UINTEGER:
			len = __strace_uintptr_to_utf8((uintptr_t)param->value,buf);
			break;

		case __STRACE_POINTER:
		case __STRACE_BUFFER:
		case __STRACE_BINARY:
			buf[0]='0'; buf[1]='x'; buf+=2; len=2;

			/* avoid eight leading zeros on x64 */
			if ((uintptr_t)param->value == (uint32_t)param->value) {
				__ntapi->tt_uint32_to_hex_utf8((uint32_t)param->value,buf);
				len += 2*sizeof(uint32_t);
			} else {
				__ntapi->tt_uintptr_to_hex_utf8((uintptr_t)param->value,buf);
				len += 2*sizeof(intptr_t);
			}

			break;

		case __STRACE_STRING:
			ch = (unsigned char *)param->value;
			ch_cap = ch + 512;

			for (dst=buf; ch<ch_cap && ch && *ch; ch++)
				if (*ch >= 0x20)
					*dst++ = *ch;
				else
					for (sub=__strace_nonprinting[*ch]; *sub; *dst++=*sub++);

			if (ch && *ch)
				for (len=0; len<3; *dst++='.',len++);

			len = dst - buf;
			break;
		default:
			len = 0;
	}

	return (int)len;
}

static void __strace(struct __strace_params * params)
{
	int32_t			status;
	struct __psx_tlca *	tlca;
	struct __strace_param	param;
	nt_iosb			iosb;
	unsigned char *		ch;
	int			i;

	tlca = __tlca_self();
	ch   = tlca->strace;
	__ntapi->tt_aligned_block_memset(ch,(uintptr_t)0x2020202020202020,sizeof(tlca->strace));

	/* (syspid,systid) */
	*ch++='(';
	param.type  = __STRACE_UINTEGER;
	param.value = pe_get_current_process_id();
	ch += __strace_sprintf(ch,&param);

	*ch++=',';
	param.value = pe_get_current_thread_id();
	ch += __strace_sprintf(ch,&param);
	ch[0] = ')'; ch[1]=':';
	ch += 3;

	/* {pgid:pid}  */
	*ch++='{';
	param.type  = __STRACE_UINTEGER;
	param.value = rtdata->alt_cid_self.pgid;
	ch += __strace_sprintf(ch,&param);

	*ch++=':';
	param.value = rtdata->alt_cid_self.pid;
	ch += __strace_sprintf(ch,&param);
	ch[0] = '}'; ch[1]=':';
	ch += 3;

	/* sysname */
	param.type  = __STRACE_STRING;
	param.value = (intptr_t)__strace_sysnames[params->sysid];
	ch += __strace_sprintf(ch,&param);
	*ch++='(';

	/* params */
	for (i=0; i<params->cparams; i++) {
		if (params->params[i].type == __STRACE_STRING)
			if (params->params[i].value) {
				*ch++ = '\"';
				ch += __strace_sprintf(ch,&params->params[i]);
				*ch++ = '\"';
			} else {
				*ch++ = 'N';
				*ch++ = 'U';
				*ch++ = 'L';
				*ch++ = 'L';
			}
		else
			ch += __strace_sprintf(ch,&params->params[i]);

		if (i < (params->cparams-1)) {
			ch[0] = ',';
			ch += 2;
		}
	}

	ch[0]=')'; ch[2]='=';
	ch += 4;

	/* return value */
	param.type  = params->ret.type;
	param.value = params->ret.value;

	if ((param.value < 0) && (-param.value < EERRORS)) {
		param.type  = __STRACE_STRING;
		param.value = (intptr_t)__psx_error_strings[-param.value];
	}

	*ch++='[';
	ch += __strace_sprintf(ch,&param);
	*ch++=',';

	/* ntstatus */
	status = (__tlca_self())->ntstatus;

	if (!status) {
		__ntapi->tt_generic_memcpy((char *)ch,"NT_STATUS_SUCCESS",17);
		ch += 17;
	} else if (status == EPSXONLY) {
		__ntapi->tt_generic_memcpy((char *)ch,"EPSXONLY",8);
		ch += 8;
	} else {
		*ch++='0';
		*ch++='x';
		__ntapi->tt_uint32_to_hex_utf8(status,ch);
		ch += 8;
	}

	*ch++=']';
	*ch++='\n';

	/* output */
	if (rtctx.ctty)
		__ntapi->pty_write(
			rtctx.ctty->info.hpty,
			0,0,0,&iosb,
			tlca->strace,
			(uint32_t)(ch-tlca->strace),
			0,0);
	else
		__ntapi->zw_write_file(
			rtdata->hlog,
			0,0,0,&iosb,
			tlca->strace,
			(uint32_t)(ch-tlca->strace),
			0,0);

	__psx_dbg_write(-1,tlca->strace,ch-tlca->strace);
}


intptr_t __strace_brk(uintptr_t brk)
{
	struct __strace_params params = {
		__STRACE_BRK,1,
		{__STRACE_POINTER,0},
		{{__STRACE_POINTER,brk}}};

	if (!brk) params.params[0].type = __STRACE_UINTEGER;
	params.ret.value = __sys_brk(brk);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_madvise(void * addr, size_t length, int advice)
{
	struct __strace_params params = {
		__STRACE_MADVISE,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_POINTER,(intptr_t)addr},
		 {__STRACE_UINTEGER,length},
		 {__STRACE_SINTEGER,advice}}};

	params.ret.value = __sys_madvise(addr,length,advice);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_dup(int fildes)
{
	struct __strace_params params = {
		__STRACE_DUP,1,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fildes}}};

	params.ret.value = __sys_dup(fildes);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_dup2(int fildes, int fildes2)
{
	struct __strace_params params = {
		__STRACE_DUP2,2,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fildes},
		 {__STRACE_SINTEGER,fildes2}}};

	params.ret.value = __sys_dup2(fildes,fildes2);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_dup3(int fildes, int fildes2, int flags)
{
	struct __strace_params params = {
		__STRACE_DUP3,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fildes},
		 {__STRACE_SINTEGER,fildes2},
		 {__STRACE_SINTEGER,flags}}};

	params.ret.value = __sys_dup3(fildes,fildes2,flags);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_fcntl(int fdidx, int cmd, void * arg)
{
	struct __strace_params params = {
		__STRACE_FCNTL,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fdidx},
		 {__STRACE_SINTEGER,cmd},
		 {__STRACE_POINTER,(intptr_t)arg}}};

	params.ret.value = __sys_fcntl(fdidx,cmd,arg);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_access(const unsigned char * path, int amode)
{
	struct __strace_params params = {
		__STRACE_ACCESS,2,
		{__STRACE_SINTEGER,0},
		{{__STRACE_STRING,(intptr_t)path},
		 {__STRACE_SINTEGER,amode}}};

	params.ret.value = __sys_access(path,amode);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_faccessat(int fdidxat, const unsigned char * path, int amode, int flag)
{
	struct __strace_params params = {
		__STRACE_ACCESS,4,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fdidxat},
		 {__STRACE_STRING,(intptr_t)path},
		 {__STRACE_SINTEGER,amode},
		 {__STRACE_SINTEGER,flag}}};

	params.ret.value = __sys_faccessat(fdidxat,path,amode,flag);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_chdir(const unsigned char * path)
{
	struct __strace_params params = {
		__STRACE_CHDIR,1,
		{__STRACE_SINTEGER,0},
		{{__STRACE_STRING,(intptr_t)path}}};

	params.ret.value = __sys_chdir(path);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_fchdir(int fdidx)
{
	struct __strace_params params = {
		__STRACE_FCHDIR,1,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,0}}};

	params.ret.value = __sys_fchdir(fdidx);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_chmod(const unsigned char * path, mode_t amode)
{
	struct __strace_params params = {
		__STRACE_CHMOD,2,
		{__STRACE_SINTEGER,0},
		{{__STRACE_STRING,(intptr_t)path},
		 {__STRACE_UINTEGER,amode}}};

	params.ret.value = __sys_chmod(path,amode);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_fchmodat(int fdidxat, const unsigned char * path, mode_t mode, int flag)
{
	struct __strace_params params = {
		__STRACE_CHMOD,4,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fdidxat},
		 {__STRACE_STRING,(intptr_t)path},
		 {__STRACE_UINTEGER,mode},
		 {__STRACE_SINTEGER,flag}}};

	params.ret.value = __sys_fchmodat(fdidxat,path,mode,flag);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_getcwd(char * buf, size_t size)
{
	struct __strace_params params = {
		__STRACE_GETCWD,2,
		{__STRACE_STRING,0},
		{{__STRACE_STRING,(intptr_t)buf},
		 {__STRACE_UINTEGER,(intptr_t)size}}};

	params.ret.value = (intptr_t)__sys_getcwd(buf,size);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_getdents(int fdidx, struct __dirent * dirent, unsigned int count)
{
	struct __strace_params params = {
		__STRACE_GETDENTS,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fdidx},
		 {__STRACE_POINTER,(intptr_t)dirent},
		 {__STRACE_UINTEGER,(intptr_t)count}}};

	params.ret.value = (intptr_t)__sys_getdents(fdidx,dirent,count);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_fstat(int fdidx, struct __stat * xstat)
{
	struct __strace_params params = {
		__STRACE_FSTAT,2,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fdidx},
		 {__STRACE_POINTER,(intptr_t)xstat}}};

	params.ret.value = __sys_fstat(fdidx,xstat);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_fstatat(int fdidxat, const unsigned char * path, struct __stat * xstat, int flag)
{
	struct __strace_params params = {
		__STRACE_FSTATAT,4,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fdidxat},
		 {__STRACE_POINTER,(intptr_t)path},
		 {__STRACE_POINTER,(intptr_t)xstat},
		 {__STRACE_SINTEGER,flag}}};

	params.ret.value = __sys_fstatat(fdidxat,path,xstat,flag);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_lstat(const unsigned char * path, struct __stat * xstat)
{
	struct __strace_params params = {
		__STRACE_LSTAT,2,
		{__STRACE_SINTEGER,0},
		{{__STRACE_STRING,(intptr_t)path},
		 {__STRACE_POINTER,(intptr_t)xstat}}};

	params.ret.value = __sys_lstat(path,xstat);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_stat(const unsigned char * path, struct __stat * xstat)
{
	struct __strace_params params = {
		__STRACE_STAT,2,
		{__STRACE_SINTEGER,0},
		{{__STRACE_STRING,(intptr_t)path},
		 {__STRACE_POINTER,(intptr_t)xstat}}};

	params.ret.value = __sys_stat(path,xstat);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_mkdir(const unsigned char * path, mode_t mode)
{
	struct __strace_params params = {
		__STRACE_MKDIR,2,
		{__STRACE_SINTEGER,0},
		{{__STRACE_STRING,(intptr_t)path},
		 {__STRACE_SINTEGER,mode}}};

	params.ret.value = __sys_mkdir(path,mode);
	__strace(&params);
	return params.ret.value;
}

ssize_t __strace_readlink(const unsigned char * path, const unsigned char * buf, size_t buflen)
{
	struct __strace_params params = {
		__STRACE_READLINK,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_STRING,(intptr_t)path},
		 {__STRACE_STRING,(intptr_t)buf},
		 {__STRACE_UINTEGER,buflen}}};

	params.ret.value = __sys_readlink(path,buf,buflen);
	__strace(&params);
	return params.ret.value;
}

ssize_t __strace_readlinkat(int fdidx, const unsigned char * path, const unsigned char * buf, size_t buflen)
{
	struct __strace_params params = {
		__STRACE_READLINK,4,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fdidx},
		 {__STRACE_STRING,(intptr_t)path},
		 {__STRACE_STRING,(intptr_t)buf},
		 {__STRACE_UINTEGER,buflen}}};

	params.ret.value = __sys_readlinkat(fdidx,path,buf,buflen);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_mkdirat(int fdidxat, const unsigned char * path, mode_t mode)
{
	struct __strace_params params = {
		__STRACE_MKDIRAT,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fdidxat},
		 {__STRACE_STRING,(intptr_t)path},
		 {__STRACE_SINTEGER,mode}}};

	params.ret.value = __sys_mkdirat(fdidxat,path,mode);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_rename(const unsigned char * src, const unsigned char * dst)
{
	struct __strace_params params = {
		__STRACE_RENAME,2,
		{__STRACE_SINTEGER,0},
		{{__STRACE_STRING,(intptr_t)src},
		 {__STRACE_STRING,(intptr_t)dst}}};

	params.ret.value = __sys_rename(src,dst);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_renameat(int srcfd, const unsigned char * src, int dstfd, const unsigned char * dst)
{
	struct __strace_params params = {
		__STRACE_RENAMEAT,4,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,srcfd},
		 {__STRACE_STRING,(intptr_t)src},
		 {__STRACE_SINTEGER,dstfd},
		 {__STRACE_STRING,(intptr_t)dst}}};

	params.ret.value = __sys_renameat(srcfd,src,dstfd,dst);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_rmdir(const unsigned char * path)
{
	struct __strace_params params = {
		__STRACE_RMDIR,1,
		{__STRACE_SINTEGER,0},
		{{__STRACE_STRING,(intptr_t)path}}};

	params.ret.value = __sys_rmdir(path);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_unlink(const unsigned char * path)
{
	struct __strace_params params = {
		__STRACE_UNLINK,1,
		{__STRACE_SINTEGER,0},
		{{__STRACE_STRING,(intptr_t)path}}};

	params.ret.value = __sys_unlink(path);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_unlinkat(int fdidxat, const unsigned char * path, int flag)
{
	struct __strace_params params = {
		__STRACE_UNLINKAT,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_STRING,(intptr_t)path}}};

	params.ret.value = __sys_unlinkat(fdidxat,path,flag);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_ioctl(int fdidx, unsigned long request, void * ptra, void * ptrb, void * ptrc, void * ptrd)
{
	struct __strace_params params = {
		__STRACE_IOCTL,6,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fdidx},
		 {__STRACE_UINTEGER,request},
		 {__STRACE_POINTER,(intptr_t)ptra},
		 {__STRACE_POINTER,(intptr_t)ptrb},
		 {__STRACE_POINTER,(intptr_t)ptrc},
		 {__STRACE_POINTER,(intptr_t)ptrd}}};

	params.ret.value = __sys_ioctl(fdidx,request,ptra,ptrb,ptrc,ptrd);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_pipe(int fildes[2])
{
	struct __strace_params params = {
		__STRACE_PIPE,6,
		{__STRACE_SINTEGER,0},
		{{__STRACE_POINTER,(intptr_t)fildes}}};

	params.ret.value = __sys_pipe(fildes);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_clock_getres(clockid_t clock_id, struct timespec * res)
{
	struct __strace_params params = {
		__STRACE_CLOCK_GETRES,2,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,clock_id},
		 {__STRACE_POINTER,(intptr_t)res}}};

	params.ret.value = __sys_clock_getres(clock_id,res);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_clock_gettime(clockid_t clock_id, struct timespec * tp)
{
	struct __strace_params params = {
		__STRACE_CLOCK_GETTIME,2,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,clock_id},
		 {__STRACE_POINTER,(intptr_t)tp}}};

	params.ret.value = __sys_clock_gettime(clock_id,tp);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_clock_settime(clockid_t clock_id, const struct timespec * tp)
{
	struct __strace_params params = {
		__STRACE_CLOCK_SETTIME,2,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,clock_id},
		 {__STRACE_POINTER,(intptr_t)tp}}};

	params.ret.value = __sys_clock_settime(clock_id,tp);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_gettimeofday(struct timeval * tp, void * tzp)
{
	struct __strace_params params = {
		__STRACE_GETTIMEOFDAY,2,
		{__STRACE_SINTEGER,0},
		{{__STRACE_POINTER,(intptr_t)tp},
		 {__STRACE_POINTER,(intptr_t)tzp}}};

	params.ret.value = __sys_gettimeofday(tp,tzp);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_sched_setscheduler(pid_t pid,int policy,const struct sched_param * param)
{
	struct __strace_params params = {
		__STRACE_SCHED_SETSCHEDULER,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,pid},
		 {__STRACE_SINTEGER,policy},
		 {__STRACE_POINTER,(intptr_t)param}}};

	params.ret.value = __sys_sched_setscheduler(pid,policy,param);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_uname(struct __utsname * utsname)
{
	struct __strace_params params = {
		__STRACE_UNAME,1,
		{__STRACE_SINTEGER,0},
		{{__STRACE_POINTER,(intptr_t)utsname}}};

	params.ret.value = __sys_uname(utsname);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_sysinfo(struct __sysinfo * info)
{
	struct __strace_params params = {
		__STRACE_SYSINFO,1,
		{__STRACE_SINTEGER,0},
		{{__STRACE_POINTER,(intptr_t)info}}};

	params.ret.value = __sys_sysinfo(info);
	__strace(&params);
	return params.ret.value;
}

void * __strace_mmap(void * addr,size_t length,int prot,int flags,int fd,off_t offset)
{
	struct __strace_params params = {
		__STRACE_MMAP,6,
		{__STRACE_POINTER,0},
		{{__STRACE_POINTER,(intptr_t)addr},
		 {__STRACE_SINTEGER,length},
		 {__STRACE_SINTEGER,prot},
		 {__STRACE_SINTEGER,flags},
		 {__STRACE_SINTEGER,fd},
		 {__STRACE_SINTEGER,offset}}};

	if (!addr) params.params[0].type = __STRACE_SINTEGER;
	params.ret.value = (intptr_t)__sys_mmap(addr,length,prot,flags,fd,offset);
	__strace(&params);
	return (void *)params.ret.value;

}

void * __strace_mremap(void * mapaddr, size_t mapsize, size_t newsize, int flags)
{
	struct __strace_params params = {
		__STRACE_MREMAP,6,
		{__STRACE_POINTER,0},
		{{__STRACE_POINTER,(intptr_t)mapaddr},
		 {__STRACE_UINTEGER,mapsize},
		 {__STRACE_UINTEGER,newsize},
		 {__STRACE_SINTEGER,flags}}};

	if (!mapaddr) params.params[0].type = __STRACE_SINTEGER;
	params.ret.value = (intptr_t)__sys_mremap(mapaddr,mapsize,newsize,flags);
	__strace(&params);
	return (void *)params.ret.value;

}

intptr_t __strace_munmap(void * addr,size_t length)
{
	struct __strace_params params = {
		__STRACE_MUNMAP,2,
		{__STRACE_SINTEGER,0},
		{{__STRACE_POINTER,(intptr_t)addr},
		 {__STRACE_SINTEGER,length}}};

	params.ret.value = __sys_munmap(addr,length);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_mount(const char * source,const char * target,const char * fstype,uintptr_t mntflags,const void * data)
{
	intptr_t ret = __sys_mount(source,target,fstype,mntflags,data);
	return ret;
}

intptr_t __strace_close(int fdidx)
{
	struct __strace_params params = {
		__STRACE_CLOSE,1,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fdidx}}};

	params.ret.value = __sys_close(fdidx);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_creat(const unsigned char * path,mode_t mode)
{
	struct __strace_params params = {
		__STRACE_CREAT,2,
		{__STRACE_SINTEGER,0},
		{{__STRACE_STRING,(intptr_t)path},
		 {__STRACE_UINTEGER,mode}}};

	params.ret.value = __sys_creat(path,mode);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_open(const unsigned char * path,int flags,mode_t mode)
{
	struct __strace_params params = {
		__STRACE_OPEN,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_STRING,(intptr_t)path},
		 {__STRACE_SINTEGER,flags},
		 {__STRACE_UINTEGER,mode}}};

	params.ret.value = __sys_open(path,flags,mode);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_openat(int fdidxat, const unsigned char * path,int flags,mode_t mode)
{
	struct __strace_params params = {
		__STRACE_OPENAT,4,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fdidxat},
		 {__STRACE_STRING,(intptr_t)path},
		 {__STRACE_SINTEGER,flags},
		 {__STRACE_UINTEGER,mode}}};

	params.ret.value = __sys_openat(fdidxat, path,flags,mode);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_execve(const unsigned char * path, const char ** argv, const char ** envp)
{
	struct __strace_params params = {
		__STRACE_EXECVE,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_STRING,(uintptr_t)path,},
		 {__STRACE_POINTER,(uintptr_t)argv},
		 {__STRACE_POINTER,(uintptr_t)envp}}};

	__strace(&params);
	params.ret.value = __sys_execve(path,argv,envp);

	params.params[0].value = 0;
	params.params[1].value = 0;
	params.params[2].value = 0;
	__strace(&params);

	return params.ret.value;
}

void __strace_exit(int status)
{
	struct __strace_params params = {
		__STRACE_EXIT,1,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,status}}};

	__tlca_self()->ntstatus = NT_STATUS_SUCCESS;
	__strace(&params);
	__sys_exit(status);
}

void __strace_exit_group(int status)
{
	struct __strace_params params = {
		__STRACE_EXIT_GROUP,1,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,status}}};

	__tlca_self()->ntstatus = NT_STATUS_SUCCESS;
	__strace(&params);
	__sys_exit_group(status);
}

intptr_t __strace_fork(void)
{
	struct __strace_params params = {
		__STRACE_FORK,0,
		{__STRACE_SINTEGER,0}};

	params.ret.value = __sys_fork();
	__strace(&params);
	return params.ret.value;
}

clock_t __strace_times(struct tms * buf)
{
	struct __strace_params params = {
		__STRACE_TIMES,1,
		{__STRACE_SINTEGER,0},
		{{__STRACE_POINTER,(intptr_t)buf}}};

	params.ret.value = __sys_times(buf);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_getrlimit(int resource, struct __rlimit * rlim)
{
	struct __strace_params params = {
		__STRACE_GETRLIMIT,2,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,resource},
		 {__STRACE_POINTER,(intptr_t)rlim}}};

	params.ret.value = __sys_getrlimit(resource,rlim);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_getrusage(int who, struct __rusage * r_usage)
{
	struct __strace_params params = {
		__STRACE_GETRUSAGE,2,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,who},
		 {__STRACE_POINTER,(intptr_t)r_usage}}};

	params.ret.value = __sys_getrusage(who,r_usage);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_prlimit(pid_t pid, int resource, const struct __rlimit * new_limit, struct __rlimit * old_limit)
{
	struct __strace_params params = {
		__STRACE_PRLIMIT,4,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,pid},
		 {__STRACE_SINTEGER,(intptr_t)resource},
		 {__STRACE_POINTER,(intptr_t)new_limit},
		 {__STRACE_POINTER,(intptr_t)old_limit}}};

	params.ret.value = __sys_prlimit(pid,resource,new_limit,old_limit);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_setrlimit(int resource, const struct __rlimit * rlim)
{
	struct __strace_params params = {
		__STRACE_SETRLIMIT,2,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,resource},
		 {__STRACE_POINTER,(intptr_t)rlim}}};

	params.ret.value = __sys_setrlimit(resource,rlim);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_chroot(const unsigned char * path)
{
	struct __strace_params params = {
		__STRACE_CHROOT,0,
		{__STRACE_SINTEGER,0},
		{{__STRACE_STRING,(intptr_t)path}}};

	params.ret.value = __sys_chroot(path);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_getpid(void)
{
	struct __strace_params params = {
		__STRACE_GETPID,0,
		{__STRACE_SINTEGER,0}};

	params.ret.value = __sys_getpid();
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_getppid(void)
{
	struct __strace_params params = {
		__STRACE_GETPPID,0,
		{__STRACE_SINTEGER,0}};

	params.ret.value = __sys_getppid();
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_getpgid(pid_t pid)
{
	struct __strace_params params = {
		__STRACE_GETPGID,1,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,pid}}};

	params.ret.value = __sys_getpgid(pid);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_getpgrp(pid_t pid)
{
	struct __strace_params params = {
		__STRACE_GETPGRP,1,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,pid}}};

	params.ret.value = __sys_getpgrp(pid);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_setpgid(pid_t pid, pid_t pgid)
{
	struct __strace_params params = {
		__STRACE_SETPGID,2,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,pid},
		 {__STRACE_SINTEGER,pgid}}};

	params.ret.value = __sys_setpgid(pid,pgid);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_wait4(pid_t pid, int * status, int options, struct __rusage * rusage)
{
	struct __strace_params params = {
		__STRACE_WAIT4,4,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,pid},
		 {__STRACE_POINTER,(intptr_t)status},
		 {__STRACE_SINTEGER,options},
		 {__STRACE_POINTER,(intptr_t)rusage}}};

	params.ret.value = __sys_wait4(pid,status,options,rusage);
	__strace(&params);
	return (pid_t)params.ret.value;
}

intptr_t __strace_waitid(int idtype, id_t id, siginfo_t * siginfo, int options)
{
	struct __strace_params params = {
		__STRACE_WAITID,4,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,idtype},
		 {__STRACE_SINTEGER,id},
		 {__STRACE_POINTER,(intptr_t)siginfo},
		 {__STRACE_SINTEGER,options}}};

	params.ret.value = __sys_waitid(idtype,id,siginfo,options);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_rt_sigaction(int signum, const struct __sigaction * act, struct __sigaction * oldact, size_t sigsetsize)
{
	struct __strace_params params = {
		__STRACE_RT_SIGACTION,4,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,signum},
		 {__STRACE_POINTER,(intptr_t)act},
		 {__STRACE_POINTER,(intptr_t)oldact},
		 {__STRACE_UINTEGER,sigsetsize}}};

	params.ret.value = __sys_rt_sigaction(signum,act,oldact,sigsetsize);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_rt_sigprocmask(int how, const sigset_t * set, sigset_t * oldset)
{
	struct __strace_params params = {
		__STRACE_RT_SIGPROCMASK,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,how},
		 {__STRACE_POINTER,(intptr_t)set},
		 {__STRACE_POINTER,(intptr_t)oldset}}};

	params.ret.value = __sys_rt_sigprocmask(how,set,oldset);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_getitimer(enum __psx_timer_type which, struct itimerval *curr_value)
{
	struct __strace_params params = {
		__STRACE_RT_GETITIMER,2,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,which},
		 {__STRACE_POINTER,(intptr_t)curr_value}}};

	params.ret.value = __sys_getitimer(which,curr_value);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_setitimer(enum __psx_timer_type which, const struct itimerval * new_value, struct itimerval * old_value)
{
	struct __strace_params params = {
		__STRACE_RT_SETITIMER,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,which},
		 {__STRACE_POINTER,(intptr_t)new_value},
		 {__STRACE_POINTER,(intptr_t)new_value}}};

	params.ret.value = __sys_setitimer(which,new_value,old_value);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_accept(int socket, struct __sockaddr * addr, socklen_t * addrlen)
{
	struct __strace_params params = {
		__STRACE_ACCEPT,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,socket},
		 {__STRACE_POINTER,(intptr_t)addr},
		 {__STRACE_POINTER,(intptr_t)addrlen}}};

	params.ret.value = __sys_accept(socket,addr,addrlen);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_bind(int socket, const struct __sockaddr * addr, socklen_t addrlen)
{
	struct __strace_params params = {
		__STRACE_BIND,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,socket},
		 {__STRACE_POINTER,(intptr_t)addr},
		 {__STRACE_POINTER,(intptr_t)addrlen}}};

		 params.ret.value = __sys_bind(socket,addr,addrlen);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_connect(int socket, const struct __sockaddr * addr, socklen_t addrlen)
{
	struct __strace_params params = {
		__STRACE_CONNECT,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,socket},
		 {__STRACE_POINTER,(intptr_t)addr},
		 {__STRACE_POINTER,(intptr_t)addrlen}}};

	params.ret.value = __sys_connect(socket,addr,addrlen);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_getpeername(int socket, struct __sockaddr * addr, socklen_t * addrlen)
{
	struct __strace_params params = {
		__STRACE_GETPEERNAME,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,socket},
		 {__STRACE_POINTER,(intptr_t)addr},
		 {__STRACE_POINTER,(intptr_t)addrlen}}};

	params.ret.value = __sys_getpeername(socket,addr,addrlen);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_getsockname(int socket, struct __sockaddr * addr, socklen_t * addrlen)
{
	struct __strace_params params = {
		__STRACE_GETSOCKNAME,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,socket},
		 {__STRACE_POINTER,(intptr_t)addr},
		 {__STRACE_POINTER,(intptr_t)addrlen}}};

	params.ret.value = __sys_getsockname(socket,addr,addrlen);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_getsockopt(int socket, int level, int optname, void * optval, socklen_t * optlen)
{
	struct __strace_params params = {
		__STRACE_GETSOCKOPT,5,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,socket},
		 {__STRACE_SINTEGER,level},
		 {__STRACE_SINTEGER,optname},
		 {__STRACE_POINTER,(intptr_t)optval},
		 {__STRACE_POINTER,(intptr_t)optlen}}};

	params.ret.value = __sys_getsockopt(socket,level,optname,optval,optlen);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_listen(int socket, int backlog)
{
	struct __strace_params params = {
		__STRACE_SHUTDOWN,2,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,socket},
		 {__STRACE_SINTEGER,backlog}}};

	params.ret.value = __sys_listen(socket,backlog);
	__strace(&params);
	return params.ret.value;
}

ssize_t __strace_recvfrom(int socket, void * msg, size_t len, int flags, struct __sockaddr * addr, socklen_t * addrlen)
{
	struct __strace_params params = {
		__STRACE_ACCEPT,6,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,socket},
		 {__STRACE_POINTER,(intptr_t)msg},
		 {__STRACE_UINTEGER,len},
		 {__STRACE_SINTEGER,flags},
		 {__STRACE_POINTER,(intptr_t)addr},
		 {__STRACE_POINTER,(intptr_t)addrlen}}};

	params.ret.value = __sys_recvfrom(socket,msg,len,flags,addr,addrlen);
	__strace(&params);
	return params.ret.value;
}

ssize_t __strace_recvmsg(int socket, struct __msghdr * msg, int flags)
{
	struct __strace_params params = {
		__STRACE_ACCEPT,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,socket},
		 {__STRACE_POINTER,(intptr_t)msg},
		 {__STRACE_SINTEGER,flags}}};

	params.ret.value = __sys_recvmsg(socket,msg,flags);
	__strace(&params);
	return params.ret.value;
}

ssize_t __strace_sendmsg(int socket, const struct __msghdr * msg, int flags)
{
	struct __strace_params params = {
		__STRACE_ACCEPT,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,socket},
		 {__STRACE_POINTER,(intptr_t)msg},
		 {__STRACE_SINTEGER,flags}}};

	params.ret.value = __sys_sendmsg(socket,msg,flags);
	__strace(&params);
	return params.ret.value;
}

ssize_t __strace_sendto(int socket, const void * msg, size_t len, int flags, const struct __sockaddr * addr, socklen_t addrlen)
{
	struct __strace_params params = {
		__STRACE_ACCEPT,6,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,socket},
		 {__STRACE_POINTER,(intptr_t)msg},
		 {__STRACE_UINTEGER,len},
		 {__STRACE_SINTEGER,flags},
		 {__STRACE_POINTER,(intptr_t)addr},
		 {__STRACE_POINTER,(intptr_t)addrlen}}};

	params.ret.value = __sys_sendto(socket,msg,len,flags,addr,addrlen);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_setsockopt(int socket, int level, int optname, const void * optval, socklen_t optlen)
{
	struct __strace_params params = {
		__STRACE_SETSOCKOPT,5,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,socket},
		 {__STRACE_SINTEGER,level},
		 {__STRACE_SINTEGER,optname},
		 {__STRACE_POINTER,(intptr_t)optval},
		 {__STRACE_POINTER,(intptr_t)optlen}}};

	params.ret.value = __sys_setsockopt(socket,level,optname,optval,optlen);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_shutdown(int socket, int how)
{
	struct __strace_params params = {
		__STRACE_SHUTDOWN,2,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,socket},
		 {__STRACE_SINTEGER,how}}};

	params.ret.value = __sys_shutdown(socket,how);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_socket(int domain, int type, int protocol)
{
	struct __strace_params params = {
		__STRACE_SOCKET,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,domain},
		 {__STRACE_SINTEGER,type},
		 {__STRACE_SINTEGER,protocol}}};

	params.ret.value = __sys_socket(domain,type,protocol);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_socketpair(int domain, int type, int protocol,int socket_vector[2])
{
	struct __strace_params params = {
		__STRACE_SOCKET,4,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,domain},
		 {__STRACE_SINTEGER,type},
		 {__STRACE_SINTEGER,protocol},
		 {__STRACE_POINTER,(intptr_t)socket_vector}}};

	params.ret.value = __sys_socketpair(domain,type,protocol,socket_vector);
	__strace(&params);
	return params.ret.value;
}

off_t __strace_fsync(int fd)
{
	struct __strace_params params = {
		__STRACE_FSYNC,1,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fd}}};

	params.ret.value = __sys_fsync(fd);
	__strace(&params);
	return params.ret.value;
}

off_t __strace_lseek(int fd,off_t offset,int whence)
{
	struct __strace_params params = {
		__STRACE_LSEEK,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fd},
		 {__STRACE_SINTEGER,offset},
		 {__STRACE_SINTEGER,whence}}};

	params.ret.value = __sys_lseek(fd,offset,whence);
	__strace(&params);
	return params.ret.value;
}

ssize_t __strace_pread(int fd,void * buf,size_t bytes, off_t offset)
{
	struct __strace_params params = {
		__STRACE_READ,4,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fd},
		 {__STRACE_BUFFER,(intptr_t)buf},
		 {__STRACE_UINTEGER,bytes},
		 {__STRACE_SINTEGER,offset}}};

	params.ret.value = __sys_pread(fd,buf,bytes,offset);

	if (params.ret.value <= 0)
		params.params[1].type = __STRACE_POINTER;
	else if (!(__ntapi->uc_validate_unicode_stream_utf8((unsigned char *)buf,bytes,0,0,0,0)))
		params.params[1].type = __STRACE_STRING;

	__strace(&params);
	return params.ret.value;
}

ssize_t __strace_pwrite(int fd,const void * buf,size_t bytes, off_t offset)
{
	uintptr_t buffer[8];

	struct __strace_params params = {
		__STRACE_PWRITE,4,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fd},
		 {__STRACE_BUFFER,(intptr_t)buf},
		 {__STRACE_UINTEGER,bytes},
		 {__STRACE_SINTEGER,offset}}};

	if (bytes == 0)
		params.params[1].type = __STRACE_POINTER;
	else if (!(__ntapi->uc_validate_unicode_stream_utf8((unsigned char *)buf,bytes,0,0,0,0))) {
		params.params[1].type = __STRACE_STRING;
		__ntapi->tt_aligned_block_memset(buffer,0,sizeof(buffer));
		__ntapi->tt_generic_memcpy(
			(char *)buffer,
			(char *)buf,
			bytes>24 ? 24 : bytes);
	}

	params.ret.value = __sys_pwrite(fd,buf,bytes,offset);
	__strace(&params);
	return params.ret.value;
}

ssize_t __strace_read(int fd,void * buf,size_t bytes)
{
	struct __strace_params params = {
		__STRACE_READ,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fd},
		 {__STRACE_BUFFER,(intptr_t)buf},
		 {__STRACE_UINTEGER,bytes}}};

	params.ret.value = __sys_read(fd,buf,bytes);

	if (params.ret.value <= 0)
		params.params[1].type = __STRACE_POINTER;
	else if (!(__ntapi->uc_validate_unicode_stream_utf8((unsigned char *)buf,bytes,0,0,0,0)))
		params.params[1].type = __STRACE_STRING;

	__strace(&params);
	return params.ret.value;
}

ssize_t __strace_readv(int fd,const struct iovec * iov,int iovcnt)
{
	struct __strace_params params = {
		__STRACE_READV,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fd},
		 {__STRACE_POINTER,(intptr_t)iov},
		 {__STRACE_SINTEGER,iovcnt}}};

	params.ret.value = __sys_readv(fd,iov,iovcnt);
	__strace(&params);
	return params.ret.value;
}

ssize_t __strace_write(int fd,const void * buf,size_t bytes)
{
	uintptr_t buffer[8];

	struct __strace_params params = {
		__STRACE_WRITE,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fd},
		 {__STRACE_BUFFER,(intptr_t)buf},
		 {__STRACE_UINTEGER,bytes}}};

	if (bytes == 0)
		params.params[1].type = __STRACE_POINTER;
	else if (!(__ntapi->uc_validate_unicode_stream_utf8((unsigned char *)buf,bytes,0,0,0,0))) {
		params.params[1].type = __STRACE_STRING;
		__ntapi->tt_aligned_block_memset(buffer,0,sizeof(buffer));
		__ntapi->tt_generic_memcpy(
			(char *)buffer,
			(char *)buf,
			bytes>24 ? 24 : bytes);
	}

	params.ret.value = __sys_write(fd,buf,bytes);
	__strace(&params);
	return params.ret.value;
}

ssize_t __strace_writev(int fd,const struct iovec * iov,int iovcnt)
{
	struct __strace_params params = {
		__STRACE_WRITEV,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,fd},
		 {__STRACE_POINTER,(intptr_t)iov},
		 {__STRACE_SINTEGER,iovcnt}}};

	params.ret.value = __sys_writev(fd,iov,iovcnt);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_gettid(void)
{
	struct __strace_params params = {
		__STRACE_GETTID,0,
		{__STRACE_SINTEGER,0}};

	params.ret.value = __sys_gettid();
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_set_tid_address(int * tidptr)
{
	struct __strace_params params = {
		__STRACE_SET_TID_ADDRESS,1,
		{__STRACE_UINTEGER,0},
		{{__STRACE_POINTER,(intptr_t)tidptr}}};

	params.ret.value = __sys_set_tid_address(tidptr);
	__strace(&params);
	return params.ret.value;
}

gid_t __strace_getegid(void)
{
	struct __strace_params params = {
		__STRACE_GETEGID,0,
		{__STRACE_SINTEGER,0}};

	params.ret.value = __sys_getegid();
	__strace(&params);
	return (gid_t)params.ret.value;
}

uid_t __strace_geteuid(void)
{
	struct __strace_params params = {
		__STRACE_GETEUID,0,
		{__STRACE_SINTEGER,0}};

	params.ret.value = __sys_geteuid();
	__strace(&params);
	return (uid_t)params.ret.value;
}

gid_t __strace_getgid(void)
{
	struct __strace_params params = {
		__STRACE_GETGID,0,
		{__STRACE_SINTEGER,0}};

	params.ret.value = __sys_getgid();
	__strace(&params);
	return (gid_t)params.ret.value;
}

uid_t __strace_getuid(void)
{
	struct __strace_params params = {
		__STRACE_GETUID,0,
		{__STRACE_SINTEGER,0}};

	params.ret.value = __sys_getuid();
	__strace(&params);
	return (uid_t)params.ret.value;
}

intptr_t __strace_setgid(gid_t gid)
{
	struct __strace_params params = {
		__STRACE_SETGID,1,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,gid}}};

	params.ret.value = __sys_setgid(gid);
	__strace(&params);
	return params.ret.value;
}

intptr_t __strace_setuid(uid_t uid)
{
	struct __strace_params params = {
		__STRACE_SETUID,1,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,uid}}};

	params.ret.value = __sys_setuid(uid);
	__strace(&params);
	return params.ret.value;
}

void __strace_signal_trampoline_prolog(void)
{
	struct __strace_params params = {
		__STRACE_SIGNAL_TRAMPOLINE_PROLOG,0,
		{__STRACE_SINTEGER,0}};

	__strace(&params);
}

void __strace_signal_trampoline_epilog(void)
{
	struct __strace_params params = {
		__STRACE_SIGNAL_TRAMPOLINE_EPILOG,0,
		{__STRACE_SINTEGER,0}};

	__strace(&params);
}

void __strace_signal_handler_invocation(int signum, siginfo_t * info, void * ctx)
{
	struct __strace_params params = {
		__STRACE_SIGNAL_HANDLER_INVOCATION,3,
		{__STRACE_SINTEGER,0},
		{{__STRACE_SINTEGER,signum},
		 {__STRACE_POINTER,(intptr_t)info},
		 {__STRACE_POINTER,(intptr_t)ctx}}};

	__strace(&params);
}
