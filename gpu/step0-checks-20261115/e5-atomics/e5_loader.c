// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
// e5_loader OBJ.bpf.o : load and attach one e5 program (under the syscall
// server), wait for SIGINT (run.sh sends it after the victim exits), then read
// ctl[], seen[] and out[] through bpf() and check the claimed indices.
// env: E5_THREADS  threads per launch (default 12288)
//      E5_LAUNCHES launches the victim makes (default 10)
//      E5_OUT      JSON result path (default: stdout only)
//      E5_LABEL    label written into the JSON
//      E5_MAX_WAIT_S give up waiting for SIGINT after this many s (default 900)
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#define E5_MAX_THREADS 12288
#define E5_OUT_ENTRIES 131072
#define E5_TAG_SHIFT 20
#define CTL_N 8
static const char *ctl_names[CTL_N] = { "counter",     "overflow",
					"cas_caphits", "cas_retries",
					"bad_gid",     "null_lookup",
					"ctl6",	       "ctl7" };

static volatile sig_atomic_t exiting;
static void on_sig(int s)
{
	(void)s;
	exiting = 1;
}

static int print_fn(enum libbpf_print_level level, const char *fmt,
		    va_list args)
{
	if (level == LIBBPF_DEBUG)
		return 0;
	return vfprintf(stderr, fmt, args);
}

static uint64_t env_u64(const char *n, uint64_t d)
{
	const char *e = getenv(n);
	return e && *e ? strtoull(e, NULL, 10) : d;
}

static int read_u64_map(int fd, uint32_t n, uint64_t *dst, uint64_t *fails)
{
	*fails = 0;
	for (uint32_t k = 0; k < n; k++) {
		uint64_t v = 0;
		if (bpf_map_lookup_elem(fd, &k, &v) != 0) {
			(*fails)++;
			v = 0;
		}
		dst[k] = v;
	}
	return 0;
}

static double now_s(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s OBJ.bpf.o\n", argv[0]);
		return 2;
	}
	const char *path = argv[1];
	uint64_t threads = env_u64("E5_THREADS", 12288);
	uint64_t launches = env_u64("E5_LAUNCHES", 10);
	uint64_t max_wait = env_u64("E5_MAX_WAIT_S", 900);
	const char *out_path = getenv("E5_OUT");
	const char *label = getenv("E5_LABEL") ? getenv("E5_LABEL") : path;
	if (threads > E5_MAX_THREADS) {
		fprintf(stderr, "E5_THREADS %" PRIu64 " > %d\n", threads,
			E5_MAX_THREADS);
		return 2;
	}
	libbpf_set_print(print_fn);
	signal(SIGINT, on_sig);
	signal(SIGTERM, on_sig);

	struct bpf_object *obj = bpf_object__open_file(path, NULL);
	if (!obj) {
		fprintf(stderr, "e5_loader: open %s failed: %d\n", path, errno);
		return 1;
	}
	int err = bpf_object__load(obj);
	if (err) {
		fprintf(stderr, "e5_loader: load failed: %d\n", err);
		return 1;
	}
	struct bpf_program *prog;
	int nlinks = 0;
	bpf_object__for_each_program(prog, obj)
	{
		struct bpf_link *l = bpf_program__attach(prog);
		if (!l) {
			fprintf(stderr, "e5_loader: attach %s failed: %d\n",
				bpf_program__name(prog), errno);
			return 1;
		}
		nlinks++;
	}
	int fd_ctl = bpf_object__find_map_fd_by_name(obj, "ctl");
	int fd_out = bpf_object__find_map_fd_by_name(obj, "out");
	int fd_seen = bpf_object__find_map_fd_by_name(obj, "seen");
	printf("e5_loader: %s attached %d program(s), maps ctl=%d out=%d seen=%d; waiting for SIGINT\n",
	       path, nlinks, fd_ctl, fd_out, fd_seen);
	fflush(stdout);
	double t0 = now_s();
	while (!exiting && now_s() - t0 < (double)max_wait)
		usleep(200 * 1000);
	int timed_out = !exiting;

	static uint64_t ctl[CTL_N], seen[E5_MAX_THREADS];
	uint64_t *out = calloc(E5_OUT_ENTRIES, sizeof(uint64_t));
	uint64_t f_ctl, f_seen, f_out;
	read_u64_map(fd_ctl, CTL_N, ctl, &f_ctl);
	read_u64_map(fd_seen, E5_MAX_THREADS, seen, &f_seen);
	read_u64_map(fd_out, E5_OUT_ENTRIES, out, &f_out);

	// firings per thread
	uint64_t calls = 0, min_seen = UINT64_MAX, max_seen = 0,
		 threads_expected = 0, seen_beyond = 0;
	for (uint64_t g = 0; g < E5_MAX_THREADS; g++) {
		calls += seen[g];
		if (g < threads) {
			if (seen[g] < min_seen)
				min_seen = seen[g];
			if (seen[g] > max_seen)
				max_seen = seen[g];
			if (seen[g] == launches)
				threads_expected++;
		} else if (seen[g]) {
			seen_beyond++;
		}
	}
	// slots and tags
	uint64_t span = calls < E5_OUT_ENTRIES ? calls : E5_OUT_ENTRIES;
	uint64_t written = 0, beyond = 0, invalid = 0, dup = 0, distinct = 0;
	uint64_t ntags = (max_seen ? max_seen : 1) * threads;
	unsigned char *bits = calloc(ntags / 8 + 1, 1);
	for (uint64_t i = 0; i < E5_OUT_ENTRIES; i++) {
		uint64_t t = out[i];
		if (!t)
			continue;
		if (i < span)
			written++;
		else
			beyond++;
		uint64_t launch = t >> E5_TAG_SHIFT;
		uint64_t low = t & ((1ull << E5_TAG_SHIFT) - 1);
		if (low == 0 || low - 1 >= threads ||
		    launch >= seen[low - 1]) {
			invalid++;
			continue;
		}
		uint64_t b = launch * threads + (low - 1);
		if (bits[b / 8] & (1u << (b % 8))) {
			dup++;
		} else {
			bits[b / 8] |= (1u << (b % 8));
			distinct++;
		}
	}
	uint64_t gaps = span - written;
	uint64_t missing = calls > distinct ? calls - distinct : 0;
	uint64_t expected_calls = threads * launches;
	int pass = !timed_out && f_ctl + f_seen + f_out == 0 &&
		   calls == expected_calls && ctl[0] == calls && gaps == 0 &&
		   beyond == 0 && invalid == 0 && dup == 0 && missing == 0 &&
		   ctl[1] == 0 && ctl[2] == 0 && ctl[4] == 0 && ctl[5] == 0 &&
		   threads_expected == threads && seen_beyond == 0;

	char buf[4096];
	int n = snprintf(
		buf, sizeof(buf),
		"{\"label\":\"%s\",\"obj\":\"%s\",\"timed_out\":%d,"
		"\"threads\":%" PRIu64 ",\"launches\":%" PRIu64
		",\"expected_calls\":%" PRIu64 ",\"calls\":%" PRIu64
		",\"threads_with_expected_firings\":%" PRIu64
		",\"min_firings_per_thread\":%" PRIu64
		",\"max_firings_per_thread\":%" PRIu64
		",\"firings_beyond_threads\":%" PRIu64,
		label, path, timed_out, threads, launches, expected_calls,
		calls, threads_expected, min_seen == UINT64_MAX ? 0 : min_seen,
		max_seen, seen_beyond);
	for (int i = 0; i < CTL_N; i++)
		n += snprintf(buf + n, sizeof(buf) - n, ",\"%s\":%" PRIu64,
			      ctl_names[i], ctl[i]);
	n += snprintf(buf + n, sizeof(buf) - n,
		      ",\"counter_equals_calls\":%d,\"slots_written\":%" PRIu64
		      ",\"gaps\":%" PRIu64 ",\"written_beyond_calls\":%" PRIu64
		      ",\"distinct_tags\":%" PRIu64
		      ",\"duplicate_tags\":%" PRIu64
		      ",\"invalid_tags\":%" PRIu64
		      ",\"lost_writes\":%" PRIu64
		      ",\"lookup_failures\":%" PRIu64 ",\"pass\":%d}",
		      ctl[0] == calls, written, gaps, beyond, distinct, dup,
		      invalid, missing, f_ctl + f_seen + f_out, pass);
	printf("E5RESULT %s\n", buf);
	printf("e5 %s: calls=%" PRIu64 " (expected %" PRIu64 ") counter=%" PRIu64
	       " gaps=%" PRIu64 " lost_writes=%" PRIu64 " dup_tags=%" PRIu64
	       " invalid=%" PRIu64 " cas_caphits=%" PRIu64 " -> %s\n",
	       label, calls, expected_calls, ctl[0], gaps, missing, dup,
	       invalid, ctl[2], pass ? "PASS" : "FAIL");
	fflush(stdout);
	if (out_path) {
		FILE *f = fopen(out_path, "w");
		if (f) {
			fprintf(f, "%s\n", buf);
			fclose(f);
		} else {
			fprintf(stderr, "e5_loader: cannot write %s\n",
				out_path);
		}
	}
	bpf_object__close(obj);
	return 0;
}
