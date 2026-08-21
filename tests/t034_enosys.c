/* 测试: R1 通用 ENOSYS 保护. 未注册 syscall (getrandom=318) 应返回
 * -ENOSYS 而非段错误. __syscall_vtbl[318]==NULL → __syscallN 返回 -ENOSYS.
 * 注: timer(222) R4 已实现, mq(240-245) R10a 已实现, 均改由 getrandom 裸
 * syscall 验证 vtbl 保护. */
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
int main(void) {
	long r;
	errno = 0;
	r = syscall(SYS_getrandom, 0, 0, 0);
	if (r != -1) { printf("expected -1, got %ld\n", r); return 1; }
	if (errno != ENOSYS) { printf("expected ENOSYS, got %s\n", strerror(errno)); return 2; }
	printf("ENOSYS ok\n");
	return 0;
}