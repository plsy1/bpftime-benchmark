// pinbench-ws v2 (eval plan J6). v2 adds direct per-iteration timing of the pin and getpid
// (pin_direct_ns, getpid_direct_ns): v1 subtracted loop times, which is swamped by the
// variance of the touch loop once the working set exceeds the L2 (x86 J6x, 2026-11-25).
// v1 description follows.
//: does the per-event pin sequence get dearer when the
// thread touches a working set between pins, as a traced server does between events?
// Question (Q6): the pin costs about 2.1-2.4x its idle price inside nginx on the Orin
// but 1.08x on x86 (E9 correction). Hypothesis: the kernel's affinity paths
// (sched_setaffinity -> migration / cpumask / rq locks) lose their cache lines to the
// application's working set, and the Orin's 256 KB per-core L2 is smaller than x86's.
//
// For each working-set size W and each starting mask, three loops of <iters>:
//   touch      : touch W bytes (one load+store per 64 B line, sequential)
//   touch+pin  : touch, then official bpftime's pin sequence (sched_getcpu,
//                sched_getaffinity, sched_setaffinity({cur}), sched_setaffinity(saved))
//   touch+getpid : touch, then getpid (a plain system call, control)
// pin_ns = (touch+pin - touch) per iteration; getpid_ns likewise. Runs are
// interleaved (touch, touch+pin, touch+getpid) so drift hits all three alike.
// Usage: pinbench-ws <run-cpu> <runs> <iters> <ws-kb-list> name=cpus...
//   e.g. pinbench-ws 3 5 200000 0,128,256,512,1024,2048 single=3 pair=3,4
// One JSON line per (ws, mask, run).
#define _GNU_SOURCE
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static double now_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	return ts.tv_sec * 1e9 + ts.tv_nsec;
}

static int parse_list(const char *s, cpu_set_t *set)
{
	CPU_ZERO(set);
	char *dup = strdup(s), *save = NULL;
	for (char *tok = strtok_r(dup, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
		int a, b;
		if (sscanf(tok, "%d-%d", &a, &b) == 2) {
			for (int c = a; c <= b; c++)
				CPU_SET(c, set);
		} else if (sscanf(tok, "%d", &a) == 1) {
			CPU_SET(a, set);
		} else {
			free(dup);
			return -1;
		}
	}
	free(dup);
	return 0;
}

static volatile uint64_t sink;

static inline void touch(volatile uint8_t *ws, size_t bytes)
{
	for (size_t off = 0; off < bytes; off += 64) {
		uint8_t v = ws[off];
		ws[off] = (uint8_t)(v + 1);
	}
}

static inline void pin_sequence(void)
{
	int cur = sched_getcpu();
	cpu_set_t mask, saved;
	CPU_ZERO(&mask);
	CPU_SET(cur, &mask);
	if (sched_getaffinity(0, sizeof(saved), &saved) < 0)
		abort();
	if (sched_setaffinity(0, sizeof(mask), &mask) < 0)
		abort();
	if (sched_setaffinity(0, sizeof(saved), &saved) < 0)
		abort();
	sink += (uint64_t)cur;
}

int main(int argc, char **argv)
{
	if (argc < 6) {
		fprintf(stderr, "usage: %s <run-cpu> <runs> <iters> <ws-kb-list> name=cpus...\n", argv[0]);
		return 2;
	}
	int run_cpu = atoi(argv[1]);
	int runs = atoi(argv[2]);
	long iters = atol(argv[3]);
	size_t sizes[32];
	int nsizes = 0;
	{
		char *dup = strdup(argv[4]), *save = NULL;
		for (char *t = strtok_r(dup, ",", &save); t && nsizes < 32; t = strtok_r(NULL, ",", &save))
			sizes[nsizes++] = (size_t)atol(t) * 1024;
		free(dup);
	}
	size_t maxb = 0;
	for (int i = 0; i < nsizes; i++)
		if (sizes[i] > maxb)
			maxb = sizes[i];
	volatile uint8_t *ws = aligned_alloc(4096, maxb + 4096);
	if (!ws)
		return 1;
	memset((void *)ws, 1, maxb + 4096);

	for (int m = 5; m < argc; m++) {
		char *eq = strchr(argv[m], '=');
		if (!eq)
			return 2;
		*eq = '\0';
		const char *name = argv[m];
		cpu_set_t orig, one;
		if (parse_list(eq + 1, &orig) < 0 || !CPU_ISSET(run_cpu, &orig)) {
			fprintf(stderr, "mask %s must contain run cpu %d\n", name, run_cpu);
			return 2;
		}
		CPU_ZERO(&one);
		CPU_SET(run_cpu, &one);
		for (int s = 0; s < nsizes; s++) {
			size_t w = sizes[s];
			for (int r = 0; r < runs; r++) {
				// Start each run on run_cpu with the starting mask, as a traced thread.
				if (sched_setaffinity(0, sizeof(one), &one) < 0 ||
				    sched_setaffinity(0, sizeof(orig), &orig) < 0) {
					perror("sched_setaffinity");
					return 1;
				}
				for (long i = 0; i < 1000; i++) { // warm the set and the paths
					touch(ws, w);
					pin_sequence();
				}
				double t0 = now_ns();
				for (long i = 0; i < iters; i++)
					touch(ws, w);
				double t_touch = now_ns() - t0;
				double pin_direct = 0, getpid_direct = 0;
				t0 = now_ns();
				for (long i = 0; i < iters; i++) {
					touch(ws, w);
					double a = now_ns();
					pin_sequence();
					pin_direct += now_ns() - a;
				}
				double t_pin = now_ns() - t0;
				t0 = now_ns();
				for (long i = 0; i < iters; i++) {
					touch(ws, w);
					double a = now_ns();
					sink += (uint64_t)getpid();
					getpid_direct += now_ns() - a;
				}
				double t_getpid = now_ns() - t0;
				// cost of one clock pair, to subtract from the direct timings
				double clk = 0;
				for (long i = 0; i < 100000; i++) {
					double a = now_ns();
					clk += now_ns() - a;
				}
				clk /= 100000;
				printf("{\"ws_kb\":%zu,\"mask\":\"%s\",\"run\":%d,\"iters\":%ld,"
				       "\"touch_ns\":%.2f,\"pin_ns\":%.2f,\"getpid_ns\":%.2f,\"pin_direct_ns\":%.2f,"
				       "\"getpid_direct_ns\":%.2f,\"clock_pair_ns\":%.2f,\"end_cpu\":%d}\n",
				       w / 1024, name, r, iters, t_touch / iters,
				       (t_pin - t_touch) / iters, (t_getpid - t_touch) / iters,
				       pin_direct / iters - clk, getpid_direct / iters - clk, clk, sched_getcpu());
				fflush(stdout);
			}
		}
	}
	return sink == 42 ? 3 : 0;
}
