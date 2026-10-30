// Value alignment of bpftime_hash_map elements: 8-byte atomics on values.
#include <bpf_map/bpftime_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <cstdio>
#include <cstdlib>
#include <map>
using namespace boost::interprocess;
using namespace bpftime;

int main(int argc, char **argv)
{
	const size_t key_size = argc > 1 ? atoi(argv[1]) : 8;
	const size_t value_size = argc > 2 ? atoi(argv[2]) : 8;
	const bool do_atomic = argc > 3;
	const char *name = "FABLE_ALIGN_HASH";
	shared_memory_object::remove(name);
	managed_shared_memory segment(create_only, name, 16 << 20);
	bpftime_hash_map map(segment, 1024, key_size, value_size);
	std::map<int, int> align_hist;
	int crossing = 0, inserted = 0;
	for (uint64_t k = 1; k < 2000 && inserted < 1024; k++) {
		uint8_t key[16] = {};
		memcpy(key, &k, key_size < 16 ? key_size : 16);
		uint64_t v = 0;
		uint8_t val[64] = {};
		if (!map.elem_update(key, val))
			continue;
		inserted++;
		auto *p = map.elem_lookup(key);
		uintptr_t a = (uintptr_t)p;
		align_hist[(int)(a % 16)]++;
		if (a % 16 > 8)
			crossing++;
	}
	printf("key=%zu value=%zu elem=%zu data%%16=%zu inserted=%d\n", key_size,
	       value_size, (4 + key_size + value_size + 3) & ~size_t{ 3 },
	       (uintptr_t)map.get_value(0) - key_size - 4 - (uintptr_t)0, inserted);
	for (auto &[m, n] : align_hist)
		printf("  value addr %% 16 == %2d: %d buckets\n", m, n);
	printf("  8-byte values crossing a 16-byte line: %d of %d\n", crossing,
	       inserted);
	if (do_atomic) {
		for (uint64_t k = 1; k < 2000; k++) {
			uint8_t key[16] = {};
			memcpy(key, &k, key_size < 16 ? key_size : 16);
			auto *p = static_cast<uint64_t *>(map.elem_lookup(key));
			if (!p)
				continue;
			if ((uintptr_t)p % 16 > 8) {
				printf("atomic add on %p (%% 16 = %zu)...\n", (void *)p,
				       (uintptr_t)p % 16);
				fflush(stdout);
				__atomic_fetch_add(p, 1, __ATOMIC_SEQ_CST);
				printf("survived\n");
				break;
			}
		}
	}
	shared_memory_object::remove(name);
	return 0;
}
