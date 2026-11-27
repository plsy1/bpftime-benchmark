#ifndef _NV_GPU_DEVICE_HASH_MAP_HPP
#define _NV_GPU_DEVICE_HASH_MAP_HPP
// The device hash map (1533, BPF_MAP_TYPE_GPU_DEVICE_HASH_MAP; prototype,
// plan results gpu/hash-build-plan-20261127). GPU probes look keys up, add
// to their values and insert them with BPF_NOEXIST without host calls; the
// device copy is a key directory (open addressing) and sharded rows of u64
// values (trampoline/gpu_devhash_layout.h). This object is the CPU's side,
// in the shared segment:
//
// Phases (PHASE, plan D7):
//   0 CPU      bpf hash semantics on a CPU table (a loader may prefill it)
//   1 SEALING  the agent that owns the map is building the device copy:
//              writes get EPERM, lookups still read the CPU table
//   2 DEVICE   the device copy is the map: writes get EPERM, lookups and
//              get_next_key read the mirror the agent's delivery keeps
// Every read path loads PHASE with acquire first.
//
// The seal (plan D8) excludes CPU writers in flight without a lock a dead
// process could hold: a writer increments INFLIGHT and then loads PHASE, the
// sealer stores PHASE and then polls INFLIGHT, all four seq_cst, so one of
// them sees the other. A poll that runs out (1 s) stores SEAL_DEAD; a later
// attempt seals again only once INFLIGHT reads 0.
//
// The mirror (plan D10): mhdr[slots] and mval[slots], indexed by slot as the
// device directory. Every non-empty slot's probe path from its home is
// non-empty: a delivery takes the mirror's own header where the snapshot
// read a non-empty slot as empty (guard), marks the empty slots between a
// key's home and its slot opaque (repair), and stores values, then the
// occupancy of new slots, a release fence, then the changed headers in
// ascending slot order. A reader that acquires a header sees its value and
// every slot the same delivery filled. One writer at a time: the seeding
// thread before the map is registered for delivery, then deliveries under
// the delivery's run mutex.
#include "bpf_map/map_common_def.hpp"
#include "bpf_map/userspace/fix_hash_map.hpp"
#include "trampoline/gpu_devhash_layout.h"
#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace bpftime
{
// Status words (plan D12): 0-5 as 1531's, then the device's loss counts,
// the reservation count, live and refused slots.
enum : uint64_t {
	DEVHASH_STATUS_DELIVERIES = 0,
	DEVHASH_STATUS_EXIT_DELIVERY = 1,
	DEVHASH_STATUS_OWNER_PID = 2,
	DEVHASH_STATUS_SYNCS_COVERED = 3,
	DEVHASH_STATUS_SYNCS_AT_EXIT = 4,
	DEVHASH_STATUS_LAUNCHES_AFTER_LAST_SYNC = 5,
	DEVHASH_STATUS_COUNTERS = 6, // NUM_COUNTERS words
	DEVHASH_STATUS_COUNT = 13,
	DEVHASH_STATUS_LIVE = 14,
	DEVHASH_STATUS_REFUSED = 15,
	DEVHASH_STATUS_WORDS = 16,
};
static_assert(uint64_t(DEVHASH_STATUS_COUNTERS) +
		      uint64_t(bpftime_devhash::NUM_COUNTERS) ==
		      DEVHASH_STATUS_COUNT,
	      "status words 6-12 are the counters");

enum : uint64_t {
	DEVHASH_PHASE_CPU = 0,
	DEVHASH_PHASE_SEALING = 1,
	DEVHASH_PHASE_DEVICE = 2,
};

// What a device copy starts from: a header and a value per slot (the value
// of a live slot goes into shard 0 of its row), the reservation count and
// the loss counters.
struct devhash_image {
	std::vector<uint64_t> headers;
	std::vector<uint64_t> values;
	uint64_t count = 0;
	std::array<uint64_t, bpftime_devhash::NUM_COUNTERS> counters{};
};

// The path an owner takes (plan D11).
enum class devhash_path {
	not_owner, // another live owner, or one in another pid namespace
	have_copy, // this process made the copy already
	seal_dead, // a seal timed out and INFLIGHT has not reached 0 since
	first_copy, // PHASE 0
	recover_sealing, // PHASE 1: a sealer died
	takeover, // PHASE 2: the owner died
};

// Owner record: pid-namespace id (low 32 bits) << 32 | pid.
uint64_t devhash_owner_record(uint64_t pid_namespace, uint32_t pid);
// Decides from what the caller read (pure; tests drive it directly).
//   owner, self    owner records (owner 0: none)
//   owner_alive    the recorded pid's process exists and is no zombie
devhash_path devhash_choose_path(uint64_t phase, bool seal_dead,
				 uint64_t inflight, uint64_t owner,
				 uint64_t self, bool has_cached_copy,
				 bool owner_alive);

// Test hooks (always compiled; null by default). Called with the point and
// an argument (the slot for after_pass_c_slot).
enum class devhash_hook_point {
	writer_after_inflight_inc,
	writer_after_phase_load,
	delivery_before_pass_a,
	delivery_after_pass_c_slot,
};
using devhash_hook_fn = void (*)(devhash_hook_point, uint64_t);
void set_devhash_hook_for_test(devhash_hook_fn fn);

class nv_gpu_device_hash_map_impl {
	fix_size_hash_map_impl table; // phase 0
	bytes_vec mirror_headers; // u64 per slot
	bytes_vec mirror_values; // u64 per slot
	bytes_vec status_buffer; // DEVHASH_STATUS_WORDS u64
	uint64_t max_entries;
	uint64_t slots;
	uint32_t log_slots;
	// shm words, accessed atomically
	uint64_t phase = DEVHASH_PHASE_CPU;
	uint64_t inflight = 0;
	uint64_t seal_dead = 0;
	uint64_t owner = 0;
	// random per map: a process's copy cache keys on (object, generation)
	uint64_t generation;
	// the companion status array's handler index, -1 when unbound
	int64_t companion = -1;

	uint64_t *mhdr()
	{
		return (uint64_t *)mirror_headers.data();
	}
	uint64_t *mval()
	{
		return (uint64_t *)mirror_values.data();
	}
	// the slot holding live key, or -1 (mirror; phase 2)
	int64_t mirror_find(uint32_t key);

    public:
	const static bool should_lock = false;
	nv_gpu_device_hash_map_impl(
		boost::interprocess::managed_shared_memory &memory,
		uint64_t max_entries);

	void *elem_lookup(const void *key);
	long elem_update(const void *key, const void *value, uint64_t flags);
	long elem_delete(const void *key);
	int map_get_next_key(const void *key, void *next_key);

	// ---- the agent's side (host code; the CUDA part is the caller's)

	// Claims the map for self (OWNER CAS) and returns the path to take.
	devhash_path claim(uint64_t self, bool has_cached_copy,
			   const std::function<bool(uint32_t pid)> &alive);
	// Gives the claim back (a failed copy, plan D15).
	void release_claim(uint64_t self);
	// PHASE = 1 and waits (bounded) for writers in flight. On a timeout:
	// PHASE 0, SEAL_DEAD, false. With SEAL_DEAD set it seals only if
	// INFLIGHT reads 0 now (and clears it), else returns false at once.
	bool seal(uint64_t bound_ns = 1000000000ull);
	// The first copy's image from the CPU table, frozen by a seal (PHASE
	// >= 1): keys in scan order, rows 0..n-1.
	bool build_image(devhash_image &out);
	// The first copy (PHASE 0): seal, then the image. False: no copy
	// (SEAL_DEAD, or the image failed; PHASE is 0 again).
	bool seal_and_build(devhash_image &out,
			    uint64_t bound_ns = 1000000000ull);
	// A sealer died (PHASE 1): writers get EPERM already, but one that
	// read PHASE 0 before its store may still run; empty the mirror, poll
	// as the seal does, then the image.
	bool recover_sealing(devhash_image &out,
			     uint64_t bound_ns = 1000000000ull);
	// The takeover's image from the mirror (plan D11e).
	devhash_image takeover_image();
	// Writes a first copy's image into the empty mirror (before the map is
	// registered for delivery).
	void seed_mirror(const devhash_image &image);
	// Empties the mirror (first copy failed, or a sealer died; no reader
	// reads it outside phase 2).
	void reset_mirror();
	// Phase changes of the agent: PHASE = 2 (release) after the mirror is
	// seeded, PHASE = 0 after a failed first copy.
	void publish_device_phase();
	void unseal();
	// One delivery: staging as gpu_devhash_layout.h's staging_for. Writes
	// the mirror, the loss counts, count, live and refused; the caller
	// then stores its sync words and calls finish_delivery.
	void apply_delivery(const uint8_t *staging);
	void finish_delivery();

	uint64_t *status_word(uint64_t i)
	{
		return (uint64_t *)status_buffer.data() + i;
	}
	uint64_t get_phase() const
	{
		return __atomic_load_n(&phase, __ATOMIC_ACQUIRE);
	}
	uint64_t get_owner() const
	{
		return __atomic_load_n(&owner, __ATOMIC_ACQUIRE);
	}
	bool is_seal_dead() const
	{
		return __atomic_load_n(&seal_dead, __ATOMIC_ACQUIRE) != 0;
	}
	uint64_t get_inflight() const
	{
		return __atomic_load_n(&inflight, __ATOMIC_SEQ_CST);
	}
	uint64_t get_generation() const
	{
		return generation;
	}
	int64_t get_companion() const
	{
		return __atomic_load_n(&companion, __ATOMIC_ACQUIRE);
	}
	void set_companion(int64_t index)
	{
		__atomic_store_n(&companion, index, __ATOMIC_RELEASE);
	}
	uint64_t get_max_entries() const
	{
		return max_entries;
	}
	uint64_t get_slots() const
	{
		return slots;
	}
	uint64_t mirror_header(uint64_t slot)
	{
		return __atomic_load_n(mhdr() + slot, __ATOMIC_ACQUIRE);
	}
	uint64_t mirror_value(uint64_t slot)
	{
		return __atomic_load_n(mval() + slot, __ATOMIC_RELAXED);
	}
	// Tests: PHASE as a sealer that died would leave it.
	void force_phase_for_test(uint64_t p)
	{
		__atomic_store_n(&phase, p, __ATOMIC_SEQ_CST);
	}
	// Tests: the CPU table (phase 0).
	fix_size_hash_map_impl &table_for_test()
	{
		return table;
	}
};
} // namespace bpftime

// The orders the seal rests on, in functions of their own so that a script
// can read them in the object code (runtime/unit-test/maps/
// check_devhash_orders.sh): the writer's INFLIGHT increment then PHASE load,
// the sealer's PHASE store then first INFLIGHT load, and the readers' PHASE
// load.
extern "C" {
uint64_t bpftime_devhash_writer_enter(uint64_t *inflight, uint64_t *phase);
uint64_t bpftime_devhash_sealer_enter(uint64_t *phase, uint64_t *inflight);
uint64_t bpftime_devhash_reader_phase(uint64_t *phase);
}

#endif
