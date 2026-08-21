#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static int ready;
static void *producer(void *arg) {
	usleep(50000);
	pthread_mutex_lock(&mu);
	ready = 1;
	pthread_cond_signal(&cv);
	pthread_mutex_unlock(&mu);
	return 0;
}
static void *consumer(void *arg) {
	struct timespec ts;
	pthread_mutex_lock(&mu);
	while (!ready) {
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += 5; /* 5s 超时, producer 50ms 内 signal */
		if (pthread_cond_timedwait(&cv, &mu, &ts) != 0) {
			printf("consumer: timed out!\n");
			pthread_mutex_unlock(&mu);
			return (void *)1;
		}
	}
	pthread_mutex_unlock(&mu);
	printf("consumer: got signal, ready=%d\n", ready);
	return 0;
}
int main(void) {
	pthread_t p, c;
	void *rc;
	pthread_create(&c, 0, consumer, 0);
	pthread_create(&p, 0, producer, 0);
	pthread_join(p, 0);
	pthread_join(c, &rc);
	printf("main: consumer rc=%ld\n", (long)rc);
	return rc != 0;
}
