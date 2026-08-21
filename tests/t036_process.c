/* t036_process: process/session 温和探针.
 * 只验证身份与账号查询 (getpid/getppid/getuid 等), 不触碰 fork/execve/
 * wait (重进程创建, pre-alpha 不稳)。全部通过则 process 基础 OK。 */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

int main(void)
{
	pid_t pid, ppid, pgid;
	int fail = 0;

	pid = getpid();
	if (pid < 0) { printf("FAIL getpid: %d\n", pid); fail++; }
	else printf("PASS getpid -> %d\n", pid);

	ppid = getppid();
	if (ppid < 0) { printf("FAIL getppid: %d\n", ppid); fail++; }
	else printf("PASS getppid -> %d\n", ppid);

	pgid = getpgid(0);
	if (pgid < 0) { printf("FAIL getpgid(0): %d\n", pgid); fail++; }
	else printf("PASS getpgid(0) -> %d\n", pgid);

	if (getpgrp() != pgid) { printf("FAIL getpgrp != getpgid(0)\n"); fail++; }
	else printf("PASS getpgrp == getpgid(0)\n");

	if (getuid() < 0) { printf("FAIL getuid\n"); fail++; }
	else printf("PASS getuid -> %d\n", getuid());

	if (geteuid() < 0) { printf("FAIL geteuid\n"); fail++; }
	else printf("PASS geteuid -> %d\n", geteuid());

	if (getgid() < 0) { printf("FAIL getgid\n"); fail++; }
	else printf("PASS getgid -> %d\n", getgid());

	if (getegid() < 0) { printf("FAIL getegid\n"); fail++; }
	else printf("PASS getegid -> %d\n", getegid());

	return fail;
}