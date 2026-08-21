#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
static void *worker(void *arg) {
	/* 模拟耗时 worker: main 退出时应被进程终止带走 */
	write(2, "worker started\n", 15);
	sleep(5);
	write(2, "worker done\n", 12);
	return 0;
}
int main(void) {
	pthread_t th;
	pthread_create(&th, 0, worker, 0);
	write(2, "main: exiting now (worker still running)\n", 40);
	return 0;  /* main 立即退出, 不应等 worker */
}
