// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
// e6_drain: the CPU reader side of check 6, run as a loader under the bpftime
// syscall server (run.sh), so every bpf() call below goes through the
// intercepted syscall path a real tool would use.
//
//   e6_drain bench          part (a): per (map type, value size): create a pool,
//                           fill it with bpf_map_update_elem, verify, then loop
//                           bpf_map_lookup_elem over all keys for E6_SECONDS;
//                           also BPF_MAP_LOOKUP_BATCH (supported or not) and a
//                           QUEUE pop loop (bpf_map_lookup_and_delete_elem).
//   e6_drain toggle         part (b): fill a 40-B pool and a 1-MiB-value pool
//                           (1513), then run the phase schedule
//                           [off d40 off d1m off hog1 off hog4] x E6_ROUNDS,
//                           E6_PHASE_S s each, logging CLOCK_MONOTONIC bounds
//                           and the work done in each phase.
//   e6_drain const MODE     part (b) with llama-bench: run MODE (off, d40, d1m,
//                           hog1, hog4) until SIGINT, logging 1-s windows.
// Output: JSON lines "E6 {...}" on stdout, and appended to $E6_OUT if set.
// After the work: wait for SIGINT (run.sh sends it), at most E6_MAX_WAIT_S.
// env: E6_SECONDS (3), E6_POOL_MIB (64; 40-B pools), E6_BIG_POOL_MIB (32;
//      4-KiB and 1-MiB pools), E6_PHASE_S (4), E6_ROUNDS (3), E6_OUT,
//      E6_MAX_WAIT_S (900), E6_TYPES ("array,gpu" = BPF_MAP_TYPE_ARRAY and 1503->1513)
#define _GNU_SOURCE
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#define BPF_MAP_TYPE_GPU_ARRAY_MAP 1503 /* created as 1513 on this board */

static volatile sig_atomic_t exiting;
static void on_sig(int s)
{
	(void)s;
	exiting = 1;
}
static FILE *outf;

static uint64_t env_u64(const char *n, uint64_t d)
{
	const char *e = getenv(n);
	return e && *e ? strtoull(e, NULL, 10) : d;
}
static uint64_t now_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}
static void emit(const char *json)
{
	printf("E6 %s\n", json);
	fflush(stdout);
	if (outf) {
		fprintf(outf, "%s\n", json);
		fflush(outf);
	}
}

struct pool {
	const char *tname;
	int type;
	uint32_t vsize, n;
	int fd;
};

static void fill_value(unsigned char *v, uint32_t vsize, uint32_t key)
{
	uint64_t a = key, b = ~(uint64_t)key;
	memset(v, (unsigned char)key, vsize);
	if (vsize >= 8)
		memcpy(v, &a, 8);
	if (vsize >= 16)
		memcpy(v + vsize - 8, &b, 8);
}

static int pool_create(struct pool *p, const char *tname, int type,
		       uint32_t vsize, uint64_t bytes)
{
	p->tname = tname;
	p->type = type;
	p->vsize = vsize;
	p->n = (uint32_t)(bytes / vsize);
	if (p->n == 0)
		p->n = 1;
	char name[16];
	snprintf(name, sizeof(name), "e6_%s_%u", type == BPF_MAP_TYPE_ARRAY ? "a" : "g", vsize);
	p->fd = bpf_map_create(type, name, 4, vsize, p->n, NULL);
	return p->fd;
}

// fill all keys; returns number of failed updates
static uint64_t pool_fill(struct pool *p, unsigned char *v)
{
	uint64_t fail = 0;
	for (uint32_t k = 0; k < p->n && !exiting; k++) {
		fill_value(v, p->vsize, k);
		if (bpf_map_update_elem(p->fd, &k, v, BPF_ANY))
			fail++;
	}
	return fail;
}

// lookup every key once and check the pattern; returns mismatches + failures
static uint64_t pool_verify(struct pool *p, unsigned char *v)
{
	uint64_t bad = 0;
	for (uint32_t k = 0; k < p->n && !exiting; k++) {
		if (bpf_map_lookup_elem(p->fd, &k, v)) {
			bad++;
			continue;
		}
		uint64_t a, b;
		memcpy(&a, v, 8);
		bad += a != k;
		if (p->vsize >= 16) {
			memcpy(&b, v + p->vsize - 8, 8);
			bad += b != ~(uint64_t)k;
		}
	}
	return bad;
}

// lookups over all keys, in order, until deadline_ns; *bad counts failed lookups
// and first-word mismatches
static uint64_t lookup_loop(struct pool *p, unsigned char *v, uint64_t deadline_ns,
			    uint32_t *cursor, uint64_t *bad)
{
	uint64_t ops = 0;
	uint32_t k = *cursor;
	while (!exiting) {
		for (int i = 0; i < 64; i++) {
			if (bpf_map_lookup_elem(p->fd, &k, v))
				(*bad)++;
			else if (*(uint64_t *)v != k)
				(*bad)++;
			ops++;
			if (++k == p->n)
				k = 0;
		}
		if (now_ns() >= deadline_ns)
			break;
	}
	*cursor = k;
	return ops;
}

/* ---- memory hog (positive control for part b): memcpy between two buffers ---- */
#define HOG_BYTES (64ull << 20) /* per buffer, 2 per thread: over 10x the CPU caches, so memcpy streams DRAM */
static unsigned char *hog_a[4], *hog_b[4];
static volatile int hog_run;
static volatile uint64_t hog_bytes[4];
static void *hog_thread(void *arg)
{
	long i = (long)arg;
	while (!exiting) {
		if (i >= hog_run) { // threads 0 .. hog_run-1 copy
			usleep(1000);
			continue;
		}
		for (uint64_t off = 0; off < HOG_BYTES && i < hog_run && !exiting; off += 1 << 20) {
			memcpy(hog_b[i] + off, hog_a[i] + off, 1 << 20);
			hog_bytes[i] += 1 << 20;
		}
	}
	return NULL;
}
static int hog_threads_started;
static pthread_t hog_tid[4];
static void hog_prepare(int nthreads)
{
	for (int i = hog_threads_started; i < nthreads; i++) {
		hog_a[i] = malloc(HOG_BYTES);
		hog_b[i] = malloc(HOG_BYTES);
		if (!hog_a[i] || !hog_b[i]) {
			fprintf(stderr, "e6_drain: hog malloc failed\n");
			exit(3);
		}
		memset(hog_a[i], i + 1, HOG_BYTES);
		memset(hog_b[i], 0, HOG_BYTES);
		pthread_create(&hog_tid[i], NULL, hog_thread, (void *)(long)i);
	}
	if (nthreads > hog_threads_started)
		hog_threads_started = nthreads;
}

/* ---- part (a) ---- */
static void bench(void)
{
	uint64_t secs = env_u64("E6_SECONDS", 3);
	uint64_t pool_mib = env_u64("E6_POOL_MIB", 64);
	uint64_t big_mib = env_u64("E6_BIG_POOL_MIB", 32);
	const char *types = getenv("E6_TYPES") ? getenv("E6_TYPES") : "array,gpu";
	unsigned char *v = malloc(1 << 20);
	char js[1024];
	uint32_t vsizes[3] = { 40, 4096, 1 << 20 };
	for (int t = 0; t < 2 && !exiting; t++) {
		const char *tname = t == 0 ? "array" : "gpu1503";
		if (!strstr(types, t == 0 ? "array" : "gpu"))
			continue;
		int type = t == 0 ? BPF_MAP_TYPE_ARRAY : BPF_MAP_TYPE_GPU_ARRAY_MAP;
		for (int s = 0; s < 3 && !exiting; s++) {
			struct pool p;
			uint64_t bytes = (s == 0 ? pool_mib : big_mib) << 20;
			int fd = pool_create(&p, tname, type, vsizes[s], bytes);
			if (fd < 0) {
				snprintf(js, sizeof(js),
					 "{\"part\":\"a\",\"map\":\"%s\",\"value_size\":%u,\"entries\":%u,\"create_errno\":%d}",
					 tname, p.vsize, p.n, errno);
				emit(js);
				continue;
			}
			uint64_t t0 = now_ns();
			uint64_t fill_fail = pool_fill(&p, v);
			uint64_t t1 = now_ns();
			uint64_t verify_bad = pool_verify(&p, v);
			uint64_t t2 = now_ns();
			uint32_t cursor = 0;
			uint64_t bad = 0;
			uint64_t ts = now_ns();
			uint64_t ops = lookup_loop(&p, v, ts + secs * 1000000000ull, &cursor, &bad);
			uint64_t te = now_ns();
			double el = (te - ts) / 1e9;
			snprintf(js, sizeof(js),
				 "{\"part\":\"a\",\"map\":\"%s\",\"value_size\":%u,\"entries\":%u,\"pool_bytes\":%" PRIu64
				 ",\"fill_s\":%.3f,\"fill_fail\":%" PRIu64 ",\"verify_s\":%.3f,\"verify_bad\":%" PRIu64
				 ",\"lookups\":%" PRIu64 ",\"seconds\":%.3f,\"lookup_bad\":%" PRIu64
				 ",\"records_per_s\":%.1f,\"bytes_per_s\":%.1f,\"ns_per_lookup\":%.1f,\"interrupted\":%d}",
				 tname, p.vsize, p.n, (uint64_t)p.n * p.vsize, (t1 - t0) / 1e9, fill_fail,
				 (t2 - t1) / 1e9, verify_bad, ops, el, bad, ops / el, ops * (double)p.vsize / el,
				 el * 1e9 / (ops ? ops : 1), (int)exiting);
			emit(js);
			// BPF_MAP_LOOKUP_BATCH: the syscall server has no case for it
			if (s == 0) {
				uint32_t keys[64], count = 64, out_batch = 0;
				unsigned char *vals = malloc((size_t)64 * p.vsize);
				errno = 0;
				int rc = bpf_map_lookup_batch(p.fd, NULL, &out_batch, keys, vals, &count, NULL);
				int e = errno;
				snprintf(js, sizeof(js),
					 "{\"part\":\"a-batch\",\"map\":\"%s\",\"value_size\":%u,\"rc\":%d,\"errno\":%d,\"errstr\":\"%s\",\"count\":%u,\"supported\":%d}",
					 tname, p.vsize, rc, e, strerror(e), count, rc == 0 && count > 0);
				emit(js);
				free(vals);
			}
			close(p.fd);
		}
	}
	// QUEUE pop: the pop-per-bpf()-call path (BPF_MAP_LOOKUP_AND_DELETE_ELEM)
	if (!exiting) {
		uint32_t qn = (uint32_t)((pool_mib << 20) / 40);
		if (qn > 1048576)
			qn = 1048576;
		int qfd = bpf_map_create(BPF_MAP_TYPE_QUEUE, "e6_queue", 0, 40, qn, NULL);
		if (qfd < 0) {
			snprintf(js, sizeof(js), "{\"part\":\"a-queue\",\"create_errno\":%d}", errno);
			emit(js);
		} else {
			uint64_t pops = 0, bad = 0, push_fail = 0, pop_ns = 0, rounds = 0;
			while (!exiting && pop_ns < secs * 1000000000ull) {
				for (uint32_t k = 0; k < qn; k++) {
					fill_value(v, 40, k);
					if (bpf_map_update_elem(qfd, NULL, v, BPF_ANY))
						push_fail++;
				}
				uint64_t t0 = now_ns();
				for (uint32_t k = 0; k < qn && !exiting; k++) {
					if (bpf_map_lookup_and_delete_elem(qfd, NULL, v) || *(uint64_t *)v != k)
						bad++;
					pops++;
				}
				pop_ns += now_ns() - t0;
				rounds++;
			}
			// one more pop on the empty queue must fail with ENOENT
			errno = 0;
			int rc = bpf_map_lookup_and_delete_elem(qfd, NULL, v);
			snprintf(js, sizeof(js),
				 "{\"part\":\"a-queue\",\"value_size\":40,\"entries\":%u,\"rounds\":%" PRIu64 ",\"pops\":%" PRIu64
				 ",\"pop_seconds\":%.3f,\"pops_per_s\":%.1f,\"pop_bad\":%" PRIu64 ",\"push_fail\":%" PRIu64
				 ",\"empty_pop_rc\":%d,\"empty_pop_errno\":%d}",
				 qn, rounds, pops, pop_ns / 1e9, pops / (pop_ns / 1e9 + 1e-12), bad, push_fail, rc, errno);
			emit(js);
			close(qfd);
		}
	}
	free(v);
}

/* ---- part (b) ---- */
struct mode_state {
	struct pool *p40, *p1m;
	uint32_t c40, c1m;
	unsigned char *v;
};

// run one mode from now until deadline; returns ops and bytes (lookups or memcpy)
static void run_mode(struct mode_state *st, const char *mode, uint64_t deadline,
		     uint64_t *ops, uint64_t *bytes, uint64_t *bad)
{
	*ops = *bytes = *bad = 0;
	if (!strcmp(mode, "d40")) {
		*ops = lookup_loop(st->p40, st->v, deadline, &st->c40, bad);
		*bytes = *ops * st->p40->vsize;
	} else if (!strcmp(mode, "d1m")) {
		*ops = lookup_loop(st->p1m, st->v, deadline, &st->c1m, bad);
		*bytes = *ops * st->p1m->vsize;
	} else if (!strcmp(mode, "hog1") || !strcmp(mode, "hog4")) {
		int n = mode[3] - '0';
		uint64_t b0 = 0, b1 = 0;
		for (int i = 0; i < 4; i++)
			b0 += hog_bytes[i];
		hog_run = n; // threads 0..n-1 copy
		while (!exiting && now_ns() < deadline)
			usleep(2000);
		hog_run = 0;
		usleep(5000);
		for (int i = 0; i < 4; i++)
			b1 += hog_bytes[i];
		*bytes = b1 - b0;
	} else { // off
		while (!exiting && now_ns() < deadline)
			usleep(2000);
	}
}

// nhog: hog threads (and their 2 x HOG_BYTES buffers) to prepare: 4 for the
// toggle schedule, N for "const hogN", 0 for the other const modes (so off and
// d40 arms do not hold memory the hog arms need)
static int setup_b(struct mode_state *st, struct pool *p40, struct pool *p1m, int nhog)
{
	char js[512];
	uint64_t pool_mib = env_u64("E6_POOL_MIB", 64);
	st->v = malloc(1 << 20);
	st->p40 = p40;
	st->p1m = p1m;
	st->c40 = st->c1m = 0;
	if (pool_create(p40, "gpu1503", BPF_MAP_TYPE_GPU_ARRAY_MAP, 40, pool_mib << 20) < 0 ||
	    pool_create(p1m, "gpu1503", BPF_MAP_TYPE_GPU_ARRAY_MAP, 1 << 20, pool_mib << 20) < 0) {
		snprintf(js, sizeof(js), "{\"part\":\"b-setup\",\"create_errno\":%d}", errno);
		emit(js);
		return -1;
	}
	uint64_t f = pool_fill(p40, st->v) + pool_fill(p1m, st->v);
	if (nhog > 0)
		hog_prepare(nhog);
	snprintf(js, sizeof(js),
		 "{\"part\":\"b-setup\",\"p40_entries\":%u,\"p1m_entries\":%u,\"fill_fail\":%" PRIu64
		 ",\"hog_threads\":%d,\"hog_bytes_per_thread\":%llu}",
		 p40->n, p1m->n, f, nhog, 2 * HOG_BYTES);
	emit(js);
	return 0;
}

static void toggle(void)
{
	uint64_t phase = env_u64("E6_PHASE_S", 4), rounds = env_u64("E6_ROUNDS", 3);
	struct pool p40, p1m;
	struct mode_state st;
	char js[512];
	if (setup_b(&st, &p40, &p1m, 4))
		return;
	const char *sched[8] = { "off", "d40", "off", "d1m", "off", "hog1", "off", "hog4" };
	// wait (at most 120 s) until the GPU workload says its copy loop runs
	const char *go = getenv("E6_GO_FILE");
	uint64_t w0 = now_ns();
	int go_seen = go == NULL;
	while (go && !exiting && now_ns() - w0 < 120000000000ull) {
		if (access(go, F_OK) == 0) {
			go_seen = 1;
			break;
		}
		usleep(50000);
	}
	snprintf(js, sizeof(js), "{\"part\":\"b-go\",\"go_file_seen\":%d,\"waited_s\":%.2f}", go_seen, (now_ns() - w0) / 1e9);
	emit(js);
	snprintf(js, sizeof(js), "{\"part\":\"b-schedule\",\"start_ns\":%" PRIu64 ",\"phase_s\":%" PRIu64 ",\"rounds\":%" PRIu64 "}",
		 now_ns(), phase, rounds);
	emit(js);
	for (uint64_t r = 0; r < rounds && !exiting; r++)
		for (int i = 0; i < 8 && !exiting; i++) {
			uint64_t t0 = now_ns(), ops, bytes, bad;
			run_mode(&st, sched[i], t0 + phase * 1000000000ull, &ops, &bytes, &bad);
			uint64_t t1 = now_ns();
			snprintf(js, sizeof(js),
				 "{\"part\":\"b-phase\",\"round\":%" PRIu64 ",\"mode\":\"%s\",\"t0_ns\":%" PRIu64 ",\"t1_ns\":%" PRIu64
				 ",\"ops\":%" PRIu64 ",\"bytes\":%" PRIu64 ",\"bad\":%" PRIu64 ",\"ops_per_s\":%.1f,\"bytes_per_s\":%.1f}",
				 r, sched[i], t0, t1, ops, bytes, bad, ops / ((t1 - t0) / 1e9), bytes / ((t1 - t0) / 1e9));
			emit(js);
		}
	snprintf(js, sizeof(js), "{\"part\":\"b-schedule-end\",\"end_ns\":%" PRIu64 ",\"interrupted\":%d}", now_ns(), (int)exiting);
	emit(js);
}

static void constant(const char *mode)
{
	struct pool p40, p1m;
	struct mode_state st;
	char js[512];
	int nhog = !strncmp(mode, "hog", 3) ? mode[3] - '0' : 0;
	if (nhog < 0 || nhog > 4)
		nhog = 4;
	if (setup_b(&st, &p40, &p1m, nhog))
		return;
	uint64_t maxs = env_u64("E6_MAX_WAIT_S", 900), start = now_ns();
	while (!exiting && now_ns() - start < maxs * 1000000000ull) {
		uint64_t t0 = now_ns(), ops, bytes, bad;
		run_mode(&st, mode, t0 + 1000000000ull, &ops, &bytes, &bad);
		uint64_t t1 = now_ns();
		snprintf(js, sizeof(js),
			 "{\"part\":\"b-const\",\"mode\":\"%s\",\"t0_ns\":%" PRIu64 ",\"t1_ns\":%" PRIu64 ",\"ops\":%" PRIu64
			 ",\"bytes\":%" PRIu64 ",\"bad\":%" PRIu64 "}",
			 mode, t0, t1, ops, bytes, bad);
		emit(js);
	}
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s bench|toggle|const MODE\n", argv[0]);
		return 2;
	}
	signal(SIGINT, on_sig);
	signal(SIGTERM, on_sig);
	const char *o = getenv("E6_OUT");
	if (o)
		outf = fopen(o, "a");
	char js[256];
	snprintf(js, sizeof(js), "{\"part\":\"start\",\"cmd\":\"%s\",\"pid\":%d,\"t_ns\":%" PRIu64 "}", argv[1], getpid(), now_ns());
	emit(js);
	if (!strcmp(argv[1], "bench"))
		bench();
	else if (!strcmp(argv[1], "toggle"))
		toggle();
	else if (!strcmp(argv[1], "const") && argc > 2)
		constant(argv[2]);
	snprintf(js, sizeof(js), "{\"part\":\"done\",\"interrupted\":%d,\"t_ns\":%" PRIu64 "}", (int)exiting, now_ns());
	emit(js);
	uint64_t maxw = env_u64("E6_MAX_WAIT_S", 900), t0 = now_ns();
	while (!exiting && now_ns() - t0 < maxw * 1000000000ull)
		usleep(200000);
	hog_run = 0;
	return 0;
}
