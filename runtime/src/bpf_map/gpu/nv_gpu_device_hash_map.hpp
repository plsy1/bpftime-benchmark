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
// lock a dead process could hold. Each writing process has a slot (a count
// of its writers in flight): a writer increments its slot's count and then
// loads PHASE; the sealer stores PHASE and then reads every slot's count;
// all seq_cst, so one side sees the other. A process holds its slot by an
// open file description lock (F_OFD_SETLK) on the slot's byte of the
// segment's file, which the kernel releases when the process exits, is
// killed or execs: a slot whose lock the sealer can take belongs to no live
// process, and the count its dead threads left is discounted. A seal that
// still finds a writer in flight after 1 s (a live process stopped or
// slowed) stores SEAL_DEAD and fails; a later attempt seals once the counts
// read 0. The owner's claim (plan D11) is the lock of the byte after the
// slots'.
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

// Decides from what the caller found (pure; tests drive it directly).
//   got_lock          this process holds the owner's lock (its claim)
//   has_cached_copy   this process made the device copy already
devhash_path devhash_choose_path(uint64_t phase, bool got_lock,
				 bool has_cached_copy);
// Tests: use only the first n writer slots (0: all), and the number of slot
// scans this process has run.
void set_devhash_usable_writer_slots_for_test(uint32_t n);
uint64_t devhash_slot_scans_for_test();

// Test hooks (always compiled; null by default). Called with the point and
// an argument (the slot for after_pass_c_slot).
enum class devhash_hook_point {
	// between the writer's increment and its PHASE load
	writer_after_inflight_inc,
	writer_after_phase_load,
	// at the start of build_image (a test may throw there)
	build_image_start,
	delivery_before_pass_a,
	delivery_after_pass_c_slot,
	// after a claim that leads to a copy (arg: the path), once while the
	// seal waits for writers in flight, between the delivery's passes B
	// and C (plan S5 test 3)
	after_claim,
	seal_waiting,
	delivery_between_b_c,
};
using devhash_hook_fn = void (*)(devhash_hook_point, uint64_t);
void set_devhash_hook_for_test(devhash_hook_fn fn);
// Tests: 1533 objects this process destroyed.
uint64_t devhash_destroyed_for_test();

class nv_gpu_device_hash_map_impl {
    public:
	// Writer slots: one per writing process, held by its lock, and an
	// overflow slot shared when all are held (never discounted). Each on a
	// 64-byte line of its own.
	static constexpr uint32_t WRITER_SLOTS = 256;
	struct writer_slot {
		uint64_t count; // its writers in flight
		// the last holder (pid namespace << 32 | pid), written by it
		// once it holds the lock: a free lock whose holder still runs
		// and maps the segment means the holder lost its description
		// (a host that closed it), not that it died
		uint64_t holder;
		// the holder's image (random per load of the runtime): the
		// same pid with another image is an earlier image (exec)
		uint64_t holder_image;
		uint64_t pad[5];
	};
	// Lock bytes in the segment's file: lock_base() + slot, and the
	// owner's after the slots'.
	static constexpr uint32_t OWNER_LOCK_BYTE = WRITER_SLOTS;

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
	uint64_t owner_pid = 0; // the claim's holder, for the status and logs
	// the claim's holder (pid namespace << 32 | pid), 0 after a release:
	// a free claim whose holder still runs and maps the segment is not
	// taken over (its holder lost its description)
	uint64_t owner_hint = 0;
	// random per map: a process's copy cache keys on (object, generation),
	// and the lock bytes derive from it
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
	// this process's writer slot's count (overflow slot when it holds
	// none)
	uint64_t *my_writer_count();
	uint64_t *slow_writer_count();
	// counts of all writer slots (seq_cst loads)
	uint64_t inflight_sum();
	// clears the count of every slot whose lock probe_fd (a description of
	// the sealer's own) can take and whose last holder is gone (ino, dev:
	// the segment's file)
	void discount_dead_writers(int probe_fd, uint64_t dev, uint64_t ino);

    public:
	const static bool should_lock = false;
	nv_gpu_device_hash_map_impl(
		boost::interprocess::managed_shared_memory &memory,
		uint64_t max_entries);
	// closes this process's description for the map (its locks)
	~nv_gpu_device_hash_map_impl();
	nv_gpu_device_hash_map_impl(const nv_gpu_device_hash_map_impl &) =
		delete;
	nv_gpu_device_hash_map_impl &
	operator=(const nv_gpu_device_hash_map_impl &) = delete;

	void *elem_lookup(const void *key);
	long elem_update(const void *key, const void *value, uint64_t flags);
	long elem_delete(const void *key);
	int map_get_next_key(const void *key, void *next_key);

	// ---- the agent's side (host code; the CUDA part is the caller's)
	// claim, release_claim, still_holds_claim and make_image take a
	// process-local mutex and make system calls: callers on application
	// threads (deliveries at a sync) run them with probes suppressed.

	// The first lock byte in the segment's file (from the generation).
	uint64_t lock_base() const;
	// Claims the map (the owner's lock, non-blocking) and returns the path
	// to take.
	devhash_path claim(bool has_cached_copy);
	// Gives the claim back.
	void release_claim();
	// Whether this process still holds its claim: its description still
	// refers to the segment's file (a host closing descriptors it does not
	// own releases the lock). The owner checks before each delivery.
	bool still_holds_claim();
	// Tests: this process's description of the segment's file for this
	// map (its number), or -1.
	int description_fd_for_test();
	// The image a device copy of this process starts from: claim, then by
	// the path the seal and the CPU table's image, a dead sealer's
	// recovery, or the takeover's image from the mirror. False: no copy
	// (path not_owner or have_copy, or the seal failed). The claim is given
	// back whenever no copy follows; an exception after the seal (rethrown)
	// also puts PHASE back to 0.
	bool make_image(bool has_cached_copy, devhash_image &out,
			devhash_path &path, uint64_t bound_ns = 1000000000ull);
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
	// the claim holder's pid (0: none recorded)
	uint64_t get_owner_pid() const
	{
		return __atomic_load_n(&owner_pid, __ATOMIC_ACQUIRE);
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
	// Tests: the claim's holder hint cleared (the lock alone then keeps
	// other claimants out).
	void clear_owner_hint_for_test()
	{
		__atomic_store_n(&owner_hint, 0, __ATOMIC_RELEASE);
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
