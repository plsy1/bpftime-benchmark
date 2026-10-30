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
// element count is kept as tokens, max_entries in all: each stripe holds some
// (protected by its lock), the rest are in a shared pool. An insert takes one
// from its home stripe, refilling a batch from the pool when it has none; a
// delete returns it there. Only when the pool is empty too does an insert
// lock every stripe and take a token from any of them.
//
// Every operation resolves the two shared-memory vectors (buckets, stripes)
// to plain pointers once (a view): each offset_ptr access costs a few
// instructions, and resolving them per access made a single-thread insert +
// lookup + delete 300 instructions longer. A stripe's state, its probe bound
// included, is one cache line: with keys spread over many stripes (memleak's
// addresses) a larger state or a separate bound array added cache and TLB
// misses to every operation.
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
	// stays at the largest such distance seen. (At most half the buckets are
	// filled, so most keys sit within a few buckets of home.)
	static constexpr size_t TRACKED_DISPLACEMENTS = 11;
	// Tokens a stripe takes from the shared pool at once, and gives back
	// when it holds twice as many.
	static constexpr uint32_t TOKEN_BATCH = 32;

	// One stripe's state, one cache line. Written under its lock; lookups
	// read only bound.
	struct stripe_state {
		pthread_spinlock_t lock;
		// Probe bound of the keys whose home bucket is in the stripe.
		uint32_t bound;
		uint32_t tokens; // element tokens held by this stripe
		uint32_t far_count;
		uint32_t far_max;
		uint32_t displacement_counts[TRACKED_DISPLACEMENTS];
	};
	static_assert(sizeof(stripe_state) == CACHE_LINE);

	struct stripe_range {
		size_t first; // stripe index
		size_t count; // stripes, at most _num_stripes
	};

	// The shared vectors resolved in this process.
	struct view {
		uint8_t *data;
		stripe_state *stripes;
	};

	view resolve() const
	{
		// Shared-memory vectors may start at any 8- or 16-byte boundary;
		// the stripes start at the first cache line inside theirs. The
		// segment is page aligned in every process, so this is the same
		// element in all of them.
		const auto stripes =
			reinterpret_cast<uintptr_t>(stripe_buffer.data());
		return { const_cast<uint8_t *>(data_buffer.data()),
			 reinterpret_cast<stripe_state *>(
				 (stripes + CACHE_LINE - 1) &
				 ~uintptr_t{ CACHE_LINE - 1 }) };
	}

	inline uint32_t *word_at(const view &v, size_t index) const
	{
		return reinterpret_cast<uint32_t *>(v.data + index * _elem_size);
	}

	inline uint8_t *key_at(const view &v, size_t index) const
	{
		return v.data + index * _elem_size + 4;
	}

	inline uint8_t *value_at(const view &v, size_t index) const
	{
		return v.data + index * _elem_size + _value_offset;
	}

	static inline uint32_t load_word(const view &v, const uint32_t *word,
					 std::memory_order order)
	{
		(void)v;
		return std::atomic_ref<uint32_t>(*const_cast<uint32_t *>(word))
			.load(order);
	}

	inline uint32_t state_at(const view &v, size_t index) const
	{
		return load_word(v, word_at(v, index), std::memory_order_relaxed) &
		       STATE_MASK;
	}

	// Writers only (under the bucket's stripe lock), so the word's
	// generation can be read without ordering.
	inline void set_state(const view &v, size_t index, uint32_t state,
			      uint32_t generation_step, std::memory_order order)
	{
		uint32_t *word = word_at(v, index);
		std::atomic_ref<uint32_t> ref(*word);
		const uint32_t old = ref.load(std::memory_order_relaxed);
		ref.store(((old & ~STATE_MASK) + generation_step) | state, order);
	}

	static inline size_t load_bound(const view &v, size_t stripe_index)
	{
		return std::atomic_ref<uint32_t>(v.stripes[stripe_index].bound)
			.load(std::memory_order_acquire);
	}

	// Under the stripe's lock.
	static inline void set_bound(const view &v, size_t stripe_index,
				     size_t bound)
	{
		std::atomic_ref<uint32_t>(v.stripes[stripe_index].bound)
			.store((uint32_t)bound, std::memory_order_release);
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
	void lock_range(const view &v, const stripe_range &range) const
	{
		const size_t end = range.first + range.count;
		if (end > _num_stripes) {
			for (size_t s = 0; s < end - _num_stripes; s++)
				pthread_spin_lock(&v.stripes[s].lock);
			for (size_t s = range.first; s < _num_stripes; s++)
				pthread_spin_lock(&v.stripes[s].lock);
		} else {
			for (size_t s = range.first; s < end; s++)
				pthread_spin_lock(&v.stripes[s].lock);
		}
	}

	void unlock_range(const view &v, const stripe_range &range) const
	{
		const size_t end = range.first + range.count;
		if (end > _num_stripes) {
			for (size_t s = range.first; s < _num_stripes; s++)
				pthread_spin_unlock(&v.stripes[s].lock);
			for (size_t s = 0; s < end - _num_stripes; s++)
				pthread_spin_unlock(&v.stripes[s].lock);
		} else {
			for (size_t s = range.first; s < end; s++)
				pthread_spin_unlock(&v.stripes[s].lock);
		}
	}

	class range_guard {
		const bpftime_hash_map &map;
		const view &v;
		stripe_range range;

	    public:
		range_guard(const bpftime_hash_map &map, const view &v,
			    stripe_range range)
			: map(map), v(v), range(range)
		{
			map.lock_range(v, range);
		}
		~range_guard()
		{
			map.unlock_range(v, range);
		}
		range_guard(const range_guard &) = delete;
		range_guard &operator=(const range_guard &) = delete;
	};

	// Under the home stripe's lock. False when the stripe and the shared
	// pool have none left (the caller then locks every stripe and calls
	// take_token_any).
	bool take_token(const view &v, size_t stripe_index)
	{
		auto &st = v.stripes[stripe_index];
		if (st.tokens > 0) {
			st.tokens--;
			return true;
		}
		std::atomic_ref<int64_t> pool(_pool_tokens);
		int64_t current = pool.load(std::memory_order_relaxed);
		while (current > 0) {
			const int64_t take =
				std::min<int64_t>(current, TOKEN_BATCH);
			if (pool.compare_exchange_weak(current, current - take,
						       std::memory_order_relaxed)) {
				st.tokens += (uint32_t)take - 1;
				return true;
			}
		}
		return false;
	}

	// With every stripe locked: a token from the home stripe, the pool or
	// any other stripe. False only when the map is full.
	bool take_token_any(const view &v, size_t stripe_index)
	{
		if (take_token(v, stripe_index))
			return true;
		for (size_t s = 0; s < _num_stripes; s++) {
			auto &st = v.stripes[s];
			if (st.tokens > 0) {
				st.tokens--;
				return true;
			}
		}
		return false;
	}

	// Under the home stripe's lock.
	void return_token(const view &v, size_t stripe_index)
	{
		auto &st = v.stripes[stripe_index];
		if (++st.tokens >= 2 * TOKEN_BATCH) {
			st.tokens -= TOKEN_BATCH;
			std::atomic_ref<int64_t>(_pool_tokens)
				.fetch_add(TOKEN_BATCH, std::memory_order_relaxed);
		}
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
		const view v = resolve();
		uint32_t key_value = 0;
		if constexpr (Uint32Key)
			std::memcpy(&key_value, key, sizeof(key_value));
		const size_t home_stripe = stripe_of(home);
		for (;;) {
			bool changed = false;
			size_t index = home;
			const size_t bound = load_bound(v, home_stripe);
			for (size_t probe = 0; probe <= bound; ++probe) {
				const uint32_t *word_ptr = word_at(v, index);
				const uint32_t word = load_word(
					v, word_ptr, std::memory_order_acquire);
				const uint32_t state = word & STATE_MASK;
				if (state == BUCKET_EMPTY)
					return nullptr;
				if (state == BUCKET_FILLED) {
					uint8_t *bucket_key = key_at(v, index);
					bool equal;
					if constexpr (Uint32Key) {
						equal = std::atomic_ref<uint32_t>(
								*reinterpret_cast<uint32_t *>(
									bucket_key))
								.load(std::memory_order_relaxed) ==
							key_value;
					} else {
						equal = std::memcmp(bucket_key, key,
								    _key_size) == 0;
					}
					if (equal) {
						// Order the key reads before the re-read.
						std::atomic_thread_fence(
							std::memory_order_acquire);
						if (load_word(v, word_ptr,
							      std::memory_order_relaxed) ==
						    word)
							return bucket_key - 4 +
							       _value_offset;
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

	void note_filled(const view &v, size_t stripe_index, size_t distance)
	{
		auto &st = v.stripes[stripe_index];
		if (distance < TRACKED_DISPLACEMENTS) {
			st.displacement_counts[distance]++;
		} else {
			st.far_count++;
			if (distance > st.far_max)
				st.far_max = (uint32_t)distance;
		}
		if (distance > st.bound)
			set_bound(v, stripe_index, distance);
	}

	void note_released(const view &v, size_t stripe_index, size_t distance)
	{
		auto &st = v.stripes[stripe_index];
		size_t bound;
		if (distance < TRACKED_DISPLACEMENTS) {
			// Common case: the bound only changes when the last element
			// at the current largest distance goes away.
			if (--st.displacement_counts[distance] != 0 ||
			    distance != st.bound)
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
		if (bound != st.bound)
			set_bound(v, stripe_index, bound);
	}

	// A deleted bucket followed by an empty one lies on no filled bucket's
	// probe sequence, so it and the deleted run before it become empty.
	// Only buckets in locked stripes are changed or relied on: the run stops
	// at the start of the range, and nothing is released when the next
	// bucket is outside it (tombstones are always correct, only slower).
	// Filled buckets never move: lookups hand out value pointers.
	void release_deleted_run(const view &v, size_t index,
				 const stripe_range &locked)
	{
		const size_t next = advance(index, 1);
		if (!range_holds(locked, next) ||
		    state_at(v, next) != BUCKET_EMPTY)
			return;
		while (range_holds(locked, index) &&
		       state_at(v, index) == BUCKET_DELETED) {
			set_state(v, index, BUCKET_EMPTY, 0,
				  std::memory_order_relaxed);
			index = index == 0 ? _num_buckets - 1 : index - 1;
		}
	}

	// Read by every lookup and written only at construction: kept apart
	// from the stripes and the token pool that writers change.
	size_t _key_size;
	size_t _value_size;
	size_t _value_offset; // from the element start, value_align aligned
	size_t _elem_size; // a multiple of value_align (and of 4)
	size_t _num_buckets; // Adjusted number of buckets (prime)
	size_t _bucket_reciprocal; // Precomputed for fast initial bucket mapping
	size_t _max_element_count; // Maximum number of elements based on the
				   // original num_buckets
	size_t _stripe_shift; // log2 of the buckets per stripe
	size_t _num_stripes;

	// The data is stored in a contiguous memory region.
	// The layout of elem is:
	// 4 bytes state and generation (atomic, so 4-aligned)
	// key_size bytes for the key, at 4
	// padding to value_align, then value_size bytes for the value
	// padding to a multiple of value_align
	// value_align is 8 for values of 8 bytes or more, else 4: programs
	// use 8-byte atomics on values (BPF atomic add, __sync_fetch_and_add)
	// and aarch64 faults on one crossing a 16-byte boundary. The kernel
	// aligns values to 8. The data buffer starts 16-aligned (boost).
	bytes_vec data_buffer;
	bytes_vec stripe_buffer; // stripe_state per stripe, line aligned

	char _read_mostly_end[CACHE_LINE] = {};
	int64_t _pool_tokens; // tokens held by no stripe
	char _pool_end[CACHE_LINE] = {};

	static constexpr size_t value_align(size_t value_size)
	{
		return value_size >= 8 ? 8 : 4;
	}

	static constexpr size_t round_up(size_t n, size_t align)
	{
		return (n + align - 1) & ~(align - 1);
	}

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
		  _value_offset(round_up(4 + key_size, value_align(value_size))),
		  _elem_size(round_up(_value_offset + value_size,
				      value_align(value_size))),
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
		  stripe_buffer(memory.get_segment_manager()),
		  _pool_tokens((int64_t)num_buckets)
	{
		data_buffer.resize(_num_buckets * _elem_size, 0);
		stripe_buffer.resize(_num_stripes * sizeof(stripe_state) +
					     CACHE_LINE,
				     0);
		const view v = resolve();
		for (size_t s = 0; s < _num_stripes; s++)
			pthread_spin_init(&v.stripes[s].lock,
					  PTHREAD_PROCESS_SHARED);
	}

	inline uint32_t get_bucket_state(size_t index) const
	{
		return state_at(resolve(), index);
	}

	inline bool is_empty(size_t index) const
	{
		return get_bucket_state(index) != BUCKET_FILLED;
	}

	inline void *get_key(size_t index)
	{
		return key_at(resolve(), index);
	}

	inline void *get_value(size_t index)
	{
		return value_at(resolve(), index);
	}

	inline size_t get_index_of_value(const void *value)
	{
		const uint8_t *data = data_buffer.data();
		const auto *p = static_cast<const uint8_t *>(value);
		if (p == nullptr || p < data ||
		    p >= data + data_buffer.size()) {
			return -1;
		}
		return (p - data) / _elem_size;
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
		const view v = resolve();
		const size_t home = home_bucket(key);
		const size_t home_stripe = stripe_of(home);
		// Buckets from home that the locked range must cover.
		size_t span = load_bound(v, home_stripe) + 1;
		for (;;) {
			const stripe_range locked = range_for(home, span);
			range_guard guard(*this, v, locked);
			const size_t bound = v.stripes[home_stripe].bound;
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
				const auto state = state_at(v, index);
				if (state != BUCKET_FILLED &&
				    free_slot == _num_buckets) {
					free_slot = index;
					free_distance = probe;
				}
				if (state == BUCKET_EMPTY)
					break;
				if (state == BUCKET_FILLED &&
				    keys_equal(key_at(v, index), key)) {
					if (mode == update_mode::no_exist)
						return update_result::exists;
					std::memcpy(value_at(v, index), value,
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
				if (state_at(v, index) != BUCKET_FILLED) {
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
			if (!(locked.count < _num_stripes ?
				      take_token(v, home_stripe) :
				      take_token_any(v, home_stripe))) {
				if (locked.count == _num_stripes)
					return update_result::full;
				span = _num_buckets; // Lock every stripe.
				continue;
			}
			// A lookup that read this bucket's previous key and then
			// sees any of the new bytes must also see the delete's new
			// generation.
			std::atomic_thread_fence(std::memory_order_release);
			std::memcpy(key_at(v, free_slot), key, _key_size);
			std::memcpy(value_at(v, free_slot), value, _value_size);
			// The bound covers the new bucket before lookups can see it.
			note_filled(v, home_stripe, free_distance);
			// Publishes the key and value.
			set_state(v, free_slot, BUCKET_FILLED, 0,
				  std::memory_order_release);
			return update_result::ok;
		}
	}

	bool elem_update(const void *key, const void *value)
	{
		return update(key, value, update_mode::any) == update_result::ok;
	}

	bool elem_delete(const void *key)
	{
		const view v = resolve();
		const size_t home = home_bucket(key);
		const size_t home_stripe = stripe_of(home);
		// The bound plus the bucket after it (release_deleted_run).
		size_t span = load_bound(v, home_stripe) + 2;
		for (;;) {
			const stripe_range locked = range_for(home, span);
			range_guard guard(*this, v, locked);
			const size_t bound = v.stripes[home_stripe].bound;
			if (bound + 2 > span && locked.count < _num_stripes) {
				span = bound + 2;
				continue;
			}
			size_t index = home;
			for (size_t probe = 0; probe <= bound; ++probe) {
				const auto state = state_at(v, index);
				if (state == BUCKET_EMPTY)
					return false; // Key not found
				if (state == BUCKET_FILLED &&
				    keys_equal(key_at(v, index), key)) {
					set_state(v, index, BUCKET_DELETED,
						  GENERATION_STEP,
						  std::memory_order_relaxed);
					return_token(v, home_stripe);
					note_released(v, home_stripe, probe);
					release_deleted_run(v, index, locked);
					return true;
				}
				index = advance(index, 1);
			}
			return false; // Key not found
		}
	}

	// The key after key in bucket order (the first one for a missing key or
	// nullptr), as the kernel. Keys added or deleted meanwhile may or may not
	// be seen. Each key is copied under its stripe's lock, so that it is not
	// overwritten meanwhile.
	bool next_key(const void *key, void *next_key)
	{
		const view v = resolve();
		size_t start = 0;
		if (key != nullptr) {
			const void *value = elem_lookup(key);
			if (value != nullptr)
				start = get_index_of_value(value) + 1;
		}
		for (size_t i = start; i < _num_buckets; i++) {
			if (state_at(v, i) != BUCKET_FILLED)
				continue;
			range_guard guard(*this, v, { stripe_of(i), 1 });
			if (state_at(v, i) != BUCKET_FILLED)
				continue;
			std::memcpy(next_key, key_at(v, i), _key_size);
			return true;
		}
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

	// Distances below this are counted exactly (see max_displacement).
	static constexpr size_t tracked_displacements()
	{
		return TRACKED_DISPLACEMENTS;
	}

	// Largest probe bound over all stripes.
	size_t max_displacement() const
	{
		const view v = resolve();
		size_t bound = 0;
		for (size_t s = 0; s < _num_stripes; s++)
			bound = std::max(bound, load_bound(v, s));
		return bound;
	}

	// Exact when no writer runs.
	size_t get_elem_count() const
	{
		const view v = resolve();
		int64_t free_tokens =
			std::atomic_ref<int64_t>(
				const_cast<int64_t &>(_pool_tokens))
				.load(std::memory_order_relaxed);
		for (size_t s = 0; s < _num_stripes; s++)
			free_tokens += v.stripes[s].tokens;
		return _max_element_count - (size_t)free_tokens;
	}
};

} // namespace bpftime

#endif // BPFTIME_HASH_MAP_HPP
