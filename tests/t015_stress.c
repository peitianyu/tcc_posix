#include <pthread.h>
#include <stdio.h>
static void *worker(void *arg) {
	long n = (long)arg;
	return (void *)(n * 7 + 3);
}
int main(void) {
	pthread_t th;
	void *ret;
	int round, fail = 0;
	/* t015_stress.c - 线程压力: 100 顺序 + 80 并发 create/join
   验证 __clone_ctxs 槽复用 (worker 退出释放) + pthreads 计数平衡 */
	for (round = 0; round < 100; round++) {
		if (pthread_create(&th, 0, worker, (void *)(long)round)) {
			printf("create fail round %d\n", round);
			return 1;
		}
		if (pthread_join(th, &ret)) {
			printf("join fail round %d\n", round);
			return 1;
		}
		if ((long)ret != round * 7 + 3) { printf("ret wrong round %d: %ld\n", round, (long)ret); fail++; }
	}
	/* 8 并发 × 10 轮 */
	pthread_t ths[8];
	for (round = 0; round < 10; round++) {
		int i;
		for (i = 0; i < 8; i++)
			if (pthread_create(&ths[i], 0, worker, (void *)(long)(round*8+i))) { printf("c fail r%d\n", round); return 1; }
		for (i = 0; i < 8; i++) {
			if (pthread_join(ths[i], &ret)) { printf("j fail r%d\n", round); return 1; }
			if ((long)ret != (round*8+i)*7+3) { printf("ret wrong\n"); fail++; }
		}
	}
	if (fail) { printf("FAIL %d\n", fail); return 1; }
	printf("stress OK: 100 seq + 80 conc\n");
	return 0;
}
