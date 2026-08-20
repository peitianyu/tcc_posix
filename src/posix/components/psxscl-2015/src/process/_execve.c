/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_impl.h"
#include "psx_systypes.h"
#include "psx_daemon.h"
#include "psx_init.h"
#include "psx_tlca.h"
#include "psx_acl.h"
#include "psx_helper.h"
#include "psx_unicode.h"
#include "psx_process.h"
#include "psx_exit.h"
#include "psx_errno.h"
#include "psx_debug.h"
#include "psx.h"

#define __EXECVE_LAST_ELEMENT_LEN	512
#define __EXECVE_CLERICAL_SUFFIX_LEN	36

enum __execve_ofd_types {
	__EXECVE_OFD_READ,
	__EXECVE_OFD_EXEC,
	__EXECVE_OFD_CAP
};


static intptr_t __execve_return(
	struct __psx_tlca *	tlca,
	struct __ofd *		interpreter,
	struct __ofd *		target[__EXECVE_OFD_CAP],
	intptr_t		ret)
{
	if (interpreter)
		__psx_ofd_free(tlca->ctx,interpreter);

	__psx_ofd_free(tlca->ctx,target[__EXECVE_OFD_READ]);
	__psx_ofd_free(tlca->ctx,target[__EXECVE_OFD_EXEC]);

	return ret
		? __psx_sig_epilog(tlca,ret,tlca->ntstatus)
		: __psx_exit(0);
}

static void __execve_init_name_from_path_info(
	struct __psx_tlca *	tlca,
	struct __path_info *	path_info,
	char *			buffer,
	char **			chnext)
{
	wchar16_t *	wch;
	size_t		len;

	wch = path_info->lastmark[0];
	len = sizeof(wchar16_t)*(uint16_t)(path_info->lastmark[1]-path_info->lastmark[0]);

	if ((path_info->lastmark[0][0] == '/') || (path_info->lastmark[0][0] == '\\')) {
		wch++;
		len -= sizeof(wchar16_t);
	};

	__psx_strconv_utf16_to_utf8(
		tlca,wch,buffer,len,
		__EXECVE_LAST_ELEMENT_LEN,
		chnext);
}

static int32_t __execve_open(
	struct __psx_tlca *	tlca,
	const unsigned char *	path,
	struct __ofd *		target[__EXECVE_OFD_CAP])
{
	int32_t			status;
	struct __path_info	path_info;
	struct __ofd *		ofdat;
	struct __ofd *		ofd;
	char **			suffix;
	char *			suffixes[] = {"",".exe",".com",".bat",0};
	unsigned char		buffer[__EXECVE_LAST_ELEMENT_LEN + __EXECVE_CLERICAL_SUFFIX_LEN];
	char *			ch;
	char *			ext;

	if ((status = __psx_path_open(
			tlca,&path_info,path,
			0,0,0,0,
			PSX_PATH_SKIP_LAST)))
		return status;

	ofd = 0;
	ofdat = path_info.ofd;
	at_locked_add_32(&ofdat->info.refcnt,sizeof(suffixes)/sizeof(char *));

	__execve_init_name_from_path_info(
		tlca,&path_info,
		(char *)buffer,&ext);

	for (suffix=suffixes; *suffix && !ofd; suffix++) {
		for (ch=ext; **suffix; (*suffix)++)
			*ch++ = **suffix;

		*ch = '\0';

		if (!(status = __psx_path_open(
				tlca,&path_info,buffer,
				0,0,ofdat,0,
				PSX_PATH_OPEN_AT \
					| PSX_PATH_ACCESS_CHECK \
					| PSX_PATH_ACCESS_READ	\
					| PSX_PATH_EXPLICIT_LAST)))
			ofd = path_info.ofd;
	}

	if (ofd && (status = __psx_path_open(
			tlca,&path_info,buffer,
			0,0,ofdat,0,
			PSX_PATH_OPEN_AT \
				| PSX_PATH_ACCESS_CHECK \
				| PSX_PATH_ACCESS_EXEC	\
				| PSX_PATH_EXPLICIT_LAST)))
		__psx_ofd_free(tlca->ctx,ofd);

	target[__EXECVE_OFD_READ] = ofd;
	target[__EXECVE_OFD_EXEC] = path_info.ofd;

	__psx_ofd_free(tlca->ctx,ofdat);

	return status;
}

static struct __ofd * __execve_open_interpreter(
	struct __psx_tlca *	tlca,
	struct __ofd *		target)
{
	int32_t		status;
	nt_iosb		iosb;
	struct __ofd *	ofd[__EXECVE_OFD_CAP];
	unsigned char * path;
	unsigned char * cap;
	unsigned char * ch;

	if ((status = __iovtbl[target->info.fdtype].read(
			target->info.hfile,
			0,0,0,
			&iosb,tlca->buffer,
			__EXECVE_LAST_ELEMENT_LEN,
			0,0)))
		return (struct __ofd *)NT_INVALID_HANDLE_VALUE;

	else if (iosb.info < 5) /* {'#','!','/'','x','\n'} */
		return (struct __ofd *)NT_INVALID_HANDLE_VALUE;

	else if ((tlca->buffer[0] & 0xffff) != ('#' + ('!' << 8)))
		return 0;

	path = (unsigned char *)tlca->buffer + 2;
	cap  = (unsigned char *)tlca->buffer + iosb.info;

	for (; (path<cap) && (*path == ' '); )
		path++;

	for (ch=path; (ch<cap) && (*ch != '\n'); )
		ch++;

	if ((ch == cap) || (*ch != '\n'))
		return (struct __ofd *)NT_INVALID_HANDLE_VALUE;
	else
		*ch = '\0';

	if (__execve_open(tlca,path,ofd))
		return (struct __ofd *)NT_INVALID_HANDLE_VALUE;

	__psx_ofd_free(tlca->ctx,ofd[__EXECVE_OFD_READ]);

	return ofd[__EXECVE_OFD_EXEC];
}


intptr_t __sys_execve(const unsigned char * path, const char ** argv, const char ** envp)
{
	struct __psx_tlca *	tlca;
	struct __ofd *		target[__EXECVE_OFD_CAP]={0};
	struct __ofd *		interpreter;
	struct __ofd *		image;
	void *			hsection;
	void *			addr;
	size_t			vsize;
	nt_large_integer	ssize;
	nt_oa			oa;
	uint32_t		flags;
	nt_alt_cid		altcid;
	__psx_create_process *	execfn;

	tlca = __tlca_self();
	__psx_sig_prolog(tlca);

	if ((tlca->ntstatus = __execve_open(tlca,path,target)))
		return __psx_sig_epilog(tlca,-ENOENT,tlca->ntstatus);

	if ((interpreter = __execve_open_interpreter(tlca,target[__EXECVE_OFD_READ])))
		if (interpreter == NT_INVALID_HANDLE_VALUE)
			return __execve_return(tlca,0,target,-ENOEXEC);

	image = interpreter
		? interpreter
		: target[__EXECVE_OFD_EXEC];

	oa.len		= sizeof(oa);
	oa.root_dir	= 0;
	oa.obj_name	= 0;
	oa.obj_attr	= 0;
	oa.sec_desc	= __PSX_DEF_SEC_DESC;
	oa.sec_qos	= __PSX_DEF_SEC_QOS;

	flags		= 0;
	addr		= 0;
	ssize.quad	= 0;
	vsize		= 0x1000;

	if ((tlca->ntstatus = __ntapi->zw_create_section(
			&hsection,
			NT_SECTION_QUERY|NT_SECTION_MAP_EXECUTE,
			&oa,&ssize,
			NT_PAGE_EXECUTE,
			NT_SEC_IMAGE,
			image->info.hfile)))
		return __execve_return(tlca,0,target,-ENOEXEC);

	if ((tlca->ntstatus = __ntapi->zw_map_view_of_section(
			hsection,
			NT_CURRENT_PROCESS_HANDLE,
			&addr,0,0,0,
			&vsize,NT_VIEW_UNMAP,
			0,NT_PAGE_EXECUTE)))
		if (tlca->ntstatus != NT_STATUS_IMAGE_NOT_AT_BASE)
			return __execve_return(tlca,0,target,-ENOEXEC);

	if (pe_get_image_named_section_addr(addr,".midipix"))
		execfn = __psx_create_native_process;
	else
		execfn = __psx_create_foreign_process;

	__ntapi->tt_aligned_block_memcpy(
		(uintptr_t *)&altcid,
		(uintptr_t *)&rtdata->alt_cid_self,
		sizeof(altcid));

	if (tlca->ctx == &rtctx)
		flags |= PSX_CREATE_PROCESS_PID_TRANSFER|PSX_CREATE_PROCESS_CLONE_FD_TABLES;
	else
		altcid.pid =  0;

	if ((tlca->ntstatus = execfn(
			tlca,image,interpreter,0,
			path,argv,envp,
			flags,&altcid,tlca->execve)))
		return __execve_return(tlca,interpreter,target,-ENOMEM);

	__ntapi->zw_unmap_view_of_section(
		NT_CURRENT_PROCESS_HANDLE,
		addr);

	if (tlca->ctx == &rtctx)
		__ntapi->zw_terminate_process(
			NT_CURRENT_PROCESS_HANDLE,
			tlca->exitcode);

	return __execve_return(tlca,interpreter,target,0);
}
