/* t019_sync.c - barrier + semaphore + detach 同步原语测试 */
#include <pthread.h>
#include <stdio.h>
#include <semaphore.h>
#define NTH 6
static pthread_barrier_t bar;
static sem_t sem;
static int arrived;
static void *worker(void *arg) {
	pthread_barrier_wait(&bar);   /* 6 线程屏障同步 */
	arrived++;
	sem_post(&sem);               /* 信号量 */
	return 0;
}
static void *simple_worker(void *arg) {
	sem_post(&sem);               /* detach 线程: 不用 barrier */
	return 0;
}
int main(void) {
	pthread_t th[NTH];
	int i, v;
	pthread_barrier_init(&bar, 0, NTH);
	sem_init(&sem, 0, 0);
	for (i = 0; i < NTH; i++) pthread_create(&th[i], 0, worker, 0);
	for (i = 0; i < NTH; i++) sem_wait(&sem);   /* 等 6 个 post */
	for (i = 0; i < NTH; i++) pthread_join(th[i], 0);
	sem_getvalue(&sem, &v);
	/* detach: 分离线程无需 join */
	{
		pthread_t dt;
		pthread_create(&dt, 0, simple_worker, 0);
		sem_wait(&sem);
		pthread_detach(dt);
	}
	if (arrived != NTH) { printf("FAIL: arrived=%d\n", arrived); return 1; }
	if (v != 0) { printf("FAIL: sem=%d\n", v); return 1; }
	printf("sync OK: barrier+sem+detach\n");
	return 0;
}
