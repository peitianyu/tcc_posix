/********************************************************/
/*  ntapi: Native API core library                      */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.NTAPI.  */
/********************************************************/

#include <psxtypes/psxtypes.h>
#include <pemagine/pemagine.h>
#include <ntapi/ntapi.h>
#include "ntapi_impl.h"


/**
 *  rules for parsing the process's command line arguments
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
 *
 *  delimiters:
 *  -----------
 *  + white space    (ascii 0x20)
 *  + horizontal tab (ascii 0x09)
 *
 *  quoted strings, and special characters
 *  --------------------------------------
 *  + delimiter characters within a quoted string ("string with white space",
 *    or string" with white "space), stand for their literal respective
 *    characters.
 *
 *  + a backslash followed by a double quote (\") stands for a literal
 *    double quote.
 *
 *  + unless followed by a double quote, a backslash is just a (literal)
 *    backslash.
 *
 *  + when followed by a double quotation mark, an even sequence of 2 or
 *    more backslashes (2n) should be interpreted as a sequence of n literal
 *    backslashes.  The double quotation mark then designates the start
 *    or end of a double quoted string.
 *
 *  + when followed by a double quotation mark, an odd sequence of 2 or
 *    more backslashes (2n+1) should be interpreted as a sequence of n
 *    literal backslashes, followed by a single literal double quote.
 *
 *  + if found within a double quoted string, a sequence of two double
 *    quotation marks should be interpreted as a single literal double
 *    quote.
 *
 *  +  balanced nesting of syntactic double quotes is permitted.
 *
**/

/* free-standing process runtime data */
static nt_runtime_data	__rtdata;

int32_t __stdcall __ntapi_tt_parse_cmd_line_args_utf16(
	__in	wchar16_t *	cmd_line,
	__out	int *		arg_count,
	__in	wchar16_t *	args_buffer,
	__in	size_t		args_buffer_len,
	__out	size_t *	args_bytes_written __optional,
	__in	wchar16_t **	argv_buffer,
	__in	size_t		argv_buffer_len,
	__in	uint32_t	arg_flags)
{
	/**
	  * parse the command line arguments pointed to by cmd_line,
	  * copy the parsed arguments to args_buffer,
	  * and return 0 upon success.
	  *
	  * cmd_line must be a valid pointer to a command line string,
	  * and args_buffer, argv_buffer, and arg_count should
	  * all be aligned; furthermore, args_buffer_len and
	  * and argv_buffer_len must be exact multiples of sizeof(size_t).
	  *
	  * In case of an error, report failure using the appropriate
	  * native status code.
	**/

	/**
	 *  UTF-16: no need to fully determine the code point of the
	 *  current character; all we need to do is validate the
	 *  character or surrogate pair, and set the value of
	 *  wch_next accordingly.
	**/

	#define HORIZONTAL_TAB	0x09
	#define WHITE_SPACE	0x20
	#define DOUBLE_QUOTE	0x22
	#define SINGLE_QUOTE	0x27
	#define BACKSLASH	0x5C

	#define IS_DELIMITER(x)	((x == HORIZONTAL_TAB) || (x == WHITE_SPACE))

	#define TEST_ARGS_BUFFER(nbytes) \
				if ((uintptr_t)arg + nbytes \
						> (uintptr_t)args_buffer + args_buffer_len) { \
					return NT_STATUS_BUFFER_TOO_SMALL; \
				}

	#define ADD_N_BACKSLASHES \
				TEST_ARGS_BUFFER(backslash_count * sizeof(wchar16_t)); \
				for (islash = 0; \
						islash < backslash_count; \
						islash++) { \
					*arg = BACKSLASH; \
					arg++; \
				} \
				backslash_count = 0;

	#define ADD_SINGLE_WCHAR16_t(x) \
				TEST_ARGS_BUFFER(sizeof(wchar16_t)); \
				*arg = x; \
				arg++;

	wchar16_t *	arg;		/* null-terminated, copied to buffer */
	wchar16_t **	parg;		/* next pointer in the argv array */
	wchar16_t *	wch;		/* character being processed */
	wchar16_t *	wch_next;
	unsigned int	backslash_count;
	unsigned int	islash;
	unsigned char	quoted_state;

	/* check parameters for validity and alignment */
	if ((!(uintptr_t)cmd_line) || (*cmd_line == 0))
		/* we require at least one argument */
		return NT_STATUS_INVALID_PARAMETER_1;

	else if (__NT_IS_MISALIGNED_BUFFER(args_buffer))
		return NT_STATUS_INVALID_PARAMETER_2;

	else if (__NT_IS_MISALIGNED_LENGTH(args_buffer_len))
		return NT_STATUS_INVALID_PARAMETER_3;

	else if (__NT_IS_MISALIGNED_BUFFER(argv_buffer))
		return NT_STATUS_INVALID_PARAMETER_5;

	else if (__NT_IS_MISALIGNED_LENGTH(argv_buffer_len))
		return NT_STATUS_INVALID_PARAMETER_6;

	else if (__NT_IS_MISALIGNED_BUFFER(arg_count))
		return NT_STATUS_INVALID_PARAMETER_7;

	/* zero-out the aligned buffers */
	__ntapi->tt_aligned_block_memset(args_buffer,0,args_buffer_len);
	__ntapi->tt_aligned_block_memset(argv_buffer,0,argv_buffer_len);

	/* initialize */
	wch		= cmd_line;
	arg		= args_buffer;
	parg		= argv_buffer;
	*parg		= arg;
	*arg_count	= 0;
	quoted_state	= 0;
	backslash_count	= 0;

	/* arg points to the first character of a command line argument */
	/* parg points to the next pointer in argv_buffer */
	while (*wch) {
		if (!(quoted_state) && (IS_DELIMITER(*wch))) {
			/* pending backslashes? */
			if (backslash_count)
				ADD_N_BACKSLASHES;

			/* reached a delimiter outside of a quoted string */
			/* argument: alignment and null-termination */
			arg = (wchar16_t *)((((uintptr_t)arg + sizeof(size_t))
					| (sizeof(size_t) - 1))
					^ (sizeof(size_t) - 1));

			/* skip this and remaining delimiters */
			wch_next = wch + 1;
			while ((*wch_next) && (IS_DELIMITER(*wch_next)))
				wch_next++;

			/* keep going? */
			if (*wch_next == 0) {
				/* no more characters to process */
				/* nothing to do */
			} else if ((uintptr_t)parg >= \
					(uintptr_t)argv_buffer \
					+ argv_buffer_len) {
				/* argv_buffer is too small */
				return NT_STATUS_BUFFER_TOO_SMALL;
			} else if ((uintptr_t)arg >= \
					(uintptr_t)args_buffer \
					+ args_buffer_len) {
				/* args_buffer is too small */
				return NT_STATUS_BUFFER_TOO_SMALL;
			} else {
				/* advance parg, set last member  */
				parg++;
				*parg = arg;
			}
		} else {
			/* the current character is not a delimiter... */
			/* determine wch_next */
			if (((*wch >= 0x0000) && (*wch < 0xD800)) \
				|| ((*wch >= 0xE000) && (*wch < 0x10000))) {
				/* in the BMP, single 16-bit representation */
				wch_next = wch + 1;
			} else if ((*wch >= 0xD800) && (*wch < 0xDC00)) {
				/* validate surrogate pair */
				wch_next = wch + 1;

				if ((*wch_next >= 0xDC00) && (*wch_next < 0xE000))
					/* this is a valid surrogate pair */
					wch_next++;
				else
					return NT_STATUS_ILLEGAL_CHARACTER;
			} else
				return NT_STATUS_ILLEGAL_CHARACTER;

			/* we now know the position of this and the next character */
			/* continue with special cases */

			if (quoted_state && (*wch == DOUBLE_QUOTE) \
					&& (*wch_next == DOUBLE_QUOTE)) {
				/**
				 *  two consecutive double quotation marks
				 *  within a quoted string:
				 *  add a single quotation mark to the argument
				**/
				ADD_SINGLE_WCHAR16_t(DOUBLE_QUOTE);
				wch_next++;
			} else if (((backslash_count % 2) == 0) \
					&& (*wch == BACKSLASH) \
					&& (*wch_next == DOUBLE_QUOTE)) {
				/* 2n+1 backslashes followed by a double quote */
				backslash_count /= 2;
				/* add n backslashes */
				ADD_N_BACKSLASHES;
				/* add a literal double quotation mark */
				ADD_SINGLE_WCHAR16_t(DOUBLE_QUOTE);
				/* get ready for next character */
				wch_next++;
			} else if (backslash_count && (*wch == DOUBLE_QUOTE)) {
				/* 2n backslashes followed by a double quote */
				backslash_count /= 2;
				/* add n backslashes */
				ADD_N_BACKSLASHES;
				/* turn quoted_state on/off */
				quoted_state = !quoted_state;
			} else if ((*wch == BACKSLASH) \
						&& (*wch_next == BACKSLASH)) {
				/* this is a sequence of two backslashes */
				backslash_count += 2;
				wch_next++;
			} else {
				/* copy pending backslashes as needed */
				if (backslash_count)
					ADD_N_BACKSLASHES;

				if (*wch == DOUBLE_QUOTE) {
					/* turn quoted_state on/off */
					quoted_state = !quoted_state;
				} else {
					/* copy either two or four bytes */
					ADD_SINGLE_WCHAR16_t(*wch);
					wch++;

					/* surrogate pair? */
					if (wch < wch_next) {
						ADD_SINGLE_WCHAR16_t(*wch);
					}
				}
			}
		}

		/* proceed to the next character (or null termination) */
		wch = wch_next;
	}

	/* pending backslashes? */
	if (backslash_count)
		ADD_N_BACKSLASHES;

	/* null termination */
	ADD_SINGLE_WCHAR16_t(0);

	/* how many arguments did you say? */
	*arg_count = (int)(((uintptr_t)parg - (uintptr_t)argv_buffer)
				/ sizeof(size_t) + 1);

	/* output bytes written */
	if (args_bytes_written)
		*args_bytes_written = (uintptr_t)arg - (uintptr_t)args_buffer;

	return NT_STATUS_SUCCESS;
}


int32_t __stdcall __ntapi_tt_get_argv_envp_utf16(
	__out	int *		argc,
	__out	wchar16_t ***	wargv,
	__out	wchar16_t ***	wenvp,
	__in	uint32_t	flags,
	__in	void *		ext_params	__optional,
	__out	void *		reserved	__optional)
{
	nt_runtime_data *		rtdata;
	nt_argv_envp_block_info		main_params_internal;
	nt_argv_envp_block_info *	main_params;
	nt_get_argv_envp_ext_params *	__ext_params;
	ntapi_internals *		__internals;

	unsigned	idx;
	int32_t		status;
	uintptr_t	addr;
	intptr_t	offset;
	wchar16_t *	wch_s;
	wchar16_t *	wch_dst;
	wchar16_t **	wch_p;
	char **		ch_p;
	uintptr_t *	psrc;
	uintptr_t *	pdst;
	uintptr_t *	paligned;
	wchar16_t *	pboundary;

	/* init */
	__internals = __ntapi_internals();

	/* use internal buffer? */
	if (flags & NT_GET_ARGV_ENVP_USE_CALLER_BUFFER) {
		__ext_params = (nt_get_argv_envp_ext_params *)ext_params;
		main_params  = &(__ext_params->argv_envp_block_info);
	} else {
		/* pointers to internal/local structures */
		main_params = &main_params_internal;

		/* init */
		__ntapi->tt_aligned_block_memset(
			main_params,0,
			sizeof(*main_params));

		/* use internal buffer */
		main_params->cmd_line		 = __ntapi_tt_get_cmd_line_utf16();
		main_params->wargv_buffer	 = __internals->ntapi_img_sec_bss->argv_envp_array;
		main_params->wargv_buffer_len	 = __NT_BSS_ARGV_BUFFER_SIZE;
		main_params->argv_envp_ptr_total	 = (int)(main_params->wargv_buffer_len
							/ sizeof(uintptr_t));
		main_params->wargs_buffer	 = (wchar16_t *)&(__internals->ntapi_img_sec_bss->args_envs_buffer);
		main_params->wargs_buffer_len	 = __NT_BSS_ARGS_BUFFER_SIZE;
	}

	/* (__ntapi_parse_cmd_line_args_utf16 will zero-out both buffers) */
	status = __ntapi_tt_parse_cmd_line_args_utf16(
		main_params->cmd_line,
		&main_params->argc,
		main_params->wargs_buffer,
		main_params->wargs_buffer_len,
		&main_params->wargs_bytes_written,
		main_params->wargv_buffer,
		main_params->wargv_buffer_len,
		0);

	if (status) return status;

	/* argv[] needs a terminating null pointer */
	if (main_params->argc == main_params->argv_envp_ptr_total)
		return NT_STATUS_BUFFER_TOO_SMALL;

	/* set idx to the envp[0] array index */
	idx = main_params->argc + 1;

	/* set wenvp[] to its starting address */
	main_params->wenvp_buffer = &main_params->wargv_buffer[idx];

	/* update wargv_buffer_len and envp_buffer_len */
	main_params->wenvp_buffer_len = main_params->wargv_buffer_len
					- (idx * sizeof(uintptr_t));

	main_params->wargv_buffer_len = idx * sizeof(uintptr_t);

	/* align wenvs at pointer-size boundary */
	main_params->wargs_bytes_written += sizeof(uintptr_t) - 1;
	main_params->wargs_bytes_written /= sizeof(uintptr_t);
	main_params->wargs_bytes_written *= sizeof(uintptr_t);

	/* book-keeping */
	main_params->wenvs_buffer  = main_params->wargs_buffer
					+ main_params->wargs_bytes_written;

	main_params->wenvs_buffer_len = main_params->wargs_buffer_len 
					- main_params->wargs_bytes_written;

	main_params->wargs_buffer_len = main_params->wargs_bytes_written;


	/* peb environment block (read-only) */
	wch_s = __ntapi_tt_get_peb_env_block_utf16();

	if ((!wch_s) || (!*wch_s))
		return NT_STATUS_DLL_INIT_FAILED;

	/* populate the envp[] array */
	while ((*wch_s) && (idx < main_params->argv_envp_ptr_total)) {
		main_params->envc++;
		wch_p = &(main_params->wargv_buffer[idx]);
		*wch_p = wch_s;

		/* skip the rest of the environment variable */
		while (*++wch_s);

		/* advance to the next variable (or final null termination) */
		wch_s++;
		idx++;
	}

	/* envp[] needs a terminating null pointer */
	if ((*wch_s) && (idx = main_params->argv_envp_ptr_total))
		return NT_STATUS_BUFFER_TOO_SMALL;

	/* copy environment? */
	if (flags & NT_GET_ARGV_ENVP_COPY_ENVIRONMENT) {
		/* wch_s now points at the final null termination */
		main_params->wenvs_bytes_used =
				((uintptr_t)wch_s
				- (uintptr_t)(*main_params->wenvp_buffer));

		/* do we have enough room? */
		if (main_params->wenvs_buffer_len < main_params->wenvs_bytes_used)
			return NT_STATUS_BUFFER_TOO_SMALL;

		/* upper boundary */
		pboundary = ++wch_s;

		/* you'd expect the peb environment block to be aligned, 
		   but one can never know... */
		wch_s 	= *main_params->wenvp_buffer;
		wch_dst = main_params->wenvs_buffer;

		while ((uintptr_t)wch_s % sizeof(uintptr_t)) {
			*wch_dst = *wch_s;
			wch_s++;
			wch_dst++;
		}

		/* copy the aligned portion of the environment block */
		addr = (uintptr_t)(pboundary);
		addr /= sizeof(uintptr_t);
		addr *= sizeof(uintptr_t);
		paligned = (uintptr_t *)addr;

		psrc = (uintptr_t *)wch_s;
		pdst = (uintptr_t *)wch_dst;

		while (psrc < paligned) {
			*pdst = *psrc;
			psrc++;
			pdst++;
		}

		/* copy any remaining bytes */
		wch_s	= (wchar16_t *)paligned;
		wch_dst	= (wchar16_t *)pdst;

		while (wch_s < pboundary) {
			*wch_dst = *wch_s;
			wch_s++;
			wch_dst++;
		}

		/* finally, we update the envp[] pointers */
		offset = (intptr_t)main_params->wenvs_buffer
			- (intptr_t)*main_params->wenvp_buffer;

		wch_p = main_params->wenvp_buffer;

		while (*wch_p) {
			addr = ((uintptr_t)*wch_p) + offset;
			*wch_p = (wchar16_t *)addr;
			wch_p++;
		}
	}

	/* (command line arguments always get validated) */
	/* validate the environment block? */
	if (flags & NT_GET_ARGV_ENVP_VALIDATE_UTF16) {
		wch_p = main_params->wenvp_buffer;

		while (*wch_p) {
			status = __ntapi->uc_validate_unicode_stream_utf16(
				*wch_p,
				0,0,0,0,0);

			if (status != NT_STATUS_SUCCESS)
				return status;
			else
				wch_p++;
		}
	}

	/* once */
	if (!__internals->rtdata) {
		__ntapi->tt_get_runtime_data(
			&__internals->rtdata,
			main_params->wargv_buffer);

		if (!__internals->rtdata) {
			__internals->rtdata = &__rtdata;

			if ((status =__ntapi->tt_init_runtime_data(&__rtdata)))
				return status;

		} else if ((status =__ntapi->tt_update_runtime_data(__internals->rtdata)))
			return status;

		rtdata = __internals->rtdata;

		rtdata->peb_envc  = main_params->envc;
		rtdata->peb_argc	  = main_params->argc;
		rtdata->peb_wargv = main_params->wargv_buffer;
		rtdata->peb_wenvp = main_params->wenvp_buffer;

		/* integral wargv, wenvp, argv, envp */
		if (rtdata->wargv) {
			rtdata->wargv += (uintptr_t)rtdata / sizeof(wchar16_t *);

			for (wch_p=rtdata->wargv; *wch_p; wch_p++)
				*wch_p += (uintptr_t)rtdata / sizeof(wchar16_t);
		};

		if (rtdata->wenvp) {
			rtdata->wenvp += (uintptr_t)rtdata / sizeof(wchar16_t *);

			for (wch_p=rtdata->wenvp; *wch_p; wch_p++)
				*wch_p += (uintptr_t)rtdata / sizeof(wchar16_t);
		}

		if (rtdata->argv) {
			rtdata->argv += (uintptr_t)rtdata / sizeof(char *);

			for (ch_p=rtdata->argv; *ch_p; ch_p++)
				*ch_p += (uintptr_t)rtdata;

			rtdata->argc = (int32_t)(ch_p - rtdata->argv);
		};

		if (rtdata->envp) {
			rtdata->envp += (uintptr_t)rtdata / sizeof(char *);

			for (ch_p=rtdata->envp; *ch_p; ch_p++)
				*ch_p += (uintptr_t)rtdata;

			rtdata->envc = (int32_t)(ch_p - rtdata->envp);
		};
	}

	/* we're good */
	*argc = main_params->argc;
	*wargv = main_params->wargv_buffer;
	*wenvp = main_params->wenvp_buffer;

	return NT_STATUS_SUCCESS;
}


int32_t __stdcall 	__ntapi_tt_get_argv_envp_utf8(
	__out	int *		argc,
	__out	char ***	argv,
	__out	char ***	envp,
	__in	uint32_t	flags,
	__in	void *		ext_params	__optional,
	__out	void *		reserved	__optional)
{
	int32_t			status;
	ntapi_internals *	__internals;

	wchar16_t **	wargv;
	wchar16_t **	wenvp;
	uint32_t	pcount;

	nt_get_argv_envp_ext_params	__ext_params_internal;
	nt_get_argv_envp_ext_params *	__ext_params;
	nt_argv_envp_block_info *	main_params;

	/* use internal buffer? */
	if (flags & NT_GET_ARGV_ENVP_USE_CALLER_BUFFER) {
		__ext_params = (nt_get_argv_envp_ext_params *)ext_params;
		main_params  = &__ext_params->argv_envp_block_info;
	} else {
		/* pointers to internal/local structures */
		__ext_params = &__ext_params_internal;
		main_params  = &__ext_params->argv_envp_block_info;

		/* init */
		__ntapi->tt_aligned_block_memset(
			main_params,0,
			sizeof(*main_params));

		__internals = __ntapi_internals();

		/* use internal buffer */
		main_params->cmd_line		 = __ntapi_tt_get_cmd_line_utf16();
		main_params->wargv_buffer	 = __internals->ntapi_img_sec_bss->argv_envp_array;
		main_params->wargv_buffer_len	 = __NT_BSS_ARGV_BUFFER_SIZE;
		main_params->argv_envp_ptr_total	 = (int)(main_params->wargv_buffer_len
							/ sizeof(uintptr_t));
		main_params->wargs_buffer	 = (wchar16_t *)&(__internals->ntapi_img_sec_bss->args_envs_buffer);
		main_params->wargs_buffer_len	 = __NT_BSS_ARGS_BUFFER_SIZE;
	}

	/* start with obtaining the utf-16 environment */
	status = __ntapi->tt_get_argv_envp_utf16(
		argc,
		&wargv,
		&wenvp,
		flags | NT_GET_ARGV_ENVP_USE_CALLER_BUFFER,
		__ext_params,
		reserved);

	if (status) return status;

	/* enough pointers left? */
	pcount = main_params->argc + 1 + main_params->envc + 1;

	if (pcount > (main_params->argv_envp_ptr_total / 2))
		return NT_STATUS_BUFFER_TOO_SMALL;
	else if ((main_params->wenvs_buffer_len - main_params->wenvs_bytes_used)
			< sizeof(uintptr_t))
		return NT_STATUS_BUFFER_TOO_SMALL;

	/* first args byte should be aligned at pointer-size boundary */
	main_params->wenvs_bytes_used += sizeof(uintptr_t) - 1;
	main_params->wenvs_bytes_used /= sizeof(uintptr_t);
	main_params->wenvs_bytes_used *= sizeof(uintptr_t);

	/* book-keeping */
	/* block reminder: wargs -- wenvs -- args -- envs */
	main_params->argv = (char **)main_params->wenvp_buffer;
	main_params->argv += main_params->envc + 1;

	main_params->args_buffer = (char *)main_params->wenvs_buffer;
	main_params->args_buffer += main_params->wenvs_bytes_used;

	main_params->args_buffer_len = main_params->wenvs_buffer_len
					- main_params->wenvs_bytes_used;

	main_params->wenvs_buffer_len = main_params->wenvs_bytes_used;

	/* create a utf-8 argv[] array */
	status = __ntapi_tt_array_convert_utf16_to_utf8(
		main_params->wargv_buffer,
		main_params->argv,
		0,
		main_params->args_buffer,
		main_params->args_buffer_len,
		&main_params->args_bytes_written);

	if (status) return status;

	/* first envs byte should be aligned to pointer-size boundary */
	main_params->args_bytes_written += sizeof(uintptr_t) - 1;
	main_params->args_bytes_written /= sizeof(uintptr_t);
	main_params->args_bytes_written *= sizeof(uintptr_t);

	/* book-keeping */
	main_params->envp = main_params->argv + main_params->argc + 1;

	main_params->envs_buffer  = main_params->args_buffer
					+ main_params->args_bytes_written;

	main_params->envs_buffer_len = main_params->args_buffer_len 
					- main_params->args_bytes_written;

	main_params->args_buffer_len = main_params->args_bytes_written;

	/* subsequent streams (if any) should be aligned to pointer-size boundary */
	main_params->envs_bytes_used += sizeof(uintptr_t) - 1;
	main_params->envs_bytes_used /= sizeof(uintptr_t);
	main_params->envs_bytes_used *= sizeof(uintptr_t);

	/* create a utf-8 envp[] array */
	status = __ntapi_tt_array_convert_utf16_to_utf8(
		main_params->wenvp_buffer,
		main_params->envp,
		0,
		main_params->envs_buffer,
		main_params->envs_buffer_len,
		&main_params->envs_bytes_used);

	if (status) return status;

	/* we're good */
	*argc = main_params->argc;
	*argv = main_params->argv;
	*envp = main_params->envp;

	return NT_STATUS_SUCCESS;
}


wchar16_t * __stdcall __ntapi_tt_get_cmd_line_utf16(void)
{
	nt_peb *		peb;
	nt_unicode_string	cmd_line;

	peb = (nt_peb *)pe_get_peb_address();

	if (peb) {
		cmd_line = peb->process_params->command_line;
		return cmd_line.buffer;
	} else
		return (wchar16_t *)0;
}


wchar16_t * __stdcall __ntapi_tt_get_peb_env_block_utf16(void)
{
	nt_peb *		peb;

	peb = (nt_peb *)pe_get_peb_address();

	if (peb)
		return peb->process_params->environment;
	else
		return (wchar16_t *)0;
}
