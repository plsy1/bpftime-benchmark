#include "bpf_map/userspace/per_cpu_array_map.cpp"
#include "platform_utils.cpp"
#include <cstdio>
using namespace boost::interprocess;
int main()
{
	const char *name = "fable2_percpu_overflow";
	shared_memory_object::remove(name);
	managed_shared_memory shm(create_only, name, 8 << 20);
	// max_entries * ncpu * value_size = 65536 * 1 * 65536 = 2^32 -> wraps to 0
	auto *m = shm.construct<bpftime::per_cpu_array_map_impl>("m")(shm, 65536u, 65536u, 1u);
	printf("constructed: raw data size (bytes) = %zu, free shm = %zu\n",
	       (size_t)(m->get_raw_data() != nullptr), shm.get_free_memory());
	uint32_t key = 1000;
	uint8_t val[8] = {1,2,3,4,5,6,7,8};
	// Writes 65536 bytes at offset 1000*65536 of an empty vector.
	printf("lookup_userspace(key=1000) -> %p (data base %p)\n",
	       m->elem_lookup_userspace(&key), (void*)m->get_raw_data());
	shared_memory_object::remove(name);
	return 0;
}
