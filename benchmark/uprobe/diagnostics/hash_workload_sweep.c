// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
#include <bpf/libbpf.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include "hash_workload_sweep.skel.h"

static volatile sig_atomic_t exiting;

static void sig_handler(int sig)
{
	(void)sig;
	exiting = 1;
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *format,
			   va_list args)
{
	(void)level;
	return vfprintf(stderr, format, args);
}

int main(void)
{
	struct hash_workload_sweep_bpf *skel;
	int err;

	libbpf_set_print(libbpf_print_fn);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	skel = hash_workload_sweep_bpf__open();
	if (!skel)
		return 1;
	err = hash_workload_sweep_bpf__load(skel);
	if (err)
		goto cleanup;
	err = hash_workload_sweep_bpf__attach(skel);
	if (err)
		goto cleanup;
	printf("Successfully started!\n");
	fflush(stdout);
	while (!exiting)
		sleep(1);

cleanup:
	hash_workload_sweep_bpf__destroy(skel);
	return err < 0 ? -err : err;
}
