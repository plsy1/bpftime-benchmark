// SPDX-License-Identifier: GPL-2.0
/* pt_loader OBJ.bpf.o OUTDIR
 * Loads and attaches every program of OBJ (under the bpftime syscall
 * server), prints a line every second, and on SIGINT/SIGTERM writes every
 * map's values: OUTDIR/<map>.bin (keys 0 .. max_entries-1, each the value
 * size bpf() returns: value_size x BPFTIME_MAP_GPU_THREAD_COUNT for the
 * per-thread types), OUTDIR/<map>.status (types 1531/1532: the delivery
 * status, key max_entries) and OUTDIR/maps.json.
 * PT_PREFILL="map:n[,map:n]" inserts keys 0 .. n-1 (value 0, BPF_NOEXIST)
 * into those maps after loading, before attaching (2026-11-26, hash maps).
 * Hash maps (types 1 and 1501) also report their entry count, by
 * bpf_map_get_next_key, in maps.json ("entries").
 * The device hash map (1533, 2026-11-28) is dumped by key instead:
 * OUTDIR/<map>.kv, {u32 key, u32 0, u64 value} per key in get_next_key
 * order, its sum and entries in maps.json; its companion status array
 * (an ARRAY named the hash's name cut to 12 bytes plus "_st") is dumped as
 * an array and its 16 words printed by name. */
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

#define MT_DEV_HASH 1533
static const char *const devhash_words[16] = {
	"deliveries", "exit_delivery", "owner", "syncs_covered", "syncs_at_exit",
	"launches_after_last_sync", "eexist", "e2big", "unstable_eexist",
	"unstable_e2big", "bad_flags", "lookup_fullwalk", "insert_fullwalk",
	"count", "live", "refused"
};
/* whether map is the companion of a 1533 map of obj (D12's name rule) */
static int is_companion(struct bpf_object *obj, struct bpf_map *map)
{
	if (bpf_map__type(map) != BPF_MAP_TYPE_ARRAY)
		return 0;
	struct bpf_map *h;
	bpf_object__for_each_map(h, obj)
	{
		if (bpf_map__type(h) != MT_DEV_HASH)
			continue;
		char want[32];
		snprintf(want, sizeof want, "%.12s_st", bpf_map__name(h));
		if (strcmp(want, bpf_map__name(map)) == 0)
			return 1;
	}
	return 0;
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
	const char *pre = getenv("PT_PREFILL");
	if (pre && *pre) {
		char spec[4096];
		snprintf(spec, sizeof spec, "%s", pre);
		for (char *tok = strtok(spec, ","); tok; tok = strtok(NULL, ",")) {
			char *colon = strchr(tok, ':');
			if (!colon)
				continue;
			*colon = 0;
			uint32_t n = (uint32_t)strtoul(colon + 1, NULL, 10);
			struct bpf_map *m =
				bpf_object__find_map_by_name(obj, tok);
			if (!m) {
				fprintf(stderr, "prefill: no map %s\n", tok);
				return 1;
			}
			uint32_t vs = bpf_map__value_size(m);
			char *zero = calloc(1, vs);
			uint32_t ok = 0;
			for (uint32_t k = 0; k < n; k++)
				ok += bpf_map_update_elem(bpf_map__fd(m), &k, zero,
							  BPF_NOEXIST) == 0;
			free(zero);
			printf("prefill %s: %u of %u keys inserted\n", tok, ok, n);
		}
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
		long long entries = -1;
		if (type == MT_DEV_HASH) {
			/* by key: the keys are anything, not 0 .. n-1 */
			snprintf(path, sizeof path, "%s/%s.kv", argv[2],
				 bpf_map__name(map));
			FILE *f = fopen(path, "wb");
			uint32_t cur = 0, next = 0;
			entries = 0;
			int e = bpf_map_get_next_key(fd, NULL, &next);
			while (e == 0 && entries <= (long long)n + 1) {
				uint64_t v = 0;
				if (bpf_map_lookup_elem(fd, &next, &v))
					failed++;
				uint32_t rec[4] = { next, 0, (uint32_t)v,
						    (uint32_t)(v >> 32) };
				if (f)
					fwrite(rec, 4, 4, f);
				nonzero += v != 0;
				sum += v;
				entries++;
				cur = next;
				e = bpf_map_get_next_key(fd, &cur, &next);
			}
			if (f)
				fclose(f);
		} else if (type != 1530) {
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
			if (is_companion(obj, map) && n >= 16) {
				printf("map %s (companion):", bpf_map__name(map));
				for (uint32_t i = 0; i < 16; i++) {
					uint64_t w = 0;
					uint32_t k = i;
					if (bpf_map_lookup_elem(fd, &k, &w) == 0)
						printf(" %s=%" PRIu64, devhash_words[i], w);
				}
				printf("\n");
			}
			free(buf);
		}
		if (type == 1 || type == 1501) {
			uint32_t cur = 0, next = 0;
			entries = 0;
			int e = bpf_map_get_next_key(fd, NULL, &next);
			while (e == 0 && entries <= (long long)n + 1) {
				entries++;
				cur = next;
				e = bpf_map_get_next_key(fd, &cur, &next);
			}
		}
		fprintf(mj,
			"%s{\"name\": \"%s\", \"type\": %d, \"value_size\": %u, \"max_entries\": %u, \"bytes_per_key\": %" PRIu64
			", \"lookup_failures\": %" PRIu64 ", \"nonzero_words\": %" PRIu64
			", \"sum\": %.0Lf, \"entries\": %lld}",
			first ? "" : ", ", bpf_map__name(map), type, vs, n, per,
			failed, nonzero, sum, entries);
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
