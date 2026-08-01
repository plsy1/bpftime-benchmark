// SPDX-License-Identifier: MIT

#include "kernel_map_runtime.skel.h"

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

struct program_info {
	const char *operation;
	const char *implementation;
	const char *victim_operation;
	const char *victim_symbol;
	struct bpf_program *program;
	struct bpf_link *link;
};

struct counters {
	uint64_t run_cnt;
	uint64_t run_time_ns;
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
	const int fd = bpf_program__fd(program);
	if (bpf_obj_get_info_by_fd(fd, &info, &length) != 0) {
		fprintf(stderr, "bpf_obj_get_info_by_fd: %s\n", strerror(errno));
		exit(1);
	}
	return (struct counters){ .run_cnt = info.run_cnt,
				  .run_time_ns = info.run_time_ns };
}

static void prime_map(int fd)
{
	for (uint32_t key = 0; key < 1000; ++key) {
		const uint64_t value = key;
		if (bpf_map_update_elem(fd, &key, &value, BPF_ANY) != 0) {
			fprintf(stderr, "failed to prime map at key %u: %s\n", key,
				strerror(errno));
			exit(1);
		}
	}
}

static void run_victim(const char *victim, const char *operation,
		       uint64_t iterations, int cpu)
{
	char iterations_text[32];
	char cpu_text[32];
	snprintf(iterations_text, sizeof(iterations_text), "%" PRIu64,
		 iterations);
	snprintf(cpu_text, sizeof(cpu_text), "%d", cpu);

	pid_t pid = fork();
	if (pid < 0) {
		perror("fork");
		exit(1);
	}
	if (pid == 0) {
		execl(victim, victim, operation, iterations_text, cpu_text,
		      (char *)NULL);
		perror("execl");
		_exit(127);
	}

	int status = 0;
	if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != 0) {
		fprintf(stderr, "victim failed for %s (status=%d)\n", operation,
			status);
		exit(1);
	}
}

static double measure_program(struct program_info *entry, const char *victim,
			      uint64_t iterations, int cpu, int array_fd,
			      int hash_fd, unsigned round)
{
	prime_map(array_fd);
	prime_map(hash_fd);
	const struct counters before = read_counters(entry->program);
	run_victim(victim, entry->victim_operation, iterations, cpu);
	const struct counters after = read_counters(entry->program);
	const uint64_t count = after.run_cnt - before.run_cnt;
	const uint64_t runtime = after.run_time_ns - before.run_time_ns;
	if (count != iterations) {
		fprintf(stderr,
			"unexpected run count for %s/%s: expected=%" PRIu64
			" actual=%" PRIu64 "\n",
			entry->operation, entry->implementation, iterations, count);
		exit(1);
	}
	const double average = (double)runtime / (double)count;
	printf("%s,%s,%u,%" PRIu64 ",%" PRIu64 ",%.6f\n",
	       entry->operation, entry->implementation, round, count, runtime,
	       average);
	fflush(stdout);
	return average;
}

static void attach_program(struct program_info *entry, const char *victim)
{
	LIBBPF_OPTS(bpf_uprobe_opts, options,
		    .func_name = entry->victim_symbol);
	entry->link = bpf_program__attach_uprobe_opts(entry->program, -1, victim,
						      0, &options);
	long error = libbpf_get_error(entry->link);
	if (error) {
		entry->link = NULL;
		fprintf(stderr, "failed to attach %s to %s: %s\n",
			entry->operation, entry->victim_symbol,
			strerror((int)-error));
		exit(1);
	}
}

int main(int argc, char **argv)
{
	if (argc < 2 || argc > 6) {
		fprintf(stderr,
			"usage: %s <victim> [iterations] [rounds] [cpu] [warmup]\n",
			argv[0]);
		return 2;
	}
	const char *victim = argv[1];
	const uint64_t iterations = argc >= 3 ?
		parse_u64(argv[2], "iteration count", false) : 20000;
	const unsigned rounds = argc >= 4 ?
		(unsigned)parse_u64(argv[3], "round count", false) : 5;
	const int cpu = argc >= 5 ?
		(int)parse_u64(argv[4], "CPU", true) : 0;
	const uint64_t warmup = argc >= 6 ?
		parse_u64(argv[5], "warmup count", false) : 1000;

	libbpf_set_strict_mode(LIBBPF_STRICT_ALL);
	struct kernel_map_runtime_bpf *skel = kernel_map_runtime_bpf__open();
	if (!skel) {
		fprintf(stderr, "failed to open BPF skeleton\n");
		return 1;
	}
	if (kernel_map_runtime_bpf__load(skel) != 0) {
		fprintf(stderr, "failed to load BPF skeleton\n");
		kernel_map_runtime_bpf__destroy(skel);
		return 1;
	}

	struct program_info programs[] = {
		{ "array_lookup", "control", "array_lookup_control",
		  "__kmr_array_lookup_control",
		  skel->progs.kmr_array_lookup_control, NULL },
		{ "array_lookup", "real", "array_lookup_real",
		  "__kmr_array_lookup_real",
		  skel->progs.kmr_array_lookup_real, NULL },
		{ "array_update", "control", "array_update_control",
		  "__kmr_array_update_control",
		  skel->progs.kmr_array_update_control, NULL },
		{ "array_update", "real", "array_update_real",
		  "__kmr_array_update_real",
		  skel->progs.kmr_array_update_real, NULL },
		{ "hash_lookup", "control", "hash_lookup_control",
		  "__kmr_hash_lookup_control",
		  skel->progs.kmr_hash_lookup_control, NULL },
		{ "hash_lookup", "real", "hash_lookup_real",
		  "__kmr_hash_lookup_real",
		  skel->progs.kmr_hash_lookup_real, NULL },
		{ "hash_update", "control", "hash_update_control",
		  "__kmr_hash_update_control",
		  skel->progs.kmr_hash_update_control, NULL },
		{ "hash_update", "real", "hash_update_real",
		  "__kmr_hash_update_real",
		  skel->progs.kmr_hash_update_real, NULL },
	};
	const size_t program_count = sizeof(programs) / sizeof(programs[0]);
	for (size_t i = 0; i < program_count; ++i)
		attach_program(&programs[i], victim);

	const int array_fd = bpf_map__fd(skel->maps.kmr_array);
	const int hash_fd = bpf_map__fd(skel->maps.kmr_hash);
	for (size_t i = 0; i < program_count; ++i) {
		prime_map(array_fd);
		prime_map(hash_fd);
		run_victim(victim, programs[i].victim_operation, warmup, cpu);
	}

	printf("operation,implementation,round,run_cnt,runtime_ns,avg_ns_per_invocation\n");
	for (unsigned round = 1; round <= rounds; ++round) {
		for (size_t pair = 0; pair < program_count; pair += 2) {
			double control;
			double real;
			if (round % 2 == 1) {
				control = measure_program(&programs[pair], victim,
					iterations, cpu, array_fd, hash_fd, round);
				real = measure_program(&programs[pair + 1], victim,
					iterations, cpu, array_fd, hash_fd, round);
			} else {
				real = measure_program(&programs[pair + 1], victim,
					iterations, cpu, array_fd, hash_fd, round);
				control = measure_program(&programs[pair], victim,
					iterations, cpu, array_fd, hash_fd, round);
			}
			printf("%s,real-minus-control,%u,%" PRIu64 ",0,%.6f\n",
			       programs[pair].operation, round, iterations,
			       (real - control) / 1000.0);
		}
	}

	for (size_t i = 0; i < program_count; ++i)
		bpf_link__destroy(programs[i].link);
	kernel_map_runtime_bpf__destroy(skel);
	return 0;
}
