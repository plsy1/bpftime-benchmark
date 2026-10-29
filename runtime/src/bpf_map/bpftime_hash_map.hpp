#ifndef BPFTIME_HASH_MAP_HPP
#define BPFTIME_HASH_MAP_HPP

#include "map_common_def.hpp"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstring>
#include <pthread.h>

namespace bpftime
{

using namespace boost::interprocess;

class bpftime_hasher{
private:
	static inline bool is_prime(size_t n)
	{
		if (n <= 1)
			return false;
		if (n <= 3)
			return true;
		if (n % 2 == 0 || n % 3 == 0)
			return false;
		// Any prime number greater than 3 can be written in the form
		// 6k ± 1 for some integer k.
		for (size_t i = 5; i * i <= n; i += 6) {
			if (n % i == 0 || n % (i + 2) == 0)
				return false;
		}
		return true;
	}

public:
	static inline size_t modulo_reciprocal(size_t divisor)
	{
#if SIZE_MAX == UINT64_MAX && defined(__SIZEOF_INT128__)
		return static_cast<size_t>(
			(static_cast<unsigned __int128>(1) << 64) / divisor);
#else
		(void)divisor;
		return 0;
#endif
	}

	static inline size_t fast_modulo(size_t value, size_t divisor,
					 size_t reciprocal)
	{
#if SIZE_MAX == UINT64_MAX && defined(__SIZEOF_INT128__)
		const size_t quotient = static_cast<size_t>(
			(static_cast<unsigned __int128>(value) * reciprocal) >> 64);
		size_t remainder = value - quotient * divisor;
		if (remainder >= divisor)
			remainder -= divisor;
		return remainder;
#else
		(void)reciprocal;
		return value % divisor;
#endif
	}

	static inline size_t next_prime(size_t n)
	{
		while (!is_prime(n)) {
			++n;
		}
		return n;
	}

	static size_t hash_func(const void *key, size_t key_size)
	{
		if (key_size == sizeof(uint32_t)) {
			const auto *bytes = static_cast<const uint8_t *>(key);
			return size_t{ bytes[0] } * 29791 +
			       size_t{ bytes[1] } * 961 +
			       size_t{ bytes[2] } * 31 + bytes[3];
		}
		size_t hash = 0;
		for (size_t i = 0; i < key_size; i++) {
			hash = hash * 31 + ((uint8_t *)key)[i];
		}
		return hash;
	}
};

// A fixed size hash map that is used in the eBPF runtime.
// with open addressing and linear probing.
//
// Lookups take no lock. Each bucket starts with a 4-byte word: the low two
// bits are the state, the rest a generation that every delete increments.
// Filled buckets never move (lookups hand out value pointers), so a lookup can
// only be misled by a bucket deleted and refilled with another key while it
// compares the key bytes: it reads the word, compares, and accepts the match
// only if the word is unchanged (a seqlock per bucket). A lookup writes
// nothing shared and never waits for a writer.
//
// Writers lock stripes: runs of buckets (a power of two, at least 64) with a
// spinlock each. An operation locks every stripe from its key's home bucket
// through the last bucket it may write or must see unchanged, always in
// increasing stripe order (the part past the end of the table first when the
// range wraps); when it finds it needs more, it unlocks and starts again with
// the larger range, so no lock is taken while a higher one is held. Each
// stripe keeps the probe bound of the keys whose home bucket is in it. The
// element count is kept as tokens, max_entries in all, in per-CPU slots: an
// insert takes one (its CPU's slot first, then the shared pool, then other
// slots), a delete returns it to its CPU's slot.
class bpftime_hash_map {
    private:
	static constexpr uint32_t BUCKET_EMPTY = 0;
	static constexpr uint32_t BUCKET_FILLED = 1;
	static constexpr uint32_t BUCKET_DELETED = 2;
	static constexpr uint32_t STATE_MASK = 3;
	static constexpr uint32_t GENERATION_STEP = 4;

	static constexpr size_t CACHE_LINE = 64;
	static constexpr size_t MIN_STRIPE_SHIFT = 6; // 64 buckets
	static constexpr size_t MAX_STRIPES = 4096;
	// Filled buckets per exact distance from the home bucket, so the probe
	// bound shrinks again when far elements are deleted. Distances from
	// TRACKED_DISPLACEMENTS on are only counted; while any remain, the bound
	// stays at the largest such distance seen.
	static constexpr size_t TRACKED_DISPLACEMENTS = 64;
	static constexpr size_t COUNTER_SLOTS = 64;

	// Writer state of one stripe, protected by its lock; whole cache lines.
	struct stripe_state {
		pthread_spinlock_t lock;
		uint32_t far_count;
		uint32_t far_max;
		uint32_t unused;
		uint32_t displacement_counts[TRACKED_DISPLACEMENTS];
		char padding[5 * CACHE_LINE - 16 - 4 * TRACKED_DISPLACEMENTS];
	};
	static_assert(sizeof(stripe_state) % CACHE_LINE == 0);

	struct token_slot {
		int64_t tokens;
		char padding[CACHE_LINE - sizeof(int64_t)];
	};
	static_assert(sizeof(token_slot) == CACHE_LINE);

	struct stripe_range {
		size_t first; // stripe index
		size_t count; // stripes, at most _num_stripes
	};

	inline size_t get_elem_offset(size_t index) const
	{
		return index * _elem_size;
	}

	inline uint32_t *state_word(size_t index) const
	{
		return reinterpret_cast<uint32_t *>(const_cast<uint8_t *>(
			&data_buffer[get_elem_offset(index)]));
	}

	inline uint32_t load_word(size_t index, std::memory_order order) const
	{
		return std::atomic_ref<uint32_t>(*state_word(index)).load(order);
	}

	inline void store_word(size_t index, uint32_t word,
			       std::memory_order order)
	{
		std::atomic_ref<uint32_t>(*state_word(index)).store(word, order);
	}

	// Shared-memory vectors may start at any 8- or 16-byte boundary; the
	// stripes and token slots start at the first cache line inside them.
	// The segment is page aligned in every process, so this is the same
	// element in all of them.
	template <typename T> static T *line_aligned(const bytes_vec &buffer)
	{
		const auto base =
			reinterpret_cast<uintptr_t>(buffer.data());
		return reinterpret_cast<T *>((base + CACHE_LINE - 1) &
					     ~uintptr_t{ CACHE_LINE - 1 });
	}

	inline stripe_state &stripe(size_t index) const
	{
		return line_aligned<stripe_state>(stripe_buffer)[index];
	}

	inline token_slot &token_slot_at(size_t index) const
	{
		return line_aligned<token_slot>(token_buffer)[index];
	}

	inline uint32_t *bound_word(size_t stripe_index) const
	{
		return const_cast<uint32_t *>(&bounds[stripe_index]);
	}

	// Probe bound of the keys whose home bucket is in the stripe.
	inline size_t load_bound(size_t stripe_index) const
	{
		return std::atomic_ref<uint32_t>(*bound_word(stripe_index))
			.load(std::memory_order_acquire);
	}

	// Under the stripe's lock; stores only when the bound changes, since
	// lookups read it from a line shared with neighbouring stripes.
	inline void set_bound(size_t stripe_index, size_t bound)
	{
		if (bound != bounds[stripe_index])
			std::atomic_ref<uint32_t>(*bound_word(stripe_index))
				.store((uint32_t)bound,
				       std::memory_order_release);
	}

	inline size_t stripe_of(size_t bucket) const
	{
		return bucket >> _stripe_shift;
	}

	inline size_t home_bucket(const void *key) const
	{
		return bpftime_hasher::fast_modulo(
			bpftime_hasher::hash_func(key, _key_size), _num_buckets,
			_bucket_reciprocal);
	}

	inline size_t advance(size_t bucket, size_t distance) const
	{
		bucket += distance;
		return bucket >= _num_buckets ? bucket - _num_buckets : bucket;
	}

	// Stripes covering the buckets home .. home + span - 1 (span <= table).
	stripe_range range_for(size_t home, size_t span) const
	{
		if (span >= _num_buckets)
			return { 0, _num_stripes };
		const size_t first = stripe_of(home);
		const size_t last = stripe_of(advance(home, span - 1));
		const bool wraps = home + span - 1 >= _num_buckets;
		const size_t count = wraps ? _num_stripes - first + last + 1 :
					     last - first + 1;
		if (count >= _num_stripes)
			return { 0, _num_stripes };
		return { first, count };
	}

	bool range_holds(const stripe_range &range, size_t bucket) const
	{
		const size_t s = stripe_of(bucket);
		const size_t offset = s >= range.first ?
					      s - range.first :
					      _num_stripes - range.first + s;
		return offset < range.count;
	}

	// Increasing stripe order: the wrapped part (from stripe 0) first.
	void lock_range(const stripe_range &range) const
	{
		const size_t end = range.first + range.count;
		if (end > _num_stripes)
			for (size_t s = 0; s < end - _num_stripes; s++)
				pthread_spin_lock(&stripe(s).lock);
		for (size_t s = range.first; s < std::min(end, _num_stripes);
		     s++)
			pthread_spin_lock(&stripe(s).lock);
	}

	void unlock_range(const stripe_range &range) const
	{
		const size_t end = range.first + range.count;
		for (size_t s = range.first; s < std::min(end, _num_stripes);
		     s++)
			pthread_spin_unlock(&stripe(s).lock);
		if (end > _num_stripes)
			for (size_t s = 0; s < end - _num_stripes; s++)
				pthread_spin_unlock(&stripe(s).lock);
	}

	class range_guard {
		const bpftime_hash_map &map;
		stripe_range range;

	    public:
		range_guard(const bpftime_hash_map &map, stripe_range range)
			: map(map), range(range)
		{
			map.lock_range(range);
		}
		~range_guard()
		{
			map.unlock_range(range);
		}
		range_guard(const range_guard &) = delete;
		range_guard &operator=(const range_guard &) = delete;
	};

	inline size_t token_slot_of_current_cpu() const
	{
		const int cpu = my_sched_getcpu();
		return cpu < 0 ? 0 : (size_t)cpu % COUNTER_SLOTS;
	}

	static bool take_one(int64_t &tokens)
	{
		std::atomic_ref<int64_t> ref(tokens);
		int64_t current = ref.load(std::memory_order_relaxed);
		while (current > 0)
			if (ref.compare_exchange_weak(current, current - 1,
						      std::memory_order_relaxed))
				return true;
		return false;
	}

	// One token per element. A delete returns its token before an insert
	// elsewhere can miss it only while that delete is in progress, so a
	// failure means the map was full at some point during the call.
	bool take_token()
	{
		const size_t own = token_slot_of_current_cpu();
		if (take_one(token_slot_at(own).tokens) ||
		    take_one(_pool_tokens))
			return true;
		for (size_t i = 1; i < COUNTER_SLOTS; i++)
			if (take_one(token_slot_at((own + i) % COUNTER_SLOTS)
					     .tokens))
				return true;
		return false;
	}

	void return_token()
	{
		std::atomic_ref<int64_t>(
			token_slot_at(token_slot_of_current_cpu()).tokens)
			.fetch_add(1, std::memory_order_relaxed);
	}

	inline bool keys_equal(const void *lhs, const void *rhs) const
	{
		if (_key_size == sizeof(uint32_t)) {
			uint32_t lhs_value;
			uint32_t rhs_value;
			std::memcpy(&lhs_value, lhs, sizeof(lhs_value));
			std::memcpy(&rhs_value, rhs, sizeof(rhs_value));
			return lhs_value == rhs_value;
		}
		return std::memcmp(lhs, rhs, _key_size) == 0;
	}

	// Every filled bucket lies at most its home stripe's bound after its
	// home bucket, so a miss stops there even when no empty bucket follows.
	// Buckets on a present key's probe path stay non-empty and the bound
	// stays at least its distance (release_deleted_run, note_released), so
	// concurrent writers cannot make a lookup miss a key present throughout.
	template <bool Uint32Key>
	void *lookup_from_bucket(const void *key, size_t home)
	{
		uint32_t key_value = 0;
		if constexpr (Uint32Key)
			std::memcpy(&key_value, key, sizeof(key_value));
		const size_t home_stripe = stripe_of(home);
		for (;;) {
			bool changed = false;
			size_t index = home;
			const size_t bound = load_bound(home_stripe);
			for (size_t probe = 0; probe <= bound; ++probe) {
				const uint32_t word =
					load_word(index, std::memory_order_acquire);
				const uint32_t state = word & STATE_MASK;
				if (state == BUCKET_EMPTY)
					return nullptr;
				if (state == BUCKET_FILLED) {
					bool equal;
					if constexpr (Uint32Key) {
						equal = std::atomic_ref<uint32_t>(
								*static_cast<uint32_t *>(
									get_key(index)))
								.load(std::memory_order_relaxed) ==
							key_value;
					} else {
						equal = std::memcmp(get_key(index), key,
								    _key_size) == 0;
					}
					if (equal) {
						// Order the key reads before the re-read.
						std::atomic_thread_fence(
							std::memory_order_acquire);
						if (load_word(index,
							      std::memory_order_relaxed) ==
						    word)
							return get_value(index);
						changed = true;
						break;
					}
				}
				if (++index == _num_buckets)
					index = 0;
			}
			if (!changed)
				return nullptr;
		}
	}

	void note_filled(size_t stripe_index, size_t distance)
	{
		auto &st = stripe(stripe_index);
		if (distance < TRACKED_DISPLACEMENTS) {
			st.displacement_counts[distance]++;
		} else {
			st.far_count++;
			if (distance > st.far_max)
				st.far_max = (uint32_t)distance;
		}
		if (distance > bounds[stripe_index])
			set_bound(stripe_index, distance);
	}

	void note_released(size_t stripe_index, size_t distance)
	{
		auto &st = stripe(stripe_index);
		size_t bound;
		if (distance < TRACKED_DISPLACEMENTS) {
			// Common case: the bound only changes when the last element
			// at the current largest distance goes away.
			if (--st.displacement_counts[distance] != 0 ||
			    distance != bounds[stripe_index])
				return;
			bound = distance;
		} else {
			if (--st.far_count != 0)
				return; // The bound stays at far_max.
			st.far_max = 0;
			bound = TRACKED_DISPLACEMENTS - 1;
		}
		while (bound > 0 && st.displacement_counts[bound] == 0)
			bound--;
		set_bound(stripe_index, bound);
	}

	// A deleted bucket followed by an empty one lies on no filled bucket's
	// probe sequence, so it and the deleted run before it become empty.
	// Only buckets in locked stripes are changed or relied on: the run stops
	// at the start of the range, and nothing is released when the next
	// bucket is outside it (tombstones are always correct, only slower).
	// Filled buckets never move: lookups hand out value pointers.
	void release_deleted_run(size_t index, const stripe_range &locked)
	{
		const size_t next = advance(index, 1);
		if (!range_holds(locked, next) ||
		    get_bucket_state(next) != BUCKET_EMPTY)
			return;
		while (range_holds(locked, index) &&
		       get_bucket_state(index) == BUCKET_DELETED) {
			set_empty(index);
			index = index == 0 ? _num_buckets - 1 : index - 1;
		}
	}

	// Copies the key in bucket index if it is filled, under its stripe's
	// lock so that it is not overwritten meanwhile.
	bool copy_key_if_filled(size_t index, void *out)
	{
		if (is_empty(index))
			return false;
		range_guard guard(*this, { stripe_of(index), 1 });
		if (is_empty(index))
			return false;
		std::memcpy(out, get_key(index), _key_size);
		return true;
	}

	// Read by every lookup and written only at construction: kept apart
	// from the stripes and token slots that writers change.
	size_t _key_size;
	size_t _value_size;
	size_t _elem_size; // state word + key + value, a multiple of 4
	size_t _num_buckets; // Adjusted number of buckets (prime)
	size_t _bucket_reciprocal; // Precomputed for fast initial bucket mapping
	size_t _max_element_count; // Maximum number of elements based on the
				   // original num_buckets
	size_t _stripe_shift; // log2 of the buckets per stripe
	size_t _num_stripes;

	// The data is stored in a contiguous memory region.
	// The layout of elem is:
	// 4 bytes state and generation
	// key_size bytes for the key
	// value_size bytes for the value
	// padding to a multiple of 4 bytes (the state word is atomic)
	bytes_vec data_buffer;
	// Probe bound per stripe, read by lookups (rarely written).
	boost::interprocess::vector<
		uint32_t, boost::interprocess::allocator<
				  uint32_t, managed_shared_memory::segment_manager>>
		bounds;
	bytes_vec stripe_buffer; // stripe_state per stripe, line aligned
	bytes_vec token_buffer; // token_slot per CPU slot, line aligned

	char _read_mostly_end[CACHE_LINE] = {};
	int64_t _pool_tokens; // tokens not yet taken by any CPU slot
	char _pool_end[CACHE_LINE] = {};

	static size_t stripe_shift_for(size_t num_buckets)
	{
		size_t shift = MIN_STRIPE_SHIFT;
		while (((num_buckets + (size_t{ 1 } << shift) - 1) >> shift) >
		       MAX_STRIPES)
			shift++;
		return shift;
	}

    public:
	bpftime_hash_map(managed_shared_memory &memory, size_t num_buckets,
			 size_t key_size, size_t value_size)
		: _key_size(key_size), _value_size(value_size),
		  _elem_size((4 + key_size + value_size + 3) & ~size_t{ 3 }),
		  // Twice the capacity keeps the load at most about 50%, so
		  // linear probing finds an empty bucket quickly on a miss.
		  _num_buckets(bpftime_hasher::next_prime(2 * num_buckets)),
		  _bucket_reciprocal(
			  bpftime_hasher::modulo_reciprocal(_num_buckets)),
		  // Use the original bucket count
		  _max_element_count(num_buckets),
		  _stripe_shift(stripe_shift_for(_num_buckets)),
		  _num_stripes((_num_buckets + (size_t{ 1 } << _stripe_shift) -
				1) >>
			       _stripe_shift),
		  data_buffer(memory.get_segment_manager()),
		  bounds(memory.get_segment_manager()),
		  stripe_buffer(memory.get_segment_manager()),
		  token_buffer(memory.get_segment_manager()),
		  _pool_tokens((int64_t)num_buckets)
	{
		data_buffer.resize(_num_buckets * _elem_size, 0);
		bounds.resize(_num_stripes, 0);
		stripe_buffer.resize(_num_stripes * sizeof(stripe_state) +
					     CACHE_LINE,
				     0);
		token_buffer.resize(COUNTER_SLOTS * sizeof(token_slot) +
					    CACHE_LINE,
				    0);
		for (size_t s = 0; s < _num_stripes; s++)
			pthread_spin_init(&stripe(s).lock,
					  PTHREAD_PROCESS_SHARED);
	}

	inline uint32_t get_bucket_state(size_t index) const
	{
		return load_word(index, std::memory_order_relaxed) & STATE_MASK;
	}

	inline bool is_empty(size_t index) const
	{
		return get_bucket_state(index) != BUCKET_FILLED;
	}

	// Writers only (under the bucket's stripe lock), so the word's
	// generation can be read without ordering.
	inline void set_empty(size_t index)
	{
		const uint32_t word = load_word(index, std::memory_order_relaxed);
		store_word(index, (word & ~STATE_MASK) | BUCKET_EMPTY,
			   std::memory_order_relaxed);
	}

	// Publishes the key and value written before it.
	inline void set_filled(size_t index)
	{
		const uint32_t word = load_word(index, std::memory_order_relaxed);
		store_word(index, (word & ~STATE_MASK) | BUCKET_FILLED,
			   std::memory_order_release);
	}

	inline void set_deleted(size_t index)
	{
		const uint32_t word = load_word(index, std::memory_order_relaxed);
		store_word(index,
			   ((word & ~STATE_MASK) + GENERATION_STEP) |
				   BUCKET_DELETED,
			   std::memory_order_relaxed);
	}

	inline void *get_key(size_t index)
	{
		return &data_buffer[get_elem_offset(index) + 4];
	}

	inline void *get_value(size_t index)
	{
		return &data_buffer[get_elem_offset(index) + 4 + _key_size];
	}

	inline size_t get_index_of_value(const void *value)
	{
		if (value == nullptr || value < &data_buffer[0] ||
		    value >= data_buffer.data() + data_buffer.size()) {
			return -1;
		}
		return (((uint8_t *)value) - &data_buffer[0]) / _elem_size;
	}

	void *elem_lookup(const void *key)
	{
		const size_t index = home_bucket(key);
		if (_key_size == sizeof(uint32_t))
			return lookup_from_bucket<true>(key, index);
		return lookup_from_bucket<false>(key, index);
	}

	enum class update_mode { any, no_exist, exist };
	enum class update_result { ok, exists, missing, full };

	// Kernel htab semantics for the three modes. An existing key's value is
	// overwritten in place, as the kernel without BPF_F_LOCK: concurrent
	// readers may see a partly written value.
	update_result update(const void *key, const void *value,
			     update_mode mode)
	{
		const size_t home = home_bucket(key);
		const size_t home_stripe = stripe_of(home);
		// Buckets from home that the locked range must cover.
		size_t span = load_bound(home_stripe) + 1;
		for (;;) {
			const stripe_range locked = range_for(home, span);
			range_guard guard(*this, locked);
			const size_t bound = bounds[home_stripe];
			if (bound + 1 > span && locked.count < _num_stripes) {
				span = bound + 1; // Grew before we locked.
				continue;
			}
			size_t index = home;
			size_t free_slot = _num_buckets;
			size_t free_distance = 0;
			size_t probe = 0;
			// An existing key is within the bound of its home bucket.
			for (; probe <= bound; ++probe) {
				const auto state = get_bucket_state(index);
				if (state != BUCKET_FILLED &&
				    free_slot == _num_buckets) {
					free_slot = index;
					free_distance = probe;
				}
				if (state == BUCKET_EMPTY)
					break;
				if (state == BUCKET_FILLED &&
				    keys_equal(get_key(index), key)) {
					if (mode == update_mode::no_exist)
						return update_result::exists;
					std::memcpy(get_value(index), value,
						    _value_size);
					return update_result::ok;
				}
				index = advance(index, 1);
			}
			if (mode == update_mode::exist)
				return update_result::missing;
			// No free bucket within the bound: take the next one.
			for (; free_slot == _num_buckets && probe < _num_buckets;
			     ++probe) {
				if (get_bucket_state(index) != BUCKET_FILLED) {
					free_slot = index;
					free_distance = probe;
				} else {
					index = advance(index, 1);
				}
			}
			if (free_slot == _num_buckets)
				return update_result::full;
			if (free_distance + 1 > span &&
			    locked.count < _num_stripes) {
				// The free bucket is past the locked stripes.
				span = free_distance + 1;
				continue;
			}
			if (!take_token())
				return update_result::full;
			// A lookup that read this bucket's previous key and then
			// sees any of the new bytes must also see the delete's new
			// generation.
			std::atomic_thread_fence(std::memory_order_release);
			std::memcpy(get_key(free_slot), key, _key_size);
			std::memcpy(get_value(free_slot), value, _value_size);
			// The bound covers the new bucket before lookups can see it.
			note_filled(home_stripe, free_distance);
			set_filled(free_slot);
			return update_result::ok;
		}
	}

	bool elem_update(const void *key, const void *value)
	{
		return update(key, value, update_mode::any) == update_result::ok;
	}

	bool elem_delete(const void *key)
	{
		const size_t home = home_bucket(key);
		const size_t home_stripe = stripe_of(home);
		// The bound plus the bucket after it (release_deleted_run).
		size_t span = load_bound(home_stripe) + 2;
		for (;;) {
			const stripe_range locked = range_for(home, span);
			range_guard guard(*this, locked);
			const size_t bound = bounds[home_stripe];
			if (bound + 2 > span && locked.count < _num_stripes) {
				span = bound + 2;
				continue;
			}
			size_t index = home;
			for (size_t probe = 0; probe <= bound; ++probe) {
				const auto state = get_bucket_state(index);
				if (state == BUCKET_EMPTY)
					return false; // Key not found
				if (state == BUCKET_FILLED &&
				    keys_equal(get_key(index), key)) {
					set_deleted(index);
					return_token();
					note_released(home_stripe, probe);
					release_deleted_run(index, locked);
					return true;
				}
				index = advance(index, 1);
			}
			return false; // Key not found
		}
	}

	// The key after key in bucket order (the first one for a missing key or
	// nullptr), as the kernel. Keys added or deleted meanwhile may or may not
	// be seen.
	bool next_key(const void *key, void *next_key)
	{
		size_t start = 0;
		if (key != nullptr) {
			const void *value = elem_lookup(key);
			if (value != nullptr)
				start = get_index_of_value(value) + 1;
		}
		for (size_t i = start; i < _num_buckets; i++)
			if (copy_key_if_filled(i, next_key))
				return true;
		return false;
	}

	size_t bucket_count() const
	{
		return _num_buckets;
	}

	size_t stripe_count() const
	{
		return _num_stripes;
	}

	size_t buckets_per_stripe() const
	{
		return size_t{ 1 } << _stripe_shift;
	}

	// Largest probe bound over all stripes.
	size_t max_displacement() const
	{
		size_t bound = 0;
		for (size_t s = 0; s < _num_stripes; s++)
			bound = std::max(bound, load_bound(s));
		return bound;
	}

	// Exact when no writer runs.
	size_t get_elem_count() const
	{
		int64_t free_tokens =
			std::atomic_ref<int64_t>(
				const_cast<int64_t &>(_pool_tokens))
				.load(std::memory_order_relaxed);
		for (size_t i = 0; i < COUNTER_SLOTS; i++)
			free_tokens += std::atomic_ref<int64_t>(
					       token_slot_at(i).tokens)
					       .load(std::memory_order_relaxed);
		return _max_element_count - (size_t)free_tokens;
	}
};

} // namespace bpftime

#endif // BPFTIME_HASH_MAP_HPP
