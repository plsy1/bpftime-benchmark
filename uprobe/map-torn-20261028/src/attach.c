// Generic loader for the dimension benchmarks: loads OBJ, attaches each PROG=FUNC (uprobe) or
// PROG=FUNC:ret (uretprobe) in BINARY for every process, prints "attached", waits for
// SIGINT/SIGTERM, then prints the sums over the 64 eight-word .bss slots ("sum<i> <value>").
// Usage: attach OBJ BINARY PROG=FUNC[:ret]...
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	printf("attached\n");
	fflush(stdout);
	while (!exiting)
		pause();
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
