// c2host.h: host-side pool setup and end-state checks shared by c2chk and c2bench (driver API).
#pragma once
#include <cuda.h>

#include <cinttypes>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "c2_shared.h"

#define C2_CU(call)                                                                                \
	do {                                                                                       \
		CUresult c2_rc_ = (call);                                                          \
		if (c2_rc_ != CUDA_SUCCESS) {                                                      \
			const char *c2_s_ = nullptr;                                               \
			cuGetErrorString(c2_rc_, &c2_s_);                                          \
			fprintf(stderr, "%s:%d: %s -> %d (%s)\n", __FILE__, __LINE__, #call,      \
				(int)c2_rc_, c2_s_ ? c2_s_ : "?");                                 \
			std::exit(10);                                                             \
		}                                                                                  \
	} while (0)

static inline std::string c2fmt(const char *f, ...)
{
	char b[4096];
	va_list ap;
	va_start(ap, f);
	vsnprintf(b, sizeof b, f, ap);
	va_end(ap);
	return b;
}

static const char *const C2_CT_NAMES[CT_N] = {
	"claims",     "claim_lanes", "misses",     "miss_lanes",  "exit_lanes",  "releases",
	"cap_claim",  "cap_push",    "cap_spin",   "tag_bad",     "dbl_claim",   "foreign",
	"freed_live", "lanetag_bad", "bad_rel",    "bad_clear",   "split_exit",  "tagctr",
	"checks",     "id_change",   "retry",      "steal",       "gaps",        "gap_max_ns",
	"hold_cap",   "bar_cap",     "launches",   "spin_extra",  "hold_ns_min",
	"mask_calls", "mask_disagree", "mask_notin",
};

static inline const char *c2_pool_name(int k)
{
	static const char *n[] = { "bmsm",   "bmglob", "stksm", "stkglob", "stksmnv",
				   "stkglobnv", "lnsm", "lnglob", "lfsm",    "lfglob" };
	return (k >= 0 && k < 10) ? n[k] : "?";
}
static inline int c2_pool_kind(const std::string &s)
{
	for (int k = 0; k < 10; k++)
		if (s == c2_pool_name(k))
			return k;
	return -1;
}
static inline bool c2_is_stack(int k)
{
	return k >= C2_STKSM && k <= C2_STKGLOB_NV;
}
static inline bool c2_is_lane(int k)
{
	return k >= C2_LNSM;
}
static inline bool c2_is_lf(int k)
{
	return k >= C2_LFSM;
}
static inline bool c2_per_sm(int k)
{
	return k == C2_BMSM || k == C2_STKSM || k == C2_STKSM_NV || k == C2_LNSM || k == C2_LFSM;
}

// device attributes the pool is sized from (CUDA driver API, documented attributes)
struct C2Dev {
	int sms = 0, max_threads_per_sm = 0, warps_per_sm = 0, resident_warps = 0;
	int max_blocks_per_sm = 0;
};
static inline C2Dev c2_dev_attrs()
{
	C2Dev d;
	CUdevice dev;
	C2_CU(cuDeviceGet(&dev, 0));
	C2_CU(cuDeviceGetAttribute(&d.sms, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, dev));
	C2_CU(cuDeviceGetAttribute(&d.max_threads_per_sm,
				   CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR, dev));
	C2_CU(cuDeviceGetAttribute(&d.max_blocks_per_sm,
				   CU_DEVICE_ATTRIBUTE_MAX_BLOCKS_PER_MULTIPROCESSOR, dev));
	d.warps_per_sm = d.max_threads_per_sm / 32;
	d.resident_warps = d.warps_per_sm * d.sms;
	return d;
}

static constexpr uint32_t C2_MAX_SLOTS = 32768; // lane pools at factor 2: 24,576 thread slots
static constexpr uint32_t C2_BM_WORDS = 16384; // bitmaps, or the lf pools' u32 flag per slot
static constexpr uint32_t C2_TAG_WORDS = 32768;  // warp pools: slots x 32; lane pools: slots

struct C2Pool {
	C2Dev dev;
	CUdeviceptr bm = 0, head = 0, next = 0, hdr = 0, tags = 0, ctr = 0;
	CUdeviceptr gpd = 0; // the module's __constant__ g_pd
	PoolDesc pd{};
	int kind = 0;
	double factor = 1;

	void alloc(CUmodule mod)
	{
		dev = c2_dev_attrs();
		C2_CU(cuMemAlloc(&bm, 8 * C2_BM_WORDS));
		C2_CU(cuMemAlloc(&head, 8 * C2_HEAD_STRIDE * 64));
		C2_CU(cuMemAlloc(&next, 4 * C2_MAX_SLOTS));
		C2_CU(cuMemAlloc(&hdr, 8ull * C2_HDR_WORDS * C2_MAX_SLOTS));
		C2_CU(cuMemAlloc(&tags, 8ull * C2_TAG_WORDS));
		C2_CU(cuMemAlloc(&ctr, 8ull * C2_CTR_STRIDE * CT_N));
		size_t bytes = 0;
		C2_CU(cuModuleGetGlobal(&gpd, &bytes, mod, "g_pd"));
		if (bytes != sizeof(PoolDesc)) {
			fprintf(stderr, "g_pd is %zu bytes, host PoolDesc %zu\n", bytes, sizeof(PoolDesc));
			std::exit(4);
		}
	}
	void free_all()
	{
		cuMemFree(bm);
		cuMemFree(head);
		cuMemFree(next);
		cuMemFree(hdr);
		cuMemFree(tags);
		cuMemFree(ctr);
	}
	// Warp pools: slots = factor x resident warps (device attributes:
	// MAX_THREADS_PER_MULTIPROCESSOR / 32 x MULTIPROCESSOR_COUNT); per-SM pools split them evenly.
	// Lane pools: thread slots = factor x resident threads, as 32 lane rows of W bits per region.
	void setup(int k, double f, uint32_t aba_ns = 0)
	{
		kind = k;
		factor = f;
		const uint32_t np = c2_per_sm(k) ? (uint32_t)dev.sms : 1;
		uint32_t nslots = (uint32_t)std::lround(f * dev.resident_warps);
		nslots -= nslots % np;
		uint32_t lw = 0;
		if (c2_is_lane(k)) {
			lw = nslots / np;  // warp slots per region = bits per lane row
			nslots = np * 32 * lw;
		}
		if (nslots == 0 || nslots > C2_MAX_SLOTS || (!c2_is_lane(k) && nslots * 32 > C2_TAG_WORDS)) {
			fprintf(stderr, "bad pool size %u\n", nslots);
			std::exit(4);
		}
		pd = PoolDesc{};
		pd.bm = bm;
		pd.head = head;
		pd.next = next;
		pd.hdr = hdr;
		pd.tags = tags;
		pd.ctr = ctr;
		pd.nslots = nslots;
		pd.npools = np;
		pd.spp = nslots / np;
		pd.wpp = c2_is_lane(k) ? (lw + 63) / 64 : (pd.spp + 63) / 64;
		pd.lw = lw;
		pd.wid_S = (uint32_t)dev.sms;
		pd.wid_W = (uint32_t)dev.warps_per_sm;
		pd.aba_ns = aba_ns;
		if (c2_is_lf(k) ? nslots > 2 * C2_BM_WORDS
				: (c2_is_lane(k) ? np * 32 * pd.wpp : np * pd.wpp) > C2_BM_WORDS) {
			fprintf(stderr, "bitmap too large\n");
			std::exit(4);
		}
		C2_CU(cuMemsetD32(bm, 0, 2 * C2_BM_WORDS));
		std::vector<uint64_t> h(C2_HEAD_STRIDE * 64, 0);
		std::vector<uint32_t> nx(C2_MAX_SLOTS, C2_NONE);
		if (c2_is_stack(k)) {
			for (uint32_t p = 0; p < np; p++) {
				h[(size_t)p * C2_HEAD_STRIDE] = (uint64_t)(p * pd.spp); // version 0
				for (uint32_t i = 0; i < pd.spp; i++) {
					const uint32_t s = p * pd.spp + i;
					nx[s] = (i + 1 < pd.spp) ? s + 1 : C2_NONE;
				}
			}
		}
		C2_CU(cuMemcpyHtoD(head, h.data(), h.size() * 8));
		C2_CU(cuMemcpyHtoD(next, nx.data(), nx.size() * 4));
		C2_CU(cuMemsetD32(hdr, 0, 2 * C2_HDR_WORDS * C2_MAX_SLOTS));
		C2_CU(cuMemsetD32(tags, 0, 2 * C2_TAG_WORDS));
		zero_counters();
		C2_CU(cuMemcpyHtoD(gpd, &pd, sizeof pd));
		C2_CU(cuCtxSynchronize());
	}
	void zero_counters()
	{
		C2_CU(cuMemsetD32(ctr, 0, 2 * C2_CTR_STRIDE * CT_N));
		const uint64_t big = ~0ull;
		C2_CU(cuMemcpyHtoD(ctr + 8ull * C2_CTR_STRIDE * CT_HOLD_NS_MIN, &big, 8));
	}
	std::vector<uint64_t> counters()
	{
		std::vector<uint64_t> raw(C2_CTR_STRIDE * CT_N), v(CT_N);
		C2_CU(cuMemcpyDtoH(raw.data(), ctr, raw.size() * 8));
		for (int i = 0; i < CT_N; i++)
			v[i] = raw[(size_t)i * C2_CTR_STRIDE];
		return v;
	}
	// After every run: is every slot free again, every live count 0, every ledger entry 0?
	struct End {
		bool clean = true;
		uint64_t free_slots = 0, busy_bits = 0, stack_dups = 0, stack_wrong_pool = 0,
			 stack_cycle = 0, live_nonzero = 0, ledger_nonzero = 0;
		std::string detail;
	};
	End end_state(bool check_live = true, bool check_ledger = true)
	{
		End e;
		const uint32_t n = pd.nslots, np = pd.npools;
		std::vector<uint64_t> hd((size_t)C2_HDR_WORDS * n);
		C2_CU(cuMemcpyDtoH(hd.data(), hdr, hd.size() * 8));
		for (uint32_t s = 0; s < n; s++) {
			if (check_live && (uint32_t)hd[(size_t)s * C2_HDR_WORDS] != 0)
				e.live_nonzero++;
			if (check_ledger && hd[(size_t)s * C2_HDR_WORDS + 2] != 0)
				e.ledger_nonzero++;
		}
		if (c2_is_lf(kind)) {
			std::vector<uint32_t> f(n);
			C2_CU(cuMemcpyDtoH(f.data(), bm, f.size() * 4));
			for (auto x : f)
				e.busy_bits += x != 0;
			e.free_slots = n - e.busy_bits;
		} else if (!c2_is_stack(kind)) {
			std::vector<uint64_t> w((size_t)np * pd.wpp * (c2_is_lane(kind) ? 32 : 1));
			C2_CU(cuMemcpyDtoH(w.data(), bm, w.size() * 8));
			for (auto x : w)
				e.busy_bits += __builtin_popcountll(x);
			e.free_slots = n - e.busy_bits;
		} else {
			std::vector<uint64_t> h(C2_HEAD_STRIDE * np);
			std::vector<uint32_t> nx(n);
			C2_CU(cuMemcpyDtoH(h.data(), head, h.size() * 8));
			C2_CU(cuMemcpyDtoH(nx.data(), next, nx.size() * 4));
			std::set<uint32_t> seen;
			for (uint32_t p = 0; p < np; p++) {
				uint32_t i = (uint32_t)h[(size_t)p * C2_HEAD_STRIDE];
				uint32_t steps = 0;
				while (i != C2_NONE) {
					if (i >= n || ++steps > n + 1) {
						e.stack_cycle++;
						break;
					}
					if (!seen.insert(i).second) {
						e.stack_dups++;
						break;
					}
					if (i / pd.spp != p)
						e.stack_wrong_pool++;
					i = nx[i];
				}
			}
			e.free_slots = seen.size();
		}
		e.clean = e.free_slots == n && e.stack_dups == 0 && e.stack_wrong_pool == 0 &&
			  e.stack_cycle == 0 && e.live_nonzero == 0 && e.ledger_nonzero == 0;
		e.detail = c2fmt("free=%" PRIu64 "/%u busy_bits=%" PRIu64 " dups=%" PRIu64
				 " wrong_pool=%" PRIu64 " cycle=%" PRIu64 " live_nonzero=%" PRIu64
				 " ledger_nonzero=%" PRIu64,
				 e.free_slots, n, e.busy_bits, e.stack_dups, e.stack_wrong_pool,
				 e.stack_cycle, e.live_nonzero, e.ledger_nonzero);
		return e;
	}
};

static inline std::string c2_json_counters(const std::vector<uint64_t> &v)
{
	std::string s = "{";
	for (int i = 0; i < CT_N; i++)
		s += c2fmt("%s\"%s\": %" PRIu64, i ? ", " : "", C2_CT_NAMES[i], v[i]);
	return s + "}";
}
static inline std::string c2_json_end(const C2Pool::End &e)
{
	return c2fmt("{\"clean\": %s, \"free_slots\": %" PRIu64 ", \"busy_bits\": %" PRIu64
		     ", \"stack_dups\": %" PRIu64 ", \"stack_wrong_pool\": %" PRIu64
		     ", \"stack_cycle\": %" PRIu64 ", \"live_nonzero\": %" PRIu64
		     ", \"ledger_nonzero\": %" PRIu64 "}",
		     e.clean ? "true" : "false", e.free_slots, e.busy_bits, e.stack_dups,
		     e.stack_wrong_pool, e.stack_cycle, e.live_nonzero, e.ledger_nonzero);
}
