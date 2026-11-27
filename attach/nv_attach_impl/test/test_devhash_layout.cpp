#include "catch2/catch_test_macros.hpp"
#include "trampoline/gpu_devhash_layout.h"
#include <cstdint>
#include <vector>

// The device hash map's one layout definition (1533; plan
// results gpu/hash-build-plan-20261127, step S1). Every other copy of these
// formulas is checked against this header: the device functions by their
// layout fingerprint, the routing pass's inline text on the GPU.
using namespace bpftime_devhash;

namespace
{
const uint64_t ENTRIES[] = { 1, 2, 48, 512, 513, 1024, MAX_ENTRIES_LIMIT };
const uint64_t SHARDS[] = { 32, 256, 512 };

// Round two's C5 home slot, written as c5.cu has it (seed as a parameter).
uint32_t c5_home_slot(uint32_t K, uint64_t seed, uint32_t logS)
{
	return (uint32_t)((((uint64_t)K ^ seed) * 0x9E3779B97F4A7C15ull) >>
			  (64 - logS));
}

// FNV-1a, written here independently of the header's.
struct fnv {
	uint64_t h = 0xcbf29ce484222325ull;
	void add(uint64_t v)
	{
		for (int i = 0; i < 8; i++) {
			h ^= (v >> (8 * i)) & 0xff;
			h *= 0x100000001b3ull;
		}
	}
};
} // namespace

TEST_CASE("Device hash layout: regions, sizes and geometry",
	  "[devhash-layout]")
{
	for (uint64_t m : ENTRIES) {
		for (uint64_t s : SHARDS) {
			INFO("max_entries " << m << " shards " << s);
			const layout l = layout_for(m, s);
			// R = min(max_entries, 512)
			REQUIRE(l.r == (m < 512 ? m : 512));
			// slots: a power of two, at least 2 x max_entries
			REQUIRE((l.slots & (l.slots - 1)) == 0);
			REQUIRE(l.slots >= 2 * m);
			REQUIRE(l.slots >= 2);
			REQUIRE(l.slots < 4 * m + 2);
			REQUIRE((uint64_t(1) << l.log_slots) == l.slots);
			// regions in order, aligned, not overlapping
			REQUIRE(l.ctl == 0);
			REQUIRE(l.counters >= uint64_t(CTL_WORDS) * 8);
			REQUIRE(l.dir >=
				l.counters + uint64_t(NUM_COUNTERS) * s * 8);
			REQUIRE(l.rows >= l.dir + l.slots * 8);
			REQUIRE(l.single >= l.rows + l.r * s * 8);
			REQUIRE(l.total >= l.single + (m - l.r) * 8);
			for (uint64_t off : { l.counters, l.dir, l.rows,
					      l.single, l.total })
				REQUIRE(off % REGION_ALIGN == 0);
			// every row has a home: r sharded rows and m - r single
			REQUIRE(value_offset(l, 0, 0) == l.rows);
			REQUIRE(value_offset(l, m - 1, uint32_t(s - 1)) <
				l.total);
			if (m > l.r) {
				REQUIRE(value_offset(l, l.r, 7) == l.single);
				REQUIRE(value_offset(l, m - 1, 7) ==
					l.single + (m - 1 - l.r) * 8);
			}
			REQUIRE(value_offset(l, l.r - 1, uint32_t(s - 1)) ==
				l.rows + (l.r * s - 1) * 8);
			REQUIRE(counter_offset(l, NUM_COUNTERS - 1,
					       uint32_t(s - 1)) ==
				l.counters + (uint64_t(NUM_COUNTERS) * s - 1) * 8);
			// the inline path's 32-bit row * shards + shard
			REQUIRE(l.r * s * 8 < (uint64_t(1) << 32));
			REQUIRE(geometry_valid(m, s));
		}
	}
	REQUIRE_FALSE(geometry_valid(0, 256));
	REQUIRE_FALSE(geometry_valid(MAX_ENTRIES_LIMIT + 1, 256));
	REQUIRE_FALSE(geometry_valid(16, 16));
	REQUIRE_FALSE(geometry_valid(16, 96));
	REQUIRE(slots_for(1) == 2);
	REQUIRE(slots_for(48) == 128);
	REQUIRE(slots_for(1024) == 2048);
	REQUIRE(slots_for(513) == 2048);
}

TEST_CASE("Device hash home slots are round two's", "[devhash-layout]")
{
	for (uint32_t logS = 1; logS <= 23; logS++) {
		for (uint32_t i = 0; i < 10000; i++) {
			const uint32_t K = i * 2654435761u + logS;
			const uint32_t h = home_slot(K, logS);
			REQUIRE(h == c5_home_slot(K, SEED, logS));
			REQUIRE(h < (uint32_t(1) << logS));
		}
	}
	// the shard formula is 1531's
	REQUIRE(shard_index(9, 31, 256) == ((9u & 7u) << 5 | 31u));
	REQUIRE(shard_index(3, 4, 32) == 4u);
}

TEST_CASE("Device hash headers and staging decode what they encode",
	  "[devhash-layout]")
{
	const uint32_t keys[] = { 0u, 1u, 0x7fffffffu, 0xffffffffu };
	const uint64_t rows[] = { 0, 1, 511, 512, MAX_ENTRIES_LIMIT - 1,
				  ROW_MASK };
	for (uint32_t k : keys) {
		for (uint64_t r : rows) {
			for (uint64_t st : { ST_CLAIMING, ST_LIVE, ST_REFUSED }) {
				const uint64_t h = make_header(k, r, st);
				REQUIRE(h != 0);
				REQUIRE(key_of(h) == k);
				REQUIRE(row_of(h) == r);
				REQUIRE(state_of(h) == st);
				REQUIRE(is_keyed(h));
				REQUIRE(holds_key(h, k));
				REQUIRE_FALSE(holds_key(h, k + 1));
			}
		}
	}
	// opaque: exactly the word 4, occupied, no key (key 0 included)
	REQUIRE(OPAQUE_WORD == 4);
	REQUIRE(state_of(OPAQUE_WORD) == ST_OPAQUE);
	REQUIRE_FALSE(is_keyed(OPAQUE_WORD));
	REQUIRE_FALSE(holds_key(OPAQUE_WORD, 0));
	REQUIRE_FALSE(holds_key(ST_EMPTY, 0));
	REQUIRE_FALSE(holds_key(make_header(0, 0, 5), 0));
	// staging: {header, value} per slot, then counters, then the count
	for (uint64_t m : ENTRIES) {
		const staging_layout st = staging_for(m);
		REQUIRE(st.slots == slots_for(m));
		REQUIRE(staging_header_offset(st.slots - 1) + 16 ==
			st.counters);
		REQUIRE(staging_value_offset(3) == 3 * 16 + 8);
		REQUIRE(st.count == st.counters + uint64_t(NUM_COUNTERS) * 8);
		REQUIRE(st.total == st.count + 8);
	}
}

TEST_CASE("Device hash layout fingerprint covers constants and offsets",
	  "[devhash-layout]")
{
	fnv f;
	const uint64_t constants[] = {
		TYPE_ID,	   ST_EMPTY,	       ST_CLAIMING,
		ST_LIVE,	   ST_REFUSED,	       ST_OPAQUE,
		STATE_MASK,	   ROW_SHIFT,	       ROW_BITS,
		KEY_SHIFT,	   SEED,	       HASH_MUL,
		REGION_ALIGN,	   CTL_MAGIC,	       CTL_MAX_ENTRIES,
		CTL_SLOTS,	   CTL_R,	       CTL_SHARDS,
		CTL_COUNT,	   CTL_TEST_OUTCOMES,  CTL_TEST_DIAG,
		CTL_WORDS,	   MAGIC,	       CNT_EEXIST,
		CNT_E2BIG,	   CNT_UNSTABLE_EEXIST, CNT_UNSTABLE_E2BIG,
		CNT_BAD_FLAGS,	   CNT_LOOKUP_FULLWALK, CNT_INSERT_FULLWALK,
		NUM_COUNTERS
	};
	for (uint64_t c : constants)
		f.add(c);
	const uint64_t sample_entries[] = { 1,	 2,    48,  512,
					    513, 1024, MAX_ENTRIES_LIMIT };
	for (uint64_t m : sample_entries) {
		for (uint64_t s : { uint64_t(256), uint64_t(512) }) {
			const layout l = layout_for(m, s);
			for (uint64_t v :
			     { l.slots, uint64_t(l.log_slots), l.r, l.ctl,
			       l.counters, l.dir, l.rows, l.single, l.total })
				f.add(v);
			f.add(value_offset(l, l.r ? l.r - 1 : 0, 3));
			f.add(value_offset(l, m - 1, 3));
			f.add(counter_offset(l, NUM_COUNTERS - 1, 5));
			f.add(home_slot(uint32_t(m), l.log_slots));
		}
		const staging_layout st = staging_for(m);
		f.add(st.counters);
		f.add(st.count);
		f.add(st.total);
	}
	REQUIRE(DEVHASH_LAYOUT_FP == f.h);
	REQUIRE(layout_fingerprint() == f.h);
}
