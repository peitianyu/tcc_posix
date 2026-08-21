#include <pthread.h>
#include <stdio.h>
static pthread_mutex_t mu = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
static int count;
static void *worker(void *arg) {
	/* 同一线程递归锁 3 次 (RECURSIVE 类型应正常) */
	pthread_mutex_lock(&mu);
	pthread_mutex_lock(&mu);
	pthread_mutex_lock(&mu);
	count++;
	pthread_mutex_unlock(&mu);
	pthread_mutex_unlock(&mu);
	pthread_mutex_unlock(&mu);
	return 0;
}
int main(void) {
	pthread_t th[2];
	int i;
	for (i = 0; i < 2; i++) pthread_create(&th[i], 0, worker, 0);
	for (i = 0; i < 2; i++) pthread_join(th[i], 0);
	printf("count=%d expect=2\n", count);
	return count == 2 ? 0 : 1;
}
