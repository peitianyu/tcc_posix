/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilboa             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/
/* tcc_posix (R10c): dlfcn 补全.
 *
 * musl-nt64 无 dlfcn 实现 (链接层缺 dlopen/dlsym/dlclose 符号)。
 * 在 psx 接口层直接提供, 不改 musl:
 *   dlopen  -> LdrLoadDll  (从 ntdll 经 pe_get_procedure_address 解析)
 *   dlclose -> LdrUnloadDll
 *   dlsym   -> pe_get_procedure_address (导出表按名解析)
 *   dlerror -> psx 本地错误缓冲
 *   dladdr  -> pe_get_symbol_module_info / pe_get_symbol_name
 *
 * 走既有 Windows 运行时 (PE LDR) 路径, 不新增 Windows API 依赖
 * (仍 ntdll zw/ldr + pemagine), 与 pe_load_framework_loader.c 同款做法。
 */

#include <ntapi/ntapi.h>
#include <ntapi/nt_atomic.h>
#include "psx_systypes.h"
#include "psx_errno.h"
#include "psx_impl.h"
#include "psx.h"

/* ---------- LdrLoadDll / LdrUnloadDll (ntdll) ---------- */

typedef int32_t __stdcall __psx_ldr_load_fn(
	wchar16_t *		image_path	__optional,
	uint32_t *		image_flags	__optional,
	struct pe_unicode_str *	image_name,
	void **			image_base);

typedef int32_t __stdcall __psx_ldr_unload_fn(void * image_base);

static __psx_ldr_load_fn *	__psx_ldr_load;
static __psx_ldr_unload_fn *	__psx_ldr_unload;

static int __psx_ldr_runtime_init(void)
{
	void * hntdll;

	if (__psx_ldr_load && __psx_ldr_unload)
		return 0;

	hntdll = pe_get_ntdll_module_handle();
	if (!hntdll)
		return -1;

	__psx_ldr_load   = (__psx_ldr_load_fn *)pe_get_procedure_address(hntdll,"LdrLoadDll");
	__psx_ldr_unload = (__psx_ldr_unload_fn *)pe_get_procedure_address(hntdll,"LdrUnloadDll");

	return (__psx_ldr_load && __psx_ldr_unload) ? 0 : -1;
}

/* ---------- Dl_info / dlerror 缓冲 ---------- */

struct __psx_dl_info {
	const char *	dli_fname;
	void *		dli_fbase;
	const char *	dli_sname;
	void *		dli_saddr;
};

static char __psx_dlerror[64];
static int  __psx_dlerror_set;

static void __psx_dlerror_hex(intptr_t code)
{
	int		i;
	uintptr_t	u = (uintptr_t)code;
	char		hex[] = "0123456789abcdef";
	int		p = 0;

	__psx_dlerror[p++] = '0';
	__psx_dlerror[p++] = 'x';
	for (i = (int)sizeof(uintptr_t)*2-1; i >= 0; i--) {
		int shift = i*4;
		if (p > 2 || ((u >> shift) & 0xf))
			__psx_dlerror[p++] = hex[(u >> shift) & 0xf];
	}
	if (p == 2) __psx_dlerror[p++] = '0';
	__psx_dlerror[p] = 0;
	__psx_dlerror_set = 1;
}

/* ---------- dlfcn API ---------- */

__psx_api
void * dlopen(const char * file, int mode)
{
	wchar16_t		wname[512];
	struct pe_unicode_str	un;
	void *			base = 0;
	int32_t			status;
	size_t			len;
	uint32_t		flags = (uint32_t)mode;

	/* dlopen(NULL) -> 主程序模块句柄 */
	if (!file || !*file) {
		__psx_dlerror_set = 0;
		return pe_get_first_module_handle();
	}

	if (__psx_ldr_runtime_init())
		return 0;

	len = 0;
	while (file[len] && len < 511) {
		wname[len] = (wchar16_t)(unsigned char)file[len];
		len++;
	}
	wname[len] = 0;

	__ntapi->tt_aligned_block_memset(&un,0,sizeof(un));
	un.strlen = (uint16_t)(len * sizeof(wchar16_t));
	un.maxlen = (uint16_t)(sizeof(wname));
	un.buffer = wname;

	status = __psx_ldr_load(0,&flags,&un,&base);
	if (status) {
		__psx_dlerror_hex((intptr_t)status);
		return 0;
	}

	__psx_dlerror_set = 0;
	return base;
}

__psx_api
int dlclose(void * handle)
{
	int32_t status;

	if (!handle)
		return -1;

	if (__psx_ldr_runtime_init())
		return -1;

	status = __psx_ldr_unload(handle);
	if (status) {
		__psx_dlerror_hex((intptr_t)status);
		return -1;
	}

	__psx_dlerror_set = 0;
	return 0;
}

__psx_api
void * dlsym(void * handle, const char * name)
{
	void * addr;

	if (!handle || !name) {
		__psx_dlerror_set = 1;
		return 0;
	}

	addr = pe_get_procedure_address(handle,name);
	if (!addr) {
		__psx_dlerror_hex(-1);
		return 0;
	}

	__psx_dlerror_set = 0;
	return addr;
}

__psx_api
char * dlerror(void)
{
	if (!__psx_dlerror_set)
		return 0;
	__psx_dlerror_set = 0;
	return __psx_dlerror;
}

__psx_api
int dladdr(const void * addr, struct __psx_dl_info * info)
{
	struct pe_ldr_tbl_entry * le;
	char * name;

	if (!info)
		return 0;

	__ntapi->tt_aligned_block_memset(info,0,sizeof(*info));

	le = pe_get_symbol_module_info((void *)addr);
	if (le && le->dll_base) {
		info->dli_fbase = le->dll_base;
		name = pe_get_symbol_name(le->dll_base,(void *)addr);
		if (name) {
			info->dli_sname = name;
			info->dli_saddr = (void *)addr;
		}
		return 1;
	}

	return 0;
}

__psx_api
int dlinfo(void * handle, int req, void * res)
{
	(void)handle;
	(void)req;
	(void)res;
	return -1;
}