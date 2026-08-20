#ifndef _PSX_HELPER_H_
#define _PSX_HELPER_H_

#include <ntapi/ntapi.h>

struct __psx_internal_thread_context {
	void *		entry;
	uintptr_t	data[];
};

/* helper functions */
int32_t __psx_create_cow_section(
	void **		hsection,
	size_t		size);

int32_t __psx_create_primary_section(
	void **		hsection,
	size_t		size);

int32_t __psx_map_cow_section(
	void *		hsection,
	void **		addr,
	size_t *	size);

int32_t __psx_map_primary_section(
	void *		hsection,
	void **		addr,
	size_t *	size);

int32_t __psx_clone_primary_section(
	void **		hlocalsec,
	void *		srcaddr,
	size_t		secsize,
	void **		mapaddr,
	size_t *	mapsize);

int32_t __psx_swap_primary_section(
	void *		hsecold,
	void *		srcaddr,
	void **		hsecnew,
	void **		mapaddr,
	size_t		secsize,
	size_t		mapsize);

int32_t __psx_blt_alloc(
	nt_blitter **	blt,
	void *		bitmap,
	void *		region,
	size_t		block_size,
	uint32_t	flags);

int32_t __psx_create_internal_thread(
	void **		hthread,
	void *		entry,
	void *		ctx,
	size_t		size);

void __stdcall __psx_terminate_internal_thread(
	void *		ctx,
	void *		code,
	void *		unused);

#endif
