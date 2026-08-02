// SPDX-License-Identifier: MIT

#include "kernel_array_update_sizes.skel.h"

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
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

struct program_info {
	unsigned value_size;
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
	if (bpf_obj_get_info_by_fd(bpf_program__fd(program), &info, &length)) {
		fprintf(stderr, "bpf_obj_get_info_by_fd: %s\n", strerror(errno));
		exit(1);
	}
	return (struct counters){ info.run_cnt, info.run_time_ns };
}

static void run_victim(const char *victim, const char *operation,
		       uint64_t iterations, int cpu)
{
	char iterations_text[32], cpu_text[32];
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

static double measure(struct program_info *entry, const char *victim,
		      uint64_t iterations, int cpu, unsigned round)
{
	const struct counters before = read_counters(entry->program);
	run_victim(victim, entry->victim_operation, iterations, cpu);
	const struct counters after = read_counters(entry->program);
	const uint64_t count = after.run_cnt - before.run_cnt;
	const uint64_t runtime = after.run_time_ns - before.run_time_ns;
	if (count != iterations) {
		fprintf(stderr,
			"unexpected run count for %u/%s: expected=%" PRIu64
			" actual=%" PRIu64 "\n", entry->value_size,
			entry->implementation, iterations, count);
		exit(1);
	}
	const double average = (double)runtime / (double)count;
	printf("%u,%s,%u,%" PRIu64 ",%" PRIu64 ",%.6f\n",
	       entry->value_size, entry->implementation, round, count, runtime,
	       average);
	fflush(stdout);
	return average;
}

static void attach(struct program_info *entry, const char *victim)
{
	LIBBPF_OPTS(bpf_uprobe_opts, opts, .func_name = entry->victim_symbol);
	entry->link = bpf_program__attach_uprobe_opts(entry->program, -1, victim,
						      0, &opts);
	long error = libbpf_get_error(entry->link);
	if (error) {
		entry->link = NULL;
		fprintf(stderr, "failed to attach %u/%s: %s\n",
			entry->value_size, entry->implementation,
			strerror((int)-error));
		exit(1);
	}
}

static void wait_for_optional_start_gate(void)
{
	const char *path = getenv("KAUS_START_GATE");
	if (!path || !*path)
		return;
	for (unsigned i = 0; i < 6000; ++i) {
		struct stat status;
		if (stat(path, &status) == 0)
			return;
		if (errno != ENOENT) {
			fprintf(stderr, "stat(%s): %s\n", path, strerror(errno));
			exit(1);
		}
		usleep(10000);
	}
	fprintf(stderr, "timed out waiting for KAUS_START_GATE=%s\n", path);
	exit(1);
}

#define PROGRAM(bits, impl)                                                   \
	{ bits, #impl, #bits "_" #impl, "__kaus_" #bits "_" #impl,          \
	  skel->progs.kaus_##bits##_##impl, NULL }

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
	struct kernel_array_update_sizes_bpf *skel =
		kernel_array_update_sizes_bpf__open_and_load();
	if (!skel) {
		fprintf(stderr, "failed to open/load BPF skeleton\n");
		return 1;
	}
	struct program_info programs[] = {
		PROGRAM(8, ctrl),   PROGRAM(8, real),
		PROGRAM(16, ctrl),  PROGRAM(16, real),
		PROGRAM(32, ctrl),  PROGRAM(32, real),
		PROGRAM(64, ctrl),  PROGRAM(64, real),
		PROGRAM(128, ctrl), PROGRAM(128, real),
		PROGRAM(256, ctrl), PROGRAM(256, real),
	};
	const size_t count = sizeof(programs) / sizeof(programs[0]);
	size_t first = 0, last = count;
	const char *only_size_text = getenv("KAUS_ONLY_SIZE");
	if (only_size_text && *only_size_text) {
		const unsigned only_size =
			(unsigned)parse_u64(only_size_text, "KAUS_ONLY_SIZE", false);
		bool found = false;
		for (size_t pair = 0; pair < count; pair += 2) {
			if (programs[pair].value_size == only_size) {
				first = pair;
				last = pair + 2;
				found = true;
				break;
			}
		}
		if (!found) {
			fprintf(stderr, "unsupported KAUS_ONLY_SIZE=%u\n", only_size);
			kernel_array_update_sizes_bpf__destroy(skel);
			return 2;
		}
	}
	for (size_t i = 0; i < count; ++i)
		attach(&programs[i], victim);
	wait_for_optional_start_gate();
	for (size_t i = first; i < last; ++i)
		run_victim(victim, programs[i].victim_operation, warmup, cpu);

	printf("value_size,implementation,round,run_cnt,runtime_ns,avg_ns_per_invocation\n");
	for (unsigned round = 1; round <= rounds; ++round) {
		for (size_t pair = first; pair < last; pair += 2) {
			double control, real;
			if (round % 2) {
				control = measure(&programs[pair], victim,
					iterations, cpu, round);
				real = measure(&programs[pair + 1], victim,
					iterations, cpu, round);
			} else {
				real = measure(&programs[pair + 1], victim,
					iterations, cpu, round);
				control = measure(&programs[pair], victim,
					iterations, cpu, round);
			}
			printf("%u,real-minus-control,%u,%" PRIu64
			       ",0,%.6f\n", programs[pair].value_size, round,
			       iterations, (real - control) / 1000.0);
		}
	}
	for (size_t i = 0; i < count; ++i)
		bpf_link__destroy(programs[i].link);
	kernel_array_update_sizes_bpf__destroy(skel);
	return 0;
}
