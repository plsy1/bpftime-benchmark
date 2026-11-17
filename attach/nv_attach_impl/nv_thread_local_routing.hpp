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

// Sum maps (1531): a lookup with a constant map fd, in any probe function,
// becomes the calling thread's shard address of the key (as the
// trampoline's sum_array_offset computes it), with the map's geometry and
// device copy as constants (a map without a device copy in this process
// keeps the trampoline's lookup). A 64-bit generic add whose address
// register only such lookups define is emitted on the global state space
// (cvta.to.global, atom/red .global): the copy is device memory from cuMemAlloc ("Allocates
// device memory"), which the CUDA programming guide calls global memory, and
// a generic address outside the const, local and shared windows maps to
// global memory (PTX ISA 6.4.1.1). ptxas then emits RED for an add whose
// old value is unused, where the generic atom became an ATOM.E whose
// returned predicate the thread waited for (results
// gpu/probe-cost-20261117, section 7).
struct sum_map_desc {
	int fd;
	uint64_t base; // device copy, 0 if none
	uint64_t value_size;
	uint64_t max_entries;
	uint64_t shards; // (SM slots, a power of two) x 32 lanes
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
	int sum_lookups = 0;	   // constant-fd 1531 lookups made inline
	int global_atomics = 0;	   // atomics on such a lookup's address
};

// Throws std::runtime_error when the PTX does not have the expected shape;
// the caller then loads the original image. With no thread-local map the
// kernels are left as they are (no area) and every probe function counts
// as not routed.
std::string route_thread_local_maps(const std::string &ptx,
				    const std::vector<std::string> &kernels,
				    const std::vector<thread_local_map_desc> &maps,
				    uint64_t area_bytes,
				    thread_local_routing_stats *stats,
				    const std::vector<sum_map_desc> &sum_maps = {});
} // namespace attach
} // namespace bpftime
#endif
