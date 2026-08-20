#include <time.h>
#include <sys/time.h>
#include "syscall.h"

int gettimeofday(struct timeval *restrict tv, void *restrict tz)
{
	/* tcc-win64: 直接 syscall (psxscl 实现), 绕开 clock_gettime 包装层 */
	return __syscall(SYS_gettimeofday, tv, tz);
}
