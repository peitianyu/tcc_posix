/* midipix_tcc: minimal sys/mman.h (freestanding; libc provides mmap()) */
#ifndef _MIDIPIX_TCC_SYS_MMAN_H
#define _MIDIPIX_TCC_SYS_MMAN_H
#include <psxtypes/psxtypes.h>
#define PROT_NONE  0
#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4
#define MAP_FILE      0
#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_TYPE      0x0f
#define MAP_FIXED     0x10
#define MAP_ANONYMOUS 0x20
#define MAP_ANON      MAP_ANONYMOUS
#define MAP_NORESERVE 0x4000
#define MAP_POPULATE  0x8000
#define MAP_FAILED    ((void *)-1)
#define MS_ASYNC     1
#define MS_INVALIDATE 2
#define MS_SYNC       4
#define MCL_CURRENT   1
#define MCL_FUTURE    2
void * mmap(void *, size_t, int, int, int, off_t);
int munmap(void *, size_t);
int mprotect(void *, size_t, int);
int msync(void *, size_t, int);
int madvise(void *, size_t, int);
int mlock(const void *, size_t);
int munlock(const void *, size_t);
void * mremap(void *, size_t, size_t, int, ...);
#endif
