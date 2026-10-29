// pthread keys of the traced program while the dlmopen-ed agent (its own libc)
// runs: a new key must read NULL first (OpenSSL takes a non-NULL value as its
// error state), and stored values must survive system calls that run
// programs. Prints the key numbers.
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
	enum { N = 8 };
	pthread_key_t keys[N];
	int bad = 0;
	for (int i = 0; i < N; i++) {
		pthread_key_create(&keys[i], NULL);
		void *first = pthread_getspecific(keys[i]);
		if (first) {
			printf("new key %u reads %p, not NULL\n", keys[i], first);
			bad++;
		}
		pthread_setspecific(keys[i], (void *)(0x1000L + i));
	}
	for (int r = 0; r < 1000; r++)
		getppid();
	for (int i = 0; i < N; i++) {
		void *v = pthread_getspecific(keys[i]);
		if (v != (void *)(0x1000L + i)) {
			printf("key %u: expected %#lx, got %p\n", keys[i], 0x1000L + i, v);
			bad++;
		}
	}
	printf("keys %u..%u, %d corrupted\n", keys[0], keys[N - 1], bad);
	return bad != 0;
}
