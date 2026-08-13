#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BENCH_FUNC(name)                                                       \
	__attribute__((noinline, noclone)) uint64_t name(char *a, int b, uint64_t c) \
	{ return a[b] + c; }

BENCH_FUNC(__closure_setup)
BENCH_FUNC(__closure_empty)
BENCH_FUNC(__closure_lookup_control)
BENCH_FUNC(__closure_update_control)
BENCH_FUNC(__closure_hash_lookup)
BENCH_FUNC(__closure_hash_update)
BENCH_FUNC(__closure_percpu_hash_lookup)
BENCH_FUNC(__closure_percpu_hash_update)

typedef uint64_t (*bench_fn)(char *, int, uint64_t);
struct bench_case { const char *name; bench_fn fn; };

static uint64_t elapsed_ns(struct timespec begin, struct timespec end)
{
	return (uint64_t)(end.tv_sec - begin.tv_sec) * 1000000000ULL +
	       (uint64_t)(end.tv_nsec - begin.tv_nsec);
}

static void invoke(bench_fn fn, uint64_t iterations)
{
	char buffer[20] = "hello world";
	volatile uint64_t sink = 0;
	for (uint64_t i = 0; i < iterations; ++i)
		sink += fn(buffer, i % 4, i);
	(void)sink;
}

static uint64_t measure(bench_fn fn, uint64_t iterations)
{
	struct timespec begin, end;
	clock_gettime(CLOCK_MONOTONIC_RAW, &begin);
	invoke(fn, iterations);
	clock_gettime(CLOCK_MONOTONIC_RAW, &end);
	return elapsed_ns(begin, end);
}

static uint64_t parse_u64(const char *text, const char *name)
{
	char *end;
	errno = 0;
	unsigned long long value = strtoull(text, &end, 10);
	if (errno || !*text || *end) {
		fprintf(stderr, "invalid %s: %s\n", name, text);
		exit(2);
	}
	return value;
}

int main(int argc, char **argv)
{
	static const struct bench_case cases[] = {
		{ "empty", __closure_empty },
		{ "lookup_control", __closure_lookup_control },
		{ "update_control", __closure_update_control },
		{ "hash_lookup", __closure_hash_lookup },
		{ "hash_update", __closure_hash_update },
		{ "percpu_hash_lookup", __closure_percpu_hash_lookup },
		{ "percpu_hash_update", __closure_percpu_hash_update },
	};
	static const unsigned count = sizeof(cases) / sizeof(cases[0]);
	const uint64_t iterations = argc > 1 ? parse_u64(argv[1], "iterations") : 20000;
	const unsigned order = argc > 2 ? parse_u64(argv[2], "order") % count : 0;
	const uint64_t warmup = argc > 3 ? parse_u64(argv[3], "warmup") : 1000;
	const char *only = argc > 4 ? argv[4] : NULL;

	invoke(__closure_setup, 1);
	for (unsigned i = 0; i < count; ++i)
		invoke(cases[i].fn, warmup);

	printf("case,order,iterations,total_ns,ns_per_invocation\n");
	for (unsigned position = 0; position < count; ++position) {
		unsigned index = (order + position) % count;
		const struct bench_case *item = &cases[index];
		if (only && strcmp(only, item->name) != 0)
			continue;
		uint64_t total = measure(item->fn, iterations);
		printf("%s,%u,%llu,%llu,%.9f\n", item->name, order,
		       (unsigned long long)iterations, (unsigned long long)total,
		       (double)total / (double)iterations);
	}
	if (only) {
		int known = 0;
		for (unsigned i = 0; i < count; ++i)
			known |= strcmp(only, cases[i].name) == 0;
		if (!known) return 2;
	}
	return 0;
}
