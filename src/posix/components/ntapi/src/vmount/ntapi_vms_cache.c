/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <ntapi/nt_port.h>
#include <ntapi/nt_vmount.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"


typedef struct nt_vms_cache_interface {
	nt_vms_system *		vms_sys;
	struct dalist_ex	cache;
	size_t			alloc_size;
	uintptr_t		buffer[1];
} nt_vms_cache_context;


typedef struct _nt_vms_cache_record {
	void *			hfile;
	uint32_t		dev_name_hash;
	nt_large_integer	index_number;
	intptr_t		client_key;
	intptr_t		server_key;
} nt_vms_cache_record;


int32_t __stdcall	__ntapi_vms_cache_free(
	__in	nt_vms_cache		vms_cache)
{
	int32_t		status;
	void *		region_addr;
	size_t		region_size;

	/* validation */
	if (!vms_cache)
		return NT_STATUS_INVALID_PARAMETER;

	/* free memory */
	region_addr = vms_cache;
	region_size = vms_cache->alloc_size;

	status = __ntapi->zw_free_virtual_memory(
		NT_CURRENT_PROCESS_HANDLE,
		&region_addr,
		&region_size,
		NT_MEM_RELEASE);

	return status;
}

/* vms optional cache functions */
nt_vms_cache __stdcall	__ntapi_vms_cache_alloc(
	__in	nt_vms_system *		vms_sys,
	__in	uint32_t		flags	__reserved,
	__in	void *			options	__reserved,
	__out	int32_t *		status	__optional)
{
	int32_t			_status;
	void *			buffer;
	size_t			buffer_size;
	nt_vms_cache_context *	vms_cache;

	/* status */
	if (!status) status = &_status;

	/* validation */
	if (!vms_sys) {
		*status = NT_STATUS_INVALID_PARAMETER;
		return (nt_vms_cache)0;
	}

	/* calculate size */
	buffer_size =  sizeof(nt_vms_cache_context);
	buffer_size += vms_sys->vms_points_cap * (sizeof(nt_vms_cache_record) - sizeof(uintptr_t));

	/* allocate buffer */
	*status = __ntapi->zw_allocate_virtual_memory(
		NT_CURRENT_PROCESS_HANDLE,
		&buffer,
		0,
		&buffer_size,
		NT_MEM_COMMIT,
		NT_PAGE_READWRITE);

	if (*status) return (nt_vms_cache)0;

	/* init vms cache */
	vms_cache = (nt_vms_cache_context *)buffer;
	vms_cache->vms_sys	= vms_sys;
	vms_cache->alloc_size	= buffer_size;

	/* init list */
	*status = dalist_init_ex(
		&vms_cache->cache,
		sizeof(nt_vms_cache_record),
		0x1000,
		__ntapi->zw_allocate_virtual_memory,
		DALIST_MEMFN_NT_ALLOCATE_VIRTUAL_MEMORY);

	if (*status != DALIST_OK) {
		*status = NT_STATUS_UNSUCCESSFUL;
		__ntapi_vms_cache_free(vms_cache);
		return (nt_vms_cache)0;
	}

	/* set list buffer */
	buffer_size -= (size_t)&(((nt_vms_cache_context *)0)->buffer);

	*status = dalist_deposit_memory_block(
		&vms_cache->cache,
		&vms_cache->buffer,
		buffer_size);

	return vms_cache;
}


int32_t __stdcall	__ntapi_vms_cache_record_append(
	__in	nt_vms_cache		cache,
	__in	void *			hfile,
	__in	uint32_t		dev_name_hash,
	__in	nt_large_integer	index_number,
	__in	intptr_t		client_key,
	__in	intptr_t		server_key)
{
	int32_t			status;
	struct dalist_node_ex *	node;
	nt_vms_cache_record *	cache_record;

	status = dalist_get_node_by_key(
			&cache->cache,
			&node,
			(uintptr_t)hfile,
			DALIST_NODE_TYPE_EXISTING,
			(uintptr_t *)0);

	if (status != DALIST_OK)
		status = NT_STATUS_INTERNAL_ERROR;
	else if (node)
		status = NT_STATUS_OBJECTID_EXISTS;
	else {
		status = dalist_get_free_node(&cache->cache,(void **)&node);

		if (status == DALIST_OK) {
			cache_record = (nt_vms_cache_record *)&node->dblock;

			__ntapi->tt_aligned_block_memset(
				node,
				0,
				(uintptr_t)&((struct dalist_node_ex *)0)->dblock + sizeof(*cache_record));

			node->key = (uintptr_t)hfile;

			cache_record->hfile		= hfile;
			cache_record->dev_name_hash	= dev_name_hash;
			cache_record->index_number.quad	= index_number.quad;
			cache_record->client_key	= client_key;
			cache_record->server_key	= server_key;

			status = dalist_insert_node_by_key(
				&cache->cache,
				node);

			if (status != DALIST_OK)
				dalist_deposit_free_node(
					&cache->cache,
					node);
		}
	}

	return status;
}


int32_t __stdcall	__ntapi_vms_cache_record_remove(
	__in	nt_vms_cache		cache,
	__in	void *			hfile,
	__in	uint32_t		dev_name_hash,
	__in	nt_large_integer	index_number)
{
	int32_t			status;
	struct dalist_node_ex *	node;

	status = dalist_get_node_by_key(
			&cache->cache,
			&node,
			(uintptr_t)hfile,
			DALIST_NODE_TYPE_EXISTING,
			(uintptr_t *)0);

	if (status != DALIST_OK)
		status = NT_STATUS_INTERNAL_ERROR;
	else if (node)
		status = NT_STATUS_INVALID_PARAMETER;
	else {
		status = dalist_discard_node(
			&cache->cache,
			node);

		if (status != DALIST_OK)
			status = NT_STATUS_INTERNAL_ERROR;
	}

	return status;
}
