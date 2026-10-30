#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
int main(int argc, char **argv) {
	int newns = argc > 1;
	void *h = newns ? dlmopen(LM_ID_NEWLM, "./lib.so", RTLD_NOW) : dlopen("./lib.so", RTLD_NOW);
	int (*f)(void) = (int (*)(void))dlsym(h, "lib_pid");
	printf("%s parent getpid=%d lib=%d\n", newns ? "dlmopen" : "dlopen", getpid(), f());
	pid_t c = fork();
	if (c == 0) { printf("  child getpid=%d lib=%d %s\n", getpid(), f(), f() == getpid() ? "ok" : "STALE"); fflush(stdout); _exit(0); }
	waitpid(c, 0, 0);
	return 0;
}
