// Spike 2: can the vDSO text be made writable and patched (COW), and does a
// fork child keep working.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
int main(void)
{
	uint8_t *vdso = (uint8_t *)getauxval(AT_SYSINFO_EHDR);
	// Find the first svc #0 in the first page (any of them; we write it back unchanged first).
	uint32_t *w = (uint32_t *)vdso, *svc = NULL;
	for (int i = 0; i < 1024; i++)
		if (w[i] == 0xd4000001) { svc = &w[i]; break; }
	printf("vdso %p, first svc at +%#lx\n", vdso, (long)((uint8_t *)svc - vdso));
	int r = mprotect(vdso, 4096, PROT_READ | PROT_WRITE | PROT_EXEC);
	printf("mprotect RWX: %d\n", r);
	if (r) { perror("mprotect"); return 1; }
	__atomic_store_n(svc, 0xd4000001, __ATOMIC_RELEASE); // same word: forces the COW copy
	__builtin___clear_cache((char *)svc, (char *)(svc + 1));
	r = mprotect(vdso, 4096, PROT_READ | PROT_EXEC);
	printf("mprotect RX back: %d\n", r);
	struct timespec ts;
	printf("clock_gettime MONOTONIC %d, CPUTIME %d\n", clock_gettime(CLOCK_MONOTONIC, &ts), clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts));
	pid_t p = fork();
	if (p == 0) _exit(clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) == 0 && clock_gettime(CLOCK_MONOTONIC, &ts) == 0 ? 0 : 3);
	int st; waitpid(p, &st, 0);
	printf("child exit %d\n", WEXITSTATUS(st));
	FILE *f = fopen("/proc/self/maps", "r"); char line[256];
	while (fgets(line, sizeof line, f)) if (strstr(line, "vdso")) fputs(line, stdout);
	return 0;
}
