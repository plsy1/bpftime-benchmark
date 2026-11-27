// Layout of the GPU device hash map (BPF_MAP_TYPE_GPU_DEVICE_HASH_MAP, 1533).
//
// The one definition of the hash, the slot header encoding, the device block
// and the snapshot's staging. Users:
// - the device functions (devhash/devhash.cu, shipped as ../devhash_ptx.h,
//   which has every constant and offset below compiled in: after changing
//   this file, regenerate it with make -C attach/nv_attach_impl/devhash);
// - the routing pass's inline hit path (constants taken from here at patch
//   time);
// - the runtime map object (seal image, delivered mirror, takeover image);
// - the snapshot set-up and the tests.
// DEVHASH_LAYOUT_FP covers the constants and the offsets at sample
// geometries; devhash_ptx.h returns the value it was built with from
// __bpftime_devhash_layout_fp, and [devhash-layout] compares the two.
//
// Header word: key << 32 | row << 3 | state.
//   0 empty, 1 claiming (no row yet), 2 live, 3 refused (no row, permanent),
//   4 opaque: exactly the word 4, occupied with no key. Only the host writes
//   it (the CPU mirror's gap repair, takeover seeding).
// A walk decides on the state first: 0 stops, 4 and states above 4 are
// walked past, and 1-3 belong to the key in the key field. (C5, round two,
// compared the key first, which would take opaque for key 0's slot.)
//
// Device block (extra_buffer, zeroed, regions REGION_ALIGN-aligned):
//   control words [CTL_WORDS]  magic, geometry, the reservation count
//   counters      [NUM_COUNTERS][shards]  loss counts, (SM, lane) sharded
//   directory     [slots]      headers
//   rows          [r][shards]  values of rows below r, (SM, lane) sharded
//   single rows   [max_entries - r]  one word each
#ifndef BPFTIME_GPU_DEVHASH_LAYOUT_H
#define BPFTIME_GPU_DEVHASH_LAYOUT_H

#include <cstdint>

#if defined(__CUDA__)
#define DEVHASH_HD __attribute__((host)) __attribute__((device))
#else
#define DEVHASH_HD
#endif

namespace bpftime_devhash
{
constexpr uint32_t MAP_TYPE = 1533;

constexpr uint64_t ST_EMPTY = 0;
constexpr uint64_t ST_CLAIMING = 1;
constexpr uint64_t ST_LIVE = 2;
constexpr uint64_t ST_REFUSED = 3;
constexpr uint64_t ST_OPAQUE = 4;
constexpr uint64_t OPAQUE_WORD = ST_OPAQUE;
constexpr uint64_t STATE_MASK = 7;
constexpr unsigned ROW_SHIFT = 3;
constexpr unsigned ROW_BITS = 29;
constexpr uint64_t ROW_MASK = (uint64_t(1) << ROW_BITS) - 1;
constexpr unsigned KEY_SHIFT = 32;

constexpr uint64_t SEED = 0x2545f4914f6cdd1dull;
constexpr uint64_t HASH_MUL = 0x9e3779b97f4a7c15ull;

constexpr uint64_t MAX_ENTRIES_LIMIT = uint64_t(1) << 22;
constexpr uint64_t R_LIMIT = 512;
constexpr uint64_t REGION_ALIGN = 256;
constexpr uint64_t MIN_SHARDS = 32;
constexpr uint64_t MAX_SHARDS = uint64_t(1) << 16;

// Control words (u64 each).
enum : uint32_t {
	CTL_MAGIC = 0,
	CTL_MAX_ENTRIES = 1,
	CTL_SLOTS = 2,
	CTL_R = 3,
	CTL_SHARDS = 4,
	// The reservation count: a winner's atom.acq_rel add takes its old
	// value as the row (refused at max_entries and above). Never goes down.
	CTL_COUNT = 5,
	// Test builds only (BPFTIME_DEVHASH_TEST): the address of a per-thread
	// outcome array, and diagnostic counters from CTL_TEST_DIAG on.
	CTL_TEST_OUTCOMES = 8,
	CTL_TEST_DIAG = 9,
	CTL_WORDS = 32
};
constexpr uint64_t MAGIC = 0x3135333368736864ull; // "dhsh3351"

// Loss counters, [counter][shard].
enum : uint32_t {
	CNT_EEXIST = 0,
	CNT_E2BIG = 1,
	CNT_UNSTABLE_EEXIST = 2,
	CNT_UNSTABLE_E2BIG = 3,
	CNT_BAD_FLAGS = 4,
	CNT_LOOKUP_FULLWALK = 5,
	CNT_INSERT_FULLWALK = 6,
	NUM_COUNTERS = 7
};

DEVHASH_HD constexpr uint64_t make_header(uint32_t key, uint64_t row,
					  uint64_t state)
{
	return (uint64_t(key) << KEY_SHIFT) | ((row & ROW_MASK) << ROW_SHIFT) |
	       (state & STATE_MASK);
}
DEVHASH_HD constexpr uint64_t state_of(uint64_t h)
{
	return h & STATE_MASK;
}
DEVHASH_HD constexpr uint32_t key_of(uint64_t h)
{
	return uint32_t(h >> KEY_SHIFT);
}
DEVHASH_HD constexpr uint64_t row_of(uint64_t h)
{
	return (h >> ROW_SHIFT) & ROW_MASK;
}
// A header that belongs to a key (claiming, live or refused).
DEVHASH_HD constexpr bool is_keyed(uint64_t h)
{
	return state_of(h) >= ST_CLAIMING && state_of(h) <= ST_REFUSED;
}
DEVHASH_HD constexpr bool holds_key(uint64_t h, uint32_t key)
{
	return is_keyed(h) && key_of(h) == key;
}

DEVHASH_HD constexpr uint64_t next_pow2(uint64_t v)
{
	uint64_t p = 1;
	while (p < v)
		p <<= 1;
	return p;
}
DEVHASH_HD constexpr uint32_t log2_pow2(uint64_t p)
{
	uint32_t l = 0;
	while ((uint64_t(1) << l) < p)
		l++;
	return l;
}
// The directory: next_pow2(2 x max_entries) slots, at least 2.
DEVHASH_HD constexpr uint64_t slots_for(uint64_t max_entries)
{
	return next_pow2(2 * max_entries < 2 ? 2 : 2 * max_entries);
}
DEVHASH_HD constexpr uint32_t log_slots_for(uint64_t max_entries)
{
	return log2_pow2(slots_for(max_entries));
}
DEVHASH_HD constexpr uint64_t r_for(uint64_t max_entries)
{
	return max_entries < R_LIMIT ? max_entries : R_LIMIT;
}
// K's home slot (log_slots >= 1).
DEVHASH_HD constexpr uint32_t home_slot(uint32_t key, uint32_t log_slots)
{
	return uint32_t(((uint64_t(key) ^ SEED) * HASH_MUL) >>
			(64 - log_slots));
}
// The (SM, lane) shard, as 1531's (shards a power of two, at least 32).
DEVHASH_HD constexpr uint32_t shard_index(uint32_t smid, uint32_t laneid,
					  uint64_t shards)
{
	return ((smid & uint32_t(shards / 32 - 1)) << 5) | laneid;
}

DEVHASH_HD constexpr uint64_t align_up(uint64_t v)
{
	return (v + REGION_ALIGN - 1) / REGION_ALIGN * REGION_ALIGN;
}

struct layout {
	uint64_t max_entries;
	uint64_t shards;
	uint64_t slots;
	uint32_t log_slots;
	uint64_t r;
	uint64_t ctl; // byte offsets from the block's start
	uint64_t counters;
	uint64_t dir;
	uint64_t rows;
	uint64_t single;
	uint64_t total; // bytes
};

DEVHASH_HD constexpr layout layout_for(uint64_t max_entries, uint64_t shards)
{
	layout l{};
	l.max_entries = max_entries;
	l.shards = shards;
	l.slots = slots_for(max_entries);
	l.log_slots = log_slots_for(max_entries);
	l.r = r_for(max_entries);
	l.ctl = 0;
	l.counters = align_up(l.ctl + uint64_t(CTL_WORDS) * 8);
	l.dir = align_up(l.counters + uint64_t(NUM_COUNTERS) * shards * 8);
	l.rows = align_up(l.dir + l.slots * 8);
	l.single = align_up(l.rows + l.r * shards * 8);
	l.total = align_up(l.single + (max_entries - l.r) * 8);
	return l;
}

// The geometry the block and the inline path rely on.
DEVHASH_HD constexpr bool geometry_valid(uint64_t max_entries, uint64_t shards)
{
	return max_entries >= 1 && max_entries <= MAX_ENTRIES_LIMIT &&
	       shards >= MIN_SHARDS && shards <= MAX_SHARDS &&
	       (shards & (shards - 1)) == 0 &&
	       // the inline path forms row * shards + shard in 32 bits
	       r_for(max_entries) * shards * 8 < (uint64_t(1) << 32);
}

DEVHASH_HD constexpr uint64_t ctl_offset(uint32_t word)
{
	return uint64_t(word) * 8;
}
DEVHASH_HD constexpr uint64_t counter_offset(const layout &l, uint32_t counter,
					     uint32_t shard)
{
	return l.counters + (uint64_t(counter) * l.shards + shard) * 8;
}
DEVHASH_HD constexpr uint64_t slot_offset(const layout &l, uint64_t slot)
{
	return l.dir + slot * 8;
}
// Where a row's value for one shard lives: sharded below r, one word above.
DEVHASH_HD constexpr uint64_t value_offset(const layout &l, uint64_t row,
					   uint32_t shard)
{
	return row < l.r ? l.rows + (row * l.shards + shard) * 8 :
			   l.single + (row - l.r) * 8;
}

// The snapshot's staging (pinned host memory, written by the snapshot
// kernel, read by the delivery): per slot {raw header, value}, then the
// summed counters, then the count.
struct staging_layout {
	uint64_t slots;
	uint64_t slot_base; // byte offsets
	uint64_t counters;
	uint64_t count;
	uint64_t total;
};
DEVHASH_HD constexpr staging_layout staging_for(uint64_t max_entries)
{
	staging_layout s{};
	s.slots = slots_for(max_entries);
	s.slot_base = 0;
	s.counters = s.slots * 16;
	s.count = s.counters + uint64_t(NUM_COUNTERS) * 8;
	s.total = s.count + 8;
	return s;
}
DEVHASH_HD constexpr uint64_t staging_header_offset(uint64_t slot)
{
	return slot * 16;
}
DEVHASH_HD constexpr uint64_t staging_value_offset(uint64_t slot)
{
	return slot * 16 + 8;
}

// FNV-1a over every constant and the layout at sample geometries.
DEVHASH_HD constexpr uint64_t fp_mix(uint64_t h, uint64_t v)
{
	for (int i = 0; i < 8; i++) {
		h ^= (v >> (8 * i)) & 0xff;
		h *= 0x100000001b3ull;
	}
	return h;
}
// Host only: device code uses the constant DEVHASH_LAYOUT_FP.
constexpr uint64_t layout_fingerprint()
{
	const uint64_t sample_entries[] = { 1,	 2,    48,
					    512, 513,  1024,
					    MAX_ENTRIES_LIMIT };
	const uint64_t sample_shards[] = { 256, 512 };
	uint64_t h = 0xcbf29ce484222325ull;
	const uint64_t constants[] = {
		MAP_TYPE,      ST_EMPTY,	  ST_CLAIMING,	  ST_LIVE,
		ST_REFUSED,    ST_OPAQUE,	  STATE_MASK,	  ROW_SHIFT,
		ROW_BITS,      KEY_SHIFT,	  SEED,		  HASH_MUL,
		REGION_ALIGN,  CTL_MAGIC,	  CTL_MAX_ENTRIES, CTL_SLOTS,
		CTL_R,	       CTL_SHARDS,	  CTL_COUNT,	  CTL_TEST_OUTCOMES,
		CTL_TEST_DIAG, CTL_WORDS,	  MAGIC,	  CNT_EEXIST,
		CNT_E2BIG,     CNT_UNSTABLE_EEXIST, CNT_UNSTABLE_E2BIG,
		CNT_BAD_FLAGS, CNT_LOOKUP_FULLWALK, CNT_INSERT_FULLWALK,
		NUM_COUNTERS
	};
	for (uint64_t c : constants)
		h = fp_mix(h, c);
	for (uint64_t m : sample_entries) {
		for (uint64_t s : sample_shards) {
			const layout l = layout_for(m, s);
			const uint64_t fields[] = { l.slots,	l.log_slots,
						    l.r,	l.ctl,
						    l.counters, l.dir,
						    l.rows,	l.single,
						    l.total };
			for (uint64_t f : fields)
				h = fp_mix(h, f);
			h = fp_mix(h, value_offset(l, l.r ? l.r - 1 : 0, 3));
			h = fp_mix(h, value_offset(l, m - 1, 3));
			h = fp_mix(h, counter_offset(l, NUM_COUNTERS - 1, 5));
			h = fp_mix(h, home_slot(uint32_t(m), l.log_slots));
		}
		const staging_layout st = staging_for(m);
		h = fp_mix(h, st.counters);
		h = fp_mix(h, st.count);
		h = fp_mix(h, st.total);
	}
	return h;
}

constexpr uint64_t DEVHASH_LAYOUT_FP = layout_fingerprint();

static_assert(OPAQUE_WORD != 0 && state_of(OPAQUE_WORD) == ST_OPAQUE,
	      "opaque must be non-empty");
static_assert(make_header(0, 0, ST_LIVE) != 0,
	      "a live header for key 0, row 0 must be non-empty");
static_assert(!is_keyed(OPAQUE_WORD) && !is_keyed(ST_EMPTY),
	      "opaque and empty belong to no key");
static_assert(MAX_ENTRIES_LIMIT - 1 <= ROW_MASK, "rows must fit the row field");
static_assert(slots_for(1) == 2 && slots_for(2) == 4 && slots_for(48) == 128,
	      "slots = next_pow2(2 x max_entries), at least 2");
static_assert(layout_for(1024, 256).dir % REGION_ALIGN == 0,
	      "regions are aligned");
} // namespace bpftime_devhash

#endif
