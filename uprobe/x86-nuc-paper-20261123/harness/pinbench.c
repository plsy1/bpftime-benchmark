// pinbench: cost of the system calls official bpftime makes per event (plan E2).
// Times the per-event CPU pinning sequence of bpf_perf_event_output in official
// bpftime 4708f409 (runtime/src/bpf_helper.cpp: sched_getcpu -> sched_getaffinity ->
// sched_setaffinity({cur}) -> ... -> sched_setaffinity(orig)) for several starting masks,
// plus single syscalls (getpid, gettid, getuid, getgid, getppid, readlinkat).
// Usage: pinbench <run-cpu> <mhz> <runs> <iters> <mask-name>=<cpu-list> ...
//   e.g. pinbench 4 3500 10 1000000 single=4 pair=4,6 all=0-15
// Prints one JSON line per (case, run): ns per operation and cycles at <mhz>.
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
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

static void emit(const char *kase, const char *mask, int run, long iters, double ns, double mhz, int cpu)
{
	double per = ns / iters;
	printf("{\"case\":\"%s\",\"mask\":\"%s\",\"run\":%d,\"iters\":%ld,\"ns_per_op\":%.2f,"
	       "\"cycles_per_op\":%.0f,\"cpu\":%d,\"mhz\":%.0f}\n",
	       kase, mask, run, iters, per, per * mhz / 1000.0, cpu, mhz);
	fflush(stdout);
}

int main(int argc, char **argv)
{
	if (argc < 6) {
		fprintf(stderr, "usage: %s <run-cpu> <mhz> <runs> <iters> name=cpus...\n", argv[0]);
		return 2;
	}
	int run_cpu = atoi(argv[1]);
	double mhz = atof(argv[2]);
	int runs = atoi(argv[3]);
	long iters = atol(argv[4]);
	volatile long sink = 0;
	char buf[256];

	for (int m = 5; m < argc; m++) {
		char *eq = strchr(argv[m], '=');
		if (!eq)
			return 2;
		*eq = '\0';
		const char *name = argv[m];
		cpu_set_t orig;
		if (parse_list(eq + 1, &orig) < 0 || !CPU_ISSET(run_cpu, &orig)) {
			fprintf(stderr, "mask %s must contain run cpu %d\n", name, run_cpu);
			return 2;
		}
		// Start on run_cpu with the starting mask, as a traced thread would be.
		cpu_set_t one;
		CPU_ZERO(&one);
		CPU_SET(run_cpu, &one);
		if (sched_setaffinity(0, sizeof(one), &one) < 0 || sched_setaffinity(0, sizeof(orig), &orig) < 0) {
			perror("sched_setaffinity");
			return 1;
		}
		for (int r = 0; r < runs; r++) {
			double t0 = now_ns();
			for (long i = 0; i < iters; i++) {
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
				sink += cur;
			}
			emit("pin_sequence", name, r, iters, now_ns() - t0, mhz, sched_getcpu());
		}
	}
	// Single system calls, on run_cpu only.
	cpu_set_t one;
	CPU_ZERO(&one);
	CPU_SET(run_cpu, &one);
	sched_setaffinity(0, sizeof(one), &one);
	for (int r = 0; r < runs; r++) {
		double t0;
		t0 = now_ns();
		for (long i = 0; i < iters; i++)
			sink += sched_getcpu();
		emit("sched_getcpu", "-", r, iters, now_ns() - t0, mhz, run_cpu);
		t0 = now_ns();
		for (long i = 0; i < iters; i++)
			sink += syscall(SYS_getppid);
		emit("getppid", "-", r, iters, now_ns() - t0, mhz, run_cpu);
		t0 = now_ns();
		for (long i = 0; i < iters; i++)
			sink += getpid();
		emit("getpid", "-", r, iters, now_ns() - t0, mhz, run_cpu);
		t0 = now_ns();
		for (long i = 0; i < iters; i++)
			sink += syscall(SYS_gettid);
		emit("gettid", "-", r, iters, now_ns() - t0, mhz, run_cpu);
		t0 = now_ns();
		for (long i = 0; i < iters; i++)
			sink += getuid() + getgid();
		emit("getuid+getgid", "-", r, iters, now_ns() - t0, mhz, run_cpu);
		t0 = now_ns();
		for (long i = 0; i < iters / 10; i++)
			sink += readlinkat(AT_FDCWD, "/proc/self/exe", buf, sizeof(buf));
		emit("readlinkat_proc_self_exe", "-", r, iters / 10, now_ns() - t0, mhz, run_cpu);
	}
	return sink == 42 ? 3 : 0;
}
