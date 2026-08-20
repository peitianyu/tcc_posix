/*****************************************************************************/
/*  dalist: a zero-dependency book-keeping library                           */
/*  Copyright (C) 2013--2021  SysDeer Technologies, LLC                      */
/*  Released under GPLv2 and GPLv3; see COPYING.DALIST.                      */
/*****************************************************************************/

#include <dalist/dalist.h>
#include "dalist_impl.h"


dalist_api
int dalist_init(struct dalist *	dlist)
{
	dlist->head = dlist->tail = 0;
	return DALIST_OK;
}


dalist_api
int dalist_init_ex(
	struct dalist_ex *	dlist,
	size_t			dblock_size,
	size_t			lblock_size,
	void *			memfn_ptr,
	enum dalist_memfn	memfn_method)

{
	if (!dlist)
		return DALIST_ELIST;

	dlist->head = dlist->tail = 0;

	dlist->dblock_size = dblock_size;
	dlist->lblock_size = lblock_size;

	dlist->memfn_ptr    = memfn_ptr;
	dlist->memfn_method = memfn_method;

	dlist->free = 0;
	dlist->bookmarks = 0;
	dlist->memfn_status = 0;

	dlist->info.list_nodes = 0;
	dlist->info.free_nodes = 0;

	dlist->info.bookmark_from = 0;
	dlist->info.bookmark_to = 0;

	dlist->info.bookmark_key_min = 0;
	dlist->info.bookmark_key_max = 0;

	dlist->info.nodes_below_first_bookmark = 0;
	dlist->info.nodes_above_last_bookmark  = 0;

	return DALIST_OK;
}


dalist_api
int dalist_insert_before(
	struct dalist *		dlist,
	struct dalist_node *	lnode,
	struct dalist_node *	nnode)
{
	if (!dlist)
		return DALIST_ELIST;

	if (lnode) {
		/* insert before an existing node */
		nnode->prev = lnode->prev;
		nnode->next = lnode;
		lnode->prev = nnode;

		if (nnode->prev)
			nnode->prev->next = nnode;
		else
			dlist->head = nnode;
	} else {
		if (dlist->head)
			return DALIST_ENODE;
		else {
			/* nnode is the new head/tail */
			nnode->next = 0;
			nnode->prev = 0;
			dlist->head = nnode;
			dlist->tail = nnode;
		}
	}

	return DALIST_OK;
}


dalist_api
int dalist_insert_after(
	struct dalist *		dlist,
	struct dalist_node *	lnode,
	struct dalist_node *	nnode)
{
	if (!dlist)
		return DALIST_ELIST;

	if (lnode) {
		/* insert after an existing node */
		nnode->prev = lnode;
		nnode->next = lnode->next;
		lnode->next = nnode;

		if (nnode->next)
			nnode->next->prev = nnode;
		else
			dlist->tail = nnode;
	} else {
		if (dlist->head)
			return DALIST_ENODE;
		else {
			/* nnode is the new head/tail */
			nnode->next = 0;
			nnode->prev = 0;
			dlist->head = nnode;
			dlist->tail = nnode;
		}
	}

	return DALIST_OK;
}


dalist_api
int dalist_unlink_node(
	struct dalist *		dlist,
	struct dalist_node *	node)
{
	struct dalist_node * prev;
	struct dalist_node * next;

	if (!dlist)
		return DALIST_ELIST;
	else if (!node)
		return DALIST_ENODE;

	prev = node->prev;
	next = node->next;

	if (prev)
		prev->next = next;
	else
		dlist->head = next;

	if (next)
		next->prev = prev;
	else
		dlist->tail = prev;

	return DALIST_OK;
}


dalist_api
int dalist_unlink_node_ex(
	struct dalist_ex *	dlist,
	struct dalist_node_ex *	node)
{
	int ret;

	ret = dalist_unlink_node(
		(struct dalist *)dlist,
		(struct dalist_node *)node);

	if (ret == DALIST_OK)
		dlist->info.list_nodes--;

	return ret;
}


dalist_api
int dalist_discard_node(
	struct dalist_ex *	dlist,
	struct dalist_node_ex *	node)
{
	int ret;

	ret = dalist_unlink_node_ex(dlist,node);

	if (ret == DALIST_OK)
		ret = dalist_deposit_free_node(dlist,node);

	return ret;
}


dalist_api
int dalist_get_node_by_key(
	struct dalist_ex *	dlist,
	struct dalist_node_ex **node,
	uintptr_t		key,
	uintptr_t		get_flags,
	uintptr_t *		node_flags)
{
	struct dalist_node_ex *	cnode;
	struct dalist_node_ex *	nnode;
	uintptr_t		flags;
	int			ret;

	if (!dlist) return DALIST_ELIST;

	/* init */
	if (!node_flags)
		node_flags = &flags;

	*node = 0;
	*node_flags = DALIST_NODE_TYPE_NONE;

	/* traverse */
	cnode = (struct dalist_node_ex *)dlist->head;

	while (cnode && (cnode->key < key))
		cnode = cnode->next;

	/* return matching node */
	if (cnode && (cnode->key == key)) {
		*node_flags = DALIST_NODE_TYPE_EXISTING;
		*node = cnode;
		return DALIST_OK;
	}

	/* key not found, possibly a new node */
	if (get_flags & DALIST_NODE_TYPE_NEW) {
		ret = dalist_get_free_node(dlist, (void **)&nnode);
		if (ret) return ret;

		nnode->key = key;

		if (cnode)
			ret = dalist_insert_before(
				(struct dalist *)dlist,
				(struct dalist_node *)cnode,
				(struct dalist_node *)nnode);
		else
			ret = dalist_insert_after(
				(struct dalist *)dlist,
				(struct dalist_node *)dlist->tail,
				(struct dalist_node *)nnode);

		if (ret == DALIST_OK) {
			*node = nnode;
			dlist->info.list_nodes++;
			*node_flags = DALIST_NODE_TYPE_NEW;
		} else
			dalist_deposit_free_node(dlist,nnode);
	} else
		ret = DALIST_OK;

	return ret;
}


dalist_api
int dalist_insert_node_by_key(
	struct dalist_ex *	dlist,
	struct dalist_node_ex *	node)
{
	struct dalist_node_ex *	cnode;
	int			ret;

	/* verify dlist */
	if (!dlist)
		return DALIST_ELIST;

	/* traverse */
	cnode = (struct dalist_node_ex *)dlist->head;

	while (cnode && (cnode->key < node->key))
		cnode = cnode->next;

	/* verify that node-> is unique */
	if (cnode && (cnode->key == node->key))
		return DALIST_EKEYEXISTS;

	/* key is unique, add a new node */
	if (cnode)
		ret = dalist_insert_before(
			(struct dalist *)dlist,
			(struct dalist_node *)cnode,
			(struct dalist_node *)node);
	else
		ret = dalist_insert_after(
			(struct dalist *)dlist,
			(struct dalist_node *)dlist->tail,
			(struct dalist_node *)node);

	if (ret == DALIST_OK)
		dlist->info.list_nodes++;

	return ret;
}
