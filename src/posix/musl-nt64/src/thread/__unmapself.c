#include "pthread_impl.h"
#include "syscall.h"

/*
 * musl-nt64: threads actually execute on OS-allocated stacks (SYS_clone ->
 * CreateThread ignores child_stack), so the musl-mapped region is NOT the
 * executing stack. However libc TLS and the psxscl TLCA both live inside that
 * region and are still read by the thread-exit path (__psx_exit ->
 * __tlca_self -> *(tls_slot_addr())), so we must NOT unmap the region before
 * exiting: that would be a use-after-free in __psx_exit.
 *
 * We therefore run SYS_exit (__psx_exit) on the thread's real, large OS stack
 * and skip the munmap. This deliberately abandons the upstream shared-stack
 * switch: the static shared_stack[256] is far too small — __psx_exit marshals a
 * ~200-byte port message plus call frames, overflowing it — and its
 * lock/set_tid_address handshake races between concurrently-exiting detached
 * threads (the aio worker pool). Both caused segfaults under load (R12).
 *
 * Cost: the per-thread map region is reclaimed only with the process address
 * space. The tid reaper is redirected to a stable .data slot so the kernel's
 * clear-child-tid write can never land in a region being torn down.
 */
static int __unmapself_tid_slot;

void __unmapself(void *base, size_t size)
{
	(void)base;
	(void)size;
	__syscall(SYS_set_tid_address, &__unmapself_tid_slot);
	__syscall(SYS_exit, 0);
}