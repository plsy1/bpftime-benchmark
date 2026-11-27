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
// The seal (plan D8) keeps CPU writers out of the device copy without a
// lock a dead process could hold. Each writing process has a slot (its
// owner record and start time, and a count of its writers in flight): a
// writer increments its slot's count and then loads PHASE; the sealer
// stores PHASE and then reads every slot's count; all seq_cst, so one side
// sees the other. A count left behind by a process that died (an exit kills
// its other threads wherever they are) is discounted once the process is
// gone. A seal that still finds a writer in flight after 1 s (a live
// process stopped or slowed) stores SEAL_DEAD and fails; a later attempt
// seals once the counts read 0.
//
// The mirror (plan D10): mhdr[slots] and mval[slots], indexed by slot as the
// device directory. Every non-empty slot's probe path from its home is
// non-empty: a delivery takes the mirror's own header where the snapshot
// read a non-empty slot as empty, keeps a live header unless the same key
// comes live there (guard), marks the empty slots between a newly seen
// key's home and its slot opaque (repair), and stores values, then the
// occupancy of new slots, a release fence, then the changed headers in
// ascending slot order. A reader that acquires a header sees its value and
// every slot the same delivery filled. One writer at a time: the seeding
// thread before the map is registered for delivery, then deliveries under
// the delivery's run mutex.
//
// The agent-side functions allocate (std::vector) and may throw
// std::bad_alloc: their callers in the agent catch and fail open (no copy,
// or no delivery; plan D15).
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
	first_copy, // PHASE 0
	recover_sealing, // PHASE 1: a sealer died
	takeover, // PHASE 2: the owner died
};

// Owner record: pid-namespace id (low 32 bits) << 32 | pid.
uint64_t devhash_owner_record(uint64_t pid_namespace, uint32_t pid);
// This process's record (its pid namespace, as the perf shards record it).
uint64_t devhash_self_record();
// The start time of a process (/proc/<pid>/stat field 22), 0 if unknown.
uint64_t devhash_process_start_time(uint32_t pid);
// Whether the process pid of this pid namespace still runs:
//   kill(pid, 0) fails with ESRCH: gone; succeeds or fails with EPERM: then
//   gone only if every task in /proc/<pid>/task is Z or X (a process whose
//   main thread exited while others run is alive; /proc unreadable: alive),
//   or if its start time differs from start (not 0): the pid was reused.
bool devhash_process_alive(uint32_t pid, uint64_t start);
// Tests: replace the three probes (null restores the real ones).
struct devhash_liveness_probes {
	int (*kill_errno)(uint32_t pid); // 0 or the errno of kill(pid, 0)
	int (*tasks_running)(uint32_t pid); // 1, 0 (all Z/X), -1 unknown
	uint64_t (*start_time)(uint32_t pid);
};
void set_devhash_liveness_probes_for_test(const devhash_liveness_probes *p);

// Decides from what the caller read (pure; tests drive it directly).
//   owner, self    owner records (owner 0: none)
//   owner_alive    the recorded owner's process runs (devhash_process_alive)
devhash_path devhash_choose_path(uint64_t phase, uint64_t owner, uint64_t self,
				 bool has_cached_copy, bool owner_alive);

// Test hooks (always compiled; null by default). Called with the point and
// an argument (the slot for after_pass_c_slot).
enum class devhash_hook_point {
	// between the writer's increment and its PHASE load
	writer_after_inflight_inc,
	writer_after_phase_load,
	// between the owner CAS and the owner's start time
	claim_after_cas,
	delivery_before_pass_a,
	delivery_after_pass_c_slot,
};
using devhash_hook_fn = void (*)(devhash_hook_point, uint64_t);
void set_devhash_hook_for_test(devhash_hook_fn fn);

class nv_gpu_device_hash_map_impl {
    public:
	// Writer slots: one per writing process, and an overflow slot shared
	// when all are taken (never discounted).
	static constexpr uint32_t WRITER_SLOTS = 64;
	struct writer_slot {
		uint64_t owner; // owner record, 0 free
		uint64_t start; // the owner's start time, 0 unknown
		uint64_t count; // its writers in flight
		uint64_t pad;
	};

    private:
	fix_size_hash_map_impl table; // phase 0
	bytes_vec mirror_headers; // u64 per slot
	bytes_vec mirror_values; // u64 per slot
	bytes_vec status_buffer; // DEVHASH_STATUS_WORDS u64
	uint64_t max_entries;
	uint64_t slots;
	uint32_t log_slots;
	// shm words, accessed atomically
	uint64_t phase = DEVHASH_PHASE_CPU;
	uint64_t seal_dead = 0;
	uint64_t owner = 0;
	// the owner's start time, and the record it was written for: a start
	// time not written for the current owner counts as unknown
	uint64_t owner_start = 0;
	uint64_t owner_start_for = 0;
	// random per map: a process's copy cache keys on (object, generation)
	uint64_t generation;
	// the companion status array's handler index, -1 when unbound
	int64_t companion = -1;
	writer_slot writers[WRITER_SLOTS + 1] = {};

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
	// this process's writer slot
	writer_slot &my_writer_slot();
	// counts of all writer slots (seq_cst loads)
	uint64_t inflight_sum();
	// frees the slots of processes that are gone, counts included
	void discount_dead_writers();

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

	using alive_fn = std::function<bool(uint32_t pid, uint64_t start)>;
	// Claims the map for self (OWNER CAS) and returns the path to take.
	devhash_path claim(uint64_t self, bool has_cached_copy,
			   const alive_fn &alive = devhash_process_alive);
	// Gives the claim back.
	void release_claim(uint64_t self);
	// The image a device copy of this process starts from: claim, then by
	// the path the seal and the CPU table's image, a dead sealer's
	// recovery, or the takeover's image from the mirror. False: no copy
	// (path not_owner or have_copy, or the seal failed); the claim is given
	// back whenever no copy follows.
	bool make_image(uint64_t self, bool has_cached_copy, devhash_image &out,
			devhash_path &path,
			const alive_fn &alive = devhash_process_alive,
			uint64_t bound_ns = 1000000000ull);
	// PHASE = 1 and waits (bounded) for writers in flight, discounting
	// processes that are gone. On a timeout: PHASE 0, SEAL_DEAD, false.
	// With SEAL_DEAD set it seals only if the counts read 0 now (and
	// clears it), else returns false at once.
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
	// writers in flight, all slots
	uint64_t get_inflight();
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
	// Tests: the writer slots.
	writer_slot *writer_slots_for_test()
	{
		return writers;
	}
};
} // namespace bpftime

// The orders the seal rests on, in functions of their own so that a script
// can read them in the object code (runtime/unit-test/maps/
// check_devhash_orders.sh): the writer's count increment then PHASE load,
// the sealer's PHASE store then its count loads, the sealer's later polls,
// and the readers' PHASE load.
extern "C" {
uint64_t bpftime_devhash_writer_enter(uint64_t *count, uint64_t *phase);
uint64_t bpftime_devhash_sealer_enter(uint64_t *phase, uint64_t *counts,
				      uint64_t n, uint64_t stride);
uint64_t bpftime_devhash_sealer_poll(uint64_t *counts, uint64_t n,
				     uint64_t stride);
uint64_t bpftime_devhash_reader_phase(uint64_t *phase);
}

#endif
