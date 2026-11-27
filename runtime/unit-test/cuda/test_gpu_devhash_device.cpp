// GPU tests of the device hash map's functions (1533; plan results
// gpu/hash-build-plan-20261127, step S3): round two's stress runs and
// controls (results gpu/hash-round2-20261127, c5) on the shipped functions
// (attach/nv_attach_impl/devhash/devhash.cu, as devhash_ptx.h and the test
// modules of devhash_test_ptx.h), and the trampoline's 1533 branches.
//
// Every stress configuration runs in its own process (this binary, exec'd
// with a hidden tag) under a total deadline: a hung kernel cannot be stopped
// from inside a process, and the CUDA state of one run must not reach the
// next. The parent reads the child's exit status: 0 when the configuration
// met what it expects (no violation for the protocol; the named violation
// for a control). Skipped without a GPU.
#include "cuda_test_support.hpp"
#include "devhash_ptx.h"
#include "devhash_test_ptx.h"
#include "bpf_map/gpu/nv_gpu_device_hash_map.hpp"
#include "bpf_map/gpu/nv_gpu_model_maps.hpp"
#include "nv_thread_local_routing.hpp"
#include "nv_attach_impl.hpp"
#include "trampoline/gpu_devhash_layout.h"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <array>
#include <chrono>
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <poll.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

using namespace bpftime;
using namespace bpftime_devhash;
using namespace boost::interprocess;
using cuda_test::counting_host;
using cuda_test::device_buffer;
using cuda_test::use_primary_context;

namespace
{
typedef uint64_t u64;
typedef uint32_t u32;
constexpr u64 SHARDS = 256; // pow2(SMs) x 32 on the Orin (8 SMs)
constexpr u32 BLOCK = 256;

// as devhash.cu and devhash_harness.cu number them
enum : u32 {
	OC_NONE = 0,
	OC_HIT = 1,
	OC_WON = 2,
	OC_EEXIST_LIVE = 3,
	OC_EEXIST_WAITED = 4,
	OC_EEXIST_UNSTABLE = 5,
	OC_E2BIG = 6,
	OC_ANOM_ROW = 8,
	OC_EINVAL = 9,
	OC_FLAG_THEN_NULL = 0x10,
};
enum : u32 {
	D_KEY_CHANGED = 0,
	D_ROW_OVERFLOW,
	D_REFUSED_PUBLISHED,
	D_NEXT_ROW,
	D_GIVEBACK,
	D_LOST_SAME,
	D_LOST_OTHER,
	D_WAITS,
	D_E2BIG_PRECHECK,
	D_PRECHECK_SLOT_TAKEN,
	D_E2BIG_SAW_REFUSED,
	D_E2BIG_AFTER_ADD,
	D_E2BIG_FULLDIR,
	D_WAIT_MAX_NS,
	D_WALK_SAW_CLAIMING,
	D_N
};
enum : u32 {
	H_BLOCKS_DONE = 0,
	H_EEXIST_THEN_NULL,
	H_UNSTABLE_THEN_NULL,
	R_POLLS,
	R_LIVE_CHECKS,
	R_ZERO_SUM,
	R_DECREASE,
	R_TRANSITIONS,
	R_TIMED_OUT,
	R_TRANSITION_SUM_MIN,
	H_WORDS
};
// devhash_harness.cu's params
struct harness_params {
	u64 block;
	u64 max_entries, shards;
	u64 hctl;
	u64 eexist, e2big, unstable;
	u64 outcomes;
	u32 n_threads, nkeys;
	u32 key_mul, key_add;
};

enum {
	V_TWO_SLOTS = 0,
	V_LIVE_GT_MAX,
	V_LIVE_COUNT,
	V_LEFT_CLAIMING,
	V_KEY_SUM,
	V_TOTAL_SUM,
	V_GARBAGE,
	V_ROW_DUP,
	V_ROW_RANGE,
	V_BOOKKEEPING,
	V_WINNERS,
	V_TALLY,
	V_EEXIST_THEN_NULL,
	V_CHAIN,
	V_ORPHAN,
	V_ANOM,
	V_READER_ZERO,
	V_READER_DECREASE,
	V_KEY_CHANGED,
	V_E2BIG_WITH_ROOM,
	V_E2BIG_LIVE_KEY,
	V_EEXIST_NOT_LIVE,
	V_N
};
const char *vname[V_N] = { "key_in_two_slots",	   "live_gt_max",
			   "live_ne_expected",	   "left_claiming",
			   "key_sum_mismatch",	   "total_sum_mismatch",
			   "garbage_header",	   "row_in_two_headers",
			   "row_out_of_range",	   "bookkeeping",
			   "winners_ne_1",	   "tally_ne_counter",
			   "eexist_then_null",	   "probe_chain_gap",
			   "orphan_adds",	   "anomaly",
			   "reader_zero_sum",	   "reader_decrease",
			   "key_changed",	   "e2big_with_room",
			   "e2big_on_live_key",	   "eexist_on_key_not_live" };

enum class seeding { none, prefill, takeover };

struct stress_config {
	const char *name;
	const char *ptx;
	bool test_build; // an outcome channel and diagnostics
	int fault; // 0, or the control's number
	u32 max_entries, keys; // keys 0: one per thread
	u32 trials, launches, threads;
	bool reader;
	seeding seed;
	int opaque_key; // >= 0: opaque slots on that key's path (one key)
	int expect; // -1: no violation; else the violation kind expected
};

const stress_config CONFIGS[] = {
	// S3.1: round two's runs
	{ "a", DEVHASH_TEST_PTX_TEST, true, 0, 128, 64, 51, 10, 1000000, false,
	  seeding::none, -1, -1 },
	{ "b", DEVHASH_TEST_PTX_TEST, true, 0, 64, 64, 51, 10, 1000000, false,
	  seeding::none, -1, -1 },
	{ "c", DEVHASH_TEST_PTX_TEST, true, 0, 48, 64, 51, 10, 1000000, false,
	  seeding::none, -1, -1 },
	{ "e", DEVHASH_TEST_PTX_TEST, true, 0, 1024, 0, 6, 10, 1000000, false,
	  seeding::none, -1, -1 },
	{ "g", DEVHASH_TEST_PTX_WAIT0, true, 0, 128, 64, 51, 10, 1000000,
	  false, seeding::none, -1, -1 },
	{ "h", DEVHASH_TEST_PTX_TEST, true, 0, 128, 64, 51, 10, 1000000, true,
	  seeding::none, -1, -1 },
	// S3.2, S3.3: run d on S2's images
	{ "d-image", DEVHASH_TEST_PTX_TEST, true, 0, 128, 64, 51, 10, 1000000,
	  false, seeding::prefill, -1, -1 },
	{ "d-takeover", DEVHASH_TEST_PTX_TEST, true, 0, 128, 64, 51, 10,
	  1000000, false, seeding::takeover, -1, -1 },
	// S3.4: the controls, each must fail the check round two names
	{ "ctl1", DEVHASH_TEST_PTX_FAULT1, true, 1, 128, 64, 201, 10, 1000000,
	  false, seeding::none, -1, V_WINNERS },
	{ "ctl2", DEVHASH_TEST_PTX_FAULT2, true, 2, 48, 64, 201, 10, 1000000,
	  false, seeding::none, -1, V_LIVE_GT_MAX },
	// control 5 at 16 keys and max 16: more threads per key when the map
	// fills. At 64 and 64 (round two's run) it fired in 2 of 201 trials on
	// this build (a miss in about 1 of 8 runs); at 16, 35 of 201; at 8, 27.
	{ "ctl5", DEVHASH_TEST_PTX_FAULT5, true, 5, 16, 16, 201, 10, 1000000,
	  false, seeding::none, -1, V_E2BIG_LIVE_KEY },
	{ "ctl6", DEVHASH_TEST_PTX_FAULT6, true, 6, 128, 64, 201, 10, 1000000,
	  false, seeding::none, -1, V_E2BIG_WITH_ROOM },
	// S3.5: the shipped build, checks without the outcome channel; and
	// those checks see control 6
	{ "shipped-a", DEVHASH_TEST_PTX_SHIPPED, false, 0, 128, 64, 51, 10,
	  1000000, false, seeding::none, -1, -1 },
	{ "shipped-b", DEVHASH_TEST_PTX_SHIPPED, false, 0, 64, 64, 51, 10,
	  1000000, false, seeding::none, -1, -1 },
	{ "shipped-checks-ctl6", DEVHASH_TEST_PTX_SHIPPEDFAULT6, false, 6, 128, 64,
	  51, 10, 1000000, false, seeding::none, -1, V_E2BIG_WITH_ROOM },
	// S3.6: termination, one key per thread at max 48 (the directory fills)
	{ "f", DEVHASH_TEST_PTX_TEST, true, 0, 48, 0, 6, 10, 1000000, false,
	  seeding::none, -1, -1 },
	// S3.8: opaque slots on the path of key 0 and of key 1000
	{ "opaque-0", DEVHASH_TEST_PTX_TEST, true, 0, 64, 1, 3, 2, 4096, false,
	  seeding::none, 0, -1 },
	{ "opaque-1000", DEVHASH_TEST_PTX_TEST, true, 0, 64, 1, 3, 2, 4096,
	  false, seeding::none, 1000, -1 },
};

const stress_config *find_config(const std::string &name)
{
	for (auto &c : CONFIGS)
		if (name == c.name)
			return &c;
	return nullptr;
}

u64 splitmix64(u64 x)
{
	x += 0x9E3779B97F4A7C15ull;
	x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
	x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
	return x ^ (x >> 31);
}
u32 keyidx_of_tid(u32 tid, u32 nkeys, u32 n_threads)
{
	if (nkeys == n_threads)
		return tid;
	return (u32)(((u64)(tid * 2654435761u) * nkeys) >> 32);
}

#define CK(x)                                                                  \
	do {                                                                   \
		CUresult e_ = (x);                                             \
		if (e_ != CUDA_SUCCESS) {                                      \
			const char *s_ = nullptr;                              \
			cuGetErrorName(e_, &s_);                               \
			printf("CUDA error %s at line %d\n", s_ ? s_ : "?",    \
			       __LINE__);                                      \
			fflush(stdout);                                        \
			_exit(90);                                             \
		}                                                              \
	} while (0)

CUmodule load_module(const std::string &ptx)
{
	CUmodule m;
	char log[16384] = {};
	CUjit_option opts[] = { CU_JIT_ERROR_LOG_BUFFER,
				CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES };
	void *vals[] = { log, (void *)(uintptr_t)sizeof(log) };
	const CUresult r = cuModuleLoadDataEx(&m, ptx.c_str(), 2, opts, vals);
	if (r != CUDA_SUCCESS) {
		printf("module load failed (%d): %s\n", (int)r, log);
		fflush(stdout);
		_exit(91);
	}
	return m;
}

// The image a device copy starts from, made by S2's code (CPU object in a
// scratch segment): the agent's first copy of a prefilled table, or a
// takeover's image of a delivered mirror.
struct seeded_image {
	devhash_image img;
	std::vector<char> prefilled; // per key index
	std::vector<u64> seeded; // per key index: value seeded
	u64 live = 0;
};

seeded_image make_image(const stress_config &c, u32 nkeys, u32 key_mul,
			u32 key_add)
{
	seeded_image s;
	s.prefilled.assign(nkeys, 0);
	s.seeded.assign(nkeys, 0);
	const std::string name =
		"bpftime-devhash-gpu-seed-" + std::to_string(getpid());
	shared_memory_object::remove(name.c_str());
	managed_shared_memory seg(create_only, name.c_str(), 64 << 20);
	auto *map = seg.construct<nv_gpu_device_hash_map_impl>(
		anonymous_instance)(seg, c.max_entries);
	auto key_at = [&](u32 j) { return j * key_mul + key_add; };
	if (c.seed == seeding::prefill) {
		// 16 keys, j = 4p, value 5: the loader's prefill
		for (u32 p = 0; p < 16; p++) {
			const u32 j = (4 * p) % nkeys, k = key_at(j);
			const u64 v = 5;
			if (map->elem_update(&k, &v, 1 /*BPF_NOEXIST*/) != 0) {
				printf("prefill failed\n");
				_exit(92);
			}
			s.prefilled[j] = 1;
			s.seeded[j] = 5;
		}
		if (!map->seal_and_build(s.img)) {
			printf("image failed\n");
			_exit(93);
		}
	} else {
		// a mirror delivered by a dead owner: 16 keys live (j = 4p,
		// value 5) in rows 1..16, and key index 1 claiming (a claim
		// that died after taking row 0: a gap in the rows)
		devhash_image empty;
		if (!map->seal_and_build(empty)) {
			printf("seal failed\n");
			_exit(94);
		}
		map->seed_mirror(empty);
		map->publish_device_phase();
		const layout l = layout_for(c.max_entries, SHARDS);
		const staging_layout st = staging_for(c.max_entries);
		std::vector<uint8_t> buf(st.total, 0);
		std::vector<u64> hdr(l.slots, 0), val(l.slots, 0);
		auto place = [&](u32 k, u64 h, u64 v) {
			u64 i = home_slot(k, l.log_slots);
			while (hdr[i])
				i = (i + 1) & (l.slots - 1);
			hdr[i] = h;
			val[i] = v;
		};
		place(key_at(1), make_header(key_at(1), 0, ST_CLAIMING), 0);
		for (u32 p = 0; p < 16; p++) {
			const u32 j = (4 * p) % nkeys;
			place(key_at(j), make_header(key_at(j), p + 1, ST_LIVE),
			      5);
			s.prefilled[j] = 1;
			s.seeded[j] = 5;
		}
		for (u64 i = 0; i < l.slots; i++) {
			std::memcpy(&buf[staging_header_offset(i)], &hdr[i], 8);
			std::memcpy(&buf[staging_value_offset(i)], &val[i], 8);
		}
		const u64 count = 17;
		std::memcpy(&buf[st.count], &count, 8);
		map->apply_delivery(buf.data());
		map->finish_delivery();
		s.img = map->takeover_image();
	}
	for (u64 h : s.img.headers)
		s.live += state_of(h) == ST_LIVE;
	seg.destroy_ptr(map);
	shared_memory_object::remove(name.c_str());
	return s;
}

struct totals {
	u64 trials = 0, trials_with_violation = 0;
	u64 viol[V_N] = {};
	u64 e2big = 0, eexist = 0, unstable = 0, then_null_unstable = 0;
	u64 refused_max = 0, live_min = ~0ull, live_max = 0;
	u64 reader_live = 0, reader_min_transition = ~0ull;
	u64 counter_unstable_e2big = 0, counter_lookup_fullwalk = 0;
};

// One configuration, in this (child) process. Returns 0 when it met its
// expectation.
int run_stress(const stress_config &c)
{
	if (!use_primary_context()) {
		printf("no GPU\n");
		return 0;
	}
	const u32 n_threads = c.threads;
	const u32 nkeys = c.keys ? c.keys : n_threads;
	const layout l = layout_for(c.max_entries, SHARDS);
	const u32 grid = (n_threads + BLOCK - 1) / BLOCK;
	CUmodule mod = load_module(c.ptx);
	CUfunction k_ops, k_reader;
	CK(cuModuleGetFunction(&k_ops, mod, "devhash_k_ops"));
	CK(cuModuleGetFunction(&k_reader, mod, "devhash_k_reader"));
	CUstream s_ops, s_reader;
	CK(cuStreamCreate(&s_ops, CU_STREAM_NON_BLOCKING));
	CK(cuStreamCreate(&s_reader, CU_STREAM_NON_BLOCKING));
	CUdeviceptr d_block, d_eex, d_e2b, d_uns, d_out, d_hctl;
	CK(cuMemAlloc(&d_block, l.total));
	CK(cuMemAlloc(&d_eex, 8ull * nkeys));
	CK(cuMemAlloc(&d_e2b, 8ull * nkeys));
	CK(cuMemAlloc(&d_uns, 8ull * nkeys));
	CK(cuMemAlloc(&d_out, (size_t)c.launches * n_threads));
	CK(cuMemAlloc(&d_hctl, 8ull * H_WORDS));
	std::vector<u64> attempts(nkeys, 0), per_launch(nkeys, 0);
	for (u32 t = 0; t < n_threads; t++)
		per_launch[keyidx_of_tid(t, nkeys, n_threads)]++;
	u64 keys_used = 0;
	for (u32 j = 0; j < nkeys; j++) {
		attempts[j] = per_launch[j] * c.launches;
		keys_used += per_launch[j] != 0;
	}
	const u64 expect_live =
		keys_used < c.max_entries ? keys_used : c.max_entries;
	std::vector<u64> block(l.total / 8), eex(nkeys), e2b(nkeys), uns(nkeys),
		hctl(H_WORDS);
	std::vector<unsigned char> outc((size_t)c.launches * n_threads);
	std::vector<u64> outcome_addr(c.launches);
	totals G;
	for (u32 tr = 0; tr < c.trials; tr++) {
		// the key set: round two's on trial 0, then another per trial
		u32 key_mul = 7919u, key_add = 1000u;
		if (tr > 0) {
			key_mul = (u32)splitmix64(tr * 2 + 1) | 1u;
			key_add = (u32)splitmix64(tr * 2 + 2);
		}
		if (c.opaque_key >= 0) {
			key_mul = 0;
			key_add = (u32)c.opaque_key;
		}
		auto key_at = [&](u32 j) { return j * key_mul + key_add; };
		std::unordered_map<u32, u32> index_of;
		index_of.reserve(nkeys * 2);
		for (u32 j = 0; j < nkeys; j++)
			index_of.emplace(key_at(j), j);
		auto jidx = [&](u32 k) -> int {
			auto it = index_of.find(k);
			return it == index_of.end() ? -1 : (int)it->second;
		};
		CK(cuMemsetD8(d_block, 0, l.total));
		CK(cuMemsetD8(d_eex, 0, 8ull * nkeys));
		CK(cuMemsetD8(d_e2b, 0, 8ull * nkeys));
		CK(cuMemsetD8(d_uns, 0, 8ull * nkeys));
		CK(cuMemsetD8(d_out, 0, (size_t)c.launches * n_threads));
		CK(cuMemsetD8(d_hctl, 0, 8ull * H_WORDS));
		const u64 ones = ~0ull;
		CK(cuMemcpyHtoD(d_hctl + 8ull * R_TRANSITION_SUM_MIN, &ones, 8));
		// seeding
		seeded_image seed;
		seed.prefilled.assign(nkeys, 0);
		seed.seeded.assign(nkeys, 0);
		u64 seed_count = 0;
		if (c.seed != seeding::none) {
			seed = make_image(c, nkeys, key_mul, key_add);
			std::vector<u64> dir(l.slots);
			for (u64 i = 0; i < l.slots; i++) {
				dir[i] = seed.img.headers[i];
				if (state_of(dir[i]) == ST_LIVE) {
					const u64 v = seed.img.values[i];
					CK(cuMemcpyHtoD(
						d_block + value_offset(
								  l, row_of(dir[i]),
								  0),
						&v, 8));
				}
			}
			CK(cuMemcpyHtoD(d_block + l.dir, dir.data(), 8 * l.slots));
			seed_count = seed.img.count;
			CK(cuMemcpyHtoD(d_block + ctl_offset(CTL_COUNT),
					&seed_count, 8));
		}
		if (c.opaque_key >= 0) {
			// three opaque slots from the key's home
			const u32 h = home_slot((u32)c.opaque_key, l.log_slots);
			for (u32 i = 0; i < 3; i++) {
				const u64 w = OPAQUE_WORD;
				CK(cuMemcpyHtoD(d_block + slot_offset(
								  l, (h + i) &
									     (l.slots -
									      1)),
						&w, 8));
			}
		}
		CK(cuCtxSynchronize());
		harness_params P{};
		P.block = d_block;
		P.max_entries = c.max_entries;
		P.shards = SHARDS;
		P.hctl = d_hctl;
		P.eexist = d_eex;
		P.e2big = d_e2b;
		P.unstable = d_uns;
		P.n_threads = n_threads;
		P.nkeys = nkeys;
		P.key_mul = key_mul;
		P.key_add = key_add;
		u64 total_blocks = (u64)grid * c.launches, tmax = 20000000000ull;
		if (c.reader) {
			void *args[] = { &P, &total_blocks, &tmax };
			CK(cuLaunchKernel(k_reader, (u32)((l.slots + 31) / 32), 1,
					  1, 32, 1, 1, 0, s_reader, args,
					  nullptr));
		}
		for (u32 ln = 0; ln < c.launches; ln++) {
			outcome_addr[ln] = d_out + (u64)ln * n_threads;
			if (c.test_build)
				CK(cuMemcpyHtoDAsync(
					d_block + ctl_offset(CTL_TEST_OUTCOMES),
					&outcome_addr[ln], 8, s_ops));
			P.outcomes = outcome_addr[ln];
			void *args[] = { &P };
			CK(cuLaunchKernel(k_ops, grid, 1, 1, BLOCK, 1, 1, 0, s_ops,
					  args, nullptr));
			CK(cuStreamSynchronize(s_ops)); // P and the address live on
		}
		CK(cuCtxSynchronize());
		CK(cuMemcpyDtoH(block.data(), d_block, l.total));
		CK(cuMemcpyDtoH(eex.data(), d_eex, 8ull * nkeys));
		CK(cuMemcpyDtoH(e2b.data(), d_e2b, 8ull * nkeys));
		CK(cuMemcpyDtoH(uns.data(), d_uns, 8ull * nkeys));
		CK(cuMemcpyDtoH(hctl.data(), d_hctl, 8ull * H_WORDS));
		CK(cuMemcpyDtoH(outc.data(), d_out, outc.size()));
		const u64 *ctlw = block.data();
		auto diag = [&](u32 d) { return ctlw[CTL_TEST_DIAG + d]; };
		auto counter = [&](u32 cnt) {
			u64 s = 0;
			for (u64 k = 0; k < SHARDS; k++)
				s += block[counter_offset(l, cnt, (u32)k) / 8];
			return s;
		};
		auto row_sum = [&](u64 row) {
			if (row >= l.r)
				return block[value_offset(l, row, 0) / 8];
			u64 s = 0;
			for (u64 k = 0; k < SHARDS; k++)
				s += block[value_offset(l, row, (u32)k) / 8];
			return s;
		};
		// ------------------------------------------------ checks
		u64 V[V_N] = {};
		std::vector<int> nslots(nkeys, 0);
		std::vector<char> live_of(nkeys, 0);
		std::vector<u64> delivered(nkeys, 0);
		std::vector<int> row_used(c.max_entries, 0);
		u64 nlive = 0, nrefused = 0, nclaiming = 0, seeded_live = 0;
		for (u64 s = 0; s < l.slots; s++) {
			const u64 h = block[(l.dir + 8 * s) / 8];
			const u64 st = state_of(h);
			if (h == 0)
				continue;
			if (h == OPAQUE_WORD)
				continue; // occupied, no key
			if (st == ST_EMPTY || st > ST_REFUSED) {
				V[V_GARBAGE]++;
				continue;
			}
			const u32 k = bpftime_devhash::key_of(h);
			const int j = jidx(k);
			if (j < 0) {
				V[V_GARBAGE]++;
				continue;
			}
			if (++nslots[j] == 2)
				V[V_TWO_SLOTS]++;
			// the probe chain from K's home is non-empty
			for (u64 i = home_slot(k, l.log_slots); i != s;
			     i = (i + 1) & (l.slots - 1))
				if (block[(l.dir + 8 * i) / 8] == 0) {
					V[V_CHAIN]++;
					break;
				}
			if (st == ST_CLAIMING) {
				nclaiming++;
				continue;
			}
			if (st == ST_REFUSED) {
				nrefused++;
				continue;
			}
			nlive++;
			live_of[j] = 1;
			const u64 row = row_of(h);
			if (row >= c.max_entries) {
				V[V_ROW_RANGE]++;
				continue;
			}
			if (row_used[row]++)
				V[V_ROW_DUP]++;
			delivered[j] += row_sum(row);
			if (seed.prefilled[j])
				seeded_live++;
		}
		if (nlive > c.max_entries)
			V[V_LIVE_GT_MAX]++;
		if (nlive != expect_live)
			V[V_LIVE_COUNT]++;
		if (nclaiming)
			V[V_LEFT_CLAIMING]++;
		u64 all_rows = 0, ref_rows = 0;
		for (u64 r = 0; r < c.max_entries; r++) {
			const u64 s = row_sum(r);
			all_rows += s;
			if (row_used[r])
				ref_rows += s;
		}
		if (all_rows != ref_rows)
			V[V_ORPHAN]++;
		const u64 count = ctlw[CTL_COUNT];
		if (c.fault == 0 || c.fault == 2 || c.fault == 5) {
			// every winner adds once to the count
			if (count != seed_count + (nlive - seeded_live) + nrefused)
				V[V_BOOKKEEPING]++;
			if (c.test_build && diag(D_REFUSED_PUBLISHED) != nrefused)
				V[V_BOOKKEEPING]++;
		}
		if (c.fault == 6 && c.test_build && diag(D_NEXT_ROW) != nlive)
			V[V_BOOKKEEPING]++;
		if (c.test_build && diag(D_ROW_OVERFLOW))
			V[V_ANOM]++;
		if (c.test_build && diag(D_KEY_CHANGED))
			V[V_KEY_CHANGED]++;
		// outcome tallies
		std::vector<std::array<u64, 10>> T(nkeys);
		for (auto &a : T)
			a.fill(0);
		for (u32 ln = 0; ln < c.launches; ln++)
			for (u32 t = 0; t < n_threads; t++) {
				const u32 oc = outc[(size_t)ln * n_threads + t];
				u32 b = oc & 0xF;
				T[keyidx_of_tid(t, nkeys, n_threads)][b < 10 ? b : 0]++;
			}
		if (hctl[H_EEXIST_THEN_NULL])
			V[V_EEXIST_THEN_NULL]++;
		u64 tot_att = 0, tot_eex = 0, tot_e2b = 0, tot_uns = 0,
		    tot_del = 0, tot_seed = 0;
		for (u32 j = 0; j < nkeys; j++) {
			const u64 pv = seed.seeded[j];
			const u64 expect = pv + attempts[j] - eex[j] - e2b[j];
			tot_att += attempts[j];
			tot_eex += eex[j];
			tot_e2b += e2b[j];
			tot_uns += uns[j];
			tot_del += delivered[j];
			tot_seed += live_of[j] ? pv : 0;
			if (delivered[j] != expect && live_of[j])
				V[V_KEY_SUM]++;
			const u64 t_eex = T[j][OC_EEXIST_LIVE] +
					  T[j][OC_EEXIST_WAITED] +
					  T[j][OC_EEXIST_UNSTABLE];
			if (t_eex != eex[j] || T[j][OC_E2BIG] != e2b[j] ||
			    T[j][OC_EEXIST_UNSTABLE] != uns[j] ||
			    T[j][OC_NONE] != 0)
				V[V_TALLY]++;
			if (pv + T[j][OC_HIT] + T[j][OC_WON] != delivered[j] &&
			    nslots[j] <= 1 && live_of[j])
				V[V_TALLY]++;
			const u64 want_won =
				(live_of[j] && !seed.prefilled[j]) ? 1 : 0;
			if (T[j][OC_WON] != want_won)
				V[V_WINNERS]++;
			if (e2b[j] && live_of[j])
				V[V_E2BIG_LIVE_KEY]++;
			if (eex[j] > uns[j] && !live_of[j])
				V[V_EEXIST_NOT_LIVE]++;
		}
		if (tot_e2b && (nlive < c.max_entries || keys_used <= c.max_entries))
			V[V_E2BIG_WITH_ROOM]++;
		if (tot_del != tot_seed + tot_att - tot_eex - tot_e2b)
			V[V_TOTAL_SUM]++;
		if (c.reader) {
			if (hctl[R_ZERO_SUM])
				V[V_READER_ZERO]++;
			if (hctl[R_DECREASE])
				V[V_READER_DECREASE]++;
			G.reader_live += hctl[R_LIVE_CHECKS];
			G.reader_min_transition =
				std::min(G.reader_min_transition,
					 hctl[R_TRANSITION_SUM_MIN]);
		}
		u64 nv = 0;
		for (int v = 0; v < V_N; v++) {
			nv += V[v];
			G.viol[v] += V[v] ? 1 : 0; // trials with this kind
		}
		G.trials++;
		G.trials_with_violation += nv ? 1 : 0;
		G.e2big += tot_e2b;
		G.eexist += tot_eex;
		G.unstable += tot_uns;
		G.then_null_unstable += hctl[H_UNSTABLE_THEN_NULL];
		G.refused_max = std::max(G.refused_max, nrefused);
		G.live_min = std::min(G.live_min, nlive);
		G.live_max = std::max(G.live_max, nlive);
		G.counter_unstable_e2big += counter(CNT_UNSTABLE_E2BIG);
		G.counter_lookup_fullwalk += counter(CNT_LOOKUP_FULLWALK);
		// the device's own loss counts agree with the harness's
		if (counter(CNT_EEXIST) != tot_eex ||
		    counter(CNT_E2BIG) != tot_e2b ||
		    (c.test_build &&
		     counter(CNT_UNSTABLE_EEXIST) != tot_uns)) {
			G.viol[V_TALLY]++;
			if (!nv)
				G.trials_with_violation++;
		}
		if (nv && G.trials_with_violation <= 3) {
			printf("trial %u violations:", tr);
			for (int v = 0; v < V_N; v++)
				if (V[v])
					printf(" %s=%" PRIu64, vname[v], V[v]);
			printf(" (live %" PRIu64 " refused %" PRIu64
			       " count %" PRIu64 ")\n",
			       nlive, nrefused, count);
		}
	}
	printf("RESULT %s: trials %" PRIu64 ", with violations %" PRIu64
	       "; E2BIG %" PRIu64 ", EEXIST %" PRIu64 " (unstable %" PRIu64
	       ", unstable then NULL %" PRIu64 "); live %" PRIu64 "-%" PRIu64
	       ", refused max %" PRIu64 "; unstable E2BIG %" PRIu64
	       ", lookup full walks %" PRIu64,
	       c.name, G.trials, G.trials_with_violation, G.e2big, G.eexist,
	       G.unstable, G.then_null_unstable, G.live_min, G.live_max,
	       G.refused_max, G.counter_unstable_e2big,
	       G.counter_lookup_fullwalk);
	if (c.reader)
		printf("; reader live checks %" PRIu64
		       ", smallest sum at a transition %" PRIu64,
		       G.reader_live, G.reader_min_transition);
	printf("\nviolations by kind (trials):");
	for (int v = 0; v < V_N; v++)
		if (G.viol[v])
			printf(" %s=%" PRIu64, vname[v], G.viol[v]);
	printf("\n");
	fflush(stdout);
	if (c.expect < 0)
		return G.trials_with_violation == 0 ? 0 : 1;
	return G.viol[c.expect] > 0 ? 0 : 1;
}

// A directory with no empty slot (all opaque) and the count below
// max_entries: inserts of new keys end, E2BIG counted unstable (S3.7).
int run_full_directory()
{
	if (!use_primary_context())
		return 0;
	const u64 max = 48;
	const layout l = layout_for(max, SHARDS);
	CUmodule mod = load_module(DEVHASH_TEST_PTX_TEST);
	CUfunction k_ops;
	CK(cuModuleGetFunction(&k_ops, mod, "devhash_k_ops"));
	CUdeviceptr d_block, d_eex, d_e2b, d_uns, d_out, d_hctl;
	const u32 n = 512, nkeys = 64;
	CK(cuMemAlloc(&d_block, l.total));
	CK(cuMemsetD8(d_block, 0, l.total));
	std::vector<u64> dir(l.slots, OPAQUE_WORD);
	CK(cuMemcpyHtoD(d_block + l.dir, dir.data(), 8 * l.slots));
	CK(cuMemAlloc(&d_eex, 8 * nkeys));
	CK(cuMemAlloc(&d_e2b, 8 * nkeys));
	CK(cuMemAlloc(&d_uns, 8 * nkeys));
	CK(cuMemAlloc(&d_out, n));
	CK(cuMemAlloc(&d_hctl, 8 * H_WORDS));
	for (auto p : { d_eex, d_e2b, d_uns, d_hctl })
		CK(cuMemsetD8(p, 0, 8 * (p == d_hctl ? H_WORDS : nkeys)));
	harness_params P{ d_block, max,	  SHARDS, d_hctl, d_eex, d_e2b,
			  d_uns,   d_out, n,	  nkeys,  7919u, 1000u };
	void *args[] = { &P };
	CK(cuLaunchKernel(k_ops, n / BLOCK, 1, 1, BLOCK, 1, 1, 0, nullptr, args,
			  nullptr));
	CK(cuCtxSynchronize());
	std::vector<u64> block(l.total / 8), e2b(nkeys);
	CK(cuMemcpyDtoH(block.data(), d_block, l.total));
	CK(cuMemcpyDtoH(e2b.data(), d_e2b, 8 * nkeys));
	u64 tot = 0, unstable = 0, fullwalk = 0;
	for (u64 v : e2b)
		tot += v;
	for (u64 k = 0; k < SHARDS; k++) {
		unstable += block[counter_offset(l, CNT_UNSTABLE_E2BIG, (u32)k) / 8];
		fullwalk +=
			block[counter_offset(l, CNT_INSERT_FULLWALK, (u32)k) / 8];
	}
	printf("RESULT full-directory: E2BIG %" PRIu64 " of %u, unstable E2BIG %" PRIu64
	       ", insert full walks %" PRIu64 "\n",
	       tot, n, unstable, fullwalk);
	fflush(stdout);
	return tot == n && unstable == n && fullwalk == n ? 0 : 1;
}

// The hash snapshot (S4.1) over a hand-built block: every state, rows below
// and at or above r, known shards and counters. Rows of slots that are not
// live hold decoy values the snapshot must not read.
int run_hash_snapshot()
{
	if (!use_primary_context())
		return 0;
	const u64 max = 600; // r = 512: rows 512..599 are single words
	const layout l = layout_for(max, SHARDS);
	const staging_layout st = staging_for(max);
	std::vector<u64> blk(l.total / 8, 0);
	auto row_word = [&](u64 row, u64 shard) -> u64 & {
		return blk[value_offset(l, row, (u32)shard) / 8];
	};
	auto hdr = [&](u64 slot) -> u64 & {
		return blk[slot_offset(l, slot) / 8];
	};
	std::map<u64, u64> want; // slot -> staged value
	// live, sharded row 3: shard s holds s + 1
	hdr(12) = make_header(6, 3, ST_LIVE);
	for (u64 s = 0; s < SHARDS; s++)
		row_word(3, s) = s + 1;
	want[12] = SHARDS * (SHARDS + 1) / 2;
	// live, single row 550
	hdr(13) = make_header(7, 550, ST_LIVE);
	row_word(550, 0) = 12345;
	want[13] = 12345;
	// live key 0, row 4: shard s holds 2
	hdr(16) = make_header(0, 4, ST_LIVE);
	for (u64 s = 0; s < SHARDS; s++)
		row_word(4, s) = 2;
	want[16] = 2 * SHARDS;
	// not live, with decoys in the rows their header names
	hdr(11) = make_header(5, 7, ST_CLAIMING);
	hdr(14) = make_header(8, 9, ST_REFUSED);
	hdr(15) = OPAQUE_WORD; // row 0
	hdr(17) = make_header(9, 2, 6); // no such state
	hdr(18) = make_header(10, 560, ST_CLAIMING); // a single row
	for (u64 row : { 0ull, 2ull, 7ull, 9ull })
		for (u64 s = 0; s < SHARDS; s++)
			row_word(row, s) = 1000;
	row_word(560, 0) = 1000;
	// counters: counter c, shard s holds c * 1000 + s + 1
	for (u32 c = 0; c < NUM_COUNTERS; c++)
		for (u64 s = 0; s < SHARDS; s++)
			blk[counter_offset(l, c, (u32)s) / 8] = c * 1000 + s + 1;
	blk[ctl_offset(CTL_COUNT) / 8] = 77;
	// slack after the block: a snapshot reading a row at or above r as
	// sharded reads zeros there instead of faulting
	const u64 slack = 4ull << 20;
	CUdeviceptr d_block, d_staging;
	CK(cuMemAlloc(&d_block, l.total + slack));
	CK(cuMemsetD8(d_block, 0, l.total + slack));
	CK(cuMemcpyHtoD(d_block, blk.data(), l.total));
	CK(cuMemAlloc(&d_staging, st.total));
	CK(cuMemsetD8(d_staging, 0xee, st.total));
	if (!gpu_model::hash_snapshot_for_test(d_block, max, SHARDS,
					       d_staging)) {
		printf("RESULT hash-snapshot: launch failed\n");
		return 2;
	}
	std::vector<u64> out(st.total / 8);
	CK(cuMemcpyDtoH(out.data(), d_staging, st.total));
	u64 bad_hdr = 0, bad_val = 0;
	for (u64 s = 0; s < l.slots; s++) {
		const u64 h = out[staging_header_offset(s) / 8];
		const u64 v = out[staging_value_offset(s) / 8];
		if (h != hdr(s))
			bad_hdr++;
		const u64 w = want.count(s) ? want[s] : 0;
		if (v != w) {
			if (bad_val < 8)
				printf("slot %" PRIu64 ": value %" PRIu64
				       " want %" PRIu64 "\n",
				       s, v, w);
			bad_val++;
		}
	}
	u64 bad_cnt = 0;
	for (u32 c = 0; c < NUM_COUNTERS; c++) {
		const u64 w = c * 1000 * SHARDS + SHARDS * (SHARDS + 1) / 2;
		if (out[(st.counters + 8 * c) / 8] != w)
			bad_cnt++;
	}
	const u64 count = out[st.count / 8];
	printf("RESULT hash-snapshot: slots %" PRIu64 ", wrong headers %" PRIu64
	       ", wrong values %" PRIu64 ", wrong counters %" PRIu64
	       ", count %" PRIu64 "\n",
	       l.slots, bad_hdr, bad_val, bad_cnt, count);
	fflush(stdout);
	return bad_hdr == 0 && bad_val == 0 && bad_cnt == 0 && count == 77 ? 0 :
									      1;
}

// ---------------------------------------------------- S6: routed calls
// A probe function as llvmbpf emits one (helper calls of map 14 with the
// key on the stack), routed by the pass and loaded, wrapped with the
// trampoline, as a patched module is. Thread i takes keys[i]: out[3 i] is
// the routed lookup's pointer, out[3 i + 1] the walk's own
// (__bpftime_devhash_lookup, from the inserted text), out[3 i + 2] the
// routed update's result (value 1, flags from test_flags).
const char ROUTED_PROBE[] = R"PTX(
.visible .global .align 8 .u64 test_keys;
.visible .global .align 8 .u64 test_out;
.visible .global .align 8 .u64 test_flags;
.visible .global .align 8 .u64 test_base;
.visible .func __retprobe_func__routed_k()
{
	.local .align 8 .b8 	__local_depot0[64];
	.reg .b64 	%SP;
	.reg .b64 	%SPL;
	.reg .pred 	%p<4>;
	.reg .b32 	%r<8>;
	.reg .b64 	%rd<64>;
	mov.u64 	%SPL, __local_depot0;
	cvta.local.u64 	%SP, %SPL;
	add.u64 	%rd2, %SP, 64;
	mov.u32 	%r1, %ctaid.x;
	mov.u32 	%r2, %ntid.x;
	mov.u32 	%r3, %tid.x;
	mad.lo.u32 	%r4, %r1, %r2, %r3;
	cvt.u64.u32 	%rd20, %r4;
	ld.global.u64 	%rd21, [test_keys];
	shl.b64 	%rd22, %rd20, 2;
	add.u64 	%rd22, %rd21, %rd22;
	ld.u32 	%r5, [%rd22];
	st.u32 	[%rd2+-8], %r5;
	add.s64 	%rd51, %rd2, -8;
	mov.u64 	%rd52, 14;
	{ // callseq 1, 0
	.reg .b32 temp_param_reg;
	.param .b64 param0;
	st.param.b64 	[param0+0], %rd52;
	.param .b64 param1;
	st.param.b64 	[param1+0], %rd51;
	.param .b64 param2;
	st.param.b64 	[param2+0], %rd40;
	.param .b64 param3;
	st.param.b64 	[param3+0], %rd41;
	.param .b64 param4;
	st.param.b64 	[param4+0], %rd42;
	.param .b64 retval0;
	call.uni (retval0),
	_bpf_helper_ext_0001,
	(
	param0,
	param1,
	param2,
	param3,
	param4
	);
	ld.param.b64 	%rd55, [retval0+0];
	} // callseq 1
	ld.global.u64 	%rd23, [test_out];
	mul.lo.u64 	%rd24, %rd20, 24;
	add.u64 	%rd24, %rd23, %rd24;
	st.u64 	[%rd24], %rd55;
	ld.global.u64 	%rd25, [test_base];
	cvt.u64.u32 	%rd26, %r5;
	mov.u64 	%rd27, ROUTED_MAX;
	mov.u64 	%rd28, ROUTED_SHARDS;
	{
	.param .b64 ra0;
	st.param.b64 	[ra0], %rd25;
	.param .b64 ra1;
	st.param.b64 	[ra1], %rd26;
	.param .b64 ra2;
	st.param.b64 	[ra2], %rd27;
	.param .b64 ra3;
	st.param.b64 	[ra3], %rd28;
	.param .b64 rr;
	call (rr), __bpftime_devhash_lookup, (ra0, ra1, ra2, ra3);
	ld.param.b64 	%rd56, [rr];
	}
	st.u64 	[%rd24+8], %rd56;
	mov.u64 	%rd30, 1;
	st.u64 	[%rd2+-16], %rd30;
	add.s64 	%rd31, %rd2, -16;
	ld.global.u64 	%rd32, [test_flags];
	{ // callseq 2, 0
	.reg .b32 temp_param_reg;
	.param .b64 param0;
	st.param.b64 	[param0+0], %rd52;
	.param .b64 param1;
	st.param.b64 	[param1+0], %rd51;
	.param .b64 param2;
	st.param.b64 	[param2+0], %rd31;
	.param .b64 param3;
	st.param.b64 	[param3+0], %rd32;
	.param .b64 param4;
	st.param.b64 	[param4+0], %rd42;
	.param .b64 retval0;
	call.uni (retval0),
	_bpf_helper_ext_0002,
	(
	param0,
	param1,
	param2,
	param3,
	param4
	);
	ld.param.b64 	%rd57, [retval0+0];
	} // callseq 2
	st.u64 	[%rd24+16], %rd57;
	ret;
}
.visible .entry routed_k(.param .u64 n)
{
	.reg .pred 	%p<2>;
	.reg .b32 	%r<5>;
	.reg .b64 	%rd<3>;
	ld.param.u64 	%rd1, [n];
	mov.u32 	%r1, %ctaid.x;
	mov.u32 	%r2, %ntid.x;
	mov.u32 	%r3, %tid.x;
	mad.lo.u32 	%r4, %r1, %r2, %r3;
	cvt.u64.u32 	%rd2, %r4;
	setp.ge.u64 	%p1, %rd2, %rd1;
	@%p1 bra 	$L_end;
	call __retprobe_func__routed_k;
$L_end:
	ret;
}
)PTX";

// The routed module over a seeded directory (S6, GPU). max_entries 600
// (r = 512: rows 512..599 are single words), every row taken (count = max,
// so a new key's insert is E2BIG); at their homes: live keys, a claiming,
// a refused and an opaque slot (its key live in the next slot), key 0
// live; most other slots empty. Threads: every seeded key, the special
// ones, and absent keys, 10^4 in all.
int run_routed()
{
	if (!use_primary_context())
		return 0;
	const u64 max = 600;
	const layout l = layout_for(max, SHARDS);
	std::vector<u64> blk(l.total / 8, 0);
	auto hdr = [&](u64 s) -> u64 & { return blk[slot_offset(l, s) / 8]; };
	// live keys at their homes, distinct homes, rows 0..max-1 (key 0
	// among them)
	std::vector<u32> live;
	std::set<u64> used;
	u32 claiming_key = 0, refused_key = 0, opaque_key = 0;
	for (u32 k = 0; live.size() < max - 1; k++) {
		const u64 h = home_slot(k, l.log_slots);
		if (used.count(h) || used.count((h + 1) & (l.slots - 1)))
			continue;
		used.insert(h);
		live.push_back(k);
	}
	u64 row = 0;
	for (u32 k : live) {
		hdr(home_slot(k, l.log_slots)) = make_header(k, row, ST_LIVE);
		for (u64 s = 0; s < (row < l.r ? SHARDS : 1); s++)
			blk[value_offset(l, row, (u32)s) / 8] = row * 1000 + s;
		row++;
	}
	// special keys at free homes (the next slot free too)
	auto free_home = [&](u32 from) {
		for (u32 k = from;; k++) {
			const u64 h = home_slot(k, l.log_slots);
			if (!used.count(h) && !used.count((h + 1) & (l.slots - 1)) &&
			    !used.count((h - 1) & (l.slots - 1))) {
				used.insert(h);
				used.insert((h + 1) & (l.slots - 1));
				return k;
			}
		}
	};
	claiming_key = free_home(100000);
	hdr(home_slot(claiming_key, l.log_slots)) =
		make_header(claiming_key, 0, ST_CLAIMING);
	refused_key = free_home(200000);
	hdr(home_slot(refused_key, l.log_slots)) =
		make_header(refused_key, 0, ST_REFUSED);
	opaque_key = free_home(300000);
	{
		const u64 h = home_slot(opaque_key, l.log_slots);
		hdr(h) = OPAQUE_WORD;
		hdr((h + 1) & (l.slots - 1)) =
			make_header(opaque_key, row, ST_LIVE); // the last row
		blk[value_offset(l, row, 0) / 8] = 4242;
		live.push_back(opaque_key);
		row++;
	}
	blk[ctl_offset(CTL_COUNT) / 8] = row; // = max: the map is full
	if (row != max) {
		printf("RESULT routed: seeding made %" PRIu64 " rows\n", row);
		return 3;
	}
	// threads: every live key, the special ones, absent keys
	std::vector<u32> keys(live.begin(), live.end());
	keys.push_back(claiming_key);
	keys.push_back(refused_key);
	for (u32 k = 400000; keys.size() < 10000; k++)
		keys.push_back(k);
	const u64 n = keys.size();
	// the module: routed by the pass, wrapped with the trampoline
	std::string probe = ROUTED_PROBE;
	auto put = [&](const std::string &what, u64 v) {
		const size_t at = probe.find(what);
		probe.replace(at, what.size(), std::to_string(v));
	};
	put("ROUTED_MAX", max);
	put("ROUTED_SHARDS", SHARDS);
	CUdeviceptr d_block, d_keys, d_out;
	CK(cuMemAlloc(&d_block, l.total));
	CK(cuMemcpyHtoD(d_block, blk.data(), l.total));
	attach::hash_map_desc h;
	h.fd = 14;
	h.base = d_block;
	h.max_entries = max;
	h.shards = SHARDS;
	attach::thread_local_routing_stats st;
	std::string routed = attach::route_thread_local_maps(
		".version 7.8\n.target " + cuda_test::device_sm_arch() +
			"\n.address_size 64\n" + probe,
		{ "routed_k" }, {}, 0, &st, {}, { h });
	if (st.hash_lookups != 1 || st.hash_inserts != 1 ||
	    !st.devhash_text_inserted) {
		printf("RESULT routed: not routed (lookups %d, inserts %d)\n",
		       st.hash_lookups, st.hash_inserts);
		return 4;
	}
	// the trampoline brings its own header
	routed.erase(0, routed.find(".address_size 64\n") +
				strlen(".address_size 64\n"));
	cuda_test::test_module mod(routed.c_str());
	CK(cuMemAlloc(&d_keys, 4 * n));
	CK(cuMemcpyHtoD(d_keys, keys.data(), 4 * n));
	CK(cuMemAlloc(&d_out, 24 * n));
	auto set_global = [&](const char *name, u64 v) {
		CUdeviceptr g;
		size_t size;
		CK(cuModuleGetGlobal(&g, &size, mod.module, name));
		CK(cuMemcpyHtoD(g, &v, 8));
	};
	set_global("test_keys", d_keys);
	set_global("test_out", d_out);
	set_global("test_base", d_block);
	int bad = 0;
	for (u64 flags : { 1ull, 0ull }) {
		set_global("test_flags", flags);
		CK(cuMemsetD8(d_out, 0, 24 * n));
		mod.launch("routed_k", (unsigned)((n + 255) / 256), 256, { n });
		std::vector<u64> out(3 * n);
		CK(cuMemcpyDtoH(out.data(), d_out, 24 * n));
		u64 differ = 0, found = 0, wrong_rc = 0;
		std::set<u32> live_set(live.begin(), live.end());
		for (u64 i = 0; i < n; i++) {
			const u64 routed_p = out[3 * i], walk_p = out[3 * i + 1];
			const int64_t rc = (int64_t)out[3 * i + 2];
			if (routed_p != walk_p) {
				if (differ < 5)
					printf("key %u: routed %#" PRIx64
					       ", walk %#" PRIx64 "\n",
					       keys[i], routed_p, walk_p);
				differ++;
			}
			found += walk_p != 0;
			const bool is_live = live_set.count(keys[i]) > 0;
			const bool claimed = keys[i] == claiming_key ||
					     keys[i] == refused_key;
			int64_t want = flags != 1 ? -22 :
				       is_live	  ? -17 :
				       claimed	  ? (keys[i] == refused_key ? -7 :
									-17) :
						    -7;
			if (rc != want) {
				if (wrong_rc < 5)
					printf("key %u flags %" PRIu64
					       ": update %" PRId64 ", want %" PRId64
					       "\n",
					       keys[i], flags, rc, want);
				wrong_rc++;
			}
		}
		printf("RESULT routed flags %" PRIu64 ": %" PRIu64
		       " threads, pointers differ %" PRIu64 ", found %" PRIu64
		       " (live %zu), wrong update results %" PRIu64 "\n",
		       flags, n, differ, found, live.size(), wrong_rc);
		if (differ || found != live.size() || wrong_rc)
			bad++;
	}
	// the emitted hash is the layout's home_slot: every live key was found
	// inline at its home (a wrong slot would read an empty one: NULL)
	fflush(stdout);
	return bad == 0 ? 0 : 1;
}

// The shipped text called as the routing pass will call it (S3.9).
const char ABI_KERNELS[] = R"PTX(
.visible .entry devhash_abi_insert(
	.param .u64 p_block, .param .u64 p_key, .param .u64 p_flags,
	.param .u64 p_max, .param .u64 p_shards, .param .u64 p_out)
{
	.local .align 8 .b8 depot[8];
	.reg .b64 %rd<12>;
	ld.param.u64 %rd1, [p_block];
	ld.param.u64 %rd2, [p_key];
	ld.param.u64 %rd3, [p_flags];
	ld.param.u64 %rd4, [p_max];
	ld.param.u64 %rd5, [p_shards];
	ld.param.u64 %rd6, [p_out];
	mov.u64 %rd7, depot;
	cvta.local.u64 %rd8, %rd7;
	mov.u64 %rd9, 1;
	st.local.u64 [depot], %rd9;
	{
	.param .b64 param0;
	st.param.b64 [param0+0], %rd1;
	.param .b64 param1;
	st.param.b64 [param1+0], %rd2;
	.param .b64 param2;
	st.param.b64 [param2+0], %rd8;
	.param .b64 param3;
	st.param.b64 [param3+0], %rd3;
	.param .b64 param4;
	st.param.b64 [param4+0], %rd4;
	.param .b64 param5;
	st.param.b64 [param5+0], %rd5;
	.param .b64 retval0;
	call.uni (retval0), __bpftime_devhash_insert, (param0, param1, param2, param3, param4, param5);
	ld.param.b64 %rd10, [retval0+0];
	}
	st.u64 [%rd6], %rd10;
	ret;
}
.visible .entry devhash_abi_lookup(
	.param .u64 p_block, .param .u64 p_key, .param .u64 p_max,
	.param .u64 p_shards, .param .u64 p_out)
{
	.reg .b64 %rd<10>;
	ld.param.u64 %rd1, [p_block];
	ld.param.u64 %rd2, [p_key];
	ld.param.u64 %rd3, [p_max];
	ld.param.u64 %rd4, [p_shards];
	ld.param.u64 %rd5, [p_out];
	{
	.param .b64 param0;
	st.param.b64 [param0+0], %rd1;
	.param .b64 param1;
	st.param.b64 [param1+0], %rd2;
	.param .b64 param2;
	st.param.b64 [param2+0], %rd3;
	.param .b64 param3;
	st.param.b64 [param3+0], %rd4;
	.param .b64 retval0;
	call.uni (retval0), __bpftime_devhash_lookup, (param0, param1, param2, param3);
	ld.param.b64 %rd6, [retval0+0];
	}
	st.u64 [%rd5], %rd6;
	ret;
}
)PTX";

int run_abi()
{
	if (!use_primary_context())
		return 0;
	const std::string ptx = ".version 7.8\n.target " +
				cuda_test::device_sm_arch() +
				"\n.address_size 64\n" + DEVHASH_PTX + ABI_KERNELS;
	CUmodule mod = load_module(ptx);
	CUfunction ins, look;
	CK(cuModuleGetFunction(&ins, mod, "devhash_abi_insert"));
	CK(cuModuleGetFunction(&look, mod, "devhash_abi_lookup"));
	const u64 max = 1;
	const layout l = layout_for(max, SHARDS);
	CUdeviceptr d_block, d_out;
	CK(cuMemAlloc(&d_block, l.total));
	CK(cuMemsetD8(d_block, 0, l.total));
	CK(cuMemAlloc(&d_out, 8));
	auto insert = [&](u64 block, u64 key, u64 flags) {
		u64 shards = SHARDS, out = d_out, m = max;
		void *args[] = { &block, &key, &flags, &m, &shards, &out };
		CK(cuLaunchKernel(ins, 1, 1, 1, 1, 1, 1, 0, nullptr, args,
				  nullptr));
		CK(cuCtxSynchronize());
		u64 r;
		CK(cuMemcpyDtoH(&r, d_out, 8));
		return r;
	};
	auto lookup = [&](u64 block, u64 key) {
		u64 shards = SHARDS, out = d_out, m = max;
		void *args[] = { &block, &key, &m, &shards, &out };
		CK(cuLaunchKernel(look, 1, 1, 1, 1, 1, 1, 0, nullptr, args,
				  nullptr));
		CK(cuCtxSynchronize());
		u64 r;
		CK(cuMemcpyDtoH(&r, d_out, 8));
		return r;
	};
	int bad = 0;
	auto expect = [&](const char *what, u64 got, u64 want) {
		if (got != want) {
			printf("%s: got %" PRId64 ", want %" PRId64 "\n", what,
			       (int64_t)got, (int64_t)want);
			bad++;
		}
	};
	expect("insert", insert(d_block, 7, 1), 0);
	expect("insert again: EEXIST", insert(d_block, 7, 1), (u64)-17);
	expect("another key, map full: E2BIG", insert(d_block, 8, 1), (u64)-7);
	expect("BPF_ANY: EINVAL", insert(d_block, 9, 0), (u64)-22);
	expect("no device copy: -1", insert(0, 7, 1), (u64)-1);
	expect("lookup without a copy", lookup(0, 7), 0);
	const u64 p = lookup(d_block, 7);
	if (p < d_block + l.rows || p >= d_block + l.rows + 8 * SHARDS) {
		printf("lookup returned %#" PRIx64 ", not a shard of row 0\n",
		       p);
		bad++;
	}
	std::vector<u64> block(l.total / 8);
	CK(cuMemcpyDtoH(block.data(), d_block, l.total));
	u64 bad_flags = 0;
	for (u64 k = 0; k < SHARDS; k++)
		bad_flags += block[counter_offset(l, CNT_BAD_FLAGS, (u32)k) / 8];
	expect("bad-flags count", bad_flags, 1);
	printf("RESULT abi: %d wrong\n", bad);
	fflush(stdout);
	return bad == 0 ? 0 : 1;
}

// ------------------------------------------------- children
const char *CHILD_ENV = "BPFTIME_DEVHASH_GPU_CHILD";

struct child_result {
	int status; // exit status; -1 killed at the deadline
	std::string output;
};

child_result run_child(const std::string &what, std::chrono::seconds deadline)
{
	int out[2];
	REQUIRE(pipe(out) == 0);
	const pid_t pid = fork();
	REQUIRE(pid >= 0);
	if (pid == 0) {
		dup2(out[1], STDOUT_FILENO);
		close(out[0]);
		close(out[1]);
		setenv(CHILD_ENV, what.c_str(), 1);
		setenv("SPDLOG_LEVEL", "off", 1);
		execl("/proc/self/exe", "bpftime_runtime_tests",
		      "[.devhash-gpu-child]", "-o", "/dev/null", nullptr);
		_exit(127);
	}
	close(out[1]);
	child_result r{ -1, "" };
	const auto end = std::chrono::steady_clock::now() + deadline;
	char buf[4096];
	for (;;) {
		const auto left = std::chrono::duration_cast<
			std::chrono::milliseconds>(end - std::chrono::steady_clock::now());
		if (left.count() <= 0)
			break;
		struct pollfd pfd = { out[0], POLLIN, 0 };
		if (poll(&pfd, 1, (int)std::min<long>(left.count(), 1000)) > 0) {
			const ssize_t n = read(out[0], buf, sizeof(buf));
			if (n <= 0)
				break; // EOF: the child is done
			r.output.append(buf, (size_t)n);
		}
	}
	int st = 0;
	if (waitpid(pid, &st, WNOHANG) != pid) {
		// still running at the deadline (or about to end): wait a moment
		const auto grace = std::chrono::steady_clock::now() +
				   std::chrono::milliseconds(200);
		while (waitpid(pid, &st, WNOHANG) != pid) {
			if (std::chrono::steady_clock::now() > grace) {
				kill(pid, SIGKILL);
				waitpid(pid, &st, 0);
				close(out[0]);
				return r; // killed
			}
			usleep(5000);
		}
	}
	close(out[0]);
	r.status = WIFEXITED(st) ? WEXITSTATUS(st) : -2;
	return r;
}

bool have_gpu()
{
	return use_primary_context();
}

void check_child(const char *what, std::chrono::seconds deadline)
{
	const child_result r = run_child(what, deadline);
	INFO("child " << what << " status " << r.status << "\n" << r.output);
	CHECK(r.status == 0);
}
} // namespace

// The child: runs the configuration named in its environment.
TEST_CASE("Device hash GPU child", "[.devhash-gpu-child]")
{
	const char *what = getenv(CHILD_ENV);
	if (what == nullptr)
		return;
	const std::string w = what;
	int rc;
	if (w == "full-directory")
		rc = run_full_directory();
	else if (w == "abi")
		rc = run_abi();
	else if (w == "hash-snapshot")
		rc = run_hash_snapshot();
	else if (w == "routed")
		rc = run_routed();
	else if (const stress_config *c = find_config(w))
		rc = run_stress(*c);
	else {
		printf("unknown configuration %s\n", what);
		rc = 99;
	}
	fflush(stdout);
	_exit(rc);
}

TEST_CASE("Device hash map on the GPU: round two's runs", "[gpu-devhash-device]")
{
	if (!have_gpu())
		SKIP("no GPU");
	for (const char *what : { "a", "b", "c", "e", "g", "h" }) {
		SECTION(what)
		{
			check_child(what, std::chrono::seconds(600));
		}
	}
}

TEST_CASE("Device hash map on the GPU: run d on S2's images",
	  "[gpu-devhash-device]")
{
	if (!have_gpu())
		SKIP("no GPU");
	check_child("d-image", std::chrono::seconds(600));
	check_child("d-takeover", std::chrono::seconds(600));
}

TEST_CASE("Device hash map on the GPU: the controls fail their checks",
	  "[gpu-devhash-device]")
{
	if (!have_gpu())
		SKIP("no GPU");
	for (const char *what : { "ctl1", "ctl2", "ctl5", "ctl6" }) {
		SECTION(what)
		{
			check_child(what, std::chrono::seconds(900));
		}
	}
}

TEST_CASE("Device hash map on the GPU: the shipped build",
	  "[gpu-devhash-device]")
{
	if (!have_gpu())
		SKIP("no GPU");
	check_child("shipped-a", std::chrono::seconds(600));
	check_child("shipped-b", std::chrono::seconds(600));
	check_child("shipped-checks-ctl6", std::chrono::seconds(600));
}

TEST_CASE("Device hash map on the GPU: walks end on a full directory",
	  "[gpu-devhash-device]")
{
	if (!have_gpu())
		SKIP("no GPU");
	check_child("f", std::chrono::seconds(300));
	check_child("full-directory", std::chrono::seconds(60));
}

TEST_CASE("Device hash map on the GPU: opaque slots are walked past",
	  "[gpu-devhash-device]")
{
	if (!have_gpu())
		SKIP("no GPU");
	check_child("opaque-0", std::chrono::seconds(60));
	check_child("opaque-1000", std::chrono::seconds(60));
}

TEST_CASE("Device hash map on the GPU: results as the routing pass calls them",
	  "[gpu-devhash-device]")
{
	if (!have_gpu())
		SKIP("no GPU");
	check_child("abi", std::chrono::seconds(60));
}

// S3.10: the shared trampoline's 1533 branches answer without a host call.
namespace
{
const char TRAMPOLINE_KERNELS[] = R"PTX(
.visible .entry devhash_tramp(
	.param .u64 p_map, .param .u64 p_out)
{
	.local .align 8 .b8 depot[16];
	.reg .b64 %SP;
	.reg .b64 %SPL;
	.reg .b64 %rd<16>;
	mov.u64 %SPL, depot;
	cvta.local.u64 %SP, %SPL;
	ld.param.u64 %rd1, [p_map];
	ld.param.u64 %rd4, [p_out];
	mov.u64 %rd2, 7;
	mov.u64 %rd5, 1;
	st.local.u64 [%SPL], %rd2;
	st.local.u64 [%SPL+8], %rd5;
	add.s64 %rd6, %SP, 8;
	mov.u64 %rd7, 0;
	mov.u64 %rd3, 1;
	{
	.param .b64 param0;
	st.param.b64 [param0+0], %rd1;
	.param .b64 param1;
	st.param.b64 [param1+0], %SP;
	.param .b64 param2;
	st.param.b64 [param2+0], %rd7;
	.param .b64 param3;
	st.param.b64 [param3+0], %rd7;
	.param .b64 param4;
	st.param.b64 [param4+0], %rd7;
	.param .b64 retval0;
	call.uni (retval0), _bpf_helper_ext_0001, (param0, param1, param2, param3, param4);
	ld.param.b64 %rd8, [retval0+0];
	}
	{
	.param .b64 param0;
	st.param.b64 [param0+0], %rd1;
	.param .b64 param1;
	st.param.b64 [param1+0], %SP;
	.param .b64 param2;
	st.param.b64 [param2+0], %rd6;
	.param .b64 param3;
	st.param.b64 [param3+0], %rd3;
	.param .b64 param4;
	st.param.b64 [param4+0], %rd7;
	.param .b64 retval0;
	call.uni (retval0), _bpf_helper_ext_0002, (param0, param1, param2, param3, param4);
	ld.param.b64 %rd9, [retval0+0];
	}
	{
	.param .b64 param0;
	st.param.b64 [param0+0], %rd1;
	.param .b64 param1;
	st.param.b64 [param1+0], %SP;
	.param .b64 param2;
	st.param.b64 [param2+0], %rd7;
	.param .b64 param3;
	st.param.b64 [param3+0], %rd7;
	.param .b64 param4;
	st.param.b64 [param4+0], %rd7;
	.param .b64 retval0;
	call.uni (retval0), _bpf_helper_ext_0003, (param0, param1, param2, param3, param4);
	ld.param.b64 %rd10, [retval0+0];
	}
	st.u64 [%rd4], %rd8;
	st.u64 [%rd4+8], %rd9;
	st.u64 [%rd4+16], %rd10;
	ret;
}
)PTX";
} // namespace

TEST_CASE("The trampoline answers 1533 calls without a host call",
	  "[gpu-devhash-device]")
{
	if (!use_primary_context())
		SKIP("no GPU");
	counting_host host;
	cuda_test::test_module module(TRAMPOLINE_KERNELS);
	module.set_comm(host.comm_on_device);
	const int fd = 3;
	device_buffer out(3 * sizeof(uint64_t));
	device_buffer fake_block(4096);
	for (void *extra : { (void *)nullptr, (void *)(uintptr_t)fake_block.ptr }) {
		INFO("extra_buffer " << extra);
		attach::MapBasicInfo info{};
		info.enabled = true;
		info.key_size = 4;
		info.value_size = 8;
		info.max_entries = 16;
		info.map_type = 1533;
		info.extra_buffer = extra;
		info.max_thread_count = SHARDS;
		module.set_map_info(fd, info);
		module.launch("devhash_tramp", 1, 1, { (uint64_t)fd, out.ptr });
		const auto r = out.read();
		CHECK(r[0] == 0); // lookup: NULL
		CHECK((int64_t)r[1] == -1); // update: no device path here
		CHECK((int64_t)r[2] == -22); // delete: EINVAL
	}
	CHECK(host.served.load() == 0);
}

TEST_CASE("The hash snapshot stages headers and live rows and counters",
	  "[gpu-devhash-device]")
{
	if (!have_gpu())
		SKIP("no GPU");
	check_child("hash-snapshot", std::chrono::seconds(60));
}

TEST_CASE("The hash snapshot acquires the headers", "[gpu-devhash-device]")
{
	const std::string ptx = gpu_model::hash_snapshot_ptx_for_test();
	auto count = [&](const std::string &x) {
		size_t n = 0;
		for (size_t p = ptx.find(x); p != std::string::npos;
		     p = ptx.find(x, p + 1))
			n++;
		return n;
	};
	// the header load: acquire, the pair of the inserts' release
	CHECK(count("ld.acquire.gpu.global.b64") == 1);
	// rows, single words, counters and the count: strong, relaxed
	CHECK(count("ld.relaxed.gpu.global.u64") == 4);
	// nothing else loads from the block
	CHECK(count("ld.global") == 0);
	CHECK(count("ld.u64") == 0);
}

TEST_CASE("Routed 1533 calls agree with the walk on the GPU",
	  "[gpu-devhash-device]")
{
	if (!have_gpu())
		SKIP("no GPU");
	check_child("routed", std::chrono::seconds(120));
}
