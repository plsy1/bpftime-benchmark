// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
/* Copyright (c) 2020 Facebook */
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <unistd.h>
#include <stdlib.h>
#include "./.output/threadhist.skel.h"
#include <inttypes.h>
#define warn(...) fprintf(stderr, __VA_ARGS__)

static int libbpf_print_fn(enum libbpf_print_level level, const char *format,
			   va_list args)
{
	return vfprintf(stderr, format, args);
}

static volatile bool exiting = false;

static void sig_handler(int sig)
{
	exiting = true;
}

/* th-hostcall: print the HASH counter (key 0) once a second. */
static int print_stat(struct threadhist_bpf *obj, uint64_t unused)
{
	time_t t;
	struct tm *tm;
	char ts[16];
	uint32_t key = 0;
	uint64_t value = 0;
	int fd = bpf_map__fd(obj->maps.call_count);
	(void)unused;
	time(&t);
	tm = localtime(&t);
	strftime(ts, sizeof(ts), "%H:%M:%S", tm);
	int err = bpf_map_lookup_elem(fd, &key, &value);
	printf("%-9s lookup=%d count=%" PRIu64 "\n", ts, err, value);
	fflush(stdout);
	return 0;
}

int main(int argc, char **argv)
{
	struct threadhist_bpf *skel;
	int err;

	/* Set up libbpf errors and debug info callback */
	libbpf_set_print(libbpf_print_fn);

	/* Cleaner handling of Ctrl-C */
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	/* Load and verify BPF application */
	skel = threadhist_bpf__open();
	if (!skel) {
		fprintf(stderr, "Failed to open and load BPF skeleton\n");
		return 1;
	}

	/* Load & verify BPF programs */
	err = threadhist_bpf__load(skel);
	if (err) {
		fprintf(stderr, "Failed to load and verify BPF skeleton\n");
		goto cleanup;
	}
	{
		uint32_t k = 0;
		uint64_t zero = 0;
		err = bpf_map_update_elem(bpf_map__fd(skel->maps.call_count), &k,
					  &zero, BPF_ANY);
		if (err) {
			fprintf(stderr, "Failed to insert key 0\n");
			goto cleanup;
		}
	}
	err = threadhist_bpf__attach(skel);
	if (err) {
		fprintf(stderr, "Failed to attach BPF skeleton\n");
		goto cleanup;
	}
	while (!exiting) {
		sleep(1);
		print_stat(skel, 0);
	}
cleanup:
	/* Clean up */
	threadhist_bpf__destroy(skel);

	return err < 0 ? -err : 0;
}
