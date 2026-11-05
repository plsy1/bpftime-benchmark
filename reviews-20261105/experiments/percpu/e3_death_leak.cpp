// E3: a process killed between acquire_slot and the table insert (here:
// blocked on the stripe lock; BPF_NOEXIST skips with_existing_value, so the
// slot is taken before the stripe lock) loses its slot. After more than ncpu such
// deaths the slot pool, not the key table, bounds the map: E2BIG below
// max_entries. The table elements are not lost (the child died before
// take_element), so this is a slot-pool-only leak.
#include <bpf_map/userspace/per_cpu_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <linux/bpf.h>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace boost::interprocess;
using namespace bpftime;

int main()
{
	const std::string name = "e3_leak_" + std::to_string(getpid());
	shared_memory_object::remove(name.c_str());
	int rc = 0;
	{
		managed_shared_memory seg(create_only, name.c_str(), 8 << 20);
		constexpr uint32_t max_entries = 64;
		const int ncpu = bpftime_cpu_index_limit();
		auto *map = seg.construct<per_cpu_hash_map_impl>(
			anonymous_instance)(seg, 4, 8, max_entries);
		auto &table = map->table_for_test();
		std::vector<uint8_t> buf(8 * bpftime_num_possible_cpus(), 1);
		const uint32_t probe = 1000;
		const size_t stripe = table.stripe_of_key(&probe);
		const int deaths = ncpu + 3;
		for (int i = 0; i < deaths; i++) {
			table.lock_stripe_for_test(stripe);
			fflush(nullptr);
			pid_t pid = fork();
			if (pid == 0) {
				// Takes a slot, then blocks on the stripe lock.
				const uint32_t k = probe;
				map->elem_update_userspace(&k, buf.data(), BPF_NOEXIST); // NOEXIST: acquire_slot first, then the stripe lock
				_exit(0);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(150));
			kill(pid, SIGKILL);
			int status;
			waitpid(pid, &status, 0);
			table.unlock_stripe_for_test(stripe);
		}
		// Now insert distinct keys until failure.
		uint32_t inserted = 0;
		for (uint32_t k = 1; k <= max_entries; k++) {
			if (map->elem_update_userspace(&k, buf.data(), BPF_NOEXIST) != 0) {
				printf("insert %u failed: errno %d %s\n", k, errno,
				       strerror(errno));
				break;
			}
			inserted++;
		}
		printf("ncpu %d, %d children killed between slot and insert, max_entries %u, inserted %u (slot capacity %u - %d leaked = %u)\n",
		       ncpu, deaths, max_entries, inserted, max_entries + ncpu,
		       deaths, max_entries + ncpu - deaths);
		rc = inserted < max_entries ? 1 : 0;
		seg.destroy_ptr(map);
	}
	shared_memory_object::remove(name.c_str());
	return rc;
}
