// Copied from step0/e3-copyout/partB/step0_copier.c; step0b C5 adds STEP0B_COPIER_MODE (the copier
// thread calls cuThreadExchangeStreamCaptureMode) and logs its result. Otherwise unchanged.
// step0_copier.c: LD_PRELOAD shim for step 0, check 3, part B (no bpftime).
//
// 1. Copier: a thread that waits until the APPLICATION has initialized CUDA
//    (it never calls cuInit), retains the primary context, creates its own
//    non-blocking stream, a device buffer and a registered host buffer made
//    like bpftime's segment, then copies device to host at a set rate.
// 2. Interposers: cudaStreamBeginCapture, cudaStreamEndCapture,
//    cudaGraphInstantiate, cudaGraphExecUpdate and cudaGraphLaunch are
//    forwarded with dlsym(RTLD_NEXT) and counted (calls, errors), so every
//    run reports whether CUDA graphs were captured and launched.
// The shim writes nothing to stdout or stderr; one JSON file at exit.
// copies.during_capture counts copies during which a capture begun through
// the interposed cudaStreamBeginCapture had not yet ended (sampled at issue
// and at the end of the wait): the number of copier/capture collisions.
//
// Environment:
//   STEP0_COPY_BYTES     bytes per copy; 0 = idle thread (context retained,
//                        buffers allocated, no copies). Default 0.
//   STEP0_COPY_PERIOD_US start a copy every N us (absolute schedule; a late
//                        copy starts at once and the skipped periods are
//                        counted, no catch-up burst); 0 = continuous.
//   STEP0_DEV_BYTES      device and host buffer size (default max(16 MiB,
//                        STEP0_COPY_BYTES)).
//   STEP0_WAIT           sync (cuStreamSynchronize, default) | event (record
//                        an event, poll cuEventQuery every 20 us, capped).
//   STEP0_API            async (cuMemcpyDtoHAsync on the shim's stream,
//                        default) | sync (cuMemcpyDtoH on the legacy default
//                        stream, as 1502's elem_lookup does today).
//   STEP0_UNSAFE         none (default) | memalloc (cuMemAlloc + cuMemFree of
//                        1 MiB before every copy) | ctxsync (cuCtxSynchronize
//                        before every copy). Positive controls for B0.
//   STEP0_COPIER         1 (default) | 0: no thread, interposers only.
//   STEP0_LOG            output path; "{pid}" is replaced by the pid.
//                        Default ./step0_shim.{pid}.json
//   STEP0_ONLY_EXE       if set, the shim stays inactive (forwarding only, no
//                        thread, no log) unless /proc/self/exe contains it.
//   STEP0_WAIT_S         give up waiting for the application's context after
//                        N s (default 600).
//   STEP0_DEVICE         device ordinal (default 0).
#define _GNU_SOURCE
#include <cuda.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../common/regmem.h" /* shm names /step0b_c5_*, unlinked right after mapping */

// ------------------------------------------------------------ interposers
typedef struct {
	atomic_ulong calls, errors;
	atomic_int first_err, last_err;
} icnt_t;

static icnt_t c_begin, c_end, c_inst, c_upd, c_launch;
static atomic_ulong begin_mode[4];
// captures begun successfully and not yet ended (any thread, any stream);
// the copier samples it at each copy to count copies that overlapped an
// open capture (evidence that a collision was possible at all)
static atomic_long g_cap_open;

static void icount(icnt_t *c, int r)
{
	atomic_fetch_add(&c->calls, 1);
	if (r != 0) {
		atomic_fetch_add(&c->errors, 1);
		int z = 0;
		atomic_compare_exchange_strong(&c->first_err, &z, r);
		atomic_store(&c->last_err, r);
	}
}

static void *real_sym(const char *name)
{
	void *p = dlsym(RTLD_NEXT, name);
	if (!p) {
		// nothing sensible to forward to; never write to the app's stdout
		abort();
	}
	return p;
}

// Signatures use void* for runtime handles and int for enums: the same ABI
// on aarch64, and no cuda_runtime_api.h macro renames (_ptsz) get in the way.
int cudaStreamBeginCapture(void *stream, int mode)
{
	static int (*real)(void *, int);
	if (!real)
		real = (int (*)(void *, int))real_sym("cudaStreamBeginCapture");
	int r = real(stream, mode);
	if (r == 0)
		atomic_fetch_add(&g_cap_open, 1);
	icount(&c_begin, r);
	atomic_fetch_add(&begin_mode[(mode >= 0 && mode < 3) ? mode : 3], 1);
	return r;
}

int cudaStreamEndCapture(void *stream, void **graph)
{
	static int (*real)(void *, void **);
	if (!real)
		real = (int (*)(void *, void **))real_sym("cudaStreamEndCapture");
	int r = real(stream, graph);
	// EndCapture ends the capture whatever it returns (901 = invalidated)
	long o = atomic_load(&g_cap_open);
	while (o > 0 && !atomic_compare_exchange_weak(&g_cap_open, &o, o - 1))
		;
	icount(&c_end, r);
	return r;
}

int cudaGraphInstantiate(void **exec, void *graph, unsigned long long flags)
{
	static int (*real)(void **, void *, unsigned long long);
	if (!real)
		real = (int (*)(void **, void *, unsigned long long))real_sym(
			"cudaGraphInstantiate");
	int r = real(exec, graph, flags);
	icount(&c_inst, r);
	return r;
}

int cudaGraphExecUpdate(void *exec, void *graph, void *result_info)
{
	static int (*real)(void *, void *, void *);
	if (!real)
		real = (int (*)(void *, void *, void *))real_sym(
			"cudaGraphExecUpdate");
	int r = real(exec, graph, result_info);
	icount(&c_upd, r);
	return r;
}

int cudaGraphLaunch(void *exec, void *stream)
{
	static int (*real)(void *, void *);
	if (!real)
		real = (int (*)(void *, void *))real_sym("cudaGraphLaunch");
	int r = real(exec, stream);
	icount(&c_launch, r);
	return r;
}

// ------------------------------------------------------------ copier state
#define HIST_US 200000 // 1 us buckets up to 200 ms, then overflow

static struct {
	int active; // shim active in this process
	int start_thread;
	size_t bytes, dev_bytes;
	long period_us;
	int wait_event, api_sync, unsafe, device;
	int copier_mode; // step0b C5: -1 = leave the thread's capture mode alone (step 0)
	double max_wait_s;
	char log_path[1024];
} cfg;

static struct {
	double t_ctor, t_ctx, t_setup, t_first, t_last, t_stop;
	unsigned long polls;
	int wait_timeout;
	int r_devget_last; // last non-NOT_INITIALIZED error while waiting
	int r_retain, r_stream, r_alloc, r_memset, r_hostreg, r_event,
		r_setup_sync;
	unsigned long copies, copy_errors, wait_errors, poll_cap_hits;
	unsigned long copies_capture_open; // a capture was open at issue or end
	int copy_first_err, wait_first_err;
	unsigned long unsafe_calls, unsafe_errors;
	int unsafe_first_err;
	unsigned long long bytes_total;
	unsigned long missed_periods, late_starts;
	unsigned long long lat_sum_ns, lat_min_ns, lat_max_ns;
	unsigned long hist_over;
	int join_timeout, log_written, atexit_registered;
	int r_mode, prev_mode; // step0b C5: cuThreadExchangeStreamCaptureMode result, old mode
} st;

static uint32_t *hist; // HIST_US + 1 buckets
// coarse timeline: 100 ms buckets from the end of set-up, up to 600 s
#define TL_N 6000
static uint32_t *tl_n;
static uint64_t *tl_sum_ns, *tl_max_ns;
static atomic_int g_stop;
static pthread_t g_thr;
static int g_thr_started;
static pthread_mutex_t g_log_mu = PTHREAD_MUTEX_INITIALIZER;

static double rel_s(double t)
{
	return t > 0 ? t - st.t_ctor : -1;
}

static double hist_pct(double p)
{
	unsigned long n = st.copies;
	if (n == 0 || !hist)
		return -1;
	unsigned long target = (unsigned long)(p / 100.0 * (double)(n - 1)) + 1;
	unsigned long acc = 0;
	for (unsigned i = 0; i <= HIST_US; ++i) {
		acc += hist[i];
		if (acc >= target)
			return (double)i; // lower edge of the 1 us bucket
	}
	return (double)HIST_US;
}

static void write_log(void)
{
	pthread_mutex_lock(&g_log_mu);
	if (st.log_written || !cfg.active) {
		pthread_mutex_unlock(&g_log_mu);
		return;
	}
	st.log_written = 1;
	FILE *f = fopen(cfg.log_path, "w");
	if (!f) {
		pthread_mutex_unlock(&g_log_mu);
		return;
	}
	const double active_s =
		(st.t_first > 0 && st.t_last > st.t_first) ? st.t_last - st.t_first : 0;
	const char *unsafe_names[] = { "none", "memalloc", "ctxsync" };
	fprintf(f, "{\n");
	fprintf(f, "  \"pid\": %d,\n", (int)getpid());
	fprintf(f,
		"  \"config\": {\"bytes\": %zu, \"period_us\": %ld, \"dev_bytes\": %zu, "
		"\"wait\": \"%s\", \"api\": \"%s\", \"unsafe\": \"%s\", "
		"\"copier_thread\": %d, \"copier_mode\": %d, \"r_mode\": %d, "
		"\"prev_mode\": %d},\n",
		cfg.bytes, cfg.period_us, cfg.dev_bytes,
		cfg.wait_event ? "event" : "sync", cfg.api_sync ? "sync" : "async",
		unsafe_names[cfg.unsafe], cfg.start_thread, cfg.copier_mode, st.r_mode,
		st.prev_mode);
	fprintf(f,
		"  \"times_s\": {\"ctx_active\": %.6f, \"setup_done\": %.6f, "
		"\"first_copy\": %.6f, \"last_copy\": %.6f, \"stop\": %.6f, "
		"\"copier_active\": %.6f},\n",
		rel_s(st.t_ctx), rel_s(st.t_setup), rel_s(st.t_first),
		rel_s(st.t_last), rel_s(st.t_stop), active_s);
	fprintf(f,
		"  \"wait\": {\"polls\": %lu, \"timeout\": %d, \"devget_err\": %d},\n",
		st.polls, st.wait_timeout, st.r_devget_last);
	fprintf(f,
		"  \"setup\": {\"retain\": %d, \"stream\": %d, \"alloc\": %d, "
		"\"memset\": %d, \"hostreg\": %d, \"event\": %d, \"sync\": %d},\n",
		st.r_retain, st.r_stream, st.r_alloc, st.r_memset, st.r_hostreg,
		st.r_event, st.r_setup_sync);
	fprintf(f,
		"  \"copies\": {\"n\": %lu, \"bytes\": %llu, \"errors\": %lu, "
		"\"first_err\": %d, \"wait_errors\": %lu, \"wait_first_err\": %d, "
		"\"poll_cap_hits\": %lu, \"missed_periods\": %lu, \"late_starts\": %lu, "
		"\"during_capture\": %lu, "
		"\"achieved_GBps\": %.4f, \"copies_per_s\": %.2f},\n",
		st.copies, st.bytes_total, st.copy_errors, st.copy_first_err,
		st.wait_errors, st.wait_first_err, st.poll_cap_hits,
		st.missed_periods, st.late_starts, st.copies_capture_open,
		active_s > 0 ? (double)st.bytes_total / active_s / 1e9 : 0,
		active_s > 0 ? (double)st.copies / active_s : 0);
	fprintf(f,
		"  \"latency_us\": {\"n\": %lu, \"mean\": %.2f, \"min\": %.2f, "
		"\"p50\": %.0f, \"p90\": %.0f, \"p99\": %.0f, \"p999\": %.0f, "
		"\"max\": %.2f, \"over_200ms\": %lu},\n",
		st.copies,
		st.copies ? (double)st.lat_sum_ns / st.copies / 1e3 : -1,
		st.copies ? st.lat_min_ns / 1e3 : -1, hist_pct(50), hist_pct(90),
		hist_pct(99), hist_pct(99.9),
		st.copies ? st.lat_max_ns / 1e3 : -1, st.hist_over);
	fprintf(f,
		"  \"unsafe\": {\"calls\": %lu, \"errors\": %lu, \"first_err\": %d},\n",
		st.unsafe_calls, st.unsafe_errors, st.unsafe_first_err);
#define IC(name, c)                                                            \
	fprintf(f,                                                             \
		"    \"%s\": {\"calls\": %lu, \"errors\": %lu, \"first_err\": %d, " \
		"\"last_err\": %d}",                                           \
		name, atomic_load(&c.calls), atomic_load(&c.errors),           \
		atomic_load(&c.first_err), atomic_load(&c.last_err))
	fprintf(f, "  \"graph_api\": {\n");
	IC("cudaStreamBeginCapture", c_begin);
	fprintf(f, ",\n");
	IC("cudaStreamEndCapture", c_end);
	fprintf(f, ",\n");
	IC("cudaGraphInstantiate", c_inst);
	fprintf(f, ",\n");
	IC("cudaGraphExecUpdate", c_upd);
	fprintf(f, ",\n");
	IC("cudaGraphLaunch", c_launch);
	fprintf(f,
		",\n    \"begin_modes\": {\"global\": %lu, \"thread_local\": %lu, "
		"\"relaxed\": %lu, \"other\": %lu}\n  },\n",
		atomic_load(&begin_mode[0]), atomic_load(&begin_mode[1]),
		atomic_load(&begin_mode[2]), atomic_load(&begin_mode[3]));
	fprintf(f, "  \"timeline_100ms\": [");
	int first = 1;
	for (unsigned i = 0; tl_n && i < TL_N; ++i) {
		if (!tl_n[i])
			continue;
		fprintf(f, "%s[%u, %u, %.1f, %.1f]", first ? "" : ", ", i, tl_n[i],
			(double)tl_sum_ns[i] / tl_n[i] / 1e3, tl_max_ns[i] / 1e3);
		first = 0;
	}
	fprintf(f, "],\n");
	fprintf(f,
		"  \"exit\": {\"join_timeout\": %d, \"atexit_registered\": %d}\n}\n",
		st.join_timeout, st.atexit_registered);
	fclose(f);
	pthread_mutex_unlock(&g_log_mu);
}

static void stop_and_join(void)
{
	if (st.t_stop == 0)
		st.t_stop = now_s();
	atomic_store(&g_stop, 1);
	if (g_thr_started) {
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += 5;
		if (pthread_timedjoin_np(g_thr, NULL, &ts) != 0)
			st.join_timeout = 1;
		g_thr_started = 0;
	}
}

static void step0_atexit(void)
{
	stop_and_join();
	write_log();
}

static void sleep_until_ns(uint64_t t)
{
	struct timespec ts = { (time_t)(t / 1000000000ull),
			       (long)(t % 1000000000ull) };
	while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) ==
	       EINTR)
		;
}

static void *copier_main(void *arg)
{
	(void)arg;
	CUdevice dev = 0;
	CUcontext ctx = NULL;
	// 1. wait for the application's CUDA initialization; never call cuInit
	for (;;) {
		if (atomic_load(&g_stop))
			return NULL;
		CUresult r = cuDeviceGet(&dev, cfg.device);
		if (r == CUDA_SUCCESS) {
			unsigned flags = 0;
			int on = 0;
			r = cuDevicePrimaryCtxGetState(dev, &flags, &on);
			if (r == CUDA_SUCCESS && on)
				break;
		}
		if (r != CUDA_SUCCESS && r != CUDA_ERROR_NOT_INITIALIZED)
			st.r_devget_last = (int)r;
		if (now_s() - st.t_ctor > cfg.max_wait_s) {
			st.wait_timeout = 1;
			return NULL;
		}
		st.polls++;
		usleep(2000);
	}
	st.t_ctx = now_s();
	// 2. retain the application's primary context, then set up once
	st.r_retain = cuDevicePrimaryCtxRetain(&ctx, dev);
	if (st.r_retain != CUDA_SUCCESS)
		return NULL;
	cuCtxSetCurrent(ctx);
	// step0b C5: the documented remedy under test. The copier thread sets its own
	// stream capture interaction mode (Driver API, cuThreadExchangeStreamCaptureMode)
	// before any other CUDA call that could be "potentially unsafe".
	st.r_mode = -1;
	st.prev_mode = -1;
	if (cfg.copier_mode >= 0) {
		CUstreamCaptureMode m = (CUstreamCaptureMode)cfg.copier_mode;
		st.r_mode = (int)cuThreadExchangeStreamCaptureMode(&m);
		st.prev_mode = (int)m;
	}
	// registered after the application's CUDA init, so it runs before
	// cudart's own teardown at exit
	atexit(step0_atexit);
	st.atexit_registered = 1;
	CUstream s = NULL;
	CUdeviceptr d = 0;
	CUevent ev = NULL;
	regbuf_t hb;
	memset(&hb, 0, sizeof(hb));
	st.r_stream = cuStreamCreate(&s, CU_STREAM_NON_BLOCKING);
	st.r_alloc = cuMemAlloc(&d, cfg.dev_bytes);
	if (st.r_stream || st.r_alloc)
		return NULL;
	st.r_memset = cuMemsetD8Async(d, 0x3c, cfg.dev_bytes, s);
	if (regbuf_create(&hb, cfg.dev_bytes, "shim") != 0) {
		st.r_hostreg = hb.reg ? (int)hb.reg : -1;
		return NULL;
	}
	st.r_event = cuEventCreate(&ev, CU_EVENT_DISABLE_TIMING);
	st.r_setup_sync = cuStreamSynchronize(s);
	st.t_setup = now_s();
	if (cfg.bytes == 0) { // idle
		while (!atomic_load(&g_stop))
			usleep(10000);
		return NULL;
	}
	const uint64_t period_ns = (uint64_t)cfg.period_us * 1000ull;
	uint64_t next = now_ns();
	st.lat_min_ns = ~0ull;
	while (!atomic_load(&g_stop)) {
		if (period_ns) {
			const uint64_t t = now_ns();
			if (t < next) {
				sleep_until_ns(next);
			} else if (t >= next + period_ns) {
				st.late_starts++;
				st.missed_periods += (t - next) / period_ns;
				next = t;
			}
			if (atomic_load(&g_stop))
				break;
		}
		if (cfg.unsafe == 1) {
			CUdeviceptr x = 0;
			CUresult r = cuMemAlloc(&x, 1u << 20);
			if (r == CUDA_SUCCESS)
				r = cuMemFree(x);
			st.unsafe_calls++;
			if (r != CUDA_SUCCESS) {
				if (!st.unsafe_errors)
					st.unsafe_first_err = (int)r;
				st.unsafe_errors++;
			}
		} else if (cfg.unsafe == 2) {
			CUresult r = cuCtxSynchronize();
			st.unsafe_calls++;
			if (r != CUDA_SUCCESS) {
				if (!st.unsafe_errors)
					st.unsafe_first_err = (int)r;
				st.unsafe_errors++;
			}
		}
		const long cap_a = atomic_load(&g_cap_open);
		const uint64_t a = now_ns();
		CUresult r;
		if (cfg.api_sync)
			r = cuMemcpyDtoH(hb.p, d, cfg.bytes);
		else
			r = cuMemcpyDtoHAsync(hb.p, d, cfg.bytes, s);
		if (r != CUDA_SUCCESS) {
			if (!st.copy_errors)
				st.copy_first_err = (int)r;
			st.copy_errors++;
			if (st.copy_errors > 100000)
				break;
			next += period_ns;
			continue;
		}
		if (!cfg.api_sync) {
			if (!cfg.wait_event) {
				r = cuStreamSynchronize(s);
			} else {
				r = cuEventRecord(ev, s);
				// poll, capped at 10 s
				unsigned long it = 0;
				while (r == CUDA_SUCCESS &&
				       (r = cuEventQuery(ev)) == CUDA_ERROR_NOT_READY) {
					if (++it > 500000) {
						st.poll_cap_hits++;
						break;
					}
					usleep(20);
				}
				if (r == CUDA_ERROR_NOT_READY)
					r = cuStreamSynchronize(s);
			}
			if (r != CUDA_SUCCESS) {
				if (!st.wait_errors)
					st.wait_first_err = (int)r;
				st.wait_errors++;
			}
		}
		const uint64_t b = now_ns();
		if (cap_a > 0 || atomic_load(&g_cap_open) > 0)
			st.copies_capture_open++;
		const uint64_t lat = b - a;
		const uint64_t us = lat / 1000ull;
		if (us <= HIST_US)
			hist[us]++;
		else
			st.hist_over++;
		if (tl_n) {
			const uint64_t bi =
				(uint64_t)(((double)a * 1e-9 - st.t_setup) * 10.0);
			if (bi < TL_N) {
				tl_n[bi]++;
				tl_sum_ns[bi] += lat;
				if (lat > tl_max_ns[bi])
					tl_max_ns[bi] = lat;
			}
		}
		st.lat_sum_ns += lat;
		if (lat < st.lat_min_ns)
			st.lat_min_ns = lat;
		if (lat > st.lat_max_ns)
			st.lat_max_ns = lat;
		if (!st.copies)
			st.t_first = (double)a * 1e-9; // same clock as now_s()
		st.copies++;
		st.bytes_total += cfg.bytes;
		st.t_last = now_s();
		next += period_ns;
	}
	return NULL;
}

static size_t env_size(const char *k, size_t dflt)
{
	const char *v = getenv(k);
	return (v && *v) ? (size_t)strtoull(v, NULL, 0) : dflt;
}

__attribute__((constructor)) static void step0_ctor(void)
{
	st.t_ctor = now_s();
	const char *only = getenv("STEP0_ONLY_EXE");
	if (only && *only) {
		char exe[1024];
		ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
		if (n <= 0)
			return;
		exe[n] = 0;
		if (!strstr(exe, only))
			return;
	}
	cfg.active = 1;
	cfg.bytes = env_size("STEP0_COPY_BYTES", 0);
	cfg.period_us = (long)env_size("STEP0_COPY_PERIOD_US", 0);
	cfg.dev_bytes = env_size("STEP0_DEV_BYTES", 16u << 20);
	if (cfg.dev_bytes < cfg.bytes)
		cfg.dev_bytes = cfg.bytes;
	const char *w = getenv("STEP0_WAIT");
	cfg.wait_event = (w && !strcmp(w, "event"));
	const char *a = getenv("STEP0_API");
	cfg.api_sync = (a && !strcmp(a, "sync"));
	const char *u = getenv("STEP0_UNSAFE");
	cfg.unsafe = (u && !strcmp(u, "memalloc")) ? 1 :
		     (u && !strcmp(u, "ctxsync")) ? 2 : 0;
	cfg.device = (int)env_size("STEP0_DEVICE", 0);
	const char *ws = getenv("STEP0_WAIT_S");
	cfg.max_wait_s = (ws && *ws) ? atof(ws) : 600.0;
	const char *cm = getenv("STEP0B_COPIER_MODE");
	cfg.copier_mode = !cm ? -1 :
			  !strcmp(cm, "global") ? (int)CU_STREAM_CAPTURE_MODE_GLOBAL :
			  !strcmp(cm, "thread_local") ? (int)CU_STREAM_CAPTURE_MODE_THREAD_LOCAL :
			  !strcmp(cm, "relaxed") ? (int)CU_STREAM_CAPTURE_MODE_RELAXED : -1;
	const char *ct = getenv("STEP0_COPIER");
	cfg.start_thread = !(ct && !strcmp(ct, "0"));
	const char *lp = getenv("STEP0_LOG");
	if (!lp || !*lp)
		lp = "./step0_shim.{pid}.json";
	// replace {pid}
	const char *ph = strstr(lp, "{pid}");
	if (ph)
		snprintf(cfg.log_path, sizeof(cfg.log_path), "%.*s%d%s",
			 (int)(ph - lp), lp, (int)getpid(), ph + 5);
	else
		snprintf(cfg.log_path, sizeof(cfg.log_path), "%s", lp);
	if (cfg.start_thread) {
		hist = (uint32_t *)calloc(HIST_US + 1, sizeof(uint32_t));
		tl_n = (uint32_t *)calloc(TL_N, sizeof(uint32_t));
		tl_sum_ns = (uint64_t *)calloc(TL_N, sizeof(uint64_t));
		tl_max_ns = (uint64_t *)calloc(TL_N, sizeof(uint64_t));
		if (hist && pthread_create(&g_thr, NULL, copier_main, NULL) == 0)
			g_thr_started = 1;
	}
}

// Fallback when the context never became active (no atexit registered):
// destructors run after all atexit handlers.
__attribute__((destructor)) static void step0_dtor(void)
{
	if (!cfg.active)
		return;
	stop_and_join();
	write_log();
}
