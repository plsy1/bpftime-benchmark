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

/* threadhist-llama: summarise every per-thread slot instead of printing 7.
 * THREADHIST_N must equal the loader's BPFTIME_MAP_GPU_THREAD_COUNT (default 1048576). */
static uint64_t slots_n(void)
{
	const char *e = getenv("THREADHIST_N");
	return e ? strtoull(e, NULL, 10) : (1024 * 1024);
}

static int print_stat(struct threadhist_bpf *obj, uint64_t thread_count)
{
	time_t t;
	struct tm *tm;
	char ts[16];
	uint32_t key = 0;
	int fd = bpf_map__fd(obj->maps.call_count);
	static uint64_t *value;
	if (!value)
		value = calloc(thread_count, sizeof(uint64_t));
	if (!value)
		return -1;

	time(&t);
	tm = localtime(&t);
	strftime(ts, sizeof(ts), "%H:%M:%S", tm);

	int err = bpf_map_lookup_elem(fd, &key, value);
	uint64_t nonzero = 0, sum = 0, max = 0, last_nonzero = 0;
	for (uint64_t i = 0; i < thread_count; i++) {
		if (value[i]) {
			nonzero++;
			last_nonzero = i;
		}
		sum += value[i];
		if (value[i] > max)
			max = value[i];
	}
	printf("%-9s lookup=%d slots=%" PRIu64 " nonzero=%" PRIu64
	       " last_nonzero=%" PRIu64 " sum=%" PRIu64 " max=%" PRIu64
	       " slot0=%" PRIu64 "\n",
	       ts, err, thread_count, nonzero, last_nonzero, sum, max,
	       value[0]);
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
	err = threadhist_bpf__attach(skel);
	if (err) {
		fprintf(stderr, "Failed to attach BPF skeleton\n");
		goto cleanup;
	}
	while (!exiting) {
		sleep(1);
		print_stat(skel, slots_n());
	}
cleanup:
	/* Clean up */
	threadhist_bpf__destroy(skel);

	return err < 0 ? -err : 0;
}
