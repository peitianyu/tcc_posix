/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx_fcntl.h"
#include "psx_ofd.h"
#include "psx_stat.h"
#include "psx_path.h"
#include "psx_impl.h"
#include "psx_tlca.h"
#include "psx.h"

static int32_t __path_open_cancel(
	struct __psx_tlca *	tlca,
	struct __path_info *	path_info,
	int32_t			ret,
	int32_t			status)
{
	if (path_info->hat && (path_info->pathflags & PSX_PATH_CLOSE_AT))
		__iovtbl[path_info->fdtypeat].close(path_info->hat);

	if (path_info->hfile)
		__iovtbl[path_info->fdtype].close(path_info->hfile);

	if (path_info->ofd)
		__psx_ofd_free(tlca->ctx,path_info->ofd);

	path_info->psxstatus = ret;
	return status;
}

int32_t __fastcall __psx_path_open(
	struct __psx_tlca *	tlca,
	struct __path_info *	path_info,
	const unsigned char *	path_arg,
	int			flags,
	mode_t			mode,
	struct __ofd *		ofdat,
	intptr_t		fdidxat,
	uint32_t		exflags)
{
	int32_t			status;
	unsigned char *		elements[64];
	nt_iosb			iosb;
	nt_fdi			fdi;

	/* validate */
	if (!path_arg || !*path_arg) {
		tlca->ntstatus = EPSXONLY;
		return -EINVAL;
	}

	/* prolog */
	__ntapi->tt_aligned_block_memset(
		path_info,0,sizeof(*path_info));

	path_info->ofdat	    = ofdat;
	path_info->fdidxat  = fdidxat;
	path_info->fexflags = exflags;
	path_info->inbuf    = tlca->buffer;
	path_info->avail    = tlca->buflen;

	/* semantics */
	if (exflags & PSX_PATH_ACCESS_CHECK) {
		if (exflags & PSX_PATH_ACCESS_EXEC)
			path_info->faccess |= NT_FILE_EXECUTE;

		if (exflags & PSX_PATH_ACCESS_WRITE)
			path_info->faccess |= NT_FILE_WRITE_DATA | NT_FILE_WRITE_ATTRIBUTES;

		if (exflags & PSX_PATH_ACCESS_READ)
			path_info->faccess |= NT_FILE_READ_DATA | NT_FILE_READ_ATTRIBUTES;

		if (exflags & PSX_PATH_ATTR_READ)
			path_info->faccess |= NT_FILE_READ_ATTRIBUTES;

		if (exflags & PSX_PATH_LIST_DIR)
			path_info->faccess |= (NT_FILE_LIST_DIRECTORY | NT_SEC_SYNCHRONIZE);
	} else {
		switch (flags & (O_RDONLY | O_WRONLY | O_RDWR | O_EXEC)) {
			case O_RDONLY:
				path_info->faccess = NT_FILE_READ_DATA | NT_FILE_READ_ATTRIBUTES;
				break;

			case O_WRONLY:
				path_info->faccess = NT_FILE_WRITE_DATA | NT_FILE_WRITE_ATTRIBUTES;
				break;

			case O_RDWR:
				path_info->faccess = NT_FILE_READ_DATA | NT_FILE_WRITE_DATA | NT_FILE_READ_ATTRIBUTES | NT_FILE_WRITE_ATTRIBUTES;
				break;

			case O_EXEC:
				path_info->faccess = NT_FILE_EXECUTE;
				break;

			default:
				return __path_open_cancel(tlca,path_info,-EINVAL,EPSXONLY);
		}
	}

	if ((flags & O_APPEND) == O_APPEND) {
		path_info->faccess &= ~NT_FILE_WRITE_DATA;
		path_info->faccess |= NT_FILE_APPEND_DATA;
	}

	if ((flags & O_TMPFILE) == O_TMPFILE) {
		path_info->fattr	    = NT_FILE_ATTRIBUTE_TEMPORARY;
		path_info->faccess |= (NT_FILE_WRITE_ATTRIBUTES | NT_SEC_DELETE);
		fdi.delete_file = 1;
	} else if ((flags & O_DIRECTORY) == O_DIRECTORY) {
		path_info->fattr	    = NT_FILE_ATTRIBUTE_DIRECTORY;
		path_info->foptions = NT_FILE_DIRECTORY_FILE;

		if ((flags & O_SEARCH) == O_SEARCH) {
			path_info->faccess &= ~(NT_FILE_EXECUTE|NT_FILE_READ_ACCESS);
			path_info->faccess |= NT_FILE_LIST_DIRECTORY;
		}
	}

	if ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL))
		path_info->fdisposition = NT_FILE_CREATE;
	else if ((flags & O_CREAT) == O_CREAT)
		path_info->fdisposition = NT_FILE_OPEN_IF;
	else
		path_info->fdisposition = NT_FILE_OPEN;

	if (exflags & PSX_PATH_SHARE_NONE)
		path_info->fshare = 0;
	else
		path_info->fshare = NT_FILE_SHARE_READ | NT_FILE_SHARE_WRITE | NT_FILE_SHARE_DELETE;

	if (exflags & PSX_PATH_ACCESS_DELETE)
		path_info->faccess |= NT_SEC_DELETE;

	if (!(exflags & PSX_PATH_INTERNAL_CALL))
		path_info->fobjattr |= NT_OBJ_INHERIT;

	/* tcc_posix: /tmp 映射到用户临时目录 (环境变量 TMP)
	   psxscl 的 root 是系统根 (C:\), /tmp 不存在.
	   用栈缓冲构造新路径 (独立于 inbuf, 不与 normalize 的
	   UTF16 输出冲突). 注意用 tt_generic_memcpy (字节拷贝),
	   tt_aligned_block_memcpy 按 uintptr_t 块会截断. */
	if ((path_arg[0] == '/') && (path_arg[1] == 't') && (path_arg[2] == 'm')
	    && (path_arg[3] == 'p') && (path_arg[4] == 0 || path_arg[4] == '/')) {
		char **envp;
		const unsigned char *tmp_path = 0;
		size_t tmplen = 0;
		size_t restlen = __ntapi->tt_string_null_offset_multibyte(path_arg + 4);
		unsigned char newpath[512];
		unsigned char *dst = newpath;

		for (envp = rtctx.envp_utf8; envp && *envp; envp++) {
			if ((*envp)[0] == 'T' && (*envp)[1] == 'M' && (*envp)[2] == 'P'
			    && (*envp)[3] == '=') {
				tmp_path = (const unsigned char *)(*envp + 4);
				tmplen = __ntapi->tt_string_null_offset_multibyte(tmp_path);
				break;
			}
		}
		if (tmp_path && tmplen && (tmplen + restlen + 2) < sizeof(newpath)) {
			__ntapi->tt_generic_memcpy(dst, tmp_path, tmplen);
			dst += tmplen;
			if (path_arg[4] == '/') {
				if (dst[-1] != '/')
					*dst++ = '/';
				__ntapi->tt_generic_memcpy(dst, path_arg + 5, restlen);
				dst += restlen;
			} else if (dst[-1] != '/') {
				*dst++ = '/';
			}
			*dst = 0;
			path_arg = newpath;
		}
	}

	/* normalize */
	if (__psx_normalize_path_utf8(path_arg,path_info))
		return __path_open_cancel(tlca,path_info,-EILSEQ,EPSXONLY);

	/* parse */
	path_info->elements	= elements;
	path_info->arptrs	= 64;

	if (__psx_parse_normalized_path_utf8(path_info,0))
		return __path_open_cancel(tlca,path_info,-ENOMEM,EPSXONLY);

	path_info->psxflags = flags;
	path_info->psxmode  = mode;
	path_info->outbuf   = (unsigned char *)path_info->inbuf + path_info->used;

	/* resolve */
	switch ((status = __psx_resolve_parsed_path_utf8(path_info))) {
		case NT_STATUS_SUCCESS:
			break;

		case NT_STATUS_OBJECT_NAME_COLLISION:
			return __path_open_cancel(tlca,path_info,-EEXIST,status);

		default:
			return __path_open_cancel(tlca,path_info,-ENOENT,status);
	}

	/* disposition */
	if (path_info->fattr == NT_FILE_ATTRIBUTE_TEMPORARY)
		if ((status = __iovtbl->set(
				path_info->hfile,
				&iosb,&fdi,sizeof(fdi),
				NT_FILE_DISPOSITION_INFORMATION)))
			return __path_open_cancel(tlca,path_info,-EPERM,status);

	/* ofd */
	if (!(path_info->ofd = __psx_ofd_alloc(tlca->ctx,&path_info->ofdidx)))
		return __path_open_cancel(tlca,path_info,-ENOMEM,EPSXONLY);

	/* alloc (virtual types only) */
	if ((status = __iovtbl[path_info->fdtype].alloc(path_info)))
		return __path_open_cancel(tlca,path_info,-ENOMEM,EPSXONLY);

	/* finalize */
	path_info->ofd->info.hfile = path_info->hfile;
	path_info->ofd->info.fdtype = path_info->fdtype;
	path_info->ofd->info.psxflags = path_info->psxflags;
	path_info->ofd->info.ntflags = ((path_info->psxflags & O_NONBLOCK)
					? NT_FILE_ASYNCHRONOUS_IO
					: NT_FILE_SYNCHRONOUS_IO_ALERT);
	return status;
}
