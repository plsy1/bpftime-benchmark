// Capacity waves: W workers (threads or processes) insert disjoint keys until
// the map reports full, then check count == max_entries, then delete all.
// Usage: exp_waves <threads|procs> <workers> <max_entries> <waves>
#include <bpf_map/bpftime_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>
using namespace boost::interprocess;
using namespace bpftime;

struct shared {
	std::atomic<uint64_t> inserted, errors, early_full, full_ok;
	std::atomic<int> barrier[64];
};

int main(int argc, char **argv)
{
	const bool procs = argc > 1 && !strcmp(argv[1], "procs");
	const int W = argc > 2 ? atoi(argv[2]) : 4;
	const size_t MAX = argc > 3 ? atoi(argv[3]) : 20000;
	const int waves = argc > 4 ? atoi(argv[4]) : 4;
	const char *name = "fable2_exp_waves";
	shared_memory_object::remove(name);
	auto *st = (shared *)mmap(nullptr, sizeof(shared), PROT_READ | PROT_WRITE,
				  MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	new (st) shared{};
	const bool unmap_parent = argc > 5 && !strcmp(argv[5], "unmap");
	const size_t segsize = (argc > 6 ? atoi(argv[6]) : 256) << 20;
	managed_shared_memory *segp = new managed_shared_memory(create_only, name, segsize);
	auto *pmap = segp->construct<bpftime_hash_map>("m")(*segp, MAX, 4, 8);
	printf("%s W=%d max=%zu buckets %zu stripes %zu unmap=%d seg=%zuMB\n",
	       procs ? "procs" : "threads", W, MAX, pmap->bucket_count(),
	       pmap->stripe_count(), (int)unmap_parent, segsize >> 20);
	if (unmap_parent) { delete segp; segp = nullptr; pmap = nullptr; }
	auto worker = [&](bpftime_hash_map *map, int p) {
		for (int w = 0; w < waves; w++) {
			std::vector<uint32_t> mine;
			for (uint32_t i = 0;; i++) {
				uint32_t k = (i * W + p) | 0x100000 | (w << 24);
				uint64_t v = k;
				if (!map->elem_update(&k, &v))
					break;
				mine.push_back(k);
				st->inserted++;
			}
			st->barrier[w * 2]++;
			while (st->barrier[w * 2].load() < W)
				usleep(100);
			if (p == 0) {
				size_t c = map->get_elem_count();
				size_t pool = map->pool_tokens(), strip = 0;
				for (size_t s = 0; s < map->stripe_count(); s++)
					strip += map->stripe_tokens(s);
				if (c != MAX || st->inserted.load() != MAX) {
					fprintf(stderr,
						"wave %d: count %zu inserted %lu pool %zu stripes %zu (expected %zu)\n",
						w, c, st->inserted.load(), pool, strip,
						MAX);
					st->errors++;
					st->early_full++;
				}
				uint32_t extra = 0xfffffff0u + w;
				uint64_t v = 1;
				if (map->elem_update(&extra, &v)) {
					st->full_ok++;
					map->elem_delete(&extra);
				}
			}
			st->barrier[w * 2 + 1]++;
			while (st->barrier[w * 2 + 1].load() < W)
				usleep(100);
			for (uint32_t k : mine) {
				auto *pv = (uint64_t *)map->elem_lookup(&k);
				if (!pv || *pv != k) {
					fprintf(stderr, "p%d w%d: key %x lookup bad\n", p,
						w, k);
					st->errors++;
				}
				if (!map->elem_delete(&k)) {
					fprintf(stderr, "p%d w%d: key %x delete failed\n",
						p, w, k);
					st->errors++;
				}
				st->inserted--;
			}
			st->barrier[32 + w]++;
			while (st->barrier[32 + w].load() < W)
				usleep(100);
			if (p == 0 && map->get_elem_count() != 0) {
				fprintf(stderr, "wave %d: count after deletes %zu\n", w,
					map->get_elem_count());
				st->errors++;
			}
			// Barrier before the next wave so p0's checks finish.
			st->barrier[48 + w]++;
			while (st->barrier[48 + w].load() < W)
				usleep(100);
		}
	};
	if (procs) {
		std::vector<pid_t> pids;
		for (int p = 0; p < W; p++) {
			pid_t pid = fork();
			if (pid == 0) {
				std::vector<void *> pads;
				for (int i = 0; i <= p; i++)
					pads.push_back(mmap(nullptr, (i + 1) * 3 << 20,
							    PROT_READ,
							    MAP_PRIVATE | MAP_ANONYMOUS, -1,
							    0));
				managed_shared_memory cseg(open_only, name);
				auto *map = cseg.find<bpftime_hash_map>("m").first;
				fprintf(stderr, "p%d segment at %p\n", p,
					cseg.get_address());
				worker(map, p);
				_exit(0);
			}
			pids.push_back(pid);
		}
		for (pid_t pid : pids) {
			int status;
			waitpid(pid, &status, 0);
		}
	} else {
		std::vector<std::thread> th;
		for (int p = 0; p < W; p++)
			th.emplace_back(worker, pmap, p);
		for (auto &t : th)
			t.join();
	}
	if (!segp) { segp = new managed_shared_memory(open_only, name); pmap = segp->find<bpftime_hash_map>("m").first; }
	size_t tokens = pmap->pool_tokens();
	for (size_t s = 0; s < pmap->stripe_count(); s++)
		tokens += pmap->stripe_tokens(s);
	printf("errors %lu early_full %lu full_insert_ok %lu final count %zu tokens %zu/%zu\n",
	       st->errors.load(), st->early_full.load(), st->full_ok.load(),
	       pmap->get_elem_count(), tokens, MAX);
	shared_memory_object::remove(name);
	return st->errors ? 2 : 0;
}
