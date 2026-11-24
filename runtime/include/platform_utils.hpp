#ifndef PLATFORM_UTIL_H
#define PLATFORM_UTIL_H
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <string_view>
#include <vector>

#if __linux__
#include <sched.h>
#elif __APPLE__
#include <sys/sysctl.h>
#include <pthread.h>
    typedef int cpu_set_t;

    inline void CPU_ZERO(cpu_set_t *set) {
        *set = 0;
    }

    inline void CPU_SET(int cpu, cpu_set_t *set) {
        *set |= (1 << cpu);
    }

    inline int CPU_ISSET(int cpu, const cpu_set_t *set) {
        return (*set & (1 << cpu)) != 0;
    }
    int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask);
    int sched_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t *mask);
#else
    #error "Unsupported platform"
#endif

int my_sched_getcpu();
// Possible CPUs (/sys/devices/system/cpu/possible): CPUs can come online
// after a map is created. The mask may have holes ("0,2-3").
// The possible CPU numbers, ascending.
const std::vector<int> &bpftime_possible_cpu_ids();
// How many there are, as libbpf counts them (libbpf_num_possible_cpus): the
// number of per-CPU values in the bpf() syscall layout.
int bpftime_num_possible_cpus();
// Highest possible CPU number + 1: the bound of sched_getcpu() results, the
// size of per-CPU arrays indexed by CPU number.
int bpftime_cpu_index_limit();
// "0-5", "0,2-3\n" -> CPU numbers; false if malformed or empty.
bool bpftime_parse_cpu_list(const char *text, std::vector<int> &ids);
// Tests: use this list instead of the system's.
void bpftime_set_possible_cpus_for_test(const std::vector<int> &ids);

namespace bpftime::rootless
{
// Use the tracefs mount path as the stable ID input. The debugfs alias must
// resolve to the same ID; otherwise the attach table cannot find that event.
inline int32_t synthetic_tracepoint_id(std::string_view id_path) noexcept
{
	constexpr std::string_view debugfs_prefix =
		"/sys/kernel/debug/tracing/events/";
	constexpr std::string_view tracefs_prefix =
		"/sys/kernel/tracing/events/";
	uint32_t hash = 2166136261U;
	const auto feed = [&](std::string_view path) {
		for (unsigned char ch : path) {
			hash ^= ch;
			hash *= 16777619U;
		}
	};
	if (id_path.starts_with(debugfs_prefix)) {
		feed(tracefs_prefix);
		id_path.remove_prefix(debugfs_prefix.size());
	}
	feed(id_path);
	return static_cast<int32_t>(1024U + hash % 100000000U);
}
} // namespace bpftime::rootless
#endif
