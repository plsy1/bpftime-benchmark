#ifndef _BPFTIME_NV_THREAD_LOCAL_ROUTING_HPP
#define _BPFTIME_NV_THREAD_LOCAL_ROUTING_HPP
// Thread-local maps (1530, prototype proto/map-model): every probed kernel
// gets a kernel-scope .local area, zeroed when the kernel starts, whose
// generic address (cvta.local) is passed to each of its probe functions as
// their one parameter. In a probe function, a lookup of a thread-local map
// with a constant map fd becomes the address itself (key bounds checked);
// every other map helper call (1, 2, 3) gets the address as its last
// argument (eBPF r5), which the trampoline uses for thread-local maps. Probe
// functions no routed kernel calls (memcapture) pass 0 there: their
// thread-local lookups return NULL.
//
// Correctness rests on: .local is the thread's own storage for the kernel's
// run (PTX ISA 5.1.5; CUDA programming guide: local memory, scope thread,
// lifetime kernel), a kernel-scope .local lives in the kernel's frame, and
// call arguments may be registers (PTX ISA 7.1). Measured: step0b C1.
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace bpftime
{
namespace attach
{
// The area holds every thread-local map of the process and lives in every
// probed kernel's frame: at most this many bytes (one zeroing store per 8).
constexpr uint64_t THREAD_LOCAL_AREA_MAX = 1024;

struct thread_local_map_desc {
	int fd;
	uint64_t offset; // in the area
	uint64_t value_size;
	uint64_t max_entries;
};

struct thread_local_routing_stats {
	int kernels = 0;
	int call_sites = 0;
	int routed_functions = 0;
	int unrouted_functions = 0;
	int direct_lookups = 0;
	int helper_calls_given_area = 0;
	int helper_calls_given_zero = 0;
	int guarded_atomics = 0;
};

// Throws std::runtime_error when the PTX does not have the expected shape;
// the caller then loads the original image.
std::string route_thread_local_maps(const std::string &ptx,
				    const std::vector<std::string> &kernels,
				    const std::vector<thread_local_map_desc> &maps,
				    uint64_t area_bytes,
				    thread_local_routing_stats *stats);
} // namespace attach
} // namespace bpftime
#endif
