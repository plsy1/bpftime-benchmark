// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include <errno.h>
#include <inttypes.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

#define DEFINE_TARGET(name)                                                    \
	NOINLINE uint64_t name(char *buffer, int index, uint64_t value)          \
	{                                                                          \
		asm volatile("" ::: "memory");                                     \
		return (uint64_t)buffer[index] + value;                              \
	}

DEFINE_TARGET(kpa_l_ctl)
DEFINE_TARGET(kpa_l_real)
DEFINE_TARGET(kpa_u_ctl)
DEFINE_TARGET(kpa_u_real)
DEFINE_TARGET(kpp_l_ctl)
DEFINE_TARGET(kpp_l_real)
DEFINE_TARGET(kpp_u_ctl)
DEFINE_TARGET(kpp_u_real)
DEFINE_TARGET(kha_l_ctl)
DEFINE_TARGET(kha_l_real)
DEFINE_TARGET(kha_u_ctl)
DEFINE_TARGET(kha_u_real)
DEFINE_TARGET(khp_l_ctl)
DEFINE_TARGET(khp_l_real)
DEFINE_TARGET(khp_u_ctl)
DEFINE_TARGET(khp_u_real)

typedef uint64_t (*target_fn)(char *, int, uint64_t);

struct target_entry {
	const char *name;
	target_fn fn;
};

static const struct target_entry targets[] = {
	{ "kpa_l_ctl", kpa_l_ctl }, { "kpa_l_real", kpa_l_real },
	{ "kpa_u_ctl", kpa_u_ctl }, { "kpa_u_real", kpa_u_real },
	{ "kpp_l_ctl", kpp_l_ctl }, { "kpp_l_real", kpp_l_real },
	{ "kpp_u_ctl", kpp_u_ctl }, { "kpp_u_real", kpp_u_real },
	{ "kha_l_ctl", kha_l_ctl }, { "kha_l_real", kha_l_real },
	{ "kha_u_ctl", kha_u_ctl }, { "kha_u_real", kha_u_real },
	{ "khp_l_ctl", khp_l_ctl }, { "khp_l_real", khp_l_real },
	{ "khp_u_ctl", khp_u_ctl }, { "khp_u_real", khp_u_real },
};

static void pin_to_cpu(int cpu)
{
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(cpu, &set);
	if (sched_setaffinity(0, sizeof(set), &set) != 0) {
		fprintf(stderr, "sched_setaffinity(cpu=%d): %s\n", cpu,
			strerror(errno));
		exit(1);
	}
}

static uint64_t parse_u64(const char *text, const char *field)
{
	char *end = NULL;
	errno = 0;
	uint64_t value = strtoull(text, &end, 10);
	if (errno || !end || *end != '\0' || value == 0) {
		fprintf(stderr, "invalid %s: %s\n", field, text);
		exit(2);
	}
	return value;
}

int main(int argc, char **argv)
{
	if (argc != 4) {
		fprintf(stderr, "usage: %s <target> <iterations> <cpu>\n", argv[0]);
		return 2;
	}

	target_fn fn = NULL;
	for (size_t i = 0; i < sizeof(targets) / sizeof(targets[0]); ++i) {
		if (strcmp(argv[1], targets[i].name) == 0) {
			fn = targets[i].fn;
			break;
		}
	}
	if (!fn) {
		fprintf(stderr, "unknown target: %s\n", argv[1]);
		return 2;
	}

	const uint64_t iterations = parse_u64(argv[2], "iteration count");
	const int cpu = (int)parse_u64(argv[3], "CPU");
	pin_to_cpu(cpu);

	char buffer[20] = "hello world";
	volatile uint64_t sink = 0;
	for (uint64_t i = 0; i < iterations; ++i)
		sink += fn(buffer, (int)(i % 4), i);
	return sink == UINT64_MAX;
}
