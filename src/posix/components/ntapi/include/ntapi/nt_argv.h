#ifndef _NT_ARGV_H_
#define _NT_ARGV_H_

/******************************************************
 *                                                    *
 *   this header is REALLY NOT what you are looking   *
 *   for, however if you are into writing your apps   *
 *   using the Native API only, then you might find   *
 *   the below interfaces somehow useful.             *
 *                                                    *
 *****************************************************/

#include <psxtypes/psxtypes.h>

/* ntapi_tt_get_argv_envp_utf16 flag bits */
#define NT_GET_ARGV_ENVP_USE_INTERNAL_BUFFER	(0x0000)
#define NT_GET_ARGV_ENVP_USE_CALLER_BUFFER	(0x0001)
#define NT_GET_ARGV_ENVP_COPY_ENVIRONMENT	(0x0002)
#define NT_GET_ARGV_ENVP_VALIDATE_UTF16		(0x0004)

/* ntapi_tt_program_option flag bits */
#define NT_OPTION_SHORT			(0x0001)
#define NT_OPTION_LONG			(0x0002)
#define NT_OPTION_ALLOWED_ONCE		(0x0100)
#define NT_OPTION_ALLOWED_MANY		(0x0200)
#define NT_OPTION_REQUIRED		(0x0400)
#define NT_OPTION_VALUE_REQUIRED	(0x1000)

typedef struct _nt_program_option {
	int		option_count;
	wchar16_t	short_name_code;
	wchar16_t *	long_name;
	uint32_t	long_name_hash;
	wchar16_t *	value;
	uint32_t	value_hash;
	uint32_t	flags;
} nt_program_option;


typedef struct _nt_program_options_meta {
	int	idx_next_argument;
	int	idx_invalid_short_name;
	int	idx_invalid_long_name;
	int	idx_invalid_argument;
	int	idx_missing_option_value;
} nt_program_options_meta;


typedef struct _nt_env_var_meta_utf16 {
	wchar16_t *	name;
	uint32_t	name_hash;
	wchar16_t *	value;
	uint32_t	value_hash;
	int		envp_index;
	uint32_t	flags;
} nt_env_var_meta_utf16;


typedef struct _nt_cmd_option_meta_utf16 {
	wchar16_t *	short_name;
	uint32_t	short_name_code;
	wchar16_t *	long_name;
	uint32_t	long_name_hash;
	wchar16_t *	value;
	uint32_t	value_hash;
	int		argv_index;
	uint32_t	flags;
} nt_cmd_option_meta_utf16;


typedef struct _nt_argv_envp_block_info {
	wchar16_t *	cmd_line;
	wchar16_t **	wargv_buffer;
	size_t		wargv_buffer_len;
	wchar16_t *	wargs_buffer;
	size_t		wargs_buffer_len;
	size_t		wargs_bytes_written;
	wchar16_t **	wenvp_buffer;
	size_t		wenvp_buffer_len;
	wchar16_t *	wenvs_buffer;
	size_t		wenvs_buffer_len;
	size_t		wenvs_bytes_used;
	char *		args_buffer;
	size_t		args_buffer_len;
	size_t		args_bytes_written;
	char *		envs_buffer;
	size_t		envs_buffer_len;
	size_t		envs_bytes_used;
	uint32_t	arg_flags;
	uint32_t	env_flags;
	uint32_t	psx_flags;
	uint32_t	psx_padding;
	uint32_t	argv_envp_ptr_total;
	int		envc;

	int		argc;
	char **		argv;
	char **		envp;
} nt_argv_envp_block_info;


typedef struct _nt_get_argv_envp_ext_params {
	nt_argv_envp_block_info	argv_envp_block_info;
} nt_get_argv_envp_ext_params;


typedef wchar16_t * __stdcall	ntapi_tt_get_cmd_line_utf16(void);


typedef wchar16_t * __stdcall	ntapi_tt_get_peb_env_block_utf16(void);


typedef int32_t __stdcall 	ntapi_tt_parse_cmd_line_args_utf16(
	__in	wchar16_t *	cmd_line,
	__out	int *		arg_count,
	__in	wchar16_t *	args_buffer,
	__in	size_t		args_buffer_len,
	__out	size_t *	args_bytes_written __optional,
	__in	wchar16_t **	argv_buffer,
	__in	size_t		argv_buffer_len,
	__in	uint32_t	arg_flags);


typedef int32_t __stdcall 	ntapi_tt_get_argv_envp_utf8(
	__out	int *		argc,
	__out	char ***	argv,
	__out	char ***	envp,
	__in	uint32_t	flags,
	__in	void *		ext_params	__optional,
	__out	void *		reserved	__optional);


typedef int32_t __stdcall 	ntapi_tt_get_argv_envp_utf16(
	__out	int *		argc,
	__out	wchar16_t ***	wargv,
	__out	wchar16_t ***	wenvp,
	__in	uint32_t	flags,
	__in	void *		ext_params	__optional,
	__out	void *		reserved	__optional);


typedef int32_t __stdcall 	ntapi_tt_get_env_var_meta_utf16(
	__in	const uint32_t *	crc32_table,
	__in	wchar16_t *		env_var_name,
	__in	uint32_t		env_var_name_hash __optional,
	__in	wchar16_t **		envp,
	__out	nt_env_var_meta_utf16 *	env_var_meta);


typedef int32_t __stdcall	ntapi_tt_get_short_option_meta_utf16(
	__in	const uint32_t *		crc32_table,
	__in	wchar16_t			option_name,
	__in	wchar16_t *			argv[],
	__out	nt_cmd_option_meta_utf16 *	cmd_opt_meta);


typedef int32_t __stdcall	ntapi_tt_get_long_option_meta_utf16(
	__in	const uint32_t *		crc32_table,
	__in	wchar16_t *			option_name,
	__in	uint32_t			option_name_hash __optional,
	__in	wchar16_t *			argv[],
	__out	nt_cmd_option_meta_utf16 *	cmd_opt_meta);

typedef int32_t __stdcall	ntapi_tt_array_copy_utf8(
	__out	int *			argc,
	__in	const char **		argv,
	__in	const char **		wenvp,
	__in	const char *		image_name	__optional,
	__in	const char *		interpreter	__optional,
	__in	const char *		optarg		__optional,
	__in	void *			base,
	__out	void *			buffer,
	__in	size_t			buflen,
	__out	size_t *		blklen);

typedef int32_t __stdcall	ntapi_tt_array_copy_utf16(
	__out	int *			argc,
	__in	const wchar16_t **	wargv,
	__in	const wchar16_t **	wenvp,
	__in	const wchar16_t *	image_name	__optional,
	__in	const wchar16_t *	interpreter	__optional,
	__in	const wchar16_t *	optarg		__optional,
	__in	void *			base,
	__out	void *			buffer,
	__in	size_t			buflen,
	__out	size_t *		blklen);

typedef int32_t __stdcall	ntapi_tt_array_convert_utf8_to_utf16(
	__in		char **		arrv,
	__in		wchar16_t **	arra,
	__in		void *		base,
	__in		wchar16_t *	buffer,
	__in		size_t		buffer_len,
	__out		size_t *	bytes_written);

typedef int32_t __stdcall	ntapi_tt_array_convert_utf16_to_utf8(
	__in		wchar16_t **	warrv,
	__in		char **		arra,
	__in		void *		base,
	__in		char *		buffer,
	__in		size_t		buffer_len,
	__out		size_t *	bytes_written);

#endif
