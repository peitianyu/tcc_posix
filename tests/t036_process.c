/* t036_process: process/session 温和探针.
 * 1) 身份与账号查询 (getpid/getppid/getuid/getgid/getpgid 等);
 * 2) fork + waitpid 冒烟 (psxscl: ZwCreateProcess + ZwCreateThread 内核复制):
 *    子进程返回 0 且 getppid()==父 pid, 父进程 waitpid 收割 SIGCHLD 得子退出码。
 * 全部通过则 process 基础 OK。 */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
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

	/* ---- fork + waitpid 冒烟 (子=新进程, 父=原进程) ---- */
	{
		pid_t parent, child, gpid;
		int st = 0, w;

		parent = getpid();
		fflush(stdout);                 /* 子进程复制 stdio 缓冲, 先清空避免双写 */
		child = fork();
		if (child == 0) {
			/* 子进程: fork 返回 0, 且 getppid() 应为父 pid */
			gpid = getppid();
			printf("PASS fork child  -> me=%d ppid=%d (parent=%d)\n",
			       getpid(), gpid, parent);
			fflush(stdout);
			_exit(gpid == parent ? 0 : 2);
		} else if (child > 0) {
			/* 父进程: fork 返回子 pid, waitpid 收割 SIGCHLD */
			printf("PASS fork parent -> child=%d (me=%d)\n", child, parent);
			do {
				w = waitpid(child, &st, 0);
			} while (w < 0 && errno == EINTR);
			if (w == child && WIFEXITED(st) && WEXITSTATUS(st) == 0)
				printf("PASS waitpid     -> child exit=0 (SIGCHLD reaped)\n");
			else { printf("FAIL waitpid     -> w=%d st=%d\n", w, st); fail++; }
		} else {
			printf("FAIL fork        -> %d (errno=%d)\n", child, errno);
			fail++;
		}
	}

	return fail;
}