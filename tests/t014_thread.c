/* t014_thread.c - pthread create/join 多线程测试 (Windows psxscl 后端) */
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define NTHREADS 8

static void *worker(void *arg)
{
	long n = (long)arg;
	long i;
	for (i = 0; i < 100000; i++)
		;
	printf("thread %ld done\n", n);
	return (void *)(n * 1000 + 42);
}

int main(void)
{
	pthread_t th[NTHREADS];
	void *ret[NTHREADS];
	int i, fail = 0;

	for (i = 0; i < NTHREADS; i++) {
		if (pthread_create(&th[i], 0, worker, (void *)(long)i) != 0) {
			printf("pthread_create %d failed\n", i);
			return 1;
		}
	}
	for (i = 0; i < NTHREADS; i++) {
		if (pthread_join(th[i], &ret[i]) != 0) {
			printf("pthread_join %d failed\n", i);
			return 1;
		}
		if ((long)ret[i] != i * 1000 + 42) {
			printf("thread %d wrong ret: %ld\n", i, (long)ret[i]);
			fail++;
		}
	}
	if (fail) return 1;
	printf("all %d threads joined OK\n", NTHREADS);
	return 0;
}
