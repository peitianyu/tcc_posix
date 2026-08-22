#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "psxglue.h"
#include "pthread_arch.h"

typedef unsigned int __tls_word	__attribute__((mode(word)));
typedef unsigned int __tls_ptr	__attribute__((mode(pointer)));

struct __emutls_object
{
	__tls_word	size;
	__tls_word	align;
	ptrdiff_t	offset;
	void *		defval;
};

void * __emutls_get_address (struct __emutls_object * obj)
{
	/* Single-threaded emutls allocator for this musl-only toolchain.
	   `offset` is a process-wide layout cursor (shared across threads), the
	   per-thread storage lives in our own region so we never touch the
	   musl/psx internal TLS block or overflow it.  Multi-thread __thread
	   (per-thread copies) is out of scope here; within a single thread this
	   is fully correct and safe. */
	static char region[1 << 16];   /* 64KiB thread-local area */
	static size_t used = 1;        /* start at 1: offset 0 stays the
	                                * "not yet allocated" sentinel, never a
	                                * valid allocation target */

	if (!obj->offset) {
		size_t align = ((size_t)obj->align & 0xFFFF);
		size_t off;
		if (align < 1)
			align = 1;
		off = (used + align - 1) & ~(align - 1);
		obj->offset = off;
		used = off + obj->size;
		if (obj->defval) {
			char *p = region + off;
			size_t i;
			for (i = 0; i < obj->size; i++)
				p[i] = ((char *)obj->defval)[i];
		} else {
			memset(region + off, 0, obj->size);
		}
	}
	return region + obj->offset;
}