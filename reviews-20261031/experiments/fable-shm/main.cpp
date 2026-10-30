#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
int main(int argc, char **argv) {
	bool newns = argc > 1 && argv[1][0] == 'n';
	void *h = newns ? dlmopen(LM_ID_NEWLM, "./liblib.so", RTLD_NOW | RTLD_LOCAL)
			: dlopen("./liblib.so", RTLD_NOW | RTLD_LOCAL);
	if (!h) { fprintf(stderr, "load failed: %s\n", dlerror()); return 2; }
	auto status = (int (*)())dlsym(h, "lib_atfork_status");
	auto identity = (int (*)())dlsym(h, "lib_identity");
	printf("%s: atfork_status=%d parent identity=%d (getpid %d)\n",
	       newns ? "dlmopen NEWLM" : "dlopen", status(), identity(), getpid());
	fflush(stdout);
	pid_t c = fork();
	if (c == 0) {
		int id = identity();
		printf("  child: identity=%d getpid=%d -> %s\n", id, getpid(),
		       id == getpid() ? "handler ran (cache cleared)" : "STALE parent identity, handler did NOT run");
		fflush(stdout); _exit(0);
	}
	int st; waitpid(c, &st, 0);
	return 0;
}
