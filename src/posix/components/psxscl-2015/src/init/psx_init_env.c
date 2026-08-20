/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/

#include <ntapi/ntapi.h>
#include "psx_systypes.h"
#include "psx_impl.h"
#include "psx_init.h"
#include "psx.h"

static int __is_alpha(char ch)
{
	return (((ch >= 'a') && (ch <= 'z'))
		|| ((ch >= 'A') && (ch <= 'Z')));
}

static int __is_path(char * var, char * val)
{
	char * ch;
	char * equal;

	ch    = val - 1;
	equal = val - 1;

	if (*equal != '=')
		return 0;

	if ((*--ch == 'H') && (*--ch == 'T') && (*--ch == 'A') && (*--ch == 'P') && (ch == var))
		return 1;
	else
		ch = equal;

	if ((*--ch == 'H') && (*--ch == 'T') && (*--ch == 'A') && (*--ch == 'P') && (*--ch == '_'))
		return 1;
	else
		ch = equal;

	if ((*--ch == 'P') && (*--ch == 'M') && (*--ch == 'E') && (*--ch == 'T') && (ch == var))
		return 1;
	else
		ch = equal;

	return ((*--ch == 'P') && (*--ch == 'M') && (*--ch == 'T') && (ch == var));
}

static void __copy_var_name(char ** src, char ** next)
{
	char * var;
	char * dst;

	var = *src;
	dst = *next;

	if (*var == '=')
		*dst++ = *var++;

	for (; *var && (*var != '='); )
		*dst++ = *var++;

	if (*var == '=')
		*dst++ = *var++;

	*src  = var;
	*next = dst;
}

static void __copy_var_value(char * src, char ** next)
{
	char * dst;

	for (dst=*next; *src; )
		*dst++ = *src++;

	*dst++ = '\0';
	*next = dst;
}

static void __copy_path_value(char * src, char ** next)
{
	char *	ch;
	char *	dst;
	int	tip;

	ch  = src;
	dst = *next;
	tip = 1;

	do {
		if (tip && (ch[1] == ':') && __is_alpha(ch[0])) {
			*dst++ = '/';
			*dst++ = '/';
			*dst++ = (*ch < 'a') ? (*ch+'a'-'A') : *ch;

			ch++;
			ch++;
			tip = 0;

			if ((*ch != '\\') && (*ch != '/'))
				*dst++ = '/';

		} else if (tip && (ch[0] == '/') && __is_alpha(ch[1]) && (ch[2] == '/')) {
			*dst++ = '/';
			*dst++ = '/';
			*dst++ = (ch[1] < 'a') ? (ch[1]+'a'-'A') : ch[1];
			*dst++ = '/';

			ch++;
			ch++;
			ch++;
			tip = 0;
		} else {
			*dst++ = (*ch == '\\') ? '/' : (*ch == ';') ? ':' : *ch;
			tip = (*ch == ';') || (*ch == ':');
			ch++;
		}
	} while (*ch);

	*dst++ = '\0';
	*next = dst;
}

int32_t __psx_init_env(void)
{
	int32_t		status;
	char **		envp;
	void *		addr;
	char *		src;
	char *		next;
	char *		mark;
	size_t		size;

	if ((__psx.__flags & PSX_CTX_FORK_CHILD) || (__psx.__flags & PSX_CTX_EXEC_CHILD))
		return NT_STATUS_SUCCESS;

	addr = 0;
	size = 0;
	envp = rtctx.envp_utf8;

	if (!envp || !*envp)
		return NT_STATUS_INTERNAL_ERROR;

	for (; *envp; envp++)
		size += 3 + __ntapi->tt_string_null_offset_multibyte(*envp);

	if ((status = __ntapi->zw_allocate_virtual_memory(
			rtdata->hprocess_self,
			&addr,0,&size,
			NT_MEM_COMMIT,
			NT_PAGE_READWRITE)))
		return status;

	__ntapi->tt_aligned_block_memset(
		addr,0,size);

	envp = rtctx.envp_utf8;
	next = (char *)addr;

	for (; *envp; envp++) {
		src  = *envp;
		mark = next;

		__copy_var_name(&src,&next);

		if (__is_path(*envp,src))
			__copy_path_value(src,&next);
		else
			__copy_var_value(src,&next);

		*envp = mark;
	}

	return NT_STATUS_SUCCESS;
}
