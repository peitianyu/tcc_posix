/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_limits.h"
#include "psx_path.h"
#include "psx_fcntl.h"
#include "psx_iovtbl.h"
#include "psx.h"


typedef int __fastcall __is_particular_element(
	struct __path_info *	path_info,
	int32_t			index);


typedef int __fastcall __process_single_element(
	struct __path_info *	path_info,
	int32_t			index);


static __is_particular_element	__path_element_is_this_dir;
static __is_particular_element	__path_element_is_parent_dir;
static __process_single_element	__path_element_utf8_to_native_element_utf16;
static __process_single_element	__path_swap_at;
static __process_single_element	__path_open_parent;
static __process_single_element	__path_open_next;
static __process_single_element	__path_open_last;
static __process_single_element __path_update_type;


int32_t __fastcall __psx_resolve_parsed_path_utf8(
	struct __path_info * path_info)
{
	int		status;
	int		index;
	int		startidx;
	unsigned char *	ch;
	unsigned char *	drive_letter;

	path_info->tlca = __tlca_self();
	path_info->ctx  = path_info->tlca->ctx;

	ch			= path_info->elements[0];
	path_info->fdtypeat	= PSX_FD_OS_FS_DIR;
	path_info->dirflags	= NT_FILE_DIRECTORY_FILE;
	path_info->ntaccess	= NT_FILE_READ_ACCESS;
	path_info->ntdisposition= NT_FILE_OPEN;
	path_info->ntshare	= NT_FILE_SHARE_READ | NT_FILE_SHARE_WRITE | NT_FILE_SHARE_DELETE;

	/* posix (implementation defined): DOS drive or UNC path? */
	if ((*ch == '/') && (*(++ch) == '/')) {
		drive_letter = ++ch;
		ch++;

		if ((*ch == '/') || (*ch == '\\') || (*ch == 0)) {
			/* dos drive */
			status = __psx_get_dos_drive_root_handle(
				&path_info->hat,
				drive_letter);

			if (*ch == 0) {
				path_info->hfile = path_info->hat;
				return status;
			} else
				startidx = 2;
		} else {
			/* UNC implemented: integrate upon validation */
			return NT_STATUS_NOT_IMPLEMENTED;
		}
	} else {
		/* native notation: UNC path? */
		ch = path_info->elements[0];

		if ((*ch == '\\') && (*(++ch) == '\\')) {
			/* todo: path_info->hat = UNC handle */
			return NT_STATUS_NOT_IMPLEMENTED;
		}

		/* all other cases */
		ch = path_info->elements[0];

		if (*ch == '/') {
			path_info->fdtypeat = PSX_FD_OS_FS_ROOT;
			path_info->hat = path_info->ctx->root.hfile;
			startidx = ((ch[1] == '.') && (ch[2] == '/')) ? 2 : 1;
		} else if (*ch == '\\') {
			/* todo: blind spot */
			path_info->hat = path_info->ctx->cwd.hat;
			startidx = 1;
		} else {
			/* DOS drive? */
			drive_letter = ch;

			if (*(++ch) == ':') {
				status = __psx_get_dos_drive_root_handle(
					&path_info->hat,
					drive_letter);

				if (*(ch++) == 0) {
					/* on failure, path_info->hat remains zero */
					path_info->hfile = path_info->hat;
					return status;
				} else
					startidx = 1;
			} else {
				/* relative path */
				path_info->hat = path_info->ctx->cwd.hfile;
				path_info->fdtypeat = path_info->ctx->cwd.fdtype;
				startidx = 0;
			}
		}

	}

	/* openat? */
	if (!startidx && (path_info->fexflags & PSX_PATH_OPEN_AT) && (path_info->fdidxat != AT_FDCWD)) {
		if (!path_info->ofdat && !(path_info->ofdat = __psx_ofd_ref_inc(
				path_info->ctx,
				path_info->fdidxat)))
			return NT_STATUS_INVALID_HANDLE;

		path_info->hat	    = path_info->ofdat->info.hfile;
		path_info->fdtypeat = path_info->ofdat->info.fdtype;
	}

	/* init marks */
	path_info->inmark[0]  = 0;
	path_info->inmark[1]  = 0;

	path_info->outmark[0] = (wchar16_t *)path_info->outbuf;
	path_info->outmark[1] = (wchar16_t *)path_info->outbuf;

	for (index=startidx; index<path_info->depth-1; index++) {
		ch = path_info->elements[index];

		if (__path_element_is_this_dir(path_info,index)) {
			/* do nothing */
		} else if (__path_element_is_parent_dir(path_info,index)) {
			if ((status = __path_open_parent(path_info,index)))
				return status;
		} else if (*ch == '/') {
			/* resolve pending path */
			if ((status = __path_open_next(path_info,index)))
				return status;

			/* copy to utf-16 buffer */
			if ((status = __path_element_utf8_to_native_element_utf16(path_info,index)))
				return status;

			/* mount point? */
			/* junction? */
			/* symbolic link? */

		} else {
			/* add pending native element */
			if ((status = __path_element_utf8_to_native_element_utf16(path_info,index)))
				return status;
		}
	}

	/* pending native elements? */
	if ((uintptr_t)path_info->outmark[1] > (uintptr_t)path_info->outbuf)
		if ((status = __path_open_next(path_info,index)))
			return status;

	/* final element */
	if ((status = __path_element_utf8_to_native_element_utf16(path_info,index)))
		return status;

	return __path_open_last(path_info,index);
}


static int __fastcall __path_element_is_this_dir(
	struct __path_info *	path_info,
	int32_t			index)
{
	unsigned char *	ch;
	unsigned char *	ch_next;

	ch	= path_info->elements[index];
	ch_next	= path_info->elements[index+1];

	if (index && (ch_next-ch == 2))
		return (ch[1] == '.');
	else if (!index && (ch_next-ch == 1))
		return (ch[0] == '.');
	else
		return 0;
}


static int __fastcall __path_element_is_parent_dir(
	struct __path_info *	path_info,
	int32_t			index)
{
	unsigned char *	ch;
	unsigned char *	ch_next;

	ch	= path_info->elements[index];
	ch_next	= path_info->elements[index+1];

	if (index && (ch_next-ch == 3))
		return (ch[1] == '.') && (ch[2] == '.');
	else if (!index && (ch_next-ch == 2))
		return (ch[0] == '.') && (ch[1] == '.');
	else
		return 0;
}


static int __fastcall __path_element_utf8_to_native_element_utf16(
	struct __path_info *	path_info,
	int32_t			index)
{
	int32_t						status;
	nt_unicode_conversion_params_utf8_to_utf16	params = {0};

	params.src		= path_info->elements[index];
	params.src_size_in_bytes= (uintptr_t)path_info->elements[index+1] - (uintptr_t)params.src;
	params.dst		= path_info->outmark[1];
	params.dst_size_in_bytes= path_info->avail;

	status = __ntapi->uc_convert_unicode_stream_utf8_to_utf16(&params);
	if (status) return status;

	path_info->outmark[1] = (wchar16_t *)((uintptr_t)path_info->outmark[1] + params.bytes_written);

	return NT_STATUS_SUCCESS;
}


static int __fastcall __path_swap_at(
	struct __path_info *	path_info,
	int32_t			index)
{
	int32_t			status;

	/* swap */
	if (path_info->pathflags & PSX_PATH_CLOSE_AT)
		if ((status = __iovtbl[path_info->fdtypeat].close(path_info->hat)))
			return status;

	path_info->fdtypeat	= path_info->fdtype;
	path_info->hat		= path_info->hfile;
	path_info->hfile 	= 0;
	path_info->pathflags	|= PSX_PATH_CLOSE_AT;

	/* reset marks */
	path_info->outmark[0] = (wchar16_t *)path_info->outbuf;
	path_info->outmark[1] = (wchar16_t *)path_info->outbuf;

	return NT_STATUS_SUCCESS;
}


static int __fastcall __path_open_next(
	struct __path_info *	path_info,
	int32_t			index)
{
	int32_t status;

	status = __iovtbl[path_info->fdtypeat].open_next(path_info,index);

	if (path_info->ofdat) {
		__psx_ofd_ref_dec(path_info->ctx,path_info->ofdat);
		path_info->ofdat = 0;
	}

	switch (status) {
		case NT_STATUS_SUCCESS:
			return __path_swap_at(path_info,index);

		case NT_STATUS_MORE_ENTRIES:
			return NT_STATUS_SUCCESS;

		default:
			return status;
	}
}


static int __fastcall __path_open_last(
	struct __path_info *	path_info,
	int32_t			index)
{
	int32_t				status;
	unsigned char *			ch;
	__process_single_element *	__path_fn;

	path_info->lastmark[0] = path_info->outmark[0];
	path_info->lastmark[1] = path_info->outmark[1];

	if ((path_info->depth == 1) && (path_info->fdtypeat == PSX_FD_OS_FS_ROOT))
		path_info->outmark[0] = path_info->outmark[1];
	else if ((path_info->psxflags & O_DIRECTORY) ^ O_DIRECTORY) {
		ch = path_info->elements[index+1];
		if ((*(--ch) != '/') && (*ch != '\\'))
			path_info->dirflags = 0;
	}

	if (path_info->fexflags & PSX_PATH_EXPLICIT_LAST)
		__path_fn = __path_open_next;
	else if (path_info->fexflags & PSX_PATH_SKIP_LAST) {
		path_info->outmark[0] = path_info->outmark[1];
		__path_fn = __path_open_next;
	} else if (__path_element_is_this_dir(path_info,index)) {
		path_info->outmark[0] = path_info->outmark[1];
		__path_fn = __path_open_next;
	} else if (__path_element_is_parent_dir(path_info,index)) {
		path_info->outmark[0] = path_info->outmark[1];
		__path_fn = __path_open_parent;
	} else
		__path_fn = __path_open_next;

	path_info->ntaccess	 = path_info->faccess;
	path_info->ntobjattr	 = path_info->fobjattr;
	path_info->ntattr	 = path_info->fattr;
	path_info->ntdisposition = path_info->fdisposition;
	path_info->ntoptions	 = path_info->foptions;

	if ((status = __path_fn(path_info,index)))
		return status;

	path_info->hfile = path_info->hat;
	path_info->hat   = 0;

	return __path_update_type(path_info,index);
}

static int __fastcall __path_open_parent(
	struct __path_info *	path_info,
	int32_t			index)
{
	int32_t		status;
	unsigned char * ch;
	int		type,i;

	ch = path_info->elements[index];

	if (*ch == '/')
		type = PSX_PARENT_DIR_PHYSICAL;

	else if (!index && (*ch == '.'))
		type = PSX_PARENT_DIR_PHYSICAL;

	else if (index && (*ch == '\\')) {
		type = PSX_PARENT_DIR_PHYSICAL;

		for (i=0; i<index; i++)
			if (!__path_element_is_parent_dir(path_info,i))
				type = PSX_PARENT_DIR_LEXICAL;

	} else
		type = PSX_PARENT_DIR_LOGICAL;

	switch (type) {
		case PSX_PARENT_DIR_LEXICAL:
			for (i=index-1, path_info->depth-=2; i<path_info->depth; i++)
				path_info->elements[i] = path_info->elements[i+2];

			return NT_STATUS_SUCCESS;

		case PSX_PARENT_DIR_PHYSICAL:
			if ((status = __path_open_next(path_info,index)))
				return status;

			if ((status = __iovtbl[path_info->fdtypeat].open_physical_parent(
					&path_info->hfile,
					path_info->hat,
					(uintptr_t *)path_info->outbuf,
					(uint32_t)path_info->avail,
					0,0,&path_info->fdtype)))
				return status;

			return __path_swap_at(path_info,index);

		case PSX_PARENT_DIR_LOGICAL:
		default:
			return NT_STATUS_NOT_SUPPORTED;
	}
}

static int __fastcall __path_update_type(
	struct __path_info *	path_info,
	int32_t			index)
{
	int32_t		status;
	nt_iosb		iosb;
	nt_fbi		fbi;

	/* needed? */
	if (path_info->dirflags || path_info->fdtype > PSX_FD_OS_FS_DIR)
		return NT_STATUS_SUCCESS;

	/* file basic information */
	if ((status = __ntapi->zw_query_information_file(
			path_info->hfile,
			&iosb,&fbi,sizeof(fbi),
			NT_FILE_BASIC_INFORMATION)))
		return status;

	/* file-system directory? */
	if (fbi.file_attr & NT_FILE_ATTRIBUTE_DIRECTORY)
		path_info->fdtype = PSX_FD_OS_FS_DIR;
	else
		path_info->fdtype = PSX_FD_OS_FS_FILE;

	return NT_STATUS_SUCCESS;
}
