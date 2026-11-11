// Loader for nohost.bpf.c: attaches, prints "BPF probes attached", and on
// SIGINT prints the recorded host-call latencies (ns): count, then all values.
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "nohost.skel.h"

static volatile sig_atomic_t exiting;
static void sig_handler(int sig)
{
	(void)sig;
	exiting = 1;
}
static int libbpf_print_fn(enum libbpf_print_level level, const char *format,
			   va_list args)
{
	if (level == LIBBPF_DEBUG)
		return 0;
	return vfprintf(stderr, format, args);
}

int main(void)
{
	struct nohost_bpf *skel;
	int err;
	libbpf_set_print(libbpf_print_fn);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	skel = nohost_bpf__open();
	if (!skel) {
		fprintf(stderr, "Failed to open and load BPF skeleton\n");
		return 1;
	}
	err = nohost_bpf__load(skel);
	if (err) {
		fprintf(stderr, "Failed to load and verify BPF skeleton\n");
		goto cleanup;
	}
	err = nohost_bpf__attach(skel);
	if (err) {
		fprintf(stderr, "Failed to attach BPF skeleton\n");
		goto cleanup;
	}
	fprintf(stderr, "BPF probes attached. Waiting for target program...\n");
	while (!exiting)
		usleep(100000);
	{
		int fd = bpf_map__fd(skel->maps.lat);
		unsigned int key = 0;
		unsigned long long n = 0, v;
		if (bpf_map_lookup_elem(fd, &key, &n) != 0) {
			fprintf(stderr, "Failed to read lat[0]\n");
			err = 1;
			goto cleanup;
		}
		printf("nohost count=%llu\n", n);
		printf("nohost ns=");
		for (key = 1; key <= n && key <= 4096; key++) {
			v = 0;
			bpf_map_lookup_elem(fd, &key, &v);
			printf("%s%llu", key > 1 ? "," : "", v);
		}
		printf("\n");
		fflush(stdout);
	}
cleanup:
	nohost_bpf__destroy(skel);
	return err < 0 ? -err : err;
}
