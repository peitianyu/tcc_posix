/* 测试: R3 sched_yield 已注册 (__sys_sched_yield, SwitchToThread).
 * 底前 __sysvtbl[24]==NULL → 段错误; 注册后返回 0。 */
#include <sched.h>
#include <stdio.h>
int main(void) {
	if (sched_yield() != 0) { perror("sched_yield"); return 1; }
	printf("sched_yield ok\n");
	return 0;
}