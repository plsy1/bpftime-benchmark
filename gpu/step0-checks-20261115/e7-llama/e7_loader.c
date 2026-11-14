// SPDX-License-Identifier: GPL-2.0
/* step0 e7-llama loader: e7_loader OBJ.bpf.o OUT.json
 * Loads and attaches every program of OBJ (under the bpftime syscall server),
 * prints a progress line every second, and on SIGINT/SIGTERM reads the whole
 * e7_hist map once more, writes it to OUT.json (raw words plus a per-kernel
 * summary) and prints the per-kernel histogram of active lanes per exit-probe
 * call. */
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include "e7_common.h"

static volatile sig_atomic_t exiting;
static void on_signal(int sig)
{
	(void)sig;
	exiting = 1;
}

static int print_fn(enum libbpf_print_level level, const char *fmt, va_list ap)
{
	if (level == LIBBPF_DEBUG)
		return 0;
	return vfprintf(stderr, fmt, ap);
}

static const char *kname[E7_NK] = { "rms", "mmq", "mmvq", "ropek", "ctrl", "ctrldiv", "ctrlmerge" };
static uint64_t words[E7_ENTRIES];

static int read_all(int fd, int *failed)
{
	*failed = 0;
	for (uint32_t key = 0; key < E7_ENTRIES; key++) {
		uint64_t v = 0;
		if (bpf_map_lookup_elem(fd, &key, &v) != 0) {
			(*failed)++;
			v = 0;
		}
		words[key] = v;
	}
	return 0;
}

struct ksum {
	uint64_t hist[E7_NB];
	uint64_t calls, threads, launches, anom_self, anom_zero;
	uint64_t calls_per_sm[E7_NS];
};

static void summarize(int k, struct ksum *s)
{
	memset(s, 0, sizeof(*s));
	for (int sm = 0; sm < E7_NS; sm++)
		for (int b = 0; b < E7_NB; b++) {
			uint64_t v = words[E7_HIST_KEY(k, sm, b)];
			s->hist[b] += v;
			s->calls += v;
			s->threads += v * (uint64_t)b;
			s->calls_per_sm[sm] += v;
		}
	s->launches = words[E7_X_KEY(k, E7_X_LAUNCHES)];
	s->anom_self = words[E7_X_KEY(k, E7_X_ANOM_SELF)];
	s->anom_zero = words[E7_X_KEY(k, E7_X_ANOM_ZERO)];
}

static int write_json(const char *path, const char *obj, int failed)
{
	FILE *f = fopen(path, "w");
	if (!f) {
		fprintf(stderr, "cannot write %s: %s\n", path, strerror(errno));
		return -1;
	}
	fprintf(f, "{\n  \"object\": \"%s\",\n  \"lookup_failures\": %d,\n", obj,
		failed);
	fprintf(f, "  \"layout\": {\"nk\": %d, \"ns\": %d, \"nb\": %d, \"stride\": %d, \"x_launches\": %d, \"x_anom_self\": %d, \"x_anom_zero\": %d},\n",
		E7_NK, E7_NS, E7_NB, E7_STRIDE, E7_X_LAUNCHES, E7_X_ANOM_SELF,
		E7_X_ANOM_ZERO);
	fprintf(f, "  \"kernels\": {\n");
	for (int k = 0; k < E7_NK; k++) {
		struct ksum s;
		summarize(k, &s);
		fprintf(f, "    \"%s\": {\"calls\": %" PRIu64 ", \"threads\": %" PRIu64
			   ", \"launches\": %" PRIu64 ", \"anom_self\": %" PRIu64
			   ", \"anom_zero\": %" PRIu64 ", \"hist\": [",
			kname[k], s.calls, s.threads, s.launches, s.anom_self,
			s.anom_zero);
		for (int b = 0; b < E7_NB; b++)
			fprintf(f, "%s%" PRIu64, b ? ", " : "", s.hist[b]);
		fprintf(f, "], \"calls_per_sm\": [");
		for (int sm = 0; sm < E7_NS; sm++)
			fprintf(f, "%s%" PRIu64, sm ? ", " : "", s.calls_per_sm[sm]);
		fprintf(f, "]}%s\n", k + 1 < E7_NK ? "," : "");
	}
	fprintf(f, "  },\n  \"words\": [");
	for (int i = 0; i < E7_ENTRIES; i++)
		fprintf(f, "%s%" PRIu64, i ? ", " : "", words[i]);
	fprintf(f, "]\n}\n");
	return fclose(f);
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s OBJ.bpf.o OUT.json\n", argv[0]);
		return 2;
	}
	setvbuf(stdout, NULL, _IOLBF, 0);
	libbpf_set_print(print_fn);
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);

	struct bpf_object *obj = bpf_object__open_file(argv[1], NULL);
	if (!obj) {
		fprintf(stderr, "cannot open %s\n", argv[1]);
		return 1;
	}
	int err = bpf_object__load(obj);
	if (err) {
		fprintf(stderr, "load failed: %d\n", err);
		return 1;
	}
	struct bpf_program *prog;
	int nprog = 0;
	bpf_object__for_each_program(prog, obj)
	{
		struct bpf_link *link = bpf_program__attach(prog);
		if (!link) {
			fprintf(stderr, "attach %s failed: %d\n",
				bpf_program__name(prog), -errno);
			return 1;
		}
		printf("attached %s (%s)\n", bpf_program__name(prog),
		       bpf_program__section_name(prog));
		nprog++;
	}
	struct bpf_map *map = bpf_object__find_map_by_name(obj, "e7_hist");
	if (!map) {
		fprintf(stderr, "no e7_hist map\n");
		return 1;
	}
	int fd = bpf_map__fd(map);
	printf("ready: %d program(s), map fd %d, %d entries\n", nprog, fd,
	       E7_ENTRIES);
	int failed = 0;
	while (!exiting) {
		sleep(1);
		read_all(fd, &failed);
		printf("%ld", (long)time(NULL));
		for (int k = 0; k < E7_NK; k++) {
			struct ksum s;
			summarize(k, &s);
			if (s.calls || s.launches || s.anom_self || s.anom_zero)
				printf(" %s:calls=%" PRIu64 ",threads=%" PRIu64
				       ",launches=%" PRIu64,
				       kname[k], s.calls, s.threads, s.launches);
		}
		printf(" lookup_failures=%d\n", failed);
	}
	read_all(fd, &failed);
	if (write_json(argv[2], argv[1], failed) != 0)
		fprintf(stderr, "writing %s failed\n", argv[2]);
	printf("final (lookup_failures=%d) -> %s\n", failed, argv[2]);
	for (int k = 0; k < E7_NK; k++) {
		struct ksum s;
		summarize(k, &s);
		if (!(s.calls || s.launches || s.anom_self || s.anom_zero))
			continue;
		printf("kernel %s: launches=%" PRIu64 " calls=%" PRIu64
		       " threads=%" PRIu64 " records_per_call=%.3f anom_self=%" PRIu64
		       " anom_zero=%" PRIu64 "\n",
		       kname[k], s.launches, s.calls, s.threads,
		       s.calls ? (double)s.threads / (double)s.calls : 0.0,
		       s.anom_self, s.anom_zero);
		for (int b = 1; b < E7_NB; b++)
			if (s.hist[b])
				printf("  lanes=%2d calls=%" PRIu64 " (%.4f%%)\n", b,
				       s.hist[b], 100.0 * (double)s.hist[b] /
						       (double)s.calls);
	}
	bpf_object__close(obj);
	return 0;
}
