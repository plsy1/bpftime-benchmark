// Loader for churn.bpf.o: attaches enter/leave to FUNC in BINARY for every process, prints
// "attached", waits for SIGINT/SIGTERM, then prints the hit/miss totals read from .bss.
// Usage: churn OBJ BINARY FUNC
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct slot {
	unsigned long long hits, misses, total_ns, pad[5];
};

static volatile sig_atomic_t exiting;
static void on_signal(int sig)
{
	(void)sig;
	exiting = 1;
}

int main(int argc, char **argv)
{
	if (argc != 4) {
		fprintf(stderr, "usage: churn OBJ BINARY FUNC\n");
		return 1;
	}
	struct bpf_object *obj = bpf_object__open_file(argv[1], NULL);
	if (!obj || bpf_object__load(obj)) {
		fprintf(stderr, "failed to open/load %s\n", argv[1]);
		return 1;
	}
	struct bpf_link *links[2] = {};
	const char *progs[2] = { "enter", "leave" };
	for (int i = 0; i < 2; i++) {
		LIBBPF_OPTS(bpf_uprobe_opts, opts, .func_name = argv[3], .retprobe = i == 1);
		struct bpf_program *p = bpf_object__find_program_by_name(obj, progs[i]);
		links[i] = p ? bpf_program__attach_uprobe_opts(p, -1, argv[2], 0, &opts) : NULL;
		if (!links[i]) {
			fprintf(stderr, "failed to attach %s\n", progs[i]);
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
	static struct slot slots[64];
	int key = 0;
	if (!bss || bpf_map_lookup_elem(bpf_map__fd(bss), &key, slots)) {
		fprintf(stderr, "failed to read .bss\n");
		return 1;
	}
	unsigned long long hits = 0, misses = 0;
	for (int i = 0; i < 64; i++) {
		hits += slots[i].hits;
		misses += slots[i].misses;
	}
	printf("hits %llu\nmisses %llu\n", hits, misses);
	for (int i = 0; i < 2; i++)
		bpf_link__destroy(links[i]);
	bpf_object__close(obj);
	return 0;
}
