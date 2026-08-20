#ifndef _NT_SLIST_H_
#define _NT_SLIST_H_

#include <psxtypes/psxtypes.h>
#include "nt_sync.h"

struct nt_slist;
struct nt_slist_node;

struct nt_slist_node {
	struct nt_slist_node *	next;
	uintptr_t		data;
};

struct __attr_aligned__(NT_SYNC_BLOCK_SIZE) nt_slist {
	struct nt_slist_node *	head;
	intptr_t		nitems;
	intptr_t		busy;
	intptr_t		padding[NT_SYNC_BLOCK_SIZE/sizeof(size_t)-3];
};

#endif
