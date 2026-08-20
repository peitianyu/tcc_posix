/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>

wchar16_t * __cdecl __ntapi_tt_aligned_memcpy_utf16(
	__in	uintptr_t *	dst,
	__in	uintptr_t *	src,
	__in	size_t		bytes)
{
	size_t 		aligned_block;
	size_t		copied;

	wchar16_t *	wch_src;
	wchar16_t *	wch_dst;

	#if defined (__X86_64_MODEL)
	uint32_t *	uint32_src;
	uint32_t *	uint32_dst;
	#endif

	aligned_block = bytes;
	aligned_block /= sizeof(uintptr_t);
	aligned_block *= sizeof(uintptr_t);

	copied = 0;

	while (copied < aligned_block) {
		*dst = *src;
		src++;
		dst++;
		copied += sizeof(uintptr_t);
	}

	#if defined (__X86_64_MODEL)
		switch (bytes % sizeof(uintptr_t)) {
			case 6:
				uint32_src = (uint32_t *)src;
				uint32_dst = (uint32_t *)dst;
				*uint32_dst = *uint32_src;

				uint32_src++;
				uint32_dst++;

				/* make the compiler happy */
				wch_src = (wchar16_t *)uint32_src;
				wch_dst = (wchar16_t *)uint32_dst;
				*wch_dst = *wch_src;
				break;

			case 4:
				uint32_src = (uint32_t *)src;
				uint32_dst = (uint32_t *)dst;
				*uint32_dst = *uint32_src;
				break;
		}
	#endif

	if (bytes % sizeof(uintptr_t)) {
		/* the remainder must be 2 */
		wch_src = (wchar16_t *)src;
		wch_dst = (wchar16_t *)dst;
		*wch_dst = *wch_src;
	}

	return (wchar16_t *)dst;
}
