// Layout for every key_size 0..72 x value_size 0..40: value address alignment
// (8 for >=8, else 4), value inside its element, element stride; map
// creation with degenerate parameters; multi-process capacity waves with the
// token pool drained; token conservation.
#include <bpf_map/bpftime_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
using namespace boost::interprocess;
using namespace bpftime;

static int layout()
{
	int bad = 0;
	const char *name = "fable2_exp_layout";
	shared_memory_object::remove(name);
	managed_shared_memory seg(create_only, name, 32 << 20);
	// Perturb the allocator so the data buffer does not start at the
	// first allocation.
	std::vector<void *> junk;
	for (size_t ks = 1; ks <= 72; ks++) {
		for (size_t vs = 1; vs <= 40; vs++) {
			junk.push_back(seg.allocate((ks * 7 + vs) % 53 + 1));
			bpftime_hash_map map(seg, 64, ks, vs);
			const size_t align = vs >= 8 ? 8 : 4;
			std::vector<uint8_t> key(ks), val(vs, 0xab);
			uintptr_t base = 0, stride = 0;
			size_t n = 0;
			for (size_t i = 0; i < 64; i++) {
				memset(key.data(), 0, ks);
				memcpy(key.data(), &i, std::min(ks, sizeof(i)));
				if (ks == 1 && i >= 256)
					break;
				if (!map.elem_update(key.data(), val.data())) {
					printf("ks %zu vs %zu: insert %zu failed\n",
					       ks, vs, i);
					bad++;
					break;
				}
				n++;
				auto *p = static_cast<uint8_t *>(
					map.elem_lookup(key.data()));
				if (!p) {
					printf("ks %zu vs %zu: lookup %zu null\n", ks,
					       vs, i);
					bad++;
					continue;
				}
				if ((uintptr_t)p % align) {
					printf("ks %zu vs %zu: value %p misaligned\n",
					       ks, vs, (void *)p);
					bad++;
				}
				// The value is inside its element: the element
				// index from the value pointer must equal the
				// bucket whose key is ours, and the value must not
				// overlap the next element's state word.
				size_t idx = map.get_index_of_value(p);
				if (idx == (size_t)-1 ||
				    memcmp(map.get_key(idx), key.data(), ks) != 0 ||
				    map.get_value(idx) != p) {
					printf("ks %zu vs %zu: value/index mismatch\n",
					       ks, vs);
					bad++;
				}
				if (map.get_value(idx) + vs >
				    (uint8_t *)map.get_key(idx + 1 < map.bucket_count() ? idx + 1 : idx) - 4 &&
				    idx + 1 < map.bucket_count()) {
					printf("ks %zu vs %zu: value overruns element\n",
					       ks, vs);
					bad++;
				}
				// 8-byte atomic on every aligned value.
				if (vs >= 8)
					__atomic_fetch_add((uint64_t *)p, 1,
							   __ATOMIC_SEQ_CST);
				else if (vs >= 4)
					__atomic_fetch_add((uint32_t *)p, 1,
							   __ATOMIC_SEQ_CST);
			}
			// The whole value is readable/writable: write and read
			// back the last byte of every value.
			for (size_t i = 0; i < n; i++) {
				memset(key.data(), 0, ks);
				memcpy(key.data(), &i, std::min(ks, sizeof(i)));
				auto *p = static_cast<uint8_t *>(
					map.elem_lookup(key.data()));
				if (!p) {
					bad++;
					continue;
				}
				p[vs - 1] = (uint8_t)i;
			}
			for (size_t i = 0; i < n; i++) {
				memset(key.data(), 0, ks);
				memcpy(key.data(), &i, std::min(ks, sizeof(i)));
				auto *p = static_cast<uint8_t *>(
					map.elem_lookup(key.data()));
				if (!p || p[vs - 1] != (uint8_t)i ||
				    memcmp(map.get_key(map.get_index_of_value(p)),
					   key.data(), ks) != 0) {
					printf("ks %zu vs %zu: key/value clobbered\n",
					       ks, vs);
					bad++;
					break;
				}
			}
			(void)base;
			(void)stride;
		}
	}
	shared_memory_object::remove(name);
	printf("layout: %d problems\n", bad);
	return bad;
}

static int degenerate()
{
	int bad = 0;
	const char *name = "fable2_exp_degen";
	shared_memory_object::remove(name);
	managed_shared_memory seg(create_only, name, 8 << 20);
	{
		bpftime_hash_map m(seg, 0, 4, 8);
		uint32_t k = 1;
		uint64_t v = 2;
		printf("max_entries 0: buckets %zu insert -> %d (kernel: EINVAL at create)\n",
		       m.bucket_count(), (int)m.elem_update(&k, &v));
	}
	{
		bpftime_hash_map m(seg, 16, 0, 8);
		uint32_t k1 = 1, k2 = 2;
		uint64_t v1 = 11, v2 = 22;
		m.elem_update(&k1, &v1);
		m.elem_update(&k2, &v2);
		auto *p = (uint64_t *)m.elem_lookup(&k1);
		printf("key_size 0: count %zu lookup(k1) -> %lu (kernel: EINVAL)\n",
		       m.get_elem_count(), p ? *p : 0);
	}
	{
		bpftime_hash_map m(seg, 16, 4, 0);
		uint32_t k1 = 1;
		printf("value_size 0: insert %d lookup %p\n",
		       (int)m.elem_update(&k1, &k1), m.elem_lookup(&k1));
	}
	shared_memory_object::remove(name);
	return bad;
}

struct shared_state {
	std::atomic<uint64_t> inserted, e2big_early, errors, waves_done;
	std::atomic<uint64_t> lk_null, lk_wrong, del_fail, cnt_bad, full_ok, churn_del_fail;
	std::atomic<uint64_t> live; // keys currently present (racy upper bound)
};

static int multiproc()
{
	const char *name = "fable2_exp_mp";
	shared_memory_object::remove(name);
	constexpr size_t MAX = 20000;
	constexpr int procs = 4, waves = 6;
	auto *st = (shared_state *)mmap(nullptr, sizeof(shared_state),
					PROT_READ | PROT_WRITE,
					MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	new (st) shared_state{};
	void *parent_addr;
	{
		managed_shared_memory seg(create_only, name, 64 << 20);
		bpftime_hash_map *map = seg.construct<bpftime_hash_map>("m")(
			seg, MAX, 4, 8);
		parent_addr = seg.get_address();
		(void)map;
	}
	// Barrier words in shared memory.
	auto *barrier = (std::atomic<int> *)mmap(nullptr, 4096,
						 PROT_READ | PROT_WRITE,
						 MAP_SHARED | MAP_ANONYMOUS, -1,
						 0);
	for (int i = 0; i < 64; i++)
		new (&barrier[i]) std::atomic<int>(0);
	std::vector<pid_t> pids;
	for (int p = 0; p < procs; p++) {
		pid_t pid = fork();
		if (pid == 0) {
			// Shift the mapping so each process sees another address.
			std::vector<void *> pads;
			for (int i = 0; i <= p; i++)
				pads.push_back(mmap(nullptr, (i + 1) * 3 << 20,
						    PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS,
						    -1, 0));
			managed_shared_memory seg(open_only, name);
			auto *map = seg.find<bpftime_hash_map>("m").first;
			if (seg.get_address() == parent_addr && p > 0)
				st->errors += 0; // informational only
			for (int w = 0; w < waves; w++) {
				// Everyone inserts its own keys until E2BIG.
				std::vector<uint32_t> mine;
				for (uint32_t i = 0;; i++) {
					uint32_t k = (i * procs + p) | 0x100000 |
						     (w << 24);
					uint64_t v = k;
					if (!map->elem_update(&k, &v)) {
						size_t pool = map->pool_tokens(), strip = 0;
						for (size_t s = 0; s < map->stripe_count(); s++)
							strip += map->stripe_tokens(s);
						bool retry = map->elem_update(&k, &v);
						fprintf(stderr, "p%d w%d: full at i=%u pool %zu stripes %zu retry %d maxdisp %zu\n", p, w, i, pool, strip, (int)retry, map->max_displacement());
						if (retry) { mine.push_back(k); st->inserted++; continue; }
						break;
					}
					mine.push_back(k);
					st->inserted++;
				}
				// Barrier 1: all full.
				barrier[w * 2]++;
				while (barrier[w * 2].load() < procs)
					usleep(100);
				if (p == 0) {
					size_t c = map->get_elem_count();
					if (c != MAX || st->inserted.load() != MAX) {
						fprintf(stderr, "wave %d: count %zu inserted %lu "
						       "(expected %zu)\n",
						       w, c, st->inserted.load(), MAX);
						st->errors++; st->cnt_bad++;
					}
					uint32_t extra = 0xfffffff0u + w;
					uint64_t v = 1;
					if (map->elem_update(&extra, &v)) {
						fprintf(stderr, "wave %d: insert into full map succeeded\n",
						       w);
						st->errors++; st->full_ok++;
					}
				}
				// Delete my keys (half in an interleaved order so
				// other processes' inserts in the next wave race
				// deletes... all of them here, every key checked).
				for (uint32_t k : mine) {
					auto *pv = (uint64_t *)map->elem_lookup(&k);
					if (!pv) { st->errors++; st->lk_null++; fprintf(stderr, "p%d w%d: key %x present but lookup null\n", p, w, k); }
					else if (*pv != k) { st->errors++; st->lk_wrong++; }
					if (!map->elem_delete(&k)) { st->errors++; st->del_fail++; fprintf(stderr, "p%d w%d: key %x delete failed\n", p, w, k); }
					st->inserted--;
				}
				barrier[w * 2 + 1]++;
				while (barrier[w * 2 + 1].load() < procs)
					usleep(100);
				if (p == 0 && map->get_elem_count() != 0) {
					fprintf(stderr, "wave %d: count after deletes %zu\n", w,
					       map->get_elem_count());
					st->errors++; st->cnt_bad++;
				}
			}
			// Churn phase: every process inserts and deletes at random
			// while total stays below MAX: no E2BIG may occur.
			{
				std::vector<uint32_t> mine;
				uint64_t seed = p * 7919 + 1;
				auto rnd = [&] {
					seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
					return seed >> 33;
				};
				std::vector<uint8_t> present(MAX / procs / 2, 0);
				for (int it = 0; it < 400000; it++) {
					uint32_t i = rnd() % present.size();
					uint32_t k = (i * procs + p) | 0x800000;
					if (present[i]) {
						if (!map->elem_delete(&k)) { st->errors++; st->churn_del_fail++; fprintf(stderr, "p%d churn: key %x delete failed\n", p, k); }
						present[i] = 0;
					} else {
						uint64_t v = k;
						if (!map->elem_update(&k, &v)) {
							st->e2big_early++;
						} else
							present[i] = 1;
					}
				}
				for (uint32_t i = 0; i < present.size(); i++)
					if (present[i]) {
						uint32_t k = (i * procs + p) | 0x800000;
						if (!map->elem_delete(&k)) { st->errors++; st->churn_del_fail++; fprintf(stderr, "p%d churn-end: key %x delete failed\n", p, k); }
					}
			}
			_exit(0);
		}
		pids.push_back(pid);
	}
	int rc = 0;
	for (pid_t pid : pids) {
		int status;
		waitpid(pid, &status, 0);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
			rc++;
	}
	managed_shared_memory seg(open_only, name);
	auto *map = seg.find<bpftime_hash_map>("m").first;
	size_t tokens = map->pool_tokens();
	for (size_t s = 0; s < map->stripe_count(); s++)
		tokens += map->stripe_tokens(s);
	fprintf(stderr, "lk_null %lu lk_wrong %lu del_fail %lu cnt_bad %lu full_ok %lu churn_del_fail %lu\n", st->lk_null.load(), st->lk_wrong.load(), st->del_fail.load(), st->cnt_bad.load(), st->full_ok.load(), st->churn_del_fail.load());
	printf("multiproc: child failures %d errors %lu e2big_while_below_max %lu "
	       "final count %zu tokens %zu (expect %zu)\n",
	       rc, st->errors.load(), st->e2big_early.load(),
	       map->get_elem_count(), tokens, MAX);
	int bad = rc + (int)st->errors.load() + (int)st->e2big_early.load() +
		  (map->get_elem_count() != 0) + (tokens != MAX);
	shared_memory_object::remove(name);
	return bad;
}

int main(int argc, char **argv)
{
	const char *which = argc > 1 ? argv[1] : "all";
	int bad = 0;
	if (!strcmp(which, "layout") || !strcmp(which, "all"))
		bad += layout();
	if (!strcmp(which, "degen") || !strcmp(which, "all"))
		bad += degenerate();
	if (!strcmp(which, "mp") || !strcmp(which, "all"))
		bad += multiproc();
	printf("total problems %d\n", bad);
	return bad ? 2 : 0;
}
