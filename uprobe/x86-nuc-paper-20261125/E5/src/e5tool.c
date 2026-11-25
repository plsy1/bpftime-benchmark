// E5 loader: attaches program "audit" of OBJ as a uprobe on FUNC in BINARY (all processes),
// prints "attached", waits for SIGINT/SIGTERM, then dumps the `recs` map to OUT:
//   # stats calls DUP FAIL
//   seq pid_tgid uid_gid hits comm_hex      (decimal, comm as 32 hex chars)
// The same binary is the kernel tool (run plainly) and the bpftime tool (run with
// LD_PRELOAD=libbpftime-syscall-server.so).
// Usage: e5tool OBJ BINARY FUNC OUT
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct rec {
	unsigned long long pid_tgid, uid_gid;
	unsigned char comm[16];
	unsigned long long hits;
};

static volatile sig_atomic_t exiting;
static void on_signal(int sig)
{
	(void)sig;
	exiting = 1;
}

int main(int argc, char **argv)
{
	if (argc != 5) {
		fprintf(stderr, "usage: e5tool OBJ BINARY FUNC OUT\n");
		return 1;
	}
	struct bpf_object *obj = bpf_object__open_file(argv[1], NULL);
	if (!obj || bpf_object__load(obj)) {
		fprintf(stderr, "failed to open/load %s\n", argv[1]);
		return 1;
	}
	struct bpf_program *p = bpf_object__find_program_by_name(obj, "audit");
	LIBBPF_OPTS(bpf_uprobe_opts, opts, .func_name = argv[3]);
	struct bpf_link *link = p ? bpf_program__attach_uprobe_opts(p, -1, argv[2], 0, &opts) : NULL;
	if (!link) {
		fprintf(stderr, "failed to attach audit to %s:%s\n", argv[2], argv[3]);
		return 1;
	}
	struct bpf_map *recs = bpf_object__find_map_by_name(obj, "recs");
	struct bpf_map *stats = bpf_object__find_map_by_name(obj, "stats");
	if (!recs || !stats) {
		fprintf(stderr, "maps missing\n");
		return 1;
	}
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	printf("attached\n");
	fflush(stdout);
	while (!exiting)
		pause();
	FILE *out = fopen(argv[4], "w");
	if (!out) {
		fprintf(stderr, "cannot open %s: %s\n", argv[4], strerror(errno));
		return 1;
	}
	unsigned long long st[4] = {};
	for (unsigned int i = 0; i < 4; i++)
		bpf_map_lookup_elem(bpf_map__fd(stats), &i, &st[i]);
	fprintf(out, "# stats calls %llu dup %llu fail %llu\n", st[0], st[1], st[2]);
	int fd = bpf_map__fd(recs);
	unsigned long long key, next, n = 0;
	int have = bpf_map_get_next_key(fd, NULL, &next) == 0;
	while (have) {
		key = next;
		struct rec r;
		if (bpf_map_lookup_elem(fd, &key, &r) == 0) {
			fprintf(out, "%llu %llu %llu %llu ", key, r.pid_tgid, r.uid_gid, r.hits);
			for (int i = 0; i < 16; i++)
				fprintf(out, "%02x", r.comm[i]);
			fputc('\n', out);
			n++;
		}
		have = bpf_map_get_next_key(fd, &key, &next) == 0;
	}
	fclose(out);
	printf("dumped %llu calls %llu dup %llu fail %llu\n", n, st[0], st[1], st[2]);
	fflush(stdout);
	bpf_link__destroy(link);
	bpf_object__close(obj);
	return 0;
}
