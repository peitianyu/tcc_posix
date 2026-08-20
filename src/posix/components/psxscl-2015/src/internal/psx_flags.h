/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#ifndef _PSX_FLAGS_H_
#define _PSX_FLAGS_H_

#include "psx_systypes.h"

struct __flag_set {
	int		psxflag;
	uint32_t	ntflag;
};


static __inline__ uint32_t __psx_convert_flags_to_native(
	const struct __flag_set	fset[],
	uint32_t		psxflags,
	uint32_t *		ntflags,
	uint32_t		zeroflags)
{
	uint32_t	cflags;
	uint32_t	fvalid;

	if (!psxflags) {
		*ntflags = zeroflags;
		return 0;
	}

	cflags = 0;
	fvalid = 0;

	for (; fset->psxflag || fset->ntflag; fset++) {
		fvalid |= fset->psxflag;

		if (psxflags & fset->psxflag)
			cflags |= fset->ntflag;
	}

	/* always update (support subsets) */
	*ntflags = cflags;

	if (psxflags & (~fvalid))
		return NT_STATUS_INVALID_PARAMETER;
	else
		return 0;
}


static __inline__ void __psx_convert_flags_to_posix(
	const struct __flag_set	fset[],
	uint32_t		ntflags,
	uint32_t *		psxflags,
	uint32_t		zeroflags)
{
	uint32_t	cflags;

	if (!ntflags) {
		*psxflags = zeroflags;
		return;
	}

	cflags = 0;

	for (; fset->psxflag || fset->ntflag; fset++)
		if (ntflags & fset->ntflag)
			cflags |= fset->psxflag;

	*psxflags = cflags;

	return;
}

#endif
