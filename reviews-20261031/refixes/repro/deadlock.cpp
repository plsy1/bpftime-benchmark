// lazy mode: thread A dlopen/dlclose (rescan: rescan_mutex -> patch_owner),
// thread B fork (atfork prepare: patch_owner, then rescan_mutex).
#include <dlfcn.h>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <atomic>
static std::atomic<int> forks{0}, opens{0};
static const char *lib;
void *opener(void *) {
	for (;;) {
		void *h = dlopen(lib, RTLD_NOW);
		if (h) dlclose(h);
		opens++;
	}
}
void *forker(void *) {
	for (;;) {
		pid_t p = fork();
		if (p == 0) _exit(0);
		if (p > 0) { int st; waitpid(p, &st, 0); }
		forks++;
	}
}
int main(int argc, char **argv) {
	lib = argv[1];
	pthread_t a, b;
	pthread_create(&a, 0, opener, 0);
	pthread_create(&b, 0, forker, 0);
	for (int i = 0; i < 20; i++) {
		sleep(1);
		int f = forks, o = opens;
		printf("t=%d forks=%d opens=%d\n", i+1, f, o);
		fflush(stdout);
		static int lf, lo;
		if (i > 1 && f == lf && o == lo) { printf("STALLED\n"); fflush(stdout); abort(); }
		lf = f; lo = o;
	}
	_exit(0);
}
