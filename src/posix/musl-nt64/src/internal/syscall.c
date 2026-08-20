#include <stdarg.h>
#include "syscall.h"

/*
 * midipix syscall dispatch: musl 原版代码 (posix_fadvise.c / __syscall_cp.c 等)
 * 直接调用 __syscall() 函数。midipix 下所有 syscall 经 __syscall_vtbl 分派
 * (psxscl 初始化填充; 见 arch/nt64/syscall_arch.h 的 __syscall6 内联)。
 * syscall.c 原为空文件, 补回此定义 (链接需 __syscall 符号)。
 * 注意: syscall.h 定义 #define __syscall(...) 变参宏, 定义需括号遮蔽。
 */
long (__syscall)(long n, ...)
{
	va_list ap;
	long a1, a2, a3, a4, a5, a6;

	va_start(ap, n);
	a1 = va_arg(ap, long);
	a2 = va_arg(ap, long);
	a3 = va_arg(ap, long);
	a4 = va_arg(ap, long);
	a5 = va_arg(ap, long);
	a6 = va_arg(ap, long);
	va_end(ap);

	return __syscall6(n, a1, a2, a3, a4, a5, a6);
}
