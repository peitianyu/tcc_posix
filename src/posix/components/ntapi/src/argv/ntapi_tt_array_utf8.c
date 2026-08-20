/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include <ntapi/nt_argv.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"

int32_t __stdcall __ntapi_tt_array_copy_utf8(
	__out	int *			argc,
	__in	const char **		argv,
	__in	const char **		envp,
	__in	const char *		image_name	__optional,
	__in	const char *		interpreter	__optional,
	__in	const char *		optarg		__optional,
	__in	void *			base,
	__out	void *			buffer,
	__in	size_t			buflen,
	__out	size_t *		blklen)
{
	const char **	parg;
	const char *	arg;
	const char *	dummy;
	char *		ch;
	ptrdiff_t	diff;
	ptrdiff_t	ptrs;
	size_t		needed;

	/* fallback */
	dummy = 0;
	argv = argv ? argv : &dummy;
	envp = envp ? envp : &dummy;

	/* ptrs, needed */
	ptrs   = 0;
	needed = 0;

	if (image_name) {
		ptrs++;
		needed += sizeof(char *)
			+ __ntapi->tt_string_null_offset_multibyte(image_name)
			+ sizeof(char);
	}

	for (parg=argv; *parg; parg++)
		needed += sizeof(char *)
			+ __ntapi->tt_string_null_offset_multibyte(*parg)
			+ sizeof(char);

	ptrs += (parg - argv);
	*argc = (int)ptrs;

	for (parg=envp; *parg; parg++)
		needed += sizeof(char *)
			+ __ntapi->tt_string_null_offset_multibyte(*parg)
			+ sizeof(char);

	ptrs += (parg - envp);

	ptrs    += 2;
	needed  += 2*sizeof(char *);
	blklen  = blklen ? blklen : &needed;
	*blklen = needed;

	if (buflen < needed)
		return NT_STATUS_BUFFER_TOO_SMALL;

	/* init */
	parg = (const char **)buffer;
	ch  = (char *)(parg+ptrs);
	diff = (ptrdiff_t)base;

	/* image_name */
	if (image_name) {
		*parg++ = ch-diff;
		for (arg=image_name; *arg; arg++,ch++)
			*ch = *arg;
		*ch++ = '\0';
	}

	/* argv */
	for (; *argv; argv++) {
		*parg++=ch-diff;
		for (arg=*argv; *arg; arg++,ch++)
			*ch = *arg;
		*ch++ = '\0';
	}

	*parg++ = 0;

	/* envp */
	for (; *envp; envp++) {
		*parg++=ch-diff;
		for (arg=*envp; *arg; arg++,ch++)
			*ch = *arg;
		*ch++ = '\0';
	}

	*parg++ = 0;

	return NT_STATUS_SUCCESS;
}

int32_t __stdcall __ntapi_tt_array_convert_utf8_to_utf16(
	__in		char **		arrv,
	__in		wchar16_t **	arra,
	__in		void *		base,
	__in		wchar16_t *	buffer,
	__in		size_t		buffer_len,
	__out		size_t *	bytes_written)
{
	return NT_STATUS_SUCCESS;
}
