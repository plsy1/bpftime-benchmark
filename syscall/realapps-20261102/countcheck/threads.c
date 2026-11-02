// T threads x N getpid through libc's syscall(): the counts syscount must see.
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
static long n;
static void *run(void *p) { for (long i = 0; i < n; i++) syscall(SYS_getpid); return p; }
int main(int argc, char **argv) {
	int t = atoi(argv[1]); n = atol(argv[2]);
	pthread_t th[64];
	for (int i = 0; i < t; i++) pthread_create(&th[i], 0, run, 0);
	for (int i = 0; i < t; i++) pthread_join(th[i], 0);
	printf("getpid %ld\n", t * n);
	return 0;
}
