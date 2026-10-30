#include <boost/interprocess/managed_shared_memory.hpp>
#include <bpf_map/userspace/per_cpu_hash_map.hpp>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>
using namespace boost::interprocess; using namespace bpftime;
int main() {
	std::string name = "b4mut_" + std::to_string(getpid());
	shared_memory_object::remove(name.c_str());
	managed_shared_memory segment(create_only, name.c_str(), 4 << 20);
	per_cpu_hash_map_impl map(segment, 8, 64, 64);
	uint64_t key = 1; std::vector<uint8_t> value(64, 0);
	map.elem_update(&key, value.data(), 0);
	auto *p = (uint8_t *)map.elem_lookup(&key);
	map.elem_delete(&key);
	std::vector<uint8_t *> blocks;
	for (int i = 0; i < 64; i++) { auto *m = (uint8_t *)segment.allocate(64 * 6); memset(m, 0x11, 64 * 6); blocks.push_back(m); }
	memset(p, 0xa5, 64);
	int bad = 0;
	for (auto *m : blocks) for (int i = 0; i < 64 * 6; i++) if (m[i] != 0x11) { bad++; break; }
	printf("blocks overwritten: %d\n", bad);
	for (uint64_t k = 2; k < 40; k++) map.elem_update(&k, value.data(), 0);
	printf("sanity end: %d\n", (int)segment.check_sanity());
	shared_memory_object::remove(name.c_str());
	return 0;
}
