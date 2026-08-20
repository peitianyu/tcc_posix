#ifndef _NT_STAT_H_
#define _NT_STAT_H_

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_object.h>
#include <ntapi/nt_file.h>

/* ntapi_tt_stat info flags bits */
#define NT_STAT_DEFAULT		(0x00000000)
#define NT_STAT_COMMON		(0x00000001)
#define NT_STAT_DEV_NAME_COPY	(0x00000002)
#define NT_STAT_NEW_HANDLE	(0x80000000)

typedef struct _nt_stat {
	nt_fbi		fbi;
	nt_fsi		fsi;
	nt_fii		fii;
	nt_fei		fei;
	nt_facci	facci;
	nt_fpi		fpi;
	nt_fmi		fmi;
	nt_falii	falii;
	nt_fssi		fssi;
	void *		hfile;
	uint32_t	flags_in;
	uint32_t	flags_out;
	uint32_t	file_name_length;
	uint32_t	file_name_hash;
	uint32_t	dev_name_hash;
	uint16_t	dev_name_strlen;
	uint16_t	dev_name_maxlen;
	wchar16_t	dev_name[];
} nt_stat;


typedef int32_t __stdcall ntapi_tt_stat(
	__in	void *			hfile	__optional,
	__in	void *			hroot	__optional,
	__in	nt_unicode_string *	path	__optional,
	__out	nt_stat *		stat,
	__out	uintptr_t *		buffer,
	__in	uint32_t		buffer_size,
	__in	uint32_t		open_options,
	__in	uint32_t		flags);

#endif
