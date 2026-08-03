// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)

#include <bpf/libbpf.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "top_hash_residual.skel.h"

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
	struct top_hash_residual_bpf *skel;
	int err;

	libbpf_set_print(libbpf_print_fn);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	skel = top_hash_residual_bpf__open();
	if (!skel) {
		fprintf(stderr, "Failed to open BPF skeleton\n");
		return 1;
	}
	err = top_hash_residual_bpf__load(skel);
	if (err) {
		fprintf(stderr, "Failed to load BPF skeleton: %d\n", err);
		goto cleanup;
	}
	err = top_hash_residual_bpf__attach(skel);
	if (err) {
		fprintf(stderr, "Failed to attach BPF skeleton: %d\n", err);
		goto cleanup;
	}

	printf("Successfully started!\n");
	fflush(stdout);
	while (!exiting)
		sleep(1);

cleanup:
	top_hash_residual_bpf__destroy(skel);
	return err < 0 ? -err : err;
}
