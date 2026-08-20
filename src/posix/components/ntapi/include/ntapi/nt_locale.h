#ifndef _NT_LOCALE_H_
#define _NT_LOCALE_H_

#include <psxtypes/psxtypes.h>

typedef uint32_t nt_lcid;
typedef uint16_t nt_langid;


typedef int32_t __stdcall ntapi_zw_query_default_locale(
	__in	unsigned char	thread_or_system,
	__out	nt_lcid *	locale);


typedef int32_t __stdcall ntapi_zw_set_default_locale(
	__in	unsigned char	thread_or_system,
	__in	nt_lcid *	locale);


typedef int32_t __stdcall ntapi_zw_query_default_ui_language(
	__out	nt_langid *	lang_id);


typedef int32_t __stdcall ntapi_zw_set_default_ui_language(
	__in	nt_langid *	lang_id);


typedef int32_t __stdcall ntapi_zw_query_install_ui_language(
	__out	nt_langid *	lang_id);

#endif
