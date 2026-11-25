// E4 loader: attaches one variant (a|b|c) of pcpu_lost.bpf.c as a uprobe on FUNC in BINARY
// (all processes), prints "attached", waits for SIGINT/SIGTERM, then sums the per-CPU values
// through bpf() map lookups and prints one JSON line. Unavailable numbers print as null.
// Usage: pcpu_lost -v a|b|c -B BINARY [-f tick] [-w 0|1|2|4|8] [-R] [-e EXPECTED | -E TARGET_JSON]
//   -w  variant b: number of bpf_ktime_get_ns() calls between the volatile load and store
//       (selects program prog_b<W>; W is a compile-time constant there). -w 0 = ld/add/st only.
//   -R  do not count program runs (timing rows): runs/nulls/lost_vs_runs/missed_runs are null;
//       "seen" (0/1) still tells whether the program ran at all.
//   -E  file holding the target's JSON line; "expected" is read from it at exit
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/* same layouts as pcpu_lost.bpf.c; the skeleton names them */
struct slot {
	unsigned long long runs, nulls, pad[6];
} __attribute__((aligned(64)));
struct seen_line {
	unsigned long long seen, pad[7];
} __attribute__((aligned(64)));
#include "pcpu_lost.skel.h"

static volatile sig_atomic_t exiting;
static void on_signal(int sig)
{
	(void)sig;
	exiting = 1;
}

static long long expected_from_file(const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return -1;
	char buf[4096];
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[n] = 0;
	char *p = strstr(buf, "\"expected\":");
	return p ? atoll(p + strlen("\"expected\":")) : -1;
}

int main(int argc, char **argv)
{
	char variant = 0;
	const char *binary = NULL, *func = "tick", *expected_file = NULL;
	long long expected = -1;
	int widen = 1, count_runs = 1, opt;
	while ((opt = getopt(argc, argv, "v:B:f:w:Re:E:")) != -1) {
		switch (opt) {
		case 'v': variant = optarg[0]; break;
		case 'B': binary = optarg; break;
		case 'f': func = optarg; break;
		case 'w': widen = atoi(optarg); break;
		case 'R': count_runs = 0; break;
		case 'e': expected = atoll(optarg); break;
		case 'E': expected_file = optarg; break;
		default: goto usage;
		}
	}
	if (!binary || (variant != 'a' && variant != 'b' && variant != 'c') ||
	    (widen != 0 && widen != 1 && widen != 2 && widen != 4 && widen != 8)) {
usage:
		fprintf(stderr, "usage: pcpu_lost -v a|b|c -B BINARY [-f FUNC] [-w 0|1|2|4|8] [-R] [-e N | -E FILE]\n");
		return 1;
	}
	struct pcpu_lost_bpf *skel = pcpu_lost_bpf__open();
	if (!skel) {
		fprintf(stderr, "open failed\n");
		return 1;
	}
	skel->rodata->count_runs = count_runs;
	struct bpf_program *progs[] = { skel->progs.prog_a, skel->progs.prog_c, skel->progs.prog_b0,
					skel->progs.prog_b1, skel->progs.prog_b2, skel->progs.prog_b4,
					skel->progs.prog_b8 };
	const int nprogs = sizeof(progs) / sizeof(progs[0]);
	struct bpf_program *prog = variant == 'a' ? progs[0] : variant == 'c' ? progs[1] :
				   widen == 0 ? progs[2] : widen == 1 ? progs[3] : widen == 2 ? progs[4] :
				   widen == 4 ? progs[5] : progs[6];
	for (int i = 0; i < nprogs; i++)
		bpf_program__set_autoload(progs[i], progs[i] == prog);
	int err = pcpu_lost_bpf__load(skel);
	if (err) {
		fprintf(stderr, "load failed: %d\n", err);
		return 1;
	}
	int ncpu = libbpf_num_possible_cpus();
	if (ncpu <= 0 || ncpu > 4096) {
		fprintf(stderr, "bad possible cpu count %d\n", ncpu);
		return 1;
	}
	unsigned long long *vals = calloc(ncpu, sizeof(*vals));
	__u32 zero = 0;
	int map_fd = bpf_map__fd(variant == 'c' ? skel->maps.pcpu_hash : skel->maps.pcpu_array);
	if (variant == 'c' && bpf_map_update_elem(map_fd, &zero, vals, BPF_NOEXIST)) {
		fprintf(stderr, "hash pre-insert failed: %d\n", -errno);
		return 1;
	}
	LIBBPF_OPTS(bpf_uprobe_opts, uopts, .func_name = func);
	struct bpf_link *link = bpf_program__attach_uprobe_opts(prog, -1, binary, 0, &uopts);
	if (!link) {
		fprintf(stderr, "attach failed: %d\n", -errno);
		return 1;
	}
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	printf("attached\n");
	fflush(stdout);
	while (!exiting)
		pause();

	if (bpf_map_lookup_elem(map_fd, &zero, vals)) {
		fprintf(stderr, "per-cpu lookup failed: %d\n", -errno);
		return 1;
	}
	unsigned long long sum = 0;
	for (int i = 0; i < ncpu; i++)
		sum += vals[i];
	int bss_fd = bpf_map__fd(skel->maps.bss);
	size_t bss_sz = bpf_map__value_size(skel->maps.bss);
	struct pcpu_lost_bpf__bss *bss = calloc(1, bss_sz < sizeof(*bss) ? sizeof(*bss) : bss_sz);
	unsigned long long runs = 0, nulls = 0;
	int bss_ok = bss && bss_fd >= 0 && bss_sz >= sizeof(*bss) && !bpf_map_lookup_elem(bss_fd, &zero, bss);
	if (bss_ok)
		for (int i = 0; i < 128; i++) {
			runs += bss->slots[i].runs;
			nulls += bss->slots[i].nulls;
		}
	if (expected_file)
		expected = expected_from_file(expected_file);
	char b_exp[32] = "null", b_lost[32] = "null", b_runs[32] = "null", b_nulls[32] = "null",
	     b_lvr[32] = "null", b_miss[32] = "null", b_seen[32] = "null";
	int have_runs = bss_ok && count_runs;
	if (expected >= 0) {
		snprintf(b_exp, sizeof(b_exp), "%lld", expected);
		snprintf(b_lost, sizeof(b_lost), "%lld", expected - (long long)sum);
	}
	if (bss_ok) {
		snprintf(b_nulls, sizeof(b_nulls), "%llu", nulls);
		snprintf(b_seen, sizeof(b_seen), "%llu", have_runs ? (runs > 0) : bss->seen_line.seen);
	}
	if (have_runs) {
		snprintf(b_runs, sizeof(b_runs), "%llu", runs);
		snprintf(b_lvr, sizeof(b_lvr), "%lld", (long long)runs - (long long)nulls - (long long)sum);
		if (expected >= 0)
			snprintf(b_miss, sizeof(b_miss), "%lld", expected - (long long)runs);
	}
	printf("{\"variant\":\"%c\",\"widen\":%s,\"prog\":\"%s\",\"count_runs\":%d,\"sum\":%llu,"
	       "\"expected\":%s,\"lost\":%s,\"runs\":%s,\"nulls\":%s,\"lost_vs_runs\":%s,"
	       "\"missed_runs\":%s,\"seen\":%s,\"bss_ok\":%s,\"ncpu\":%d,\"per_cpu\":[",
	       variant, variant == 'b' ? (widen == 0 ? "0" : widen == 1 ? "1" : widen == 2 ? "2" : widen == 4 ? "4" : "8") : "null",
	       bpf_program__name(prog), count_runs, sum, b_exp, b_lost, b_runs, b_nulls, b_lvr, b_miss,
	       b_seen, bss_ok ? "true" : "false", ncpu);
	for (int i = 0; i < ncpu; i++)
		printf("%s%llu", i ? "," : "", vals[i]);
	printf("]}\n");
	fflush(stdout);
	bpf_link__destroy(link);
	pcpu_lost_bpf__destroy(skel);
	return 0;
}
