// SPDX-License-Identifier: MIT

#include "kernel_percpu_hash_delete_runtime.skel.h"

#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <errno.h>
#include <inttypes.h>
#include <linux/bpf.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

struct counters {
	uint64_t run_cnt;
	uint64_t run_time_ns;
};

struct program_info {
	const char *map_kind;
	const char *allocation;
	const char *implementation;
	const char *victim_symbol;
	struct bpf_program *program;
	struct bpf_link *link;
	int map_fd;
	bool percpu;
};

static uint64_t parse_u64(const char *text, const char *field, bool allow_zero)
{
	char *end = NULL;
	errno = 0;
	uint64_t value = strtoull(text, &end, 10);
	if (errno || !end || *end != '\0' || (!allow_zero && value == 0)) {
		fprintf(stderr, "invalid %s: %s\n", field, text);
		exit(2);
	}
	return value;
}

static struct counters read_counters(struct bpf_program *program)
{
	struct bpf_prog_info info = {};
	__u32 length = sizeof(info);
	if (bpf_obj_get_info_by_fd(bpf_program__fd(program), &info, &length) != 0) {
		fprintf(stderr, "bpf_obj_get_info_by_fd: %s\n", strerror(errno));
		exit(1);
	}
	return (struct counters){ .run_cnt = info.run_cnt,
				  .run_time_ns = info.run_time_ns };
}

static void prime_map(const struct program_info *entry)
{
	uint64_t values[64] = {};
	for (uint32_t key = 0; key < 1000; ++key) {
		uint64_t value = key;
		const void *value_ptr = entry->percpu ? (const void *)values :
						  (const void *)&value;
		if (entry->percpu)
			for (size_t cpu = 0; cpu < 64; ++cpu)
				values[cpu] = value;
		if (bpf_map_update_elem(entry->map_fd, &key, value_ptr, BPF_ANY) != 0) {
			fprintf(stderr, "failed to prime %s/%s key %u: %s\n",
				entry->map_kind, entry->allocation, key, strerror(errno));
			exit(1);
		}
	}
}

static void run_victim_once(const char *victim, const char *symbol, int cpu)
{
	char cpu_text[32];
	snprintf(cpu_text, sizeof(cpu_text), "%d", cpu);
	pid_t pid = fork();
	if (pid < 0) {
		perror("fork");
		exit(1);
	}
	if (pid == 0) {
		execl(victim, victim, symbol, "1", cpu_text, (char *)NULL);
		perror("execl");
		_exit(127);
	}
	int status = 0;
	if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != 0) {
		fprintf(stderr, "victim failed for %s (status=%d)\n", symbol, status);
		exit(1);
	}
}

static double measure(struct program_info *entry, const char *victim,
			      unsigned samples, int cpu, unsigned round)
{
	const struct counters before = read_counters(entry->program);
	for (unsigned sample = 0; sample < samples; ++sample) {
		prime_map(entry);
		run_victim_once(victim, entry->victim_symbol, cpu);
	}
	const struct counters after = read_counters(entry->program);
	const uint64_t count = after.run_cnt - before.run_cnt;
	const uint64_t runtime = after.run_time_ns - before.run_time_ns;
	if (count != samples) {
		fprintf(stderr, "unexpected run count for %s/%s: expected=%u actual=%" PRIu64
			"\n", entry->map_kind, entry->implementation, samples, count);
		exit(1);
	}
	const double average = (double)runtime / (double)count;
	printf("%s,%s,%s,%u,%u,%" PRIu64 ",%.9f\n", entry->map_kind,
	       entry->allocation, entry->implementation, round, samples, runtime,
	       average);
	fflush(stdout);
	return average;
}

static void attach(struct program_info *entry, const char *victim)
{
	LIBBPF_OPTS(bpf_uprobe_opts, options, .func_name = entry->victim_symbol);
	entry->link = bpf_program__attach_uprobe_opts(entry->program, -1, victim,
						     0, &options);
	long error = libbpf_get_error(entry->link);
	if (error) {
		entry->link = NULL;
		fprintf(stderr, "failed to attach %s: %s\n", entry->victim_symbol,
			strerror((int)-error));
		exit(1);
	}
}

int main(int argc, char **argv)
{
	if (argc < 2 || argc > 6) {
		fprintf(stderr, "usage: %s <victim> [samples] [rounds] [cpu]\n",
			argv[0]);
		return 2;
	}
	const char *victim = argv[1];
	const unsigned samples = argc >= 3 ?
		(unsigned)parse_u64(argv[2], "samples", false) : 20;
	const unsigned rounds = argc >= 4 ?
		(unsigned)parse_u64(argv[3], "rounds", false) : 5;
	const int cpu = argc >= 5 ? (int)parse_u64(argv[4], "CPU", true) : 0;

	libbpf_set_strict_mode(LIBBPF_STRICT_ALL);
	struct kernel_percpu_hash_delete_runtime_bpf *skel =
		kernel_percpu_hash_delete_runtime_bpf__open();
	if (!skel)
		return 1;
	if (kernel_percpu_hash_delete_runtime_bpf__load(skel) != 0) {
		kernel_percpu_hash_delete_runtime_bpf__destroy(skel);
		return 1;
	}

	struct program_info programs[] = {
		{ "hash", "prealloc", "control", "khd_hash_ctl",
			skel->progs.khd_hash_ctl, NULL,
			bpf_map__fd(skel->maps.khd_hash), false },
		{ "hash", "prealloc", "real", "khd_hash_real",
			skel->progs.khd_hash_real, NULL,
			bpf_map__fd(skel->maps.khd_hash), false },
		{ "percpu_hash", "prealloc", "control", "khd_percpu_hash_ctl",
			skel->progs.khd_percpu_hash_ctl, NULL,
			bpf_map__fd(skel->maps.khd_percpu_hash), true },
		{ "percpu_hash", "prealloc", "real", "khd_percpu_hash_real",
			skel->progs.khd_percpu_hash_real, NULL,
			bpf_map__fd(skel->maps.khd_percpu_hash), true },
		{ "hash", "no_prealloc", "control", "khd_hash_noprealloc_ctl",
			skel->progs.khd_hash_noprealloc_ctl, NULL,
			bpf_map__fd(skel->maps.khd_hash_noprealloc), false },
		{ "hash", "no_prealloc", "real", "khd_hash_noprealloc_real",
			skel->progs.khd_hash_noprealloc_real, NULL,
			bpf_map__fd(skel->maps.khd_hash_noprealloc), false },
		{ "percpu_hash", "no_prealloc", "control",
			"khd_percpu_hash_noprealloc_ctl",
			skel->progs.khd_percpu_hash_noprealloc_ctl, NULL,
			bpf_map__fd(skel->maps.khd_percpu_hash_noprealloc), true },
		{ "percpu_hash", "no_prealloc", "real",
			"khd_percpu_hash_noprealloc_real",
			skel->progs.khd_percpu_hash_noprealloc_real, NULL,
			bpf_map__fd(skel->maps.khd_percpu_hash_noprealloc), true },
	};
	const size_t count = sizeof(programs) / sizeof(programs[0]);
	for (size_t i = 0; i < count; ++i)
		attach(&programs[i], victim);

	printf("map_kind,allocation,implementation,round,samples,runtime_ns,"
	       "avg_ns_per_invocation\n");
	for (unsigned round = 1; round <= rounds; ++round) {
		for (size_t i = 0; i < count; i += 2) {
			double control, real;
			if (round & 1) {
				control = measure(&programs[i], victim, samples, cpu, round);
				real = measure(&programs[i + 1], victim, samples, cpu, round);
			} else {
				real = measure(&programs[i + 1], victim, samples, cpu, round);
				control = measure(&programs[i], victim, samples, cpu, round);
			}
			printf("%s,%s,real-minus-control,%u,%u,0,%.9f\n",
			       programs[i].map_kind, programs[i].allocation, round,
			       samples, (real - control) / 1000.0);
		}
	}
	for (size_t i = 0; i < count; ++i)
		bpf_link__destroy(programs[i].link);
	kernel_percpu_hash_delete_runtime_bpf__destroy(skel);
	return 0;
}
