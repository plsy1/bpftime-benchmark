// Loads OBJ, attaches every program by its section (tracepoints), prints
// "attached", waits for SIGINT/SIGTERM, then prints the first 8 u64 of .bss
// as "count<i> <value>".
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t stop;
static void on_signal(int sig) { (void)sig; stop = 1; }

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: tp_attach OBJ\n");
		return 1;
	}
	struct bpf_object *obj = bpf_object__open_file(argv[1], NULL);
	if (!obj || bpf_object__load(obj)) {
		fprintf(stderr, "failed to open/load %s\n", argv[1]);
		return 1;
	}
	struct bpf_program *prog;
	bpf_object__for_each_program(prog, obj) {
		if (!bpf_program__attach(prog)) {
			fprintf(stderr, "failed to attach %s\n", bpf_program__name(prog));
			return 1;
		}
	}
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	printf("attached\n");
	fflush(stdout);
	while (!stop)
		pause();
	struct bpf_map *m, *bss = NULL;
	bpf_object__for_each_map(m, obj) {
		const char *name = bpf_map__name(m);
		size_t n = strlen(name);
		if (n >= 4 && !strcmp(name + n - 4, ".bss"))
			bss = m;
	}
	unsigned long long value[64] = {};
	int key = 0;
	if (!bss || bpf_map__value_size(bss) > sizeof(value) ||
	    bpf_map_lookup_elem(bpf_map__fd(bss), &key, value)) {
		fprintf(stderr, "failed to read .bss\n");
		return 1;
	}
	for (int i = 0; i < 8; i++)
		printf("count%d %llu\n", i, value[i]);
	return 0;
}
