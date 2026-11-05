// A lookup_read reader that copies the value with plain loads (memcpy) on a
// 4-byte-key map, where the lookup puts no fence between read() and its word
// re-check (the header says the reader must use acquire loads; CLAUDE.md says
// a lookup_read copy holds one value whole). Two updaters replace the key's
// 16-byte value {x, x}; three readers copy with memcpy, three with acquire
// loads; torn copies are counted per reader kind.
// usage: plain_reader <seconds> <value words (8 B each, default 2)>
#include "shim.hpp"
#include <bpf_map/bpftime_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
using namespace boost::interprocess;
using namespace bpftime;
int main(int argc, char **argv)
{
	const int seconds = argc > 1 ? atoi(argv[1]) : 2;
	const int words = argc > 2 ? atoi(argv[2]) : 2;
	shared_memory_object::remove("hm_plain_reader");
	managed_shared_memory seg(create_only, "hm_plain_reader", 16 << 20);
	bpftime_hash_map map(seg, 64, 4, 8 * words);
	const uint32_t key = 9;
	std::vector<uint64_t> init(words, 0);
	map.elem_update(&key, init.data());
	std::atomic<bool> stop{ false };
	std::atomic<uint64_t> torn_plain{ 0 }, torn_acq{ 0 }, copies_plain{ 0 }, copies_acq{ 0 };
	std::vector<std::thread> threads;
	for (int w = 0; w < 2; w++)
		threads.emplace_back([&, w] {
			for (uint64_t x = w; !stop.load(std::memory_order_relaxed); x += 2) {
				std::vector<uint64_t> value(words, x);
				map.elem_update(&key, value.data());
			}
		});
	for (int r = 0; r < 3; r++)
		threads.emplace_back([&] {
			uint64_t torn = 0, n = 0;
			while (!stop.load(std::memory_order_relaxed)) {
				std::vector<uint64_t> copy(words, 0);
				const void *p = map.lookup_read(&key, [&](const void *v) {
					std::memcpy(copy.data(), v, 8 * words); // plain loads
				});
				if (p == nullptr)
					continue;
				n++;
				for (int i = 1; i < words; i++)
					if (copy[i] != copy[0]) {
						torn++;
						break;
					}
			}
			torn_plain += torn;
			copies_plain += n;
		});
	for (int r = 0; r < 3; r++)
		threads.emplace_back([&] {
			uint64_t torn = 0, n = 0;
			while (!stop.load(std::memory_order_relaxed)) {
				std::vector<uint64_t> acq(words, 0);
				const void *p = map.lookup_read(&key, [&](const void *v) {
					auto *q = static_cast<uint64_t *>(const_cast<void *>(v));
					for (int i = 0; i < words; i++)
						acq[i] = std::atomic_ref<uint64_t>(q[i]).load(std::memory_order_acquire);
				});
				if (p == nullptr)
					continue;
				n++;
				for (int i = 1; i < words; i++)
					if (acq[i] != acq[0]) {
						torn++;
						break;
					}
			}
			torn_acq += torn;
			copies_acq += n;
		});
	std::this_thread::sleep_for(std::chrono::seconds(seconds));
	stop = true;
	for (auto &t : threads)
		t.join();
	printf("value %d B: memcpy reader: %lu copies, %lu torn; acquire reader: %lu copies, %lu torn\n", 8 * words, copies_plain.load(),
	       torn_plain.load(), copies_acq.load(), torn_acq.load());
	shared_memory_object::remove("hm_plain_reader");
	return 0;
}
