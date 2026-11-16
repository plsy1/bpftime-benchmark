// SPDX-License-Identifier: GPL-2.0
/* pt_loader OBJ.bpf.o OUTDIR
 * Loads and attaches every program of OBJ (under the bpftime syscall
 * server), prints a line every second, and on SIGINT/SIGTERM writes every
 * map's values: OUTDIR/<map>.bin (keys 0 .. max_entries-1, each the value
 * size bpf() returns: value_size x BPFTIME_MAP_GPU_THREAD_COUNT for the
 * per-thread types), OUTDIR/<map>.status (types 1531/1532: the delivery
 * status, key max_entries) and OUTDIR/maps.json. */
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

static uint64_t threads_env(void)
{
	const char *v = getenv("BPFTIME_MAP_GPU_THREAD_COUNT");
	return v && *v ? strtoull(v, NULL, 10) : 0;
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s OBJ.bpf.o OUTDIR\n", argv[0]);
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
	if (bpf_object__load(obj)) {
		fprintf(stderr, "load failed\n");
		return 1;
	}
	struct bpf_program *prog;
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
	}
	printf("ready\n");
	int ticks = 0;
	while (!exiting) {
		sleep(1);
		if (++ticks % 10 == 0)
			printf("tick %d\n", ticks);
	}
	char path[4096];
	snprintf(path, sizeof path, "%s/maps.json", argv[2]);
	FILE *mj = fopen(path, "w");
	if (!mj) {
		fprintf(stderr, "cannot write %s\n", path);
		return 1;
	}
	fprintf(mj, "{\"object\": \"%s\", \"maps\": [", argv[1]);
	struct bpf_map *map;
	int first = 1;
	bpf_object__for_each_map(map, obj)
	{
		int type = bpf_map__type(map);
		uint32_t vs = bpf_map__value_size(map);
		uint32_t n = bpf_map__max_entries(map);
		uint64_t per = vs;
		if (type == 1502 || type == 1512 || type == 1532)
			per = (uint64_t)vs * threads_env();
		int fd = bpf_map__fd(map);
		uint64_t failed = 0, nonzero = 0;
		long double sum = 0;
		if (type != 1530) {
			char *buf = calloc(1, per);
			snprintf(path, sizeof path, "%s/%s.bin", argv[2],
				 bpf_map__name(map));
			FILE *f = fopen(path, "wb");
			for (uint32_t k = 0; k < n && f && buf; k++) {
				memset(buf, 0, per);
				if (bpf_map_lookup_elem(fd, &k, buf))
					failed++;
				fwrite(buf, 1, per, f);
				for (uint64_t i = 0; i + 8 <= per; i += 8) {
					uint64_t w;
					memcpy(&w, buf + i, 8);
					nonzero += w != 0;
					sum += w;
				}
			}
			if (f)
				fclose(f);
			if (type == 1531 || type == 1532) {
				uint64_t w[6] = { 0, 0, 0, 0, 0, 0 };
				int r = 0;
				for (uint32_t i = 0; i < 6; i++) {
					uint32_t k = n + i;
					memset(buf, 0, per);
					r |= bpf_map_lookup_elem(fd, &k, buf);
					memcpy(&w[i], buf, 8);
				}
				snprintf(path, sizeof path, "%s/%s.status",
					 argv[2], bpf_map__name(map));
				FILE *s = fopen(path, "wb");
				if (s) {
					fwrite(w, 8, 6, s);
					fclose(s);
				}
				printf("map %s status: rc=%d deliveries=%" PRIu64
				       " exit_delivery=%" PRIu64 " owner=%" PRIu64
				       " syncs_covered=%" PRIu64
				       " syncs_at_exit=%" PRIu64 " launches_after_last_sync=%" PRIu64 "\n",
				       bpf_map__name(map), r, w[0], w[1], w[2],
				       w[3], w[4], w[5]);
			}
			free(buf);
		}
		fprintf(mj,
			"%s{\"name\": \"%s\", \"type\": %d, \"value_size\": %u, \"max_entries\": %u, \"bytes_per_key\": %" PRIu64
			", \"lookup_failures\": %" PRIu64 ", \"nonzero_words\": %" PRIu64
			", \"sum\": %.0Lf}",
			first ? "" : ", ", bpf_map__name(map), type, vs, n, per,
			failed, nonzero, sum);
		first = 0;
		printf("map %s type %d: sum %.0Lf, nonzero words %" PRIu64
		       ", failed lookups %" PRIu64 "\n",
		       bpf_map__name(map), type, sum, nonzero, failed);
	}
	fprintf(mj, "]}\n");
	fclose(mj);
	printf("done\n");
	return 0;
}
