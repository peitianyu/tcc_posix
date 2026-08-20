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


nt_vms_node * __stdcall	__ntapi_vms_get_end_component_first_node(
	__in	nt_vms_system *		pvms_sys,
        __in	uint32_t		end_component_hash)
{
	nt_vms_node *	node;

	/* verify non-empty list and valid input */
	if (!pvms_sys->dev_name_head_node || !end_component_hash)
		return (nt_vms_node *)0;

	/* find first node by end component hash */
	node = (nt_vms_node *)((uintptr_t)pvms_sys + pvms_sys->end_component_head_node);

	while (node->next && (node->end_component_hash < end_component_hash))
		node = (nt_vms_node *)((uintptr_t)pvms_sys + node->next);

	if (node->end_component_hash == end_component_hash)
		return node;
	else
		return (nt_vms_node *)0;
}


static nt_vms_node * __stdcall	__ntapi_vms_get_node(
	__in	nt_vms_system *		pvms_sys,
        __in	uint32_t		end_component_hash,
        __in	uint32_t		dev_name_hash,
        __in	nt_large_integer	index_number)
{
	nt_vms_node *	node;

	/* verify non-empty list */
	if (!pvms_sys->dev_name_head_node)
		return (nt_vms_node *)0;

	/* end_component_hash */
	if (end_component_hash) {
		node = (nt_vms_node *)((uintptr_t)pvms_sys + pvms_sys->end_component_head_node);

		while (node->next && (node->end_component_hash < end_component_hash))
			node = (nt_vms_node *)((uintptr_t)pvms_sys + node->next);

		if (node->end_component_hash != end_component_hash)
			return (nt_vms_node *)0;
	} else
		node = (nt_vms_node *)((uintptr_t)pvms_sys + pvms_sys->dev_name_head_node);

	/* find device nodes */
	while (node->next && (node->dev_name_hash < dev_name_hash))
		node = (nt_vms_node *)((uintptr_t)pvms_sys + node->next);

	if (node->dev_name_hash != dev_name_hash)
		return (nt_vms_node *)0;

	/* find mount-point nodes */
	while (node->next && (node->index_number.quad < index_number.quad))
		node = (nt_vms_node *)((uintptr_t)pvms_sys + node->next);

	if (node->index_number.quad != index_number.quad)
		return (nt_vms_node *)0;

	return node;
}


nt_vms_node * __stdcall		__ntapi_vms_get_node_by_dev_name(
	__in	nt_vms_system *		pvms_sys,
        __in	uint32_t		dev_name_hash,
        __in	nt_large_integer	index_number)
{
	return __ntapi_vms_get_node(
		pvms_sys,
		0,
		dev_name_hash,
		index_number);
}


nt_vms_node * __stdcall		__ntapi_vms_get_node_by_end_component(
	__in	nt_vms_system *		pvms_sys,
        __in	uint32_t		end_component_hash,
        __in	uint32_t		dev_name_hash,
        __in	nt_large_integer	index_number)
{
	return __ntapi_vms_get_node(
		pvms_sys,
		end_component_hash,
		dev_name_hash,
		index_number);
}


nt_vms_point * __stdcall	__ntapi_vms_get_top_of_stack_mount_point(
	__in	nt_vms_system *		pvms_sys,
	__in	nt_vms_node *		node)
{
	nt_vms_point *	point;

	point = (nt_vms_point *)((uintptr_t)pvms_sys + node->stack);

	while (point->next)
		point = (nt_vms_point *)((uintptr_t)pvms_sys + point->next);

	return point;
}
