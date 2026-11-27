// E5-real loader. Loads OBJ, attaches program PROG: BINARY "-" = the program's own SEC (tracepoint); otherwise a
// uprobe on FUNC in BINARY (all processes). If OBJ has a "cfg" map, cfg[0] = CGID (the workload cgroup id) is
// written after load and before attach. Prints "attached", waits for SIGINT/SIGTERM, then dumps `recs` to OUT:
//   # stats calls C fail F
//   t pid_tgid uid_gid fd buf count comm_hex          (decimal; comm as 32 hex chars)
// The same binary is the kernel tool (run plainly) and the bpftime tool (LD_PRELOAD=libbpftime-syscall-server.so).
// Usage: e5rtool OBJ PROG BINARY FUNC OUT CGID
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/types.h>
#include "e5real.h"

static volatile sig_atomic_t exiting;
static void on_signal(int sig)
{
	(void)sig;
	exiting = 1;
}

int main(int argc, char **argv)
{
	if (argc != 7) {
		fprintf(stderr, "usage: e5rtool OBJ PROG BINARY FUNC OUT CGID\n");
		return 1;
	}
	struct bpf_object *obj = bpf_object__open_file(argv[1], NULL);
	if (!obj) {
		fprintf(stderr, "failed to open %s\n", argv[1]);
		return 1;
	}
	struct bpf_program *p = bpf_object__find_program_by_name(obj, argv[2]);
	if (!p) {
		fprintf(stderr, "no program %s\n", argv[2]);
		return 1;
	}
	struct bpf_program *q;
	bpf_object__for_each_program(q, obj)
		bpf_program__set_autoload(q, q == p);
	if (bpf_object__load(obj)) {
		fprintf(stderr, "failed to load %s\n", argv[1]);
		return 1;
	}
	struct bpf_map *cfg = bpf_object__find_map_by_name(obj, "cfg");
	if (cfg) {
		__u32 z = 0;
		__u64 cg = strtoull(argv[6], NULL, 10);
		if (bpf_map_update_elem(bpf_map__fd(cfg), &z, &cg, BPF_ANY)) {
			fprintf(stderr, "cfg update failed\n");
			return 1;
		}
	}
	struct bpf_link *link;
	if (strcmp(argv[3], "-") == 0) {
		link = bpf_program__attach(p);
	} else {
		LIBBPF_OPTS(bpf_uprobe_opts, opts, .func_name = argv[4]);
		link = bpf_program__attach_uprobe_opts(p, -1, argv[3], 0, &opts);
	}
	if (!link) {
		fprintf(stderr, "failed to attach %s (%s:%s): %s\n", argv[2], argv[3], argv[4], strerror(errno));
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
	bpf_link__destroy(link);  // no new records while dumping
	FILE *out = fopen(argv[5], "w");
	if (!out) {
		fprintf(stderr, "cannot open %s: %s\n", argv[5], strerror(errno));
		return 1;
	}
	unsigned long long st[2] = {};
	for (__u32 i = 0; i < 2; i++)
		bpf_map_lookup_elem(bpf_map__fd(stats), &i, &st[i]);
	fprintf(out, "# stats calls %llu fail %llu\n", st[0], st[1]);
	int fd = bpf_map__fd(recs);
	struct rkey key, next;
	unsigned long long n = 0;
	int have = bpf_map_get_next_key(fd, NULL, &next) == 0;
	while (have) {
		key = next;
		struct rrec r;
		if (bpf_map_lookup_elem(fd, &key, &r) == 0) {
			fprintf(out, "%llu %llu %llu %llu %llu %llu ", (unsigned long long)r.t,
				(unsigned long long)r.pid_tgid, (unsigned long long)r.uid_gid,
				(unsigned long long)r.fd, (unsigned long long)r.buf, (unsigned long long)r.count);
			for (int i = 0; i < 16; i++)
				fprintf(out, "%02x", (unsigned char)r.comm[i]);
			fputc('\n', out);
			n++;
		}
		have = bpf_map_get_next_key(fd, &key, &next) == 0;
	}
	fclose(out);
	printf("dumped %llu calls %llu fail %llu\n", n, st[0], st[1]);
	fflush(stdout);
	bpf_object__close(obj);
	return 0;
}
