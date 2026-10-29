#ifndef BPFTIME_HASH_MAP_HPP
#define BPFTIME_HASH_MAP_HPP

#include "map_common_def.hpp"
#include <atomic>
#include <cassert>
#include <cstring>

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
// Lookups take no lock; writers (update, delete, get_next_key) are
// serialized by the caller. Each bucket starts with a 4-byte word: the low
// two bits are the state, the rest a generation that every delete
// increments. Filled buckets never move (lookups hand out value pointers), so
// a lookup can only be misled by a bucket deleted and refilled with another
// key while it compares the key bytes: it reads the word, compares, and
// accepts the match only if the word is unchanged (a seqlock per bucket).
// A lookup writes nothing shared and never waits for a writer.
class bpftime_hash_map {
    private:
	static constexpr uint32_t BUCKET_EMPTY = 0;
	static constexpr uint32_t BUCKET_FILLED = 1;
	static constexpr uint32_t BUCKET_DELETED = 2;
	static constexpr uint32_t STATE_MASK = 3;
	static constexpr uint32_t GENERATION_STEP = 4;

	inline size_t get_elem_offset(size_t index) const
	{
		return index * _elem_size;
	}

	inline uint32_t *state_word(size_t index)
	{
		return reinterpret_cast<uint32_t *>(
			&data_buffer[get_elem_offset(index)]);
	}

	inline const uint32_t *state_word(size_t index) const
	{
		return reinterpret_cast<const uint32_t *>(
			&data_buffer[get_elem_offset(index)]);
	}

	inline uint32_t load_word(size_t index, std::memory_order order) const
	{
		// atomic_ref of a const object is C++26.
		return std::atomic_ref<uint32_t>(
			       *const_cast<uint32_t *>(state_word(index)))
			.load(order);
	}

	inline void store_word(size_t index, uint32_t word,
			       std::memory_order order)
	{
		std::atomic_ref<uint32_t>(*state_word(index)).store(word, order);
	}

	inline size_t load_max_displacement() const
	{
		return std::atomic_ref<size_t>(
			       const_cast<size_t &>(_max_displacement))
			.load(std::memory_order_acquire);
	}

	// Writers only; stores only when the bound changes, since lookups read
	// it from the same cache line as the table's geometry.
	inline void set_max_displacement(size_t bound)
	{
		if (bound != _max_displacement)
			std::atomic_ref<size_t>(_max_displacement)
				.store(bound, std::memory_order_release);
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

	// Every filled bucket lies at most _max_displacement buckets after its
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
		for (;;) {
			bool changed = false;
			size_t index = home;
			const size_t bound = load_max_displacement();
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

	void note_filled(size_t distance)
	{
		if (distance < TRACKED_DISPLACEMENTS) {
			_displacement_counts[distance]++;
		} else {
			_far_count++;
			if (distance > _far_max)
				_far_max = distance;
		}
		if (distance > _max_displacement)
			set_max_displacement(distance);
	}

	void note_released(size_t distance)
	{
		size_t bound;
		if (distance < TRACKED_DISPLACEMENTS) {
			// Common case: the bound only changes when the last element
			// at the current largest distance goes away.
			if (--_displacement_counts[distance] != 0 ||
			    distance != _max_displacement)
				return;
			bound = distance;
		} else {
			if (--_far_count != 0)
				return; // The bound stays at _far_max.
			_far_max = 0;
			bound = TRACKED_DISPLACEMENTS - 1;
		}
		while (bound > 0 && _displacement_counts[bound] == 0)
			bound--;
		set_max_displacement(bound);
	}

	// A deleted bucket followed by an empty one lies on no filled bucket's
	// probe sequence, so it and the deleted run before it become empty.
	// Filled buckets never move: lookups hand out value pointers.
	void release_deleted_run(size_t index)
	{
		const size_t next = index + 1 == _num_buckets ? 0 : index + 1;
		if (get_bucket_state(next) != BUCKET_EMPTY)
			return;
		while (get_bucket_state(index) == BUCKET_DELETED) {
			set_empty(index);
			index = index == 0 ? _num_buckets - 1 : index - 1;
		}
	}

	// Read by every lookup and written only at construction (the bound only
	// when it changes): kept on cache lines of their own, away from the
	// counters that every insert and delete writes.
	size_t _key_size;
	size_t _value_size;
	size_t _elem_size; // state word + key + value, a multiple of 4
	size_t _num_buckets; // Adjusted number of buckets (prime)
	size_t _bucket_reciprocal; // Precomputed for fast initial bucket mapping
	size_t _max_element_count; // Maximum number of elements based on the
				   // original num_buckets
	size_t _max_displacement; // Largest distance of a filled bucket from
				  // its home bucket (see note_filled)

	// The data is stored in a contiguous memory region.
	// The layout of elem is:
	// 4 bytes state and generation
	// key_size bytes for the key
	// value_size bytes for the value
	// padding to a multiple of 4 bytes (the state word is atomic)
	bytes_vec data_buffer;

	char _read_mostly_end[64] = {};

	size_t _count; // Current number of elements
	// Filled buckets per exact distance from the home bucket, so the probe
	// bound shrinks again when far elements are deleted. Distances from
	// TRACKED_DISPLACEMENTS on are only counted; while any remain, the bound
	// stays at the largest such distance seen.
	static constexpr size_t TRACKED_DISPLACEMENTS = 64;
	uint32_t _displacement_counts[TRACKED_DISPLACEMENTS] = {};
	size_t _far_count = 0;
	size_t _far_max = 0;

	char _write_hot_end[64] = {};

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
		  _max_element_count(num_buckets), _max_displacement(0),
		  data_buffer(memory.get_segment_manager()),
		  // Initialize count to 0
		  _count(0)
	{
		data_buffer.resize(_num_buckets * _elem_size, 0);
	}

	inline uint32_t get_bucket_state(size_t index) const
	{
		return load_word(index, std::memory_order_relaxed) & STATE_MASK;
	}

	inline bool is_empty(size_t index) const
	{
		return get_bucket_state(index) != BUCKET_FILLED;
	}

	// Writers only (serialized by the caller), so the word's generation can
	// be read without ordering.
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
		const size_t index = bpftime_hasher::fast_modulo(
			bpftime_hasher::hash_func(key, _key_size), _num_buckets,
			_bucket_reciprocal);
		if (_key_size == sizeof(uint32_t))
			return lookup_from_bucket<true>(key, index);
		return lookup_from_bucket<false>(key, index);
	}

	bool elem_update(const void *key, const void *value)
	{
		size_t index = bpftime_hasher::fast_modulo(
			bpftime_hasher::hash_func(key, _key_size), _num_buckets,
			_bucket_reciprocal);
		size_t free_slot = _num_buckets;
		size_t free_distance = 0;
		// An existing key is within _max_displacement of its home bucket.
		size_t probe = 0;
		for (; probe <= _max_displacement; ++probe) {
			const auto state = get_bucket_state(index);
			if (state != BUCKET_FILLED && free_slot == _num_buckets) {
				free_slot = index;
				free_distance = probe;
			}
			if (state == BUCKET_EMPTY)
				break;
			if (state == BUCKET_FILLED &&
			    keys_equal(get_key(index), key)) {
				// In place, as the kernel without BPF_F_LOCK:
				// concurrent readers may see a partly written value.
				std::memcpy(get_value(index), value, _value_size);
				return true;
			}
			if (++index == _num_buckets)
				index = 0;
		}
		if (_count >= _max_element_count)
			return false; // Reject a new key if the hash map is full
		// No free bucket within the current displacement: take the next one.
		for (; free_slot == _num_buckets && probe < _num_buckets;
		     ++probe) {
			if (get_bucket_state(index) != BUCKET_FILLED) {
				free_slot = index;
				free_distance = probe;
			} else if (++index == _num_buckets) {
				index = 0;
			}
		}
		if (free_slot == _num_buckets)
			return false;
		// A lookup that read this bucket's previous key and then sees any
		// of the new bytes must also see the delete's new generation.
		std::atomic_thread_fence(std::memory_order_release);
		std::memcpy(get_key(free_slot), key, _key_size);
		std::memcpy(get_value(free_slot), value, _value_size);
		_count++;
		// The bound covers the new bucket before lookups can see it.
		note_filled(free_distance);
		set_filled(free_slot);
		return true;
	}

	bool elem_delete(const void *key)
	{
		size_t index = bpftime_hasher::fast_modulo(
			bpftime_hasher::hash_func(key, _key_size), _num_buckets,
			_bucket_reciprocal);
		for (size_t probe = 0; probe <= _max_displacement; ++probe) {
			const auto state = get_bucket_state(index);
			if (state == BUCKET_EMPTY)
				return false; // Key not found
			if (state == BUCKET_FILLED &&
			    keys_equal(get_key(index), key)) {
				set_deleted(index);
				_count--;
				note_released(probe);
				release_deleted_run(index);
				return true;
			}
			if (++index == _num_buckets)
				index = 0;
		}
		return false; // Key not found
	}

	size_t bucket_count() const
	{
		return _num_buckets;
	}

	size_t max_displacement() const
	{
		return _max_displacement;
	}

	size_t get_elem_count() const
	{
		return _count;
	}
};

} // namespace bpftime

#endif // BPFTIME_HASH_MAP_HPP
