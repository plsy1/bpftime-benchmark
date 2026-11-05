// The memcmp key path (8- and 16-byte keys) under update-in-place storms:
// two keys of one bucket updated at once by two threads, three readers; a
// lookup of either key must never miss, and copies must be whole and the
// key's own. (The unit test covers 4-byte keys only.) Also: a third key of
// the bucket deleted and re-inserted by a fourth thread, so the chain is
// relinked and elements pass through the free lists meanwhile.
// usage: replace_long_keys <key_size 8|16> <seconds>
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
	const size_t KS = argc > 1 ? atoi(argv[1]) : 8;
	const int seconds = argc > 2 ? atoi(argv[2]) : 2;
	shared_memory_object::remove("hm_replace_long");
	managed_shared_memory seg(create_only, "hm_replace_long", 16 << 20);
	bpftime_hash_map map(seg, 64, KS, 24);
	// Three keys of one bucket; key bytes differ only in their last word so
	// that memcmp has to look past the first 8 bytes for 16-byte keys.
	std::vector<std::vector<uint8_t>> keys;
	auto make = [&](uint64_t n) {
		std::vector<uint8_t> k(KS, 0);
		if (KS == 16) {
			const uint64_t fixed = 0x1122334455667788ULL;
			std::memcpy(k.data(), &fixed, 8);
			std::memcpy(k.data() + 8, &n, 8);
		} else
			std::memcpy(k.data(), &n, 8);
		return k;
	};
	keys.push_back(make(1));
	for (uint64_t n = 2; keys.size() < 3; n++) {
		auto k = make(n);
		if (map.bucket_of_key(k.data()) == map.bucket_of_key(keys[0].data()))
			keys.push_back(k);
	}
	for (int i = 0; i < 2; i++) {
		const uint64_t init[3] = { (uint64_t)i, 0, 0 };
		if (!map.elem_update(keys[i].data(), init))
			return 1;
	}
	std::atomic<bool> stop{ false };
	std::atomic<uint64_t> misses{ 0 }, torn{ 0 }, reads{ 0 }, churn{ 0 };
	std::vector<std::thread> threads;
	for (int w = 0; w < 2; w++)
		threads.emplace_back([&, w] {
			for (uint64_t x = 1; !stop.load(std::memory_order_relaxed); x++) {
				const uint64_t value[3] = { (uint64_t)w, x, x };
				map.elem_update(keys[w].data(), value);
			}
		});
	threads.emplace_back([&] {
		uint64_t n = 0;
		const uint64_t value[3] = { 2, 5, 5 };
		while (!stop.load(std::memory_order_relaxed)) {
			map.elem_update(keys[2].data(), value);
			map.elem_delete(keys[2].data());
			n++;
		}
		churn = n;
	});
	for (int r = 0; r < 3; r++)
		threads.emplace_back([&] {
			uint64_t miss = 0, bad = 0, n = 0;
			while (!stop.load(std::memory_order_relaxed)) {
				for (int i = 0; i < 2; i++) {
					uint64_t k = 0, a = 0, b = 0;
					const void *p = map.lookup_read(keys[i].data(), [&](const void *v) {
						auto *q = static_cast<uint64_t *>(const_cast<void *>(v));
						k = std::atomic_ref<uint64_t>(q[0]).load(std::memory_order_acquire);
						a = std::atomic_ref<uint64_t>(q[1]).load(std::memory_order_acquire);
						b = std::atomic_ref<uint64_t>(q[2]).load(std::memory_order_acquire);
					});
					n++;
					if (p == nullptr)
						miss++;
					else if (k != (uint64_t)i || a != b)
						bad++;
				}
			}
			misses += miss;
			torn += bad;
			reads += n;
		});
	std::this_thread::sleep_for(std::chrono::seconds(seconds));
	stop = true;
	for (auto &t : threads)
		t.join();
	printf("key_size %zu: reads %lu misses %lu torn %lu churn %lu count %zu %s\n", KS, reads.load(),
	       misses.load(), torn.load(), churn.load(), map.get_elem_count(),
	       misses == 0 && torn == 0 && map.get_elem_count() == 2 ? "PASS" : "FAIL");
	shared_memory_object::remove("hm_replace_long");
	return misses == 0 && torn == 0 ? 0 : 1;
}
