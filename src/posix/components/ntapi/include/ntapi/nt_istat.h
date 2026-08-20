#ifndef _NT_ISTAT_H_
#define _NT_ISTAT_H_

#include <psxtypes/psxtypes.h>
#include "nt_object.h"
#include "nt_file.h"

/* ntapi_tt_istat info flag bits */
#define NT_ISTAT_DEFAULT	0x00000000
#define NT_ISTAT_COMMON		0x00000001
#define NT_ISTAT_DEV_NAME_COPY	0x00000002
#define NT_ISTAT_NEW_HANDLE	0x80000000

typedef struct _nt_istat {
	void *		hfile;
	nt_fii		fii;
	nt_ftagi	ftagi;
	uint32_t	flags_in;
	uint32_t	flags_out;
	uint32_t	dev_name_hash;
	uint16_t	dev_name_strlen;
	uint16_t	dev_name_maxlen;
	wchar16_t	dev_name[];
} nt_istat;


typedef int32_t __stdcall ntapi_tt_istat(
	__in	void *			hfile	__optional,
	__in	void *			hroot	__optional,
	__in	nt_unicode_string *	path	__optional,
	__out	nt_istat *		istat,
	__out	uintptr_t *		buffer,
	__in	uint32_t		buffer_size,
	__in	uint32_t		open_options,
	__in	uint32_t		flags);


typedef int32_t __stdcall ntapi_tt_validate_fs_handle(
	__in	void *		hfile,
	__in	uint32_t	dev_name_hash,
	__in	nt_fii		fii,
	__out	uintptr_t *	buffer,
	__in	uint32_t	buffer_size);

#endif
