#include <stdlib.h>
#include <signal.h>
#include "syscall.h"

_Noreturn void abort(void)
{
	raise(SIGABRT);
	/* 本端口 (nt64) 的 raise 不派发默认动作: SIGABRT 无默认处理器时 raise
	 * 直接返回. 若用户已注册处理器, raise 会调用它 (处理器 longjmp 则
	 * 不走到这里, 符合 abort 可被拦截语义); 处理器返回/无处理器则按
	 * SIGABRT 语义终止: 退出码 128+6=134 (waitpid WIFSIGNALED 判定兼容).
	 * 此前用 for(;;) 挂死 — assert/abort 无法判退出 (KNOWN_ISSUES §1). */
	_Exit(134);
}
