/********************************************************/
/*  psxscl: a thread-safe system call layer library     */
/*  Copyright (C) 2013,2014,2015  Z. Gilbao             */
/*  Released under GPLv2 and GPLv3; see COPYING.PSXSCL. */
/********************************************************/
/* tcc_posix (R13b): TCC -bt 运行时 (bt-exe.o) 依赖的少量
 * Windows 异常/内存查询入口.
 *
 * musl-nt64 的可执行文件不 import kernel32 的 API (零 Windows
 * API 依赖), 但 bt-exe.o 为了捕获崩溃并打印回溯需要:
 *   - AddVectoredExceptionHandler: 注册 VEH (ntdll 原生名
 *     RtlAddVectoredExceptionHandler), CPU 异常时 tccrun.c
 *     的 cpu_exception_handler 打印调用栈.
 *   - VirtualQuery: 判定地址是否落在内存映射内 (回溯 PC 解析).
 * 这里在 psx 接口层按需从 ntdll/kernel32 经 pe_get_procedure_address
 * 惰性解析, 不改 musl, 也不引入新的静态 import 依赖.
 */

#include <pemagine/pemagine.h>
#include "psx.h"

/* ---------- VEH (RtlAddVectoredExceptionHandler) ---------- */
typedef void * (*__bt_veh_fn)(unsigned long first, void * handler);

void * AddVectoredExceptionHandler(unsigned long first, void * handler)
{
	static __bt_veh_fn	fn;
	void *			mod;

	if (!fn) {
		mod = pe_get_ntdll_module_handle();
		if (mod)
			fn = (__bt_veh_fn)pe_get_procedure_address(
				mod, "RtlAddVectoredExceptionHandler");
		if (!fn) {
			mod = pe_get_kernel32_module_handle();
			if (mod)
				fn = (__bt_veh_fn)pe_get_procedure_address(
					mod, "AddVectoredExceptionHandler");
		}
	}

	return fn ? fn(first, handler) : 0;
}

/* ---------- VirtualQuery (kernel32) ---------- */
typedef unsigned long long (*__bt_vquery_fn)(
	const void * addr, void * mbi, unsigned long len);

unsigned long long VirtualQuery(const void * addr, void * mbi,
	unsigned long len)
{
	static __bt_vquery_fn	fn;
	void *			mod;

	if (!fn) {
		mod = pe_get_kernel32_module_handle();
		if (mod)
			fn = (__bt_vquery_fn)pe_get_procedure_address(
				mod, "VirtualQuery");
	}

	return fn ? fn(addr, mbi, len) : 0;
}