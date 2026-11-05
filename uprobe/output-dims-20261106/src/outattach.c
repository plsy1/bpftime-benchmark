// attach.c of map-table-20261028 plus a consumer for a map named "events" (perf buffer or
// ring buffer) polled by a thread; prints "received N" and "lost N" at the end.
// output-dims-20261106: PB_PAGES (perf buffer pages per CPU, default 64 as libbpf-tools)
// and CONSUMER_NS (the consumer spins this long per record, as a tool formatting output).
// LATENCY=1: each record's first word is its CLOCK_MONOTONIC send time (ns); the consumer takes
// the delivery delay (its own clock on entering the callback minus the send time) of every record,
// and of each record sent more than 1 ms after every record seen before it (the first of a burst,
// arriving at a consumer that had caught up and slept: the wakeup latency); prints percentiles.
// Loads OBJ,
// attaches PROG=FUNC uprobes in BINARY, prints "attached", waits for SIGINT, prints the sums.
// Usage: attach OBJ BINARY PROG=FUNC[:ret]...
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

static unsigned long long received, lost;
static struct perf_buffer *pb;
static struct ring_buffer *rb;
static volatile int stop_consumer;
static long consumer_ns;
static int latency;
#define LAT_CAP (1u << 23)
#define FIRST_CAP (1u << 18)
static unsigned int *lat, *first;
static size_t nlat, nfirst;
static unsigned long long max_key, negative;

static unsigned long long mono_ns(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000ULL + t.tv_nsec;
}

static void note(const void *data, size_t size)
{
	if (!latency || size < 8)
		return;
	unsigned long long key, t = mono_ns();
	memcpy(&key, data, 8);
	long long d = (long long)(t - key);
	if (d < 0) {
		negative++;
		d = 0;
	}
	unsigned int v = d > 0xffffffffLL ? 0xffffffffu : (unsigned int)d;
	if (nlat < LAT_CAP)
		lat[nlat++] = v;
	if (key > max_key + 1000000ULL && nfirst < FIRST_CAP)
		first[nfirst++] = v;
	if (key > max_key)
		max_key = key;
}

static int cmp_u32(const void *a, const void *b)
{
	unsigned int x = *(const unsigned int *)a, y = *(const unsigned int *)b;
	return (x > y) - (x < y);
}

static void print_pcts(const char *name, unsigned int *v, size_t n)
{
	printf("%s_n %zu\n", name, n);
	if (!n)
		return;
	qsort(v, n, sizeof(*v), cmp_u32);
	static const int pct[] = { 50, 90, 99 };
	for (int i = 0; i < 3; i++)
		printf("%s_p%d %u\n", name, pct[i], v[(n - 1) * pct[i] / 100]);
	printf("%s_max %u\n", name, v[n - 1]);
}

static void spin(void)
{
	if (consumer_ns <= 0)
		return;
	struct timespec t0, t;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	do
		clock_gettime(CLOCK_MONOTONIC, &t);
	while ((t.tv_sec - t0.tv_sec) * 1000000000L + (t.tv_nsec - t0.tv_nsec) < consumer_ns);
}

static void on_sample(void *ctx, int cpu, void *data, __u32 size)
{
	(void)ctx, (void)cpu;
	note(data, size);
	received++;
	spin();
}
static void on_lost(void *ctx, int cpu, __u64 n)
{
	(void)ctx, (void)cpu;
	lost += n;
}
static int on_record(void *ctx, void *data, size_t size)
{
	(void)ctx;
	note(data, size);
	received++;
	spin();
	return 0;
}
static void *consume(void *arg)
{
	(void)arg;
	while (!stop_consumer) {
		if (pb)
			perf_buffer__poll(pb, 100);
		else if (rb)
			ring_buffer__poll(rb, 100);
	}
	if (pb)
		while (perf_buffer__poll(pb, 0) > 0) {
		}
	if (rb)
		while (ring_buffer__poll(rb, 0) > 0) {
		}
	return NULL;
}

static volatile sig_atomic_t exiting;
static void on_signal(int sig)
{
	(void)sig;
	exiting = 1;
}

int main(int argc, char **argv)
{
	if (argc < 4) {
		fprintf(stderr, "usage: attach OBJ BINARY PROG=FUNC[:ret]...\n");
		return 1;
	}
	struct bpf_object *obj = bpf_object__open_file(argv[1], NULL);
	if (!obj || bpf_object__load(obj)) {
		fprintf(stderr, "failed to open/load %s\n", argv[1]);
		return 1;
	}
	for (int i = 3; i < argc; i++) {
		char spec[256];
		snprintf(spec, sizeof(spec), "%s", argv[i]);
		char *func = strchr(spec, '=');
		if (!func) {
			fprintf(stderr, "bad spec %s\n", argv[i]);
			return 1;
		}
		*func++ = 0;
		char *ret = strchr(func, ':');
		if (ret)
			*ret++ = 0;
		LIBBPF_OPTS(bpf_uprobe_opts, opts, .func_name = func,
			    .retprobe = ret && !strcmp(ret, "ret"));
		struct bpf_program *p = bpf_object__find_program_by_name(obj, spec);
		if (!p || !bpf_program__attach_uprobe_opts(p, -1, argv[2], 0, &opts)) {
			fprintf(stderr, "failed to attach %s\n", argv[i]);
			return 1;
		}
	}
	if (getenv("CONSUMER_NS"))
		consumer_ns = atol(getenv("CONSUMER_NS"));
	if (getenv("LATENCY") && atoi(getenv("LATENCY"))) {
		lat = malloc(LAT_CAP * sizeof(*lat));
		first = malloc(FIRST_CAP * sizeof(*first));
		if (!lat || !first) {
			fprintf(stderr, "no memory for latencies\n");
			return 1;
		}
		latency = 1;
	}
	pthread_t consumer;
	struct bpf_map *events = bpf_object__find_map_by_name(obj, "events");
	if (events) {
		if (bpf_map__type(events) == BPF_MAP_TYPE_PERF_EVENT_ARRAY)
			pb = perf_buffer__new(bpf_map__fd(events),
					      getenv("PB_PAGES") ? atoi(getenv("PB_PAGES")) : 64,
					      on_sample,
					      on_lost, NULL, NULL);
		else
			rb = ring_buffer__new(bpf_map__fd(events), on_record, NULL,
					      NULL);
		if (!pb && !rb) {
			fprintf(stderr, "failed to open the events consumer\n");
			return 1;
		}
	}
	sigset_t block;
	sigemptyset(&block);
	sigaddset(&block, SIGINT);
	sigaddset(&block, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &block, NULL); // the consumer never takes them
	if (events)
		pthread_create(&consumer, NULL, consume, NULL);
	pthread_sigmask(SIG_UNBLOCK, &block, NULL);
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	printf("attached\n");
	fflush(stdout);
	while (!exiting)
		pause();
	if (events) {
		stop_consumer = 1;
		pthread_join(consumer, NULL);
		printf("received %llu\nlost %llu\n", received, lost);
		if (latency) {
			printf("lat_negative %llu\n", negative);
			print_pcts("lat", lat, nlat);
			print_pcts("first", first, nfirst);
		}
	}
	struct bpf_map *bss = NULL, *m;
	bpf_object__for_each_map(m, obj) {
		const char *name = bpf_map__name(m);
		size_t n = strlen(name);
		if (n >= 4 && !strcmp(name + n - 4, ".bss"))
			bss = m;
	}
	// The slots must be the first .bss object; the buffer covers the whole value.
	static unsigned long long value[1 << 16];
	unsigned long long(*slots)[8] = (void *)value;
	int key = 0;
	if (!bss || bpf_map__value_size(bss) < 64 * 8 * 8 ||
	    bpf_map__value_size(bss) > sizeof(value) ||
	    bpf_map_lookup_elem(bpf_map__fd(bss), &key, value)) {
		fprintf(stderr, "failed to read .bss\n");
		return 1;
	}
	for (int f = 0; f < 8; f++) {
		unsigned long long sum = 0;
		for (int s = 0; s < 64; s++)
			sum += slots[s][f];
		printf("sum%d %llu\n", f, sum);
	}
	// SUM_MAP=name: also sum every u64 value of that map (all CPUs for per-CPU maps).
	const char *sum_map = getenv("SUM_MAP");
	if (sum_map) {
		struct bpf_map *map = bpf_object__find_map_by_name(obj, sum_map);
		if (!map || bpf_map__value_size(map) != 8) {
			fprintf(stderr, "no u64 map %s\n", sum_map);
			return 1;
		}
		enum bpf_map_type type = bpf_map__type(map);
		int ncpu = (type == BPF_MAP_TYPE_PERCPU_HASH || type == BPF_MAP_TYPE_PERCPU_ARRAY) ?
				   libbpf_num_possible_cpus() :
				   1;
		static unsigned long long values[1024];
		unsigned int k, next, *prev = NULL;
		unsigned long long total = 0, entries = 0;
		while (!bpf_map_get_next_key(bpf_map__fd(map), prev, &next)) {
			k = next;
			prev = &k;
			if (bpf_map_lookup_elem(bpf_map__fd(map), &k, values))
				continue;
			entries++;
			for (int c = 0; c < ncpu; c++)
				total += values[c];
		}
		printf("map_sum %llu\nmap_entries %llu\n", total, entries);
	}
	return 0;
}
