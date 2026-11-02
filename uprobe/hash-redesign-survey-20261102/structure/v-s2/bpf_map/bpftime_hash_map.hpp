#ifndef BPFTIME_HASH_MAP_HPP
#define BPFTIME_HASH_MAP_HPP

#include "map_common_def.hpp"
#include "platform_utils.hpp"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstring>
#include <stdexcept>
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

// The fixed-size hash map of the eBPF runtime (BPF_MAP_TYPE_HASH, and the key
// table of the per-CPU hash), laid out as the kernel's preallocated htab in
// offsets: max_entries preallocated elements {word, next, key, value}, and a
// power of two of chain heads, each the index of its chain's first element or
// a terminator that names the bucket (NULLS | bucket). Elements never move
// (lookups hand out value pointers, valid while the key stays, as with the
// kernel); a delete unlinks its element and frees it, so there are no
// tombstones and every value is stored once.
//
// It replaced (11-03) an open-addressing table with linear probing (buckets
// of key and value inline, twice max_entries of them, tombstones, the
// per-byte hash_func): keys with fixed low bits (id << 12) clustered into runs
// of thousands of buckets (1.7 us lookups), tombstones piled up under churn,
// and its striped writers needed probe-range locking and exact-capacity
// tokens (results uprobe/hash-redesign-survey-20261102).
//
// Lookups take no lock and write nothing shared. Each element's word holds a
// generation (every delete increments it) and the in-use bit. A lookup
// follows the chain from its head; at each element it reads the word
// (acquire), skips the element unless it is in use, compares the key, and
// accepts a match only if the word is still the same (the element was not
// freed and reused meanwhile). As in the kernel, a stale `next` is caught at
// the end of the walk, not at each step: an element freed while a lookup
// stands on it links into a free list (its elements are not in use, the
// list ends in a terminator naming no bucket) or, reused, into another
// chain, which ends in another bucket's terminator; either way the lookup
// starts again. A lookup cannot find its key in a wrong chain (a key's
// element is always in its own bucket's chain), and missing a key inserted
// meanwhile is allowed. (A re-check of the word before following `next` was
// tried: no test could tell it apart, and it cost a fence per step.) Writers lock the stripe of the key's head (a power of two of heads
// per stripe, one cache line per lock): one lock per operation.
//
// Free elements are kept in per-CPU lists (the kernel's pcpu_freelist), each
// under its own lock on its own line: an insert pops from its thread's list
// and, when that is empty, from the others one at a time; a delete pushes to
// its thread's list. The free elements are the capacity: E2BIG exactly when
// none is left. The list is chosen from the thread pointer (no system call,
// no TLS variable in the agent's namespace).
class bpftime_hash_map {
    public:
	enum class update_mode { any, no_exist, exist };
	enum class update_result { ok, exists, missing, full };

    private:
	static constexpr uint32_t NULLS = 0x80000000u;
	static constexpr uint32_t ELEM_USED = 1;
	static constexpr uint32_t GENERATION_STEP = 2;
	static constexpr size_t CACHE_LINE = 64;
	static constexpr size_t MIN_HEADS = 16;
	static constexpr size_t MIN_HEADS_PER_STRIPE = 16;
	static constexpr size_t MAX_STRIPES = 4096;
	static constexpr size_t MAX_LISTS = 64;

	struct alignas(CACHE_LINE) stripe_lock {
		pthread_spinlock_t lock;
	};
	struct alignas(CACHE_LINE) free_list {
		pthread_spinlock_t lock;
		uint32_t head; // element index, or list_end(): empty
		uint32_t count;
	};
	static_assert(sizeof(stripe_lock) == CACHE_LINE &&
		      sizeof(free_list) == CACHE_LINE);

	// The shared buffer resolved in this process.
	struct view {
		stripe_lock *stripes;
		free_list *lists;
		uint32_t *heads;
		uint8_t *elems;
	};

	static constexpr size_t round_up(size_t n, size_t align)
	{
		return (n + align - 1) / align * align;
	}
	static constexpr size_t value_align(size_t value_size)
	{
		return value_size >= 8 ? 8 : 4;
	}
	static size_t pow2_at_least(size_t n)
	{
		size_t p = 1;
		while (p < n)
			p <<= 1;
		return p;
	}
	static unsigned log2_of(size_t pow2)
	{
		return (unsigned)__builtin_ctzll((unsigned long long)pow2);
	}

	// Read by every lookup and written only at construction.
	size_t _key_size;
	size_t _value_size;
	size_t _value_offset; // from the element start
	size_t _elem_size; // a multiple of value_align (and of 4)
	size_t _max_elements;
	size_t _num_heads; // a power of two
	unsigned _head_shift; // 64 - log2(_num_heads)
	unsigned _stripe_shift; // log2 of the heads per stripe
	size_t _num_stripes;
	size_t _num_lists;
	size_t _lists_offset, _heads_offset, _elems_offset; // in the buffer
	bytes_vec buffer;

	view resolve() const
	{
		// The buffer starts at any 16-byte boundary; its first cache
		// line inside is the base. The segment is page aligned in every
		// process, so this is the same address offset in all of them.
		const uintptr_t raw = reinterpret_cast<uintptr_t>(buffer.data());
		uint8_t *base = reinterpret_cast<uint8_t *>(
			(raw + CACHE_LINE - 1) & ~uintptr_t{ CACHE_LINE - 1 });
		return { reinterpret_cast<stripe_lock *>(base),
			 reinterpret_cast<free_list *>(base + _lists_offset),
			 reinterpret_cast<uint32_t *>(base + _heads_offset),
			 base + _elems_offset };
	}

	inline uint8_t *elem_at(const view &v, uint32_t index) const
	{
		return v.elems + (size_t)index * _elem_size;
	}
	static inline uint32_t *word_of(uint8_t *elem)
	{
		return reinterpret_cast<uint32_t *>(elem);
	}
	static inline uint32_t *next_of(uint8_t *elem)
	{
		return reinterpret_cast<uint32_t *>(elem + 4);
	}
	static inline uint8_t *key_of(uint8_t *elem)
	{
		return elem + 8;
	}
	inline uint8_t *value_of(uint8_t *elem) const
	{
		return elem + _value_offset;
	}

	static inline uint32_t load(const uint32_t *p, std::memory_order order)
	{
		return std::atomic_ref<uint32_t>(*const_cast<uint32_t *>(p))
			.load(order);
	}
	static inline void store(uint32_t *p, uint32_t value,
				 std::memory_order order)
	{
		std::atomic_ref<uint32_t>(*p).store(value, order);
	}

	// 4- and 8-byte keys are their own hash; longer keys are mixed eight
	// bytes at a time. The bucket is the top bits of one multiply
	// (Fibonacci hashing), well mixed whatever the key's low bits are.
	inline uint64_t key_hash(const void *key) const
	{
		const auto *bytes = static_cast<const uint8_t *>(key);
		if (_key_size == 4) {
			uint32_t k;
			std::memcpy(&k, bytes, 4);
			return k;
		}
		if (_key_size == 8) {
			uint64_t k;
			std::memcpy(&k, bytes, 8);
			return k;
		}
		uint64_t h = _key_size * 0x9e3779b97f4a7c15ULL;
		size_t i = 0;
		for (; i + 8 <= _key_size; i += 8) {
			uint64_t w;
			std::memcpy(&w, bytes + i, 8);
			h = (h ^ w) * 0xbf58476d1ce4e5b9ULL;
			h ^= h >> 31;
		}
		if (i < _key_size) {
			// Byte by byte: a memcpy of a run-time size keeps w in
			// memory, and every caller gets a stack canary.
			uint64_t w = 0;
			for (size_t j = 0; i + j < _key_size; j++)
				w |= (uint64_t)bytes[i + j] << (8 * j);
			h = (h ^ w) * 0xbf58476d1ce4e5b9ULL;
			h ^= h >> 31;
		}
		return h;
	}
	inline uint32_t bucket_of(const void *key) const
	{
		return (uint32_t)((key_hash(key) * 0x9e3779b97f4a7c15ULL) >>
				  _head_shift);
	}
	inline size_t stripe_of(uint32_t bucket) const
	{
		return bucket >> _stripe_shift;
	}

	// A key or value copy: with a size known only at run time memcpy is a
	// call (about 15 instructions on aarch64); 4 and 8 bytes, the common
	// sizes, are inlined.
	__attribute__((always_inline)) static inline void
	copy_bytes(void *dst, const void *src, size_t size)
	{
		if (size == 8)
			std::memcpy(dst, src, 8);
		else if (size == 4)
			std::memcpy(dst, src, 4);
		else
			std::memcpy(dst, src, size);
	}

	inline bool keys_equal(const void *lhs, const void *rhs) const
	{
		if (_key_size == sizeof(uint32_t)) {
			uint32_t a, b;
			std::memcpy(&a, lhs, 4);
			std::memcpy(&b, rhs, 4);
			return a == b;
		}
		if (_key_size == sizeof(uint64_t)) {
			uint64_t a, b;
			std::memcpy(&a, lhs, 8);
			std::memcpy(&b, rhs, 8);
			return a == b;
		}
		return std::memcmp(lhs, rhs, _key_size) == 0;
	}

	// Ends every free list: a terminator naming no bucket, so that a
	// lookup that wandered into a free list (an element freed while it read
	// it) starts again instead of taking it for its own chain's end.
	inline uint32_t list_end() const
	{
		return NULLS | (uint32_t)_num_heads;
	}

	// The thread's free list: from the thread pointer, distinct per thread.
	inline size_t my_list() const
	{
		const uint64_t tp = (uint64_t)(uintptr_t)__builtin_thread_pointer();
		return (size_t)(((tp >> 4) * 0x9e3779b97f4a7c15ULL) >> 32) %
		       _num_lists;
	}

	// Under list's lock: an element index, or NULLS when it is empty.
	// Values, not out-parameters, throughout: a local whose address is
	// passed on stays in memory and gives the caller a stack canary.
	uint32_t pop_locked(const view &v, free_list &list)
	{
		const uint32_t index = list.head;
		if (index & NULLS)
			return NULLS;
		list.head = load(next_of(elem_at(v, index)),
				 std::memory_order_relaxed);
		store(&list.count, list.count - 1, std::memory_order_relaxed);
		return index;
	}

	// A free element: from this thread's list, else from the others one
	// lock at a time (try-locked, so that a process that died holding one
	// list's lock blocks only that list's own users). NULLS when none is
	// left: the map holds max_entries elements.
	uint32_t take_element(const view &v)
	{
		const size_t mine = my_list();
		{
			free_list &list = v.lists[mine];
			bpftime_spin_lock(&list.lock);
			const uint32_t index = pop_locked(v, list);
			bpftime_spin_unlock(&list.lock);
			if (!(index & NULLS))
				return index;
		}
		for (size_t pass = 0; pass < 2; pass++) {
			for (size_t k = 1; k < _num_lists; k++) {
				free_list &list = v.lists[(mine + k) % _num_lists];
				if (load(&list.count, std::memory_order_relaxed) == 0)
					continue;
				if (pass == 0) {
					if (pthread_spin_trylock(const_cast<
							pthread_spinlock_t *>(
							&list.lock)) != 0)
						continue;
				} else {
					bpftime_spin_lock(&list.lock);
				}
				const uint32_t index = pop_locked(v, list);
				bpftime_spin_unlock(&list.lock);
				if (!(index & NULLS))
					return index;
			}
		}
		return NULLS;
	}

	void give_element(const view &v, uint32_t index)
	{
		free_list &list = v.lists[my_list()];
		bpftime_spin_lock(&list.lock);
		store(next_of(elem_at(v, index)), list.head,
		      std::memory_order_relaxed);
		list.head = index;
		store(&list.count, list.count + 1, std::memory_order_relaxed);
		bpftime_spin_unlock(&list.lock);
	}

	// The link that points to the element holding key in bucket's chain
	// (under its stripe lock), or nullptr.
	uint32_t *find_link_locked(const view &v, uint32_t bucket,
				   const void *key)
	{
		uint32_t *link = &v.heads[bucket];
		uint32_t index = *link;
		while (!(index & NULLS)) {
			uint8_t *e = elem_at(v, index);
			if (keys_equal(key_of(e), key))
				return link;
			link = next_of(e);
			index = *link;
		}
		return nullptr;
	}

	// The element holding key in bucket's chain (under its stripe lock).
	uint8_t *find_locked(const view &v, uint32_t bucket, const void *key)
	{
		uint32_t *link = find_link_locked(v, bucket, key);
		return link ? elem_at(v, *link) : nullptr;
	}

	// read(value) runs after the key matched and before the word is
	// checked again, so what it reads belongs to the key when the lookup
	// succeeds (it may run again on a retry); it must read with acquire
	// loads.
	template <bool Uint32Key, class Read>
	void *lookup_in_bucket(const void *key, uint32_t bucket, Read &read)
	{
		const view v = resolve();
		uint32_t key_value = 0;
		if constexpr (Uint32Key)
			std::memcpy(&key_value, key, sizeof(key_value));
		const uint32_t end = NULLS | bucket;
	restart:
		uint32_t index = load(&v.heads[bucket], std::memory_order_acquire);
		for (size_t steps = 0;; steps++) {
			if (index & NULLS) {
				if (index == end)
					return nullptr;
				goto restart; // ended in another chain
			}
			if (index >= _max_elements || steps > _max_elements)
				goto restart;
			uint8_t *e = elem_at(v, index);
			const uint32_t *word = word_of(e);
			const uint32_t w = load(word, std::memory_order_acquire);
			if (!(w & ELEM_USED))
				goto restart;
			bool equal;
			if constexpr (Uint32Key) {
				// Acquire orders the reads below after it.
				equal = std::atomic_ref<uint32_t>(
						*reinterpret_cast<uint32_t *>(
							key_of(e)))
						.load(std::memory_order_acquire) ==
					key_value;
			} else {
				equal = std::memcmp(key_of(e), key, _key_size) ==
					0;
			}
			if (equal) {
				read(value_of(e));
				if constexpr (!Uint32Key)
					std::atomic_thread_fence(
						std::memory_order_acquire);
				if (load(word, std::memory_order_relaxed) == w)
					return value_of(e);
				goto restart;
			}
			index = load(next_of(e), std::memory_order_relaxed);
		}
	}

    public:
	bpftime_hash_map(managed_shared_memory &memory, size_t max_entries,
			 size_t key_size, size_t value_size)
		: _key_size(key_size), _value_size(value_size),
		  _value_offset(round_up(8 + key_size, value_align(value_size))),
		  _elem_size(round_up(_value_offset + value_size,
				      value_align(value_size))),
		  _max_elements(max_entries),
		  _num_heads(std::max(pow2_at_least(max_entries), MIN_HEADS)),
		  _head_shift(64 - log2_of(_num_heads)),
		  _num_stripes(std::min(_num_heads / MIN_HEADS_PER_STRIPE,
					MAX_STRIPES)),
		  buffer(memory.get_segment_manager())
	{
		if (max_entries == 0 || max_entries >= NULLS ||
		    _num_heads >= NULLS)
			throw std::length_error("hash map size out of range");
		_stripe_shift = log2_of(_num_heads) - log2_of(_num_stripes);
		const int cpus = bpftime_cpu_index_limit();
		_num_lists = std::min<size_t>(cpus > 0 ? (size_t)cpus : 1,
					      MAX_LISTS);
		_lists_offset = _num_stripes * CACHE_LINE;
		_heads_offset = _lists_offset + _num_lists * CACHE_LINE;
		_elems_offset =
			round_up(_heads_offset + _num_heads * sizeof(uint32_t), 16);
		size_t elems_bytes, total;
		if (__builtin_mul_overflow(_max_elements, _elem_size,
					   &elems_bytes) ||
		    __builtin_add_overflow(_elems_offset + CACHE_LINE,
					   elems_bytes, &total))
			throw std::length_error("hash map size overflows");
		buffer.resize(total, 0);
		const view v = resolve();
		for (size_t s = 0; s < _num_stripes; s++)
			pthread_spin_init(&v.stripes[s].lock,
					  PTHREAD_PROCESS_SHARED);
		for (size_t b = 0; b < _num_heads; b++)
			v.heads[b] = NULLS | (uint32_t)b;
		// Each list a contiguous share of the elements, in order.
		for (size_t l = 0; l < _num_lists; l++) {
			free_list &list = v.lists[l];
			pthread_spin_init(&list.lock, PTHREAD_PROCESS_SHARED);
			const size_t first = _max_elements * l / _num_lists,
				     last = _max_elements * (l + 1) / _num_lists;
			list.head = list_end();
			list.count = 0;
			for (size_t i = last; i-- > first;) {
				*next_of(elem_at(v, (uint32_t)i)) = list.head;
				list.head = (uint32_t)i;
				list.count++;
			}
		}
	}

	template <class Read> void *lookup_read(const void *key, Read &&read)
	{
		const uint32_t bucket = bucket_of(key);
		if (_key_size == sizeof(uint32_t))
			return lookup_in_bucket<true>(key, bucket, read);
		return lookup_in_bucket<false>(key, bucket, read);
	}

	void *elem_lookup(const void *key)
	{
		return lookup_read(key, [](const void *) {});
	}

	// fn(value) on the key's value under its stripe lock, so that a delete
	// cannot free the element meanwhile. False when the key is missing.
	template <class Fn> bool with_existing_value(const void *key, Fn &&fn)
	{
		const view v = resolve();
		const uint32_t bucket = bucket_of(key);
		stripe_lock &st = v.stripes[stripe_of(bucket)];
		bpftime_spin_lock(&st.lock);
		uint8_t *e = find_locked(v, bucket, key);
		if (e != nullptr)
			fn(value_of(e));
		bpftime_spin_unlock(&st.lock);
		return e != nullptr;
	}

	// Kernel htab semantics for the three modes. An existing key's value is
	// overwritten in place, as the kernel without BPF_F_LOCK: concurrent
	// readers may see a partly written value.
	update_result update(const void *key, const void *value,
			     update_mode mode)
	{
		const view v = resolve();
		const uint32_t bucket = bucket_of(key);
		stripe_lock &st = v.stripes[stripe_of(bucket)];
		bpftime_spin_lock(&st.lock);
		update_result result;
		if (uint8_t *e = find_locked(v, bucket, key); e != nullptr) {
			if (mode == update_mode::no_exist) {
				result = update_result::exists;
			} else {
				copy_bytes(value_of(e), value, _value_size);
				result = update_result::ok;
			}
		} else if (mode == update_mode::exist) {
			result = update_result::missing;
		} else {
			const uint32_t index = take_element(v);
			if (index & NULLS) {
				result = update_result::full;
			} else {
				uint8_t *e = elem_at(v, index);
				// A lookup that read this element's previous
				// key and then sees any of the new bytes must
				// also see the delete's new generation.
				std::atomic_thread_fence(
					std::memory_order_release);
				copy_bytes(key_of(e), key, _key_size);
				copy_bytes(value_of(e), value, _value_size);
				store(next_of(e), v.heads[bucket],
				      std::memory_order_relaxed);
				const uint32_t w =
					load(word_of(e), std::memory_order_relaxed);
				store(word_of(e), w | ELEM_USED,
				      std::memory_order_release);
				// Publishes the element.
				store(&v.heads[bucket], index,
				      std::memory_order_release);
				result = update_result::ok;
			}
		}
		bpftime_spin_unlock(&st.lock);
		return result;
	}

	bool elem_update(const void *key, const void *value)
	{
		return update(key, value, update_mode::any) == update_result::ok;
	}

	// value_out: the value is copied there first, under the stripe lock
	// (lookup and delete as one step, like the kernel's).
	bool elem_delete(const void *key, void *value_out = nullptr)
	{
		const view v = resolve();
		const uint32_t bucket = bucket_of(key);
		stripe_lock &st = v.stripes[stripe_of(bucket)];
		bpftime_spin_lock(&st.lock);
		uint32_t *link = find_link_locked(v, bucket, key);
		if (link == nullptr) {
			bpftime_spin_unlock(&st.lock);
			return false;
		}
		const uint32_t index = *link;
		uint8_t *e = elem_at(v, index);
		if (value_out != nullptr)
			copy_bytes(value_out, value_of(e), _value_size);
		const uint32_t w = load(word_of(e), std::memory_order_relaxed);
		// A new generation, not in use; readers on the element restart.
		store(word_of(e), (w & ~ELEM_USED) + GENERATION_STEP,
		      std::memory_order_relaxed);
		store(link, load(next_of(e), std::memory_order_relaxed),
		      std::memory_order_release);
		bpftime_spin_unlock(&st.lock);
		give_element(v, index);
		return true;
	}

	// The key after key in bucket order, chain order within a bucket (the
	// first one for a missing key or nullptr), as the kernel. Keys added or
	// deleted meanwhile may or may not be seen. Each key is copied under its
	// stripe's lock, so that it is not overwritten meanwhile.
	bool next_key(const void *key, void *next_key)
	{
		const view v = resolve();
		size_t bucket = 0;
		if (key != nullptr) {
			const uint32_t home = bucket_of(key);
			stripe_lock &st = v.stripes[stripe_of(home)];
			bpftime_spin_lock(&st.lock);
			uint8_t *e = find_locked(v, home, key);
			if (e != nullptr) {
				const uint32_t next = *next_of(e);
				if (!(next & NULLS)) {
					std::memcpy(next_key,
						    key_of(elem_at(v, next)),
						    _key_size);
					bpftime_spin_unlock(&st.lock);
					return true;
				}
				bucket = (size_t)home + 1;
			}
			bpftime_spin_unlock(&st.lock);
		}
		for (; bucket < _num_heads; bucket++) {
			if (load(&v.heads[bucket], std::memory_order_relaxed) &
			    NULLS)
				continue;
			stripe_lock &st = v.stripes[stripe_of((uint32_t)bucket)];
			bpftime_spin_lock(&st.lock);
			const uint32_t first = v.heads[bucket];
			if (!(first & NULLS)) {
				std::memcpy(next_key, key_of(elem_at(v, first)),
					    _key_size);
				bpftime_spin_unlock(&st.lock);
				return true;
			}
			bpftime_spin_unlock(&st.lock);
		}
		return false;
	}

	// Element index of a value pointer handed out by a lookup.
	inline size_t get_index_of_value(const void *value) const
	{
		const auto *p = static_cast<const uint8_t *>(value);
		return (size_t)(p - _value_offset - resolve().elems) / _elem_size;
	}

	// Chain heads (buckets): a power of two, at least max_entries.
	size_t bucket_count() const
	{
		return _num_heads;
	}

	size_t stripe_count() const
	{
		return _num_stripes;
	}

	size_t free_list_count() const
	{
		return _num_lists;
	}

	// Tests: the bucket and stripe of a key, and holding locks as writers
	// and free-list users would.
	uint32_t bucket_of_key(const void *key) const
	{
		return bucket_of(key);
	}

	size_t stripe_of_key(const void *key) const
	{
		return stripe_of(bucket_of(key));
	}

	void lock_stripe_for_test(size_t stripe) const
	{
		bpftime_spin_lock(&resolve().stripes[stripe].lock);
	}

	void unlock_stripe_for_test(size_t stripe) const
	{
		bpftime_spin_unlock(&resolve().stripes[stripe].lock);
	}

	void lock_free_list_for_test(size_t list) const
	{
		bpftime_spin_lock(&resolve().lists[list].lock);
	}

	void unlock_free_list_for_test(size_t list) const
	{
		bpftime_spin_unlock(&resolve().lists[list].lock);
	}

	// Elements in the chain of the key's bucket (tests and measurements;
	// exact when no writer runs).
	size_t chain_length_of_key(const void *key) const
	{
		const view v = resolve();
		size_t n = 0;
		for (uint32_t i = v.heads[bucket_of(key)]; !(i & NULLS);
		     i = *next_of(elem_at(v, i)))
			n++;
		return n;
	}

	// Exact when no writer runs.
	size_t get_elem_count() const
	{
		const view v = resolve();
		size_t free_count = 0;
		for (size_t l = 0; l < _num_lists; l++)
			free_count +=
				load(&v.lists[l].count, std::memory_order_relaxed);
		return _max_elements - free_count;
	}
};

} // namespace bpftime

#endif // BPFTIME_HASH_MAP_HPP
