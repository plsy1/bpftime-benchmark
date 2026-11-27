#ifndef _BPFTIME_NV_THREAD_LOCAL_ROUTING_HPP
#define _BPFTIME_NV_THREAD_LOCAL_ROUTING_HPP
// Thread-local maps (1530, prototype proto/map-model): a probed kernel
// keeps the thread-local area, zeroed when the kernel starts, either by
// value (below) or as a kernel-scope .local area whose generic address
// (cvta.local) is passed to each of its probe functions as their one
// parameter. In the address form, a lookup of a thread-local map with a
// constant map fd becomes the address itself (key bounds checked), and
// every other map helper call (1, 2, 3) gets the address as its last
// argument (eBPF r5), which the trampoline uses for thread-local maps. Probe
// functions no routed kernel calls (memcapture), and probe functions by
// value, pass 0 there: the trampoline's thread-local lookups return NULL.
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

// An inline lookup (either kind) whose key the program stored into a stack
// slot just before, in the same basic block with nothing in between that
// could write the slot, takes the stored register instead of loading the
// key back through the slot's generic address (one store and one load per
// lookup otherwise; measured 0.028 ns per thread on mul_mat_vec_q, the
// three lookups of `lat`).
//
// By value (2026-11-26): a kernel whose probe functions reach the area only
// through lookups of thread-local maps (helper 1) with constant keys the
// program just stored, whose other map lookups, updates and deletes (helpers
// 1-3) name their map with a constant, and which use each lookup's result
// only in compares with 0 and in 32-bit loads and stores at 4-byte offsets
// inside the value with 64-bit registers (unguarded, each on a line of its
// own, never storing the result itself), keeps the area (one 8-byte word:
// a writing function returns it, and a device function returns at most one
// value, PTX ISA 7.1.1) in a .local slot of its own instead and passes it by
// value: a probe function that accesses the area takes it as a parameter,
// one that writes it also returns it; inside, the area is two .b32 halves, a
// lookup the non-null constant 1, a load cvt.u64.u32 and a store
// cvt.u32.u64 of the half. The rewrite works on lines, so a function with a
// block comment, a directive glued to a statement or a guard on a line of
// its own, and a kernel whose probe call shares its line, keep the address
// form. Any other form, in any probe function of the kernel, keeps the
// address form for the whole kernel (byvalue_fallbacks says why). Map calls
// with a constant fd of another map type get 0 for the area (helpers 1-3
// read it for thread-local maps only). A kernel whose probes never reach
// the area gets none. Measured on mul_mat_vec_q with `lat` (the text this pass
// emits, through the debug override): 0.021-0.024 ns per thread less than
// the area in memory, 54.3 -> 55.9 t/s on tg128.
//
// Correctness rests on, besides the above: the area is the thread's own
// (.local, PTX ISA 5.1.5) and its address never leaves the probe functions'
// rewritten accesses, so its bytes can live in registers; function input
// and return parameters are .param (5.1.6, 5.1.6.4: return parameters
// written with st.param; 7.1 call); ld/st with a register wider than the
// type zero-extend a load and store the low bits (9.4.1), as cvt.u64.u32 and
// cvt.u32.u64 do (6.5.1, zext and chop). The halves are packed and unpacked
// only by this code, so their order inside a word is never observed.
//
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

// Device hash maps (1533; plan results gpu/hash-build-plan-20261127, D1,
// S6): with a device copy (base, the block from the agent's pre-pass), a
// lookup with a constant map fd becomes the inline hit path (the key's
// home slot, one ld.relaxed.gpu.global of its header; live K: the calling
// thread's shard of its row, or the row's single word; empty: NULL;
// anything else: a call of __bpftime_devhash_lookup, which walks), an
// update the call of __bpftime_devhash_insert (its 64-bit result in the
// program's register), a delete -22. The three functions (devhash_ptx.h,
// sm_70 forms) are put once at the start of the module body, only when a
// lookup or update was rewritten. Without a copy, or with a non-constant
// fd, the trampoline keeps the call (lookup NULL, update -1, delete -22).
// Adds through a lookup's result take the global form, as for 1531.
struct hash_map_desc {
	int fd;
	uint64_t base; // device block, 0 if none
	uint64_t max_entries;
	uint64_t shards; // as 1531's
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
	int keys_forwarded = 0;	   // inline lookups given the stored key
	int byvalue_kernels = 0;   // kernels passing the area by value
	int byvalue_functions = 0; // probe functions taking it by value
	int area_unused_kernels = 0;   // kernels whose probes never reach it
	int area_unused_functions = 0; // their probe functions (no area)
	int byvalue_lookups = 0;   // lookups made a constant
	int byvalue_accesses = 0;  // accesses made register moves
	int hash_lookups = 0;	   // constant-fd 1533 lookups made inline
	int hash_inserts = 0;	   // constant-fd 1533 updates made calls
	int hash_deletes = 0;	   // constant-fd 1533 deletes made -22
	int devhash_text_inserted = 0; // the devhash functions put in
	// why a kernel kept the area in memory (function: reason)
	std::vector<std::string> byvalue_fallbacks;
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
				    const std::vector<sum_map_desc> &sum_maps = {},
				    const std::vector<hash_map_desc> &hash_maps = {});
} // namespace attach
} // namespace bpftime
#endif
