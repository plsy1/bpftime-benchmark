// SPDX-License-Identifier: MIT

#include "kernel_percpu_map_runtime.skel.h"

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
	const char *operation;
	const char *map_kind;
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
			fprintf(stderr, "failed to prime %s key %u: %s\n",
				entry->map_kind, key, strerror(errno));
			exit(1);
		}
	}
}

static void run_victim(const char *victim, const char *symbol,
			       uint64_t iterations, int cpu)
{
	char iterations_text[32];
	char cpu_text[32];
	snprintf(iterations_text, sizeof(iterations_text), "%" PRIu64, iterations);
	snprintf(cpu_text, sizeof(cpu_text), "%d", cpu);

	pid_t pid = fork();
	if (pid < 0) {
		perror("fork");
		exit(1);
	}
	if (pid == 0) {
		execl(victim, victim, symbol, iterations_text, cpu_text,
		      (char *)NULL);
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
			      uint64_t iterations, int cpu, unsigned round)
{
	prime_map(entry);
	struct counters before = read_counters(entry->program);
	run_victim(victim, entry->victim_symbol, iterations, cpu);
	struct counters after = read_counters(entry->program);
	uint64_t count = after.run_cnt - before.run_cnt;
	uint64_t runtime = after.run_time_ns - before.run_time_ns;
	if (count != iterations) {
		fprintf(stderr, "unexpected run count for %s/%s: expected=%" PRIu64
			" actual=%" PRIu64 "\n", entry->map_kind,
			entry->implementation, iterations, count);
		exit(1);
	}
	double average = (double)runtime / (double)count;
	printf("%s,%s,%s,%u,%" PRIu64 ",%" PRIu64 ",%.9f\n",
	       entry->map_kind, entry->operation, entry->implementation, round,
	       count, runtime, average);
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
		fprintf(stderr, "usage: %s <victim> [iterations] [rounds] [cpu] [warmup]\n",
			argv[0]);
		return 2;
	}
	const char *victim = argv[1];
	uint64_t iterations = argc >= 3 ? parse_u64(argv[2], "iterations", false) : 20000;
	unsigned rounds = argc >= 4 ? (unsigned)parse_u64(argv[3], "rounds", false) : 5;
	int cpu = argc >= 5 ? (int)parse_u64(argv[4], "CPU", true) : 0;
	uint64_t warmup = argc >= 6 ? parse_u64(argv[5], "warmup", false) : 1000;

	libbpf_set_strict_mode(LIBBPF_STRICT_ALL);
	struct kernel_percpu_map_runtime_bpf *skel =
		kernel_percpu_map_runtime_bpf__open();
	if (!skel)
		return 1;
	if (kernel_percpu_map_runtime_bpf__load(skel) != 0) {
		kernel_percpu_map_runtime_bpf__destroy(skel);
		return 1;
	}

	struct program_info programs[] = {
		{ "array_lookup", "array", "control", "kpa_l_ctl",
			skel->progs.kpa_l_ctl, NULL, bpf_map__fd(skel->maps.kpmr_array), false },
		{ "array_lookup", "array", "real", "kpa_l_real",
			skel->progs.kpa_l_real, NULL, bpf_map__fd(skel->maps.kpmr_array), false },
		{ "array_update", "array", "control", "kpa_u_ctl",
			skel->progs.kpa_u_ctl, NULL, bpf_map__fd(skel->maps.kpmr_array), false },
		{ "array_update", "array", "real", "kpa_u_real",
			skel->progs.kpa_u_real, NULL, bpf_map__fd(skel->maps.kpmr_array), false },
		{ "array_lookup", "percpu_array", "control", "kpp_l_ctl",
			skel->progs.kpp_l_ctl, NULL, bpf_map__fd(skel->maps.kpmr_percpu_array), true },
		{ "array_lookup", "percpu_array", "real", "kpp_l_real",
			skel->progs.kpp_l_real, NULL, bpf_map__fd(skel->maps.kpmr_percpu_array), true },
		{ "array_update", "percpu_array", "control", "kpp_u_ctl",
			skel->progs.kpp_u_ctl, NULL, bpf_map__fd(skel->maps.kpmr_percpu_array), true },
		{ "array_update", "percpu_array", "real", "kpp_u_real",
			skel->progs.kpp_u_real, NULL, bpf_map__fd(skel->maps.kpmr_percpu_array), true },
		{ "hash_lookup", "hash", "control", "kha_l_ctl",
			skel->progs.kha_l_ctl, NULL, bpf_map__fd(skel->maps.kpmr_hash), false },
		{ "hash_lookup", "hash", "real", "kha_l_real",
			skel->progs.kha_l_real, NULL, bpf_map__fd(skel->maps.kpmr_hash), false },
		{ "hash_update", "hash", "control", "kha_u_ctl",
			skel->progs.kha_u_ctl, NULL, bpf_map__fd(skel->maps.kpmr_hash), false },
		{ "hash_update", "hash", "real", "kha_u_real",
			skel->progs.kha_u_real, NULL, bpf_map__fd(skel->maps.kpmr_hash), false },
		{ "hash_lookup", "percpu_hash", "control", "khp_l_ctl",
			skel->progs.khp_l_ctl, NULL, bpf_map__fd(skel->maps.kpmr_percpu_hash), true },
		{ "hash_lookup", "percpu_hash", "real", "khp_l_real",
			skel->progs.khp_l_real, NULL, bpf_map__fd(skel->maps.kpmr_percpu_hash), true },
		{ "hash_update", "percpu_hash", "control", "khp_u_ctl",
			skel->progs.khp_u_ctl, NULL, bpf_map__fd(skel->maps.kpmr_percpu_hash), true },
		{ "hash_update", "percpu_hash", "real", "khp_u_real",
			skel->progs.khp_u_real, NULL, bpf_map__fd(skel->maps.kpmr_percpu_hash), true },
	};
	const size_t count = sizeof(programs) / sizeof(programs[0]);
	for (size_t i = 0; i < count; ++i)
		attach(&programs[i], victim);
	for (size_t i = 0; i < count; ++i) {
		prime_map(&programs[i]);
		run_victim(victim, programs[i].victim_symbol, warmup, cpu);
	}

	printf("map_kind,operation,implementation,round,run_cnt,runtime_ns,avg_ns_per_invocation\n");
	for (unsigned round = 1; round <= rounds; ++round) {
		for (size_t i = 0; i < count; i += 2) {
			double control, real;
			if (round & 1) {
				control = measure(&programs[i], victim, iterations, cpu, round);
				real = measure(&programs[i + 1], victim, iterations, cpu, round);
			} else {
				real = measure(&programs[i + 1], victim, iterations, cpu, round);
				control = measure(&programs[i], victim, iterations, cpu, round);
			}
			printf("%s,%s,real-minus-control,%u,%" PRIu64 ",0,%.9f\n",
			       programs[i].map_kind, programs[i].operation, round,
			       iterations, (real - control) / 1000.0);
		}
	}
	for (size_t i = 0; i < count; ++i)
		bpf_link__destroy(programs[i].link);
	kernel_percpu_map_runtime_bpf__destroy(skel);
	return 0;
}
