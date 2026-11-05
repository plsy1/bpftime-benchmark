// Each thread's first traced call maps a 192 KiB hook stack (two VMAs) that
// is never unmapped. Create and join threads until pthread_create fails or
// the limit is reached, reporting the mapping count and VmSize.
#include "harness.hpp"
#include <cstdio>
#include <cstdlib>
#include <pthread.h>

static void *worker(void *)
{
	getppid(); // one hooked call: maps this thread's hook stack
	return nullptr;
}

static size_t rss_kb;
static int count_maps(size_t *vm_kb)
{
	int n = 0;
	FILE *f = fopen("/proc/self/maps", "r");
	char line[512];
	while (f && fgets(line, sizeof(line), f))
		n++;
	if (f)
		fclose(f);
	*vm_kb = 0;
	f = fopen("/proc/self/status", "r");
	while (f && fgets(line, sizeof(line), f))
		if (sscanf(line, "VmSize: %zu kB", vm_kb) == 1)
			continue;
		else if (sscanf(line, "VmRSS: %zu kB", &rss_kb) == 1)
			break;
	if (f)
		fclose(f);
	return n;
}

int main(int argc, char **argv)
{
	const int limit = argc > 1 ? atoi(argv[1]) : 40000;
	const bool hook = argc > 2 ? atoi(argv[2]) != 0 : true;
	if (hook)
		install_hook();
	size_t vm;
	printf("start: %d mappings, VmSize %zu kB, VmRSS %zu kB\n", count_maps(&vm), vm, rss_kb);
	for (int i = 1; i <= limit; i++) {
		pthread_t t;
		int rc = pthread_create(&t, nullptr, worker, nullptr);
		if (rc != 0) {
			printf("pthread_create failed after %d threads: errno %d; %d mappings, VmSize %zu kB\n",
			       i - 1, rc, count_maps(&vm), vm);
			return 1;
		}
		pthread_join(t, nullptr);
		if (i % 8192 == 0)
			printf("after %d threads: %d mappings, VmSize %zu kB, VmRSS %zu kB\n",
			       i, count_maps(&vm), vm, rss_kb);
	}
	printf("done: %d threads, %d mappings, VmSize %zu kB, VmRSS %zu kB\n", limit,
	       count_maps(&vm), vm, rss_kb);
	return 0;
}
