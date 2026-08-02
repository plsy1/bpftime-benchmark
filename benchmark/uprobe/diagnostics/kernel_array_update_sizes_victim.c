// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include <errno.h>
#include <inttypes.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NOINLINE __attribute__((noinline))
#define DEFINE_TARGET(bits, impl)                                             \
	NOINLINE uint64_t __kaus_##bits##_##impl(char *buffer, int index,        \
						 uint64_t value)                 \
	{                                                                        \
		asm volatile("" ::: "memory");                                    \
		return (uint64_t)buffer[index] + value;                            \
	}

#define DEFINE_SIZE_TARGETS(bits)                                             \
	DEFINE_TARGET(bits, ctrl)                                               \
	DEFINE_TARGET(bits, real)

DEFINE_SIZE_TARGETS(8)
DEFINE_SIZE_TARGETS(16)
DEFINE_SIZE_TARGETS(32)
DEFINE_SIZE_TARGETS(64)
DEFINE_SIZE_TARGETS(128)
DEFINE_SIZE_TARGETS(256)

typedef uint64_t (*target_fn)(char *, int, uint64_t);
struct target_entry {
	const char *name;
	target_fn fn;
};

#define TARGET_ENTRY(bits, impl)                                              \
	{ #bits "_" #impl, __kaus_##bits##_##impl }

static const struct target_entry targets[] = {
	TARGET_ENTRY(8, ctrl),   TARGET_ENTRY(8, real),
	TARGET_ENTRY(16, ctrl),  TARGET_ENTRY(16, real),
	TARGET_ENTRY(32, ctrl),  TARGET_ENTRY(32, real),
	TARGET_ENTRY(64, ctrl),  TARGET_ENTRY(64, real),
	TARGET_ENTRY(128, ctrl), TARGET_ENTRY(128, real),
	TARGET_ENTRY(256, ctrl), TARGET_ENTRY(256, real),
};

static uint64_t parse_u64(const char *text, const char *field, int allow_zero)
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

int main(int argc, char **argv)
{
	if (argc != 4) {
		fprintf(stderr, "usage: %s <size_impl> <iterations> <cpu>\n",
			argv[0]);
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
		fprintf(stderr, "unknown operation: %s\n", argv[1]);
		return 2;
	}

	const uint64_t iterations = parse_u64(argv[2], "iteration count", 0);
	const int cpu = (int)parse_u64(argv[3], "CPU", 1);
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(cpu, &set);
	if (sched_setaffinity(0, sizeof(set), &set) != 0) {
		fprintf(stderr, "sched_setaffinity(cpu=%d): %s\n", cpu,
			strerror(errno));
		return 1;
	}

	char buffer[20] = "hello world";
	volatile uint64_t sink = 0;
	for (uint64_t i = 0; i < iterations; ++i)
		sink += fn(buffer, (int)(i % 4), i);
	return sink == UINT64_MAX;
}
