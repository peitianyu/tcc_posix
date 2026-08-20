/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#ifndef _PSX_PATH_H_
#define _PSX_PATH_H_

#include <dalist/dalist.h>
#include "psx_systypes.h"

enum __psx_path_type {
	PSX_PATH_TYPE_NATIVE,
	PSX_PATH_TYPE_UNC,
	PSX_PATH_TYPE_DOS,
	PSX_PATH_TYPE_POSIX
};

enum __psx_parent_dir_type {
	PSX_PARENT_DIR_LEXICAL,
	PSX_PARENT_DIR_LOGICAL,
	PSX_PARENT_DIR_PHYSICAL
};

#define PSX_PATH_OPEN_AT	0x00000001
#define PSX_PATH_ROOT_RELATIVE	0x00000002
#define PSX_PATH_EXPLICIT_LAST	0x00000004
#define PSX_PATH_SKIP_LAST	0x00000008
#define PSX_PATH_ACCESS_EXEC	0x00001000
#define PSX_PATH_ACCESS_WRITE	0x00002000
#define PSX_PATH_ACCESS_READ	0x00004000
#define PSX_PATH_ACCESS_CHECK	0x00008000
#define PSX_PATH_ACCESS_DELETE	0x00010000
#define PSX_PATH_ATTR_READ	0x00020000
#define PSX_PATH_ATTR_WRITE	0x00040000
#define PSX_PATH_LIST_DIR	0x00080000
#define PSX_PATH_ACCESS_FULL	0x10000000
#define PSX_PATH_SHARE_NONE	0x20000000
#define PSX_PATH_CLOSE_AT	0x40000000
#define PSX_PATH_INTERNAL_CALL	0x80000000


struct __path_info {
	intptr_t	fdidx;
	intptr_t	fdidxat;
	intptr_t	ofdidx;

	struct __fd *	fd;
	struct __ofd *	ofd;
	struct __ofd *	ofdat;

	void *		hfile;
	void *		hat;
	int32_t		fdtype;
	int32_t		fdtypeat;

	uint32_t	psxflags;
	uint32_t	psxmode;
	uint32_t	pathflags;
	uint32_t	dirflags;

	void *		inbuf;
	void *		outbuf;
	size_t		used;
	size_t		avail;
	int32_t		depth;
	int32_t		arptrs;
	char *		inmark[2];
	wchar16_t *	outmark[2];
	wchar16_t *	lastmark[2];

	int32_t		ntstatus;
	int32_t		psxstatus;

	uint32_t	faccess;
	uint32_t	fobjattr;
	uint32_t	fattr;
	uint32_t	fdisposition;
	uint32_t	foptions;
	uint32_t	fexflags;
	uint32_t	fshare;

	uint32_t	ntaccess;
	uint32_t	ntobjattr;
	uint32_t	ntattr;
	uint32_t	ntdisposition;
	uint32_t	ntoptions;
	uint32_t	ntexflags;
	uint32_t	ntshare;

	char *		fsbuf;
	size_t		fsbuflen;
	char *		fsname_utf8;
	wchar16_t *	fsname_utf16;
	size_t		fsnamelen_utf8;
	size_t		fsnamelen_utf16;

	struct __psx_tlca *	tlca;
	struct __psx_ctx *	ctx;

	union {
		unsigned char **	elements;
		unsigned char **	elements_utf8;
		wchar16_t **		elements_utf16;
	};
};

struct __psx_tlca;

int32_t __fastcall __psx_normalize_path_utf8(
	const unsigned char *	path_arg,
	struct __path_info *	path_obj);

int32_t __fastcall __psx_parse_normalized_path_utf8(
	struct __path_info *	path_info,
	int32_t			expected_depth);

int32_t __fastcall __psx_parse_native_path_utf16(
	struct __path_info *	path_info,
	int32_t			expected_depth);

int32_t __fastcall __psx_resolve_parsed_path_utf8(
	struct __path_info *	path_info);

int32_t __fastcall __psx_path_open(
	struct __psx_tlca *	tlca,
	struct __path_info *	path_info,
	const unsigned char *	path_arg,
	int			flags,
	mode_t			mode,
	struct __ofd *		ofdat,
	intptr_t		fdidxat,
	uint32_t		exflags);

int32_t __psx_path_get_name_info(
	struct __path_info *	path_info);

#endif
