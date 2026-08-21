#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#define NTH 8
#define NITER 200000
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static long counter;
static void *worker(void *arg) {
	long i;
	for (i = 0; i < NITER; i++) {
		pthread_mutex_lock(&mu);
		counter++;
		pthread_mutex_unlock(&mu);
	}
	return 0;
}
int main(void) {
	pthread_t th[NTH];
	int i;
	for (i = 0; i < NTH; i++) pthread_create(&th[i], 0, worker, 0);
	for (i = 0; i < NTH; i++) pthread_join(th[i], 0);
	printf("counter=%ld expect=%ld\n", counter, (long)NTH*NITER);
	return counter == (long)NTH*NITER ? 0 : 1;
}
