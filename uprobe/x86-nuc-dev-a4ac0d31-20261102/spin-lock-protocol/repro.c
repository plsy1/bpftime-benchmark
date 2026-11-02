/* x86 NUC, 11/03: dev 0ffe368b's inline spin lock (runtime/src/bpf_map/map_common_def.hpp)
 * assumes glibc's pthread_spinlock_t protocol is "0 free, exchange 1 to take, store 0 to
 * give back". Shows what glibc on this machine actually stores, and whether the inline
 * lock can take a lock that pthread_spin_init made. Build: gcc -O2 -pthread repro.c */
#include <pthread.h>
#include <stdio.h>

static int inline_trylock(volatile pthread_spinlock_t *lock) /* one try of bpftime_spin_lock */
{
	return __atomic_exchange_n((int *)lock, 1, __ATOMIC_ACQUIRE) == 0;
}

int main(void)
{
	pthread_spinlock_t l;
	pthread_spin_init(&l, PTHREAD_PROCESS_SHARED);
	printf("after pthread_spin_init:   word = %d\n", *(volatile int *)&l);
	pthread_spin_lock(&l);
	printf("after pthread_spin_lock:   word = %d\n", *(volatile int *)&l);
	pthread_spin_unlock(&l);
	printf("after pthread_spin_unlock: word = %d\n", *(volatile int *)&l);
	printf("inline lock takes the free lock made by pthread_spin_init: %s\n",
	       inline_trylock(&l) ? "yes" : "NO (bpftime_spin_lock would spin forever)");
	pthread_spin_init(&l, PTHREAD_PROCESS_SHARED);
	*(volatile int *)&l = 0; /* what bpftime_spin_unlock stores */
	printf("pthread_spin_trylock after an inline unlock (word 0): %s\n",
	       pthread_spin_trylock(&l) == 0 ? "taken" : "EBUSY (pthread sees it held)");
	return 0;
}
