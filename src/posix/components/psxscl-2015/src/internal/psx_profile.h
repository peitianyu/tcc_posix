#ifndef _PSX_PROFILE_H_
#define _PSX_PROFILE_H_

#include "psx_systypes.h"

struct tms {
	clock_t tms_utime;
	clock_t tms_stime;
	clock_t tms_cutime;
	clock_t tms_cstime;
};

#endif
