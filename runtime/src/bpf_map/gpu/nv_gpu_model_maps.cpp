#include "nv_gpu_model_maps.hpp"
#include "nv_gpu_device_hash_map.hpp"
#include "base_attach_impl.hpp"
#include "bpftime_shm.hpp"
#include "bpftime_shm_internal.hpp"
#include "spdlog/spdlog.h"
#include "probed_launch_activity.hpp"
#include "trampoline/gpu_devhash_layout.h"
#include <algorithm>
#include <signal.h>
#include <time.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <linux/bpf.h>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace bpftime;

// ---------------------------------------------------------------- 1530

nv_gpu_thread_local_map_impl::nv_gpu_thread_local_map_impl(
	boost::interprocess::managed_shared_memory &, uint64_t value_size,
	uint64_t max_entries)
	: value_size(value_size), max_entries(max_entries)
{
}

void *nv_gpu_thread_local_map_impl::elem_lookup(const void *)
{
	// the values live in GPU threads' local memory only
	errno = ENOENT;
	return nullptr;
}

long nv_gpu_thread_local_map_impl::elem_update(const void *, const void *,
					       uint64_t)
{
	errno = EINVAL;
	return -1;
}

long nv_gpu_thread_local_map_impl::elem_delete(const void *)
{
	errno = EINVAL;
	return -1;
}

static int array_next_key(const void *key, void *next_key, uint64_t entries)
{
	auto &next = *(uint32_t *)next_key;
	if (key == nullptr || *(const uint32_t *)key >= entries) {
		next = 0;
		return 0;
	}
	auto k = *(const uint32_t *)key;
	if (k + 1 >= entries) {
		errno = ENOENT;
		return -1;
	}
	next = k + 1;
	return 0;
}

int nv_gpu_thread_local_map_impl::map_get_next_key(const void *key,
						   void *next_key)
{
	return array_next_key(key, next_key, max_entries);
}

// ---------------------------------------------------------- 1531, 1532

nv_gpu_device_array_map_impl::nv_gpu_device_array_map_impl(
	boost::interprocess::managed_shared_memory &memory, uint64_t value_size,
	uint64_t max_entries, uint64_t thread_count, bool per_thread)
	: data_buffer(memory.get_segment_manager()),
	  status_buffer(memory.get_segment_manager()), value_size(value_size),
	  max_entries(max_entries),
	  thread_count(per_thread ? thread_count : 1), per_thread(per_thread)
{
	if (value_size == 0 || value_size % 8 != 0)
		throw std::runtime_error(
			"GPU device array maps take u64 words: value size must be a multiple of 8");
	data_buffer.resize(max_entries * this->thread_count * value_size);
	std::memset(data_buffer.data(), 0, data_buffer.size());
	// one slot per status word, each a whole value as bpf() returns it
	status_buffer.resize(GPU_MODEL_STATUS_WORDS * status_slot_bytes());
	std::memset(status_buffer.data(), 0, status_buffer.size());
	SPDLOG_INFO(
		"GPU device array map ({}): value_size={}, max_entries={}, threads per key={}, host copy {} bytes",
		per_thread ? "1532 per-thread" : "1531 sum", value_size,
		max_entries, this->thread_count, data_buffer.size());
}

void *nv_gpu_device_array_map_impl::elem_lookup(const void *key)
{
	auto k = *(const uint32_t *)key;
	// pairs with the delivery's release increment of the status
	(void)__atomic_load_n(status_word(GPU_MODEL_STATUS_DELIVERIES),
			      __ATOMIC_ACQUIRE);
	if (k >= max_entries && k < max_entries + GPU_MODEL_STATUS_WORDS)
		// status word k - max_entries, then zeros up to a value
		return status_buffer.data() +
		       (k - max_entries) * status_slot_bytes();
	if (k >= max_entries) {
		errno = ENOENT;
		return nullptr;
	}
	return data_buffer.data() + (uint64_t)k * thread_count * value_size;
}

long nv_gpu_device_array_map_impl::elem_update(const void *, const void *,
					       uint64_t)
{
	// the device copy is the map; the CPU's is a delivered copy
	errno = EPERM;
	return -1;
}

long nv_gpu_device_array_map_impl::elem_delete(const void *)
{
	errno = EINVAL;
	return -1;
}

int nv_gpu_device_array_map_impl::map_get_next_key(const void *key,
						   void *next_key)
{
	return array_next_key(key, next_key, max_entries);
}

// -------------------------------------------------------------- delivery

namespace
{
// dst[e * W + w] = sum over s < S of src[(e * S + s) * W + w], read with
// ld.relaxed.gpu.global on the cvta.to.global address, the form of the
// programs' adds into the copy (atom/red .global, nv_thread_local_routing,
// which runs for every module of a process with a 1531 or 1532 map): the
// same address in the same state space, so the load and the adds are
// morally strong (PTX ISA 8.7).
const char *snapshot_ptx = R"PTX(
.version 7.0
.target sm_70
.address_size 64
.visible .entry bpftime_model_snapshot(
	.param .u64 p_src, .param .u64 p_dst, .param .u64 p_n,
	.param .u64 p_w, .param .u64 p_s)
{
	.reg .pred %p<3>;
	.reg .b32 %r<5>;
	.reg .b64 %rd<20>;
	ld.param.u64 %rd1, [p_src];
	ld.param.u64 %rd2, [p_dst];
	ld.param.u64 %rd3, [p_n];
	ld.param.u64 %rd4, [p_w];
	ld.param.u64 %rd5, [p_s];
	mov.u32 %r1, %ctaid.x;
	mov.u32 %r2, %ntid.x;
	mov.u32 %r3, %tid.x;
	mul.wide.u32 %rd6, %r1, %r2;
	cvt.u64.u32 %rd7, %r3;
	add.u64 %rd6, %rd6, %rd7;
	mov.u32 %r4, %nctaid.x;
	mul.wide.u32 %rd8, %r4, %r2;
$L_word:
	setp.ge.u64 %p1, %rd6, %rd3;
	@%p1 bra $L_done;
	div.u64 %rd9, %rd6, %rd4;
	rem.u64 %rd10, %rd6, %rd4;
	mul.lo.u64 %rd11, %rd9, %rd5;
	mov.u64 %rd12, 0;
	mov.u64 %rd13, 0;
$L_shard:
	setp.ge.u64 %p2, %rd13, %rd5;
	@%p2 bra $L_store;
	add.u64 %rd14, %rd11, %rd13;
	mad.lo.u64 %rd14, %rd14, %rd4, %rd10;
	shl.b64 %rd14, %rd14, 3;
	add.u64 %rd14, %rd1, %rd14;
	cvta.to.global.u64 %rd17, %rd14;
	ld.relaxed.gpu.global.u64 %rd15, [%rd17];
	add.u64 %rd12, %rd12, %rd15;
	add.u64 %rd13, %rd13, 1;
	bra.uni $L_shard;
$L_store:
	shl.b64 %rd16, %rd6, 3;
	add.u64 %rd16, %rd2, %rd16;
	st.u64 [%rd16], %rd12;
	add.u64 %rd6, %rd6, %rd8;
	bra.uni $L_word;
$L_done:
	ret;
}
)PTX";

// ------------------------------------------- the device hash map (1533)
//
// The hash snapshot (plan S4, D14): one item per directory slot, then one
// per loss counter, then the reservation count.
//   slot i     the raw header, loaded with ld.acquire.gpu.global (it pairs
//              with the inserts' st.release.gpu of live and refused: a
//              live header's first add is seen), and for a live header its
//              row's value: the sum of its shards below r, its single word
//              at r and above (ld.relaxed.gpu.global, the form of the
//              programs' adds); 0 for every other state
//   counter c  the sum of its shards
//   count      the reservation count (relaxed)
// Staged as gpu_devhash_layout.h's staging_for: {header, value} per slot,
// then the counters, then the count.
static_assert(bpftime_devhash::STATE_MASK == 7 &&
		      bpftime_devhash::ST_LIVE == 2 &&
		      bpftime_devhash::ROW_SHIFT == 3 &&
		      bpftime_devhash::ROW_MASK == 536870911 &&
		      bpftime_devhash::NUM_COUNTERS == 7,
	      "the hash snapshot's PTX has these constants written in");
static_assert(bpftime_devhash::staging_for(1).counters ==
			      bpftime_devhash::slots_for(1) * 16 &&
		      bpftime_devhash::staging_for(1).count ==
			      bpftime_devhash::slots_for(1) * 16 + 56 &&
		      bpftime_devhash::staging_header_offset(3) == 48 &&
		      bpftime_devhash::staging_value_offset(3) == 56,
	      "the hash snapshot's PTX stages in this layout");

const char *hash_snapshot_ptx = R"PTX(
.version 7.0
.target sm_70
.address_size 64
.visible .entry bpftime_model_hash_snapshot(
	.param .u64 p_block, .param .u64 p_staging, .param .u64 p_slots,
	.param .u64 p_max, .param .u64 p_r, .param .u64 p_shards,
	.param .u64 p_dir, .param .u64 p_rows, .param .u64 p_single,
	.param .u64 p_counters, .param .u64 p_count)
{
	.reg .pred %p<8>;
	.reg .b32 %r<5>;
	.reg .b64 %rd<34>;
	ld.param.u64 %rd1, [p_block];
	ld.param.u64 %rd2, [p_staging];
	ld.param.u64 %rd3, [p_slots];
	ld.param.u64 %rd4, [p_max];
	ld.param.u64 %rd5, [p_r];
	ld.param.u64 %rd6, [p_shards];
	ld.param.u64 %rd7, [p_dir];
	ld.param.u64 %rd8, [p_rows];
	ld.param.u64 %rd9, [p_single];
	ld.param.u64 %rd10, [p_counters];
	ld.param.u64 %rd11, [p_count];
	mov.u32 %r1, %ctaid.x;
	mov.u32 %r2, %ntid.x;
	mov.u32 %r3, %tid.x;
	mul.wide.u32 %rd12, %r1, %r2;
	cvt.u64.u32 %rd13, %r3;
	add.u64 %rd12, %rd12, %rd13;
	mov.u32 %r4, %nctaid.x;
	mul.wide.u32 %rd14, %r4, %r2;
	add.u64 %rd15, %rd3, 8;
	shl.b64 %rd28, %rd3, 4;
	add.u64 %rd28, %rd28, %rd2;
$L_item:
	setp.ge.u64 %p1, %rd12, %rd15;
	@%p1 bra $L_done;
	setp.lt.u64 %p2, %rd12, %rd3;
	@!%p2 bra $L_tail;
	shl.b64 %rd16, %rd12, 3;
	add.u64 %rd16, %rd16, %rd7;
	add.u64 %rd16, %rd16, %rd1;
	cvta.to.global.u64 %rd16, %rd16;
	ld.acquire.gpu.global.b64 %rd17, [%rd16];
	mov.u64 %rd18, 0;
	and.b64 %rd19, %rd17, 7;
	setp.ne.u64 %p3, %rd19, 2;
	@%p3 bra $L_stage;
	shr.u64 %rd20, %rd17, 3;
	and.b64 %rd20, %rd20, 536870911;
	setp.ge.u64 %p4, %rd20, %rd4;
	@%p4 bra $L_stage;
	setp.ge.u64 %p5, %rd20, %rd5;
	@%p5 bra $L_single;
	mul.lo.u64 %rd21, %rd20, %rd6;
	shl.b64 %rd21, %rd21, 3;
	add.u64 %rd21, %rd21, %rd8;
	add.u64 %rd21, %rd21, %rd1;
	cvta.to.global.u64 %rd21, %rd21;
	mov.u64 %rd22, 0;
$L_shard:
	setp.ge.u64 %p6, %rd22, %rd6;
	@%p6 bra $L_stage;
	shl.b64 %rd23, %rd22, 3;
	add.u64 %rd23, %rd21, %rd23;
	ld.relaxed.gpu.global.u64 %rd24, [%rd23];
	add.u64 %rd18, %rd18, %rd24;
	add.u64 %rd22, %rd22, 1;
	bra.uni $L_shard;
$L_single:
	sub.u64 %rd25, %rd20, %rd5;
	shl.b64 %rd25, %rd25, 3;
	add.u64 %rd25, %rd25, %rd9;
	add.u64 %rd25, %rd25, %rd1;
	cvta.to.global.u64 %rd25, %rd25;
	ld.relaxed.gpu.global.u64 %rd18, [%rd25];
$L_stage:
	shl.b64 %rd26, %rd12, 4;
	add.u64 %rd26, %rd26, %rd2;
	st.u64 [%rd26], %rd17;
	st.u64 [%rd26+8], %rd18;
	bra.uni $L_next;
$L_tail:
	sub.u64 %rd27, %rd12, %rd3;
	setp.lt.u64 %p7, %rd27, 7;
	@!%p7 bra $L_count;
	mul.lo.u64 %rd29, %rd27, %rd6;
	shl.b64 %rd29, %rd29, 3;
	add.u64 %rd29, %rd29, %rd10;
	add.u64 %rd29, %rd29, %rd1;
	cvta.to.global.u64 %rd29, %rd29;
	mov.u64 %rd30, 0;
	mov.u64 %rd22, 0;
$L_cshard:
	setp.ge.u64 %p6, %rd22, %rd6;
	@%p6 bra $L_cstore;
	shl.b64 %rd23, %rd22, 3;
	add.u64 %rd23, %rd29, %rd23;
	ld.relaxed.gpu.global.u64 %rd24, [%rd23];
	add.u64 %rd30, %rd30, %rd24;
	add.u64 %rd22, %rd22, 1;
	bra.uni $L_cshard;
$L_cstore:
	shl.b64 %rd31, %rd27, 3;
	add.u64 %rd31, %rd31, %rd28;
	st.u64 [%rd31], %rd30;
	bra.uni $L_next;
$L_count:
	add.u64 %rd32, %rd11, %rd1;
	cvta.to.global.u64 %rd32, %rd32;
	ld.relaxed.gpu.global.u64 %rd33, [%rd32];
	st.u64 [%rd28+56], %rd33;
$L_next:
	add.u64 %rd12, %rd12, %rd14;
	bra.uni $L_item;
$L_done:
	ret;
}
)PTX";

// One launch of the hash snapshot over a block (generic address) into a
// staging buffer (a device-visible address).
CUresult launch_hash_snapshot(CUfunction fn, CUstream stream, uint64_t block,
			      uint64_t max_entries, uint64_t shards,
			      uint64_t staging)
{
	const bpftime_devhash::layout l =
		bpftime_devhash::layout_for(max_entries, shards);
	uint64_t slots = l.slots, r = l.r, dir = l.dir, rows = l.rows,
		 single = l.single, counters = l.counters,
		 count = bpftime_devhash::ctl_offset(bpftime_devhash::CTL_COUNT);
	void *args[] = { &block,  &staging, &slots,	&max_entries,
			 &r,	  &shards,  &dir,	&rows,
			 &single, &counters, &count };
	const uint64_t items = slots + bpftime_devhash::NUM_COUNTERS + 1;
	const unsigned blocks =
		(unsigned)std::min<uint64_t>((items + 255) / 256, 1024);
	return cuLaunchKernel(fn, blocks, 1, 1, 256, 1, 1, 0, stream, args,
			      nullptr);
}

uint64_t now_ns()
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(
		       std::chrono::steady_clock::now().time_since_epoch())
		.count();
}

uint64_t env_ms(const char *name, uint64_t dflt)
{
	const char *v = getenv(name);
	if (!v || !*v)
		return dflt;
	return strtoull(v, nullptr, 10);
}

struct registered_map {
	nv_gpu_device_array_map_impl *map;
	CUdeviceptr device;
	uint64_t entries, shards, words_per_entry;
	uint64_t *staging = nullptr; // pinned host memory, snapshot output
	CUdeviceptr staging_device = 0;
	// schedule: every busy_ns while its values change, else idle_ns; an
	// application synchronization not yet covered makes it due busy_ns
	// after its last delivery
	uint64_t busy_ns = 0, idle_ns = 0, next_due_ns = 0, last_ns = 0;
	uint64_t sync_gap_ns = 0;
	uint64_t covered_syncs = 0;
	uint64_t deliveries = 0, ns_total = 0, ns_max = 0;
};

// A device hash map (1533, plan S5): its block, its shards, pinned staging
// for the hash snapshot, and the companion's value words (D12; null: none
// bound).
struct registered_hash {
	nv_gpu_device_hash_map_impl *map;
	CUdeviceptr block;
	uint64_t max_entries, shards;
	uint8_t *staging = nullptr;
	CUdeviceptr staging_device = 0;
	uint64_t staging_bytes = 0;
	uint64_t *companion = nullptr;
	bool dropped = false; // the claim was lost: no more deliveries
	uint64_t busy_ns = 0, idle_ns = 0, next_due_ns = 0, last_ns = 0;
	uint64_t sync_gap_ns = 0;
	uint64_t covered_syncs = 0;
	uint64_t last_sum = 0;
	uint64_t deliveries = 0, ns_total = 0, ns_max = 0;
};

struct delivery_state {
	std::mutex reg_mutex; // the list
	std::timed_mutex run_mutex; // one delivery at a time
	std::vector<registered_map> maps;
	std::vector<registered_hash> hashes;
	pid_t owner = 0; // the process that registered (fork children skip)
	CUcontext ctx = nullptr;
	CUmodule module = nullptr;
	CUfunction fn = nullptr;
	CUstream stream = nullptr;
	bool broken = false;
	// the hash snapshot (D14): a module of its own, its own warm-up and
	// broken flag, so a failure there leaves the arrays' deliveries alone
	CUmodule hash_module = nullptr;
	CUfunction hash_fn = nullptr;
	bool hash_broken = false;
	std::thread thread;
	std::mutex wake_mutex;
	std::condition_variable wake;
	std::atomic<bool> stop{ false };
	std::atomic<bool> sync_pending{ false };
	std::atomic<bool> drain_running{ false };
	bool final_done = false;
	uint64_t deliveries = 0;
	uint64_t snapshot_ns_total = 0;
	uint64_t setup_ns = 0;
	uint64_t wait_timeouts = 0;
};

delivery_state &state()
{
	// never destroyed: the exit handler and the thread may outlive
	// static destructors
	static auto *s = new delivery_state();
	return *s;
}

bool owned_here()
{
	auto &s = state();
	return s.owner != 0 && s.owner == getpid();
}

// The application's synchronizations (agent hooks): wake the drain thread
// so that a delivery covers them. One wake per batch of syncs.
int deliver_in_sync_call();
bool deliver_at_sync()
{
	static const bool on = [] {
		const char *v = getenv("BPFTIME_GPU_DELIVERY_AT_SYNC");
		return v && *v && strcmp(v, "0") != 0;
	}();
	return on;
}

void on_app_sync()
{
	auto &s = state();
	if (deliver_at_sync()) {
		deliver_in_sync_call();
		return;
	}
	if (!s.sync_pending.exchange(true, std::memory_order_acq_rel)) {
		std::lock_guard<std::mutex> lk(s.wake_mutex);
		s.wake.notify_all();
	}
}

// Waits for the delivery stream without blocking in the driver: polls
// cuStreamQuery (a successful query behaves as the synchronize, PTX ISA
// 8.9.4), gives up at the deadline or when told to stop.
CUresult wait_stream(delivery_state &s, uint64_t deadline_ns, bool stoppable)
{
	for (;;) {
		CUresult r = cuStreamQuery(s.stream);
		if (r != CUDA_ERROR_NOT_READY)
			return r;
		if ((stoppable && s.stop.load(std::memory_order_acquire)) ||
		    now_ns() > deadline_ns) {
			s.wait_timeouts++;
			return CUDA_ERROR_NOT_READY;
		}
		struct timespec ts = { 0, 50000 };
		nanosleep(&ts, nullptr);
	}
}

// with run_mutex held, the context current and relaxed capture mode
bool prepare_locked(delivery_state &s, uint64_t deadline_ns, bool stoppable)
{
	if (s.broken)
		return false;
	if (s.fn)
		return true;
	CUresult r = cuModuleLoadData(&s.module, snapshot_ptx);
	if (r == CUDA_SUCCESS)
		r = cuModuleGetFunction(&s.fn, s.module,
					"bpftime_model_snapshot");
	if (r == CUDA_SUCCESS && !s.stream)
		r = cuStreamCreate(&s.stream, CU_STREAM_NON_BLOCKING);
	if (r == CUDA_SUCCESS) {
		// the first launch compiles the PTX (lazy loading): here, so
		// that it counts as set-up
		CUdeviceptr none = 0;
		uint64_t zero = 0, one = 1;
		void *args[] = { &none, &none, &zero, &one, &one };
		r = cuLaunchKernel(s.fn, 1, 1, 1, 32, 1, 1, 0, s.stream, args,
				   nullptr);
		if (r == CUDA_SUCCESS)
			r = wait_stream(s, deadline_ns, stoppable);
	}
	if (r != CUDA_SUCCESS) {
		SPDLOG_ERROR("GPU map delivery: cannot set up ({})", (int)r);
		if (r != CUDA_ERROR_NOT_READY)
			s.broken = true;
		s.fn = r == CUDA_ERROR_NOT_READY ? s.fn : nullptr;
		return false;
	}
	return true;
}

// drain: the delivery thread; at_sync: in the application's thread, inside
// its synchronize call (BPFTIME_GPU_DELIVERY_AT_SYNC=1), every map whose
// last delivery does not cover the application's latest synchronization;
// exit: the exit handler, every map
// Tests (S5): fail the hash module's load, fail a hash map's registration.
std::atomic<bool> fail_hash_module_for_test{ false };
std::atomic<bool> fail_hash_registration_for_test{ false };

// nothing left to deliver: every kind broken or without maps
bool nothing_to_deliver(delivery_state &s)
{
	std::lock_guard<std::mutex> g(s.reg_mutex);
	return (s.broken || s.maps.empty()) &&
	       (s.hash_broken || s.hashes.empty());
}

// with run_mutex held, the context current and relaxed capture mode
bool prepare_hash_locked(delivery_state &s, uint64_t deadline_ns,
			 bool stoppable)
{
	if (s.hash_broken)
		return false;
	if (s.hash_fn)
		return true;
	CUresult r = fail_hash_module_for_test.load() ?
			     CUDA_ERROR_INVALID_IMAGE :
			     cuModuleLoadData(&s.hash_module, hash_snapshot_ptx);
	if (r == CUDA_SUCCESS)
		r = cuModuleGetFunction(&s.hash_fn, s.hash_module,
					"bpftime_model_hash_snapshot");
	if (r == CUDA_SUCCESS && !s.stream)
		r = cuStreamCreate(&s.stream, CU_STREAM_NON_BLOCKING);
	// the first launch compiles the PTX (lazy loading): on a small block
	// of its own, so that it counts as set-up
	CUdeviceptr wb = 0, ws = 0;
	if (r == CUDA_SUCCESS) {
		const bpftime_devhash::layout l =
			bpftime_devhash::layout_for(1, 32);
		r = cuMemAlloc(&wb, l.total);
		if (r == CUDA_SUCCESS)
			r = cuMemsetD8Async(wb, 0, l.total, s.stream);
		if (r == CUDA_SUCCESS)
			r = cuMemAlloc(&ws,
				       bpftime_devhash::staging_for(1).total);
		if (r == CUDA_SUCCESS)
			r = launch_hash_snapshot(s.hash_fn, s.stream, wb, 1, 32,
						 ws);
		if (r == CUDA_SUCCESS)
			r = wait_stream(s, deadline_ns, stoppable);
	}
	if (r != CUDA_ERROR_NOT_READY) {
		if (wb)
			cuMemFree(wb);
		if (ws)
			cuMemFree(ws);
	}
	if (r != CUDA_SUCCESS) {
		SPDLOG_ERROR("GPU map delivery: cannot set up the hash snapshot ({})",
			     (int)r);
		if (r != CUDA_ERROR_NOT_READY)
			s.hash_broken = true;
		s.hash_fn = r == CUDA_ERROR_NOT_READY ? s.hash_fn : nullptr;
		return false;
	}
	return true;
}

enum class delivery_kind { drain, at_sync, exit };

// One delivery of every map that is due (all of them at exit); returns the
// time the next one is due. The application's synchronization count is
// read before each snapshot is enqueued: everything of the kernels those
// synchronizations waited for happens before the snapshot's reads (PTX ISA
// 8.9.4: completion of a stream's last task synchronizes with the return of
// its synchronize; the hook's count increment follows that return; the
// drain thread reads the count before the call that enqueues the snapshot,
// whose start synchronizes with the snapshot's start).
uint64_t deliver_locked(delivery_state &s, delivery_kind kind,
			uint64_t deadline_ns, int *delivered)
{
	const bool at_exit = kind == delivery_kind::exit;
	std::vector<registered_map> maps;
	std::vector<registered_hash> hashes;
	{
		std::lock_guard<std::mutex> g(s.reg_mutex);
		if (!s.broken)
			maps = s.maps;
		if (!s.hash_broken)
			hashes = s.hashes;
	}
	if (maps.empty() && hashes.empty())
		return now_ns() + 200000000ull;
	if (!maps.empty() && !s.fn) {
		const uint64_t t = now_ns();
		if (!prepare_locked(s, deadline_ns,
				    kind == delivery_kind::drain)) {
			if (!s.broken)
				return now_ns() + 20000000ull;
			maps.clear(); // the hash maps still go
		}
		s.setup_ns += now_ns() - t;
	}
	const uint64_t t0 = now_ns();
	const uint64_t syncs = attach::launch_activity().app_sync_count();
	uint64_t t_launch = 0, t_wait = 0, t_copy = 0;
	uint64_t next_due = UINT64_MAX;
	for (size_t mi = 0; mi < maps.size(); mi++) {
		auto &m = maps[mi];
		uint64_t due = m.next_due_ns;
		if (syncs != m.covered_syncs)
			due = std::min(due, kind == delivery_kind::at_sync ?
						    0 :
						    m.last_ns + m.sync_gap_ns);
		if (kind == delivery_kind::at_sync &&
		    syncs == m.covered_syncs)
			due = UINT64_MAX;
		if (!at_exit && due > t0) {
			next_due = std::min(next_due, due);
			continue;
		}
		const uint64_t m0 = now_ns();
		bool changed = false;
		uint64_t n = m.entries * m.words_per_entry;
		void *args[] = { &m.device, &m.staging_device, &n,
				 &m.words_per_entry, &m.shards };
		unsigned blocks =
			(unsigned)std::min<uint64_t>((n + 255) / 256, 1024);
		uint64_t a = now_ns();
		CUresult r = cuLaunchKernel(s.fn, blocks, 1, 1, 256, 1, 1, 0,
					    s.stream, args, nullptr);
		uint64_t b = now_ns();
		if (r == CUDA_SUCCESS)
			r = wait_stream(s, deadline_ns,
					kind == delivery_kind::drain);
		t_launch += b - a;
		t_wait += now_ns() - b;
		if (r == CUDA_ERROR_NOT_READY) {
			// stopped or late: no copy of this map now
			return UINT64_MAX;
		}
		if (r != CUDA_SUCCESS) {
			SPDLOG_ERROR("GPU map delivery: snapshot failed ({})",
				     (int)r);
			s.broken = true;
			break; // the hash maps still go
		}
		uint64_t *dst = m.map->host_words();
		uint64_t c0 = now_ns();
		for (uint64_t i = 0; i < n; i++) {
			uint64_t v = m.staging[i];
			if (__atomic_load_n(&dst[i], __ATOMIC_RELAXED) != v) {
				__atomic_store_n(&dst[i], v, __ATOMIC_RELAXED);
				changed = true;
			}
		}
		t_copy += now_ns() - c0;
		auto *map = m.map;
		if (!at_exit)
			__atomic_store_n(
				map->status_word(GPU_MODEL_STATUS_SYNCS_COVERED),
				syncs, __ATOMIC_RELAXED);
		if (at_exit)
			__atomic_store_n(
				map->status_word(GPU_MODEL_STATUS_EXIT_DELIVERY),
				1, __ATOMIC_RELAXED);
		__atomic_fetch_add(map->status_word(GPU_MODEL_STATUS_DELIVERIES),
				   1, __ATOMIC_RELEASE);
		if (delivered)
			(*delivered)++;
		const uint64_t m1 = now_ns();
		{
			std::lock_guard<std::mutex> g(s.reg_mutex);
			auto &sm = s.maps[mi];
			sm.last_ns = m1;
			sm.covered_syncs = syncs;
			sm.next_due_ns =
				m1 + (changed ? sm.busy_ns : sm.idle_ns);
			sm.deliveries++;
			sm.ns_total += m1 - m0;
			sm.ns_max = std::max(sm.ns_max, m1 - m0);
			next_due = std::min(next_due, sm.next_due_ns);
		}
	}
	if (!hashes.empty() && !s.hash_fn) {
		const uint64_t t = now_ns();
		if (!prepare_hash_locked(s, deadline_ns,
					 kind == delivery_kind::drain))
			hashes.clear();
		s.setup_ns += now_ns() - t;
	}
	for (size_t hi = 0; hi < hashes.size(); hi++) {
		auto &h = hashes[hi];
		if (h.dropped)
			continue;
		uint64_t due = h.next_due_ns;
		if (syncs != h.covered_syncs)
			due = std::min(due, kind == delivery_kind::at_sync ?
						    0 :
						    h.last_ns + h.sync_gap_ns);
		if (kind == delivery_kind::at_sync && syncs == h.covered_syncs)
			due = UINT64_MAX;
		if (!at_exit && due > t0) {
			next_due = std::min(next_due, due);
			continue;
		}
		const uint64_t m0 = now_ns();
		bool holds;
		{
			// it takes a process-local mutex and makes system
			// calls: no probe of this thread may run inside
			attach::attach_callback_suppression_scope quiet;
			holds = h.map->still_holds_claim();
		}
		if (!holds) {
			SPDLOG_ERROR(
				"GPU map delivery: this process no longer holds the device hash map's claim; it stops delivering it");
			std::lock_guard<std::mutex> g(s.reg_mutex);
			s.hashes[hi].dropped = true;
			continue;
		}
		CUresult r = launch_hash_snapshot(s.hash_fn, s.stream, h.block,
						  h.max_entries, h.shards,
						  h.staging_device);
		if (r == CUDA_SUCCESS)
			r = wait_stream(s, deadline_ns,
					kind == delivery_kind::drain);
		if (r == CUDA_ERROR_NOT_READY)
			return UINT64_MAX;
		if (r != CUDA_SUCCESS) {
			SPDLOG_ERROR("GPU map delivery: hash snapshot failed ({})",
				     (int)r);
			s.hash_broken = true;
			break;
		}
		uint64_t sum = 0;
		for (uint64_t i = 0; i < h.staging_bytes / 8; i++)
			sum = sum * 0x100000001b3ull +
			      ((const uint64_t *)h.staging)[i];
		h.map->apply_delivery(h.staging);
		auto *map = h.map;
		if (!at_exit)
			__atomic_store_n(
				map->status_word(DEVHASH_STATUS_SYNCS_COVERED),
				syncs, __ATOMIC_RELAXED);
		else
			__atomic_store_n(
				map->status_word(DEVHASH_STATUS_EXIT_DELIVERY),
				1, __ATOMIC_RELAXED);
		map->finish_delivery();
		if (h.companion)
			for (uint64_t w = 0; w < DEVHASH_STATUS_WORDS; w++)
				__atomic_store_n(
					h.companion + w,
					__atomic_load_n(map->status_word(w),
							__ATOMIC_RELAXED),
					__ATOMIC_RELAXED);
		if (delivered)
			(*delivered)++;
		const uint64_t m1 = now_ns();
		{
			std::lock_guard<std::mutex> g(s.reg_mutex);
			auto &sh = s.hashes[hi];
			const bool changed = sum != sh.last_sum;
			sh.last_sum = sum;
			sh.last_ns = m1;
			sh.covered_syncs = syncs;
			sh.next_due_ns =
				m1 + (changed ? sh.busy_ns : sh.idle_ns);
			sh.deliveries++;
			sh.ns_total += m1 - m0;
			sh.ns_max = std::max(sh.ns_max, m1 - m0);
			next_due = std::min(next_due, sh.next_due_ns);
		}
	}
	s.deliveries++;
	const uint64_t took = now_ns() - t0;
	s.snapshot_ns_total += took;
	if (s.deliveries <= 3 || at_exit || took > 20000000)
		SPDLOG_INFO(
			"GPU map delivery {}{}: {} us (launch {}, wait {}, copy {}), {} application syncs covered",
			s.deliveries, at_exit ? " (exit)" : "", took / 1000,
			t_launch / 1000, t_wait / 1000, t_copy / 1000, syncs);
	return next_due;
}

struct context_scope {
	CUcontext prev = nullptr;
	CUstreamCaptureMode mode = CU_STREAM_CAPTURE_MODE_RELAXED;
	bool ok = false;
	explicit context_scope(CUcontext ctx)
	{
		cuCtxGetCurrent(&prev);
		ok = cuCtxSetCurrent(ctx) == CUDA_SUCCESS;
		// this thread's calls must not invalidate another thread's
		// global-mode capture (step0b C5, claim 10)
		cuThreadExchangeStreamCaptureMode(&mode);
	}
	~context_scope()
	{
		cuThreadExchangeStreamCaptureMode(&mode);
		cuCtxSetCurrent(prev);
	}
};

void drain_loop()
{
	auto &s = state();
	s.drain_running.store(true, std::memory_order_release);
	{
		context_scope scope(s.ctx);
		uint64_t due = now_ns() + 20000000ull;
		// with deliveries in the synchronize calls the thread only
		// keeps the periodic ones
		while (!s.stop.load(std::memory_order_acquire)) {
			{
				const uint64_t now = now_ns();
				const uint64_t wait_ns =
					due > now ? std::min<uint64_t>(
							    due - now,
							    1000000000ull) :
						    0;
				std::unique_lock<std::mutex> lk(s.wake_mutex);
				s.wake.wait_for(
					lk, std::chrono::nanoseconds(wait_ns),
					[&] {
						return s.stop.load() ||
						       s.sync_pending.load();
					});
			}
			if (s.stop.load(std::memory_order_acquire))
				break;
			s.sync_pending.store(false, std::memory_order_release);
			std::unique_lock<std::timed_mutex> g(s.run_mutex);
			if (s.final_done || nothing_to_deliver(s))
				break;
			due = deliver_locked(s, delivery_kind::drain,
					     UINT64_MAX, nullptr);
		}
	}
	s.drain_running.store(false, std::memory_order_release);
	{
		std::lock_guard<std::mutex> lk(s.wake_mutex);
		s.wake.notify_all();
	}
}

// At exit (atexit): best effort only. CUDA calls after main are undefined
// by the programming guide ("using any of these interfaces ... during
// program initiation or termination after main will result in undefined
// behavior"), so the documented content of a map is that of the last
// in-process delivery (status SYNCS_COVERED); this one sets EXIT_DELIVERY.
// Bounded: at most about a second, and nothing at all in a fork child.
void final_delivery_at_exit()
{
	auto &s = state();
	if (!owned_here())
		return;
	const uint64_t syncs = attach::launch_activity().app_sync_count();
	const uint64_t after = attach::launch_activity().notes_since_last_sync();
	{
		std::lock_guard<std::mutex> g(s.reg_mutex);
		for (auto &m : s.maps) {
			__atomic_store_n(
				m.map->status_word(GPU_MODEL_STATUS_SYNCS_AT_EXIT),
				syncs, __ATOMIC_RELEASE);
			__atomic_store_n(
				m.map->status_word(
					GPU_MODEL_STATUS_LAUNCHES_AFTER_LAST_SYNC),
				after, __ATOMIC_RELEASE);
		}
		for (auto &h : s.hashes) {
			__atomic_store_n(
				h.map->status_word(DEVHASH_STATUS_SYNCS_AT_EXIT),
				syncs, __ATOMIC_RELEASE);
			__atomic_store_n(
				h.map->status_word(
					DEVHASH_STATUS_LAUNCHES_AFTER_LAST_SYNC),
				after, __ATOMIC_RELEASE);
		}
	}
	s.stop.store(true, std::memory_order_release);
	{
		std::unique_lock<std::mutex> lk(s.wake_mutex);
		s.wake.notify_all();
		s.wake.wait_for(lk, std::chrono::milliseconds(500), [&] {
			return !s.drain_running.load(std::memory_order_acquire);
		});
	}
	// never join: a thread stuck in the driver would hold the exit
	if (!s.drain_running.load(std::memory_order_acquire) &&
	    s.thread.joinable())
		s.thread.join();
	int n = 0;
	{
		std::unique_lock<std::timed_mutex> g(s.run_mutex,
						     std::defer_lock);
		if (s.ctx && g.try_lock_for(std::chrono::milliseconds(200))) {
			context_scope scope(s.ctx);
			if (!s.final_done && !nothing_to_deliver(s))
				deliver_locked(s, delivery_kind::exit,
					       now_ns() + 1000000000ull, &n);
			s.final_done = true;
		}
	}
	SPDLOG_INFO(
		"GPU map delivery: exit delivery of {} maps; {} deliveries in all, {} us of snapshots, {} us of set-up, {} waits given up, {} application syncs",
		n, s.deliveries, s.snapshot_ns_total / 1000, s.setup_ns / 1000,
		s.wait_timeouts, syncs);
	std::lock_guard<std::mutex> g(s.reg_mutex);
	for (auto &m : s.maps)
		SPDLOG_INFO(
			"GPU map delivery: map ({} words, {} shards): {} deliveries, {} us in all, longest {} us, syncs covered {}",
			m.entries * m.words_per_entry, m.shards, m.deliveries,
			m.ns_total / 1000, m.ns_max / 1000, m.covered_syncs);
	for (auto &h : s.hashes)
		SPDLOG_INFO(
			"GPU map delivery: device hash map ({} entries, {} shards): {} deliveries, {} us in all, longest {} us, syncs covered {}{}",
			h.max_entries, h.shards, h.deliveries, h.ns_total / 1000,
			h.ns_max / 1000, h.covered_syncs,
			h.dropped ? " (dropped: the claim was lost)" : "");
}
// In the application's synchronize call (opt-in): the delivery completes
// inside it, so the latest synchronization is covered before the call
// returns to the application (inside main).
int deliver_in_sync_call()
{
	auto &s = state();
	if (!s.ctx || !owned_here() || s.final_done || nothing_to_deliver(s))
		return 0;
	context_scope scope(s.ctx);
	std::unique_lock<std::timed_mutex> g(s.run_mutex);
	if (s.final_done)
		return 0;
	int n = 0;
	deliver_locked(s, delivery_kind::at_sync, now_ns() + 1000000000ull,
		       &n);
	return n;
}
} // namespace

namespace
{
// the delivery thread and the exit handler, at the first registration
void start_delivery(delivery_state &s)
{
	if (!s.ctx) {
		SPDLOG_ERROR(
			"GPU map delivery: no current context at registration");
		return;
	}
	attach::launch_activity().set_sync_listener(on_app_sync);
	atexit(final_delivery_at_exit);
	s.thread = std::thread(drain_loop);
}
} // namespace

namespace bpftime::gpu_model
{
bool register_device_hash(nv_gpu_device_hash_map_impl *map,
			  CUdeviceptr block, uint64_t max_entries,
			  uint64_t shards, uint64_t *companion)
{
	auto &s = state();
	if (fail_hash_registration_for_test.load())
		return false;
	registered_hash h{};
	h.map = map;
	h.block = block;
	h.max_entries = max_entries;
	h.shards = shards;
	h.companion = companion;
	// as a sum map: every 20 ms while it changes, else 200 ms
	h.busy_ns = 1000000ull * env_ms("BPFTIME_GPU_DELIVERY_BUSY_MS", 20);
	h.idle_ns = 1000000ull * env_ms("BPFTIME_GPU_DELIVERY_IDLE_MS", 200);
	h.sync_gap_ns =
		1000000ull * env_ms("BPFTIME_GPU_DELIVERY_SYNC_GAP_MS", 2);
	h.next_due_ns = now_ns() + h.busy_ns;
	h.last_ns = now_ns();
	h.staging_bytes = bpftime_devhash::staging_for(max_entries).total;
	if (cuMemAllocHost((void **)&h.staging, h.staging_bytes) !=
		    CUDA_SUCCESS ||
	    cuMemHostGetDevicePointer(&h.staging_device, h.staging, 0) !=
		    CUDA_SUCCESS) {
		SPDLOG_ERROR(
			"GPU map delivery: no staging buffer of {} bytes for a device hash map",
			h.staging_bytes);
		if (h.staging)
			cuMemFreeHost(h.staging);
		return false;
	}
	bool first = false;
	try {
		{
			std::lock_guard<std::mutex> g(s.reg_mutex);
			first = s.maps.empty() && s.hashes.empty();
			if (first) {
				s.owner = getpid();
				if (cuCtxGetCurrent(&s.ctx) != CUDA_SUCCESS)
					s.ctx = nullptr;
			}
			s.hashes.push_back(h);
		}
		if (first)
			start_delivery(s);
	} catch (...) {
		// no thread (or no room): the copy rolls back, so nothing may
		// keep delivering its block
		{
			std::lock_guard<std::mutex> g(s.reg_mutex);
			for (size_t i = s.hashes.size(); i-- > 0;)
				if (s.hashes[i].map == map &&
				    s.hashes[i].block == block) {
					s.hashes.erase(s.hashes.begin() + i);
					break;
				}
		}
		cuMemFreeHost(h.staging);
		throw;
	}
	return true;
}

namespace
{
// This process's device copies of 1533 maps (D11a): keyed by the object
// and its generation (a new map at an old object's address is another
// map). The mutex is held from the claim to the cache insert.
struct devhash_copy {
	const nv_gpu_device_hash_map_impl *map;
	uint64_t generation;
	CUdeviceptr block;
	uint64_t shards;
};
std::mutex devhash_copies_mutex;
std::vector<devhash_copy> devhash_copies;
std::atomic<bool> fail_devhash_copy_for_test{ false };
std::atomic<void (*)(nv_gpu_device_hash_map_impl *)> devhash_copy_hook{
	nullptr
};

// as 1531's: (SM slots, a power of two) x 32 lanes
uint64_t device_shards()
{
	CUdevice dev = 0;
	int sms = 0;
	if (cuCtxGetDevice(&dev) != CUDA_SUCCESS ||
	    cuDeviceGetAttribute(&sms, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT,
				 dev) != CUDA_SUCCESS ||
	    sms <= 0)
		sms = 1;
	uint64_t pow2 = 1;
	while (pow2 < (uint64_t)sms)
		pow2 <<= 1;
	return pow2 * 32;
}

// The block a device copy starts from: the image's headers in the
// directory, each live slot's value in shard 0 of its row (or its single
// word), the count, the counters' totals in shard 0.
std::vector<uint64_t> block_of(const bpftime_devhash::layout &l,
			       const devhash_image &img)
{
	std::vector<uint64_t> b(l.total / 8, 0);
	for (uint64_t s = 0; s < l.slots; s++) {
		const uint64_t h = img.headers[s];
		b[bpftime_devhash::slot_offset(l, s) / 8] = h;
		if (bpftime_devhash::state_of(h) == bpftime_devhash::ST_LIVE &&
		    bpftime_devhash::row_of(h) < l.max_entries)
			b[bpftime_devhash::value_offset(
				  l, bpftime_devhash::row_of(h), 0) /
			  8] = img.values[s];
	}
	// the geometry for anyone reading a dump (the device code takes it
	// as arguments)
	b[bpftime_devhash::ctl_offset(bpftime_devhash::CTL_MAGIC) / 8] =
		bpftime_devhash::MAGIC;
	b[bpftime_devhash::ctl_offset(bpftime_devhash::CTL_MAX_ENTRIES) / 8] =
		l.max_entries;
	b[bpftime_devhash::ctl_offset(bpftime_devhash::CTL_SLOTS) / 8] = l.slots;
	b[bpftime_devhash::ctl_offset(bpftime_devhash::CTL_R) / 8] = l.r;
	b[bpftime_devhash::ctl_offset(bpftime_devhash::CTL_SHARDS) / 8] =
		l.shards;
	b[bpftime_devhash::ctl_offset(bpftime_devhash::CTL_COUNT) / 8] =
		img.count;
	for (uint32_t c = 0; c < bpftime_devhash::NUM_COUNTERS; c++)
		b[bpftime_devhash::counter_offset(l, c, 0) / 8] = img.counters[c];
	return b;
}
} // namespace

CUdeviceptr find_device_hash_copy(const nv_gpu_device_hash_map_impl *map,
				  uint64_t *shards)
{
	std::lock_guard<std::mutex> g(devhash_copies_mutex);
	for (auto &c : devhash_copies)
		if (c.map == map && c.generation == map->get_generation()) {
			if (shards)
				*shards = c.shards;
			return c.block;
		}
	return 0;
}

CUdeviceptr make_device_hash_copy(nv_gpu_device_hash_map_impl *map,
				  uint64_t *companion)
{
	std::lock_guard<std::mutex> g(devhash_copies_mutex);
	for (auto &c : devhash_copies)
		if (c.map == map && c.generation == map->get_generation())
			return c.block;
	devhash_image img;
	devhash_path path = devhash_path::not_owner;
	try {
		// claim, then the seal and the CPU table's image, a dead
		// sealer's recovery, or the takeover's image (D11e); it gives
		// the claim back whenever it returns false
		if (!map->make_image(false, img, path))
			return 0;
	} catch (const std::exception &e) {
		SPDLOG_ERROR("device hash map: no image ({})", e.what());
		return 0;
	}
	const bool first = path != devhash_path::takeover;
	const uint64_t max = map->get_max_entries();
	CUdeviceptr block = 0;
	CUstream st = nullptr;
	bool mirror_seeded = false;
	// D15: any failure after the seal rolls back
	auto roll_back = [&](const char *why) -> CUdeviceptr {
		SPDLOG_ERROR("device hash map: no device copy ({}); rolled back",
			     why);
		if (block)
			cuMemFree(block);
		if (st)
			cuStreamDestroy(st);
		if (first) {
			if (mirror_seeded)
				map->reset_mirror();
			map->unseal(); // PHASE 0: CPU writes work again
		}
		// a takeover keeps PHASE 2 and the mirror
		map->release_claim();
		return 0;
	};
	uint64_t shards = 0;
	bpftime_devhash::layout l{};
	// Everything up to PHASE 2 may throw (allocations, a thread, a test's
	// hook): rolled back here, never out of the agent's init.
	try {
		if (auto hook = devhash_copy_hook.load())
			hook(map); // tests: after the claim and the seal
		shards = device_shards();
		if (!bpftime_devhash::geometry_valid(max, shards))
			return roll_back("geometry");
		l = bpftime_devhash::layout_for(max, shards);
		const std::vector<uint64_t> host = block_of(l, img);
		// on a stream of our own, waited for before the base is
		// published (relaxed capture mode: this may run on an
		// application thread)
		CUstreamCaptureMode mode = CU_STREAM_CAPTURE_MODE_RELAXED;
		cuThreadExchangeStreamCaptureMode(&mode);
		CUresult r = fail_devhash_copy_for_test.load() ?
				     CUDA_ERROR_OUT_OF_MEMORY :
				     cuMemAlloc(&block, l.total);
		if (r == CUDA_SUCCESS)
			r = cuStreamCreate(&st, CU_STREAM_NON_BLOCKING);
		if (r == CUDA_SUCCESS)
			r = cuMemcpyHtoDAsync(block, host.data(), l.total, st);
		if (r == CUDA_SUCCESS) {
			// the agent's own synchronization, not the
			// application's
			attach::probed_launch_activity::agent_sync_scope
				agent_call;
			r = cuStreamSynchronize(st);
		}
		cuThreadExchangeStreamCaptureMode(&mode);
		if (r != CUDA_SUCCESS)
			return roll_back("the copy failed");
		cuStreamDestroy(st);
		st = nullptr;
		// the cache's room before PHASE 2: its insert cannot throw
		devhash_copies.reserve(devhash_copies.size() + 1);
		if (first) {
			// the mirror before any delivery writes it
			map->seed_mirror(img);
			mirror_seeded = true;
		}
		if (!register_device_hash(map, block, max, shards, companion))
			return roll_back("registration failed");
	} catch (const std::exception &e) {
		return roll_back(e.what());
	} catch (...) {
		return roll_back("an exception");
	}
	if (first)
		map->publish_device_phase(); // PHASE 2
	devhash_copies.push_back({ map, map->get_generation(), block, shards });
	try {
		SPDLOG_INFO(
			"device hash map: device copy of {} entries ({} slots, {} shards, {} bytes) at {:x}, {}",
			max, l.slots, shards, l.total, (uint64_t)block,
			first ? "first copy" : "taken over");
	} catch (...) {
	}
	return block;
}

void set_devhash_copy_failure_for_test(bool fail)
{
	fail_devhash_copy_for_test.store(fail);
}

void set_devhash_copy_hook_for_test(void (*hook)(nv_gpu_device_hash_map_impl *))
{
	devhash_copy_hook.store(hook);
}

size_t registered_hash_count_for_test()
{
	auto &s = state();
	std::lock_guard<std::mutex> g(s.reg_mutex);
	return s.hashes.size();
}

void set_devhash_delivery_failures_for_test(bool module, bool registration)
{
	fail_hash_module_for_test.store(module);
	fail_hash_registration_for_test.store(registration);
}

void register_device_array(nv_gpu_device_array_map_impl *map,
			   CUdeviceptr device, uint64_t entries,
			   uint64_t shards, uint64_t words_per_entry)
{
	auto &s = state();
	registered_map m{ map, device, entries, shards, words_per_entry };
	// a sum map is small: every 20 ms while it changes, else 200 ms; a
	// per-thread map is the size of the grid: every 1 s, else 2 s
	const bool big = map->is_per_thread();
	m.busy_ns = 1000000ull *
		    env_ms(big ? "BPFTIME_GPU_DELIVERY_PERTHREAD_BUSY_MS" :
				 "BPFTIME_GPU_DELIVERY_BUSY_MS",
			   big ? 1000 : 20);
	m.idle_ns = 1000000ull *
		    env_ms(big ? "BPFTIME_GPU_DELIVERY_PERTHREAD_IDLE_MS" :
				 "BPFTIME_GPU_DELIVERY_IDLE_MS",
			   big ? 2000 : 200);
	// a synchronization not yet covered: a delivery at most this long
	// after the previous one
	m.sync_gap_ns = 1000000ull *
			env_ms("BPFTIME_GPU_DELIVERY_SYNC_GAP_MS", big ? 1000 : 2);
	m.next_due_ns = now_ns() + m.busy_ns;
	m.last_ns = now_ns();
	uint64_t bytes = entries * words_per_entry * 8;
	if (cuMemAllocHost((void **)&m.staging, bytes) != CUDA_SUCCESS ||
	    cuMemHostGetDevicePointer(&m.staging_device, m.staging, 0) !=
		    CUDA_SUCCESS) {
		SPDLOG_ERROR(
			"GPU map delivery: no staging buffer of {} bytes: the map is not delivered",
			bytes);
		return;
	}
	bool first;
	{
		std::lock_guard<std::mutex> g(s.reg_mutex);
		first = s.maps.empty() && s.hashes.empty();
		if (first) {
			s.owner = getpid();
			if (cuCtxGetCurrent(&s.ctx) != CUDA_SUCCESS)
				s.ctx = nullptr;
		}
		s.maps.push_back(m);
	}
	if (first)
		start_delivery(s);
}

int deliver_now(bool final)
{
	auto &s = state();
	if (!s.ctx || !owned_here())
		return 0;
	context_scope scope(s.ctx);
	std::unique_lock<std::timed_mutex> g(s.run_mutex);
	if (s.final_done)
		return 0;
	int n = 0;
	deliver_locked(s, final ? delivery_kind::exit : delivery_kind::drain,
		       now_ns() + 1000000000ull, &n);
	if (final)
		s.final_done = true;
	return n;
}

const char *hash_snapshot_ptx_for_test()
{
	return hash_snapshot_ptx;
}

bool hash_snapshot_for_test(uint64_t block, uint64_t max_entries,
			    uint64_t shards, uint64_t staging)
{
	CUmodule m = nullptr;
	CUfunction fn = nullptr;
	CUresult r = cuModuleLoadData(&m, hash_snapshot_ptx);
	if (r == CUDA_SUCCESS)
		r = cuModuleGetFunction(&fn, m, "bpftime_model_hash_snapshot");
	if (r == CUDA_SUCCESS)
		r = launch_hash_snapshot(fn, nullptr, block, max_entries,
					 shards, staging);
	if (r == CUDA_SUCCESS)
		r = cuCtxSynchronize();
	if (m != nullptr)
		cuModuleUnload(m);
	return r == CUDA_SUCCESS;
}
} // namespace bpftime::gpu_model

CUdeviceptr nv_gpu_device_array_map_impl::get_gpu_mem_buffer()
{
	if (shm_holder.global_shared_memory.get_open_type() !=
	    shm_open_type::SHM_OPEN_ONLY)
		return 0; // the server never runs GPU code
	// one device copy per map per process; the handler holds a reference
	// on the map for this process (bpf_map_handler::
	// get_gpu_map_extra_buffer), so the object is never freed under the
	// delivery and its address is never reused meanwhile
	static std::mutex mu;
	struct made_copy {
		nv_gpu_device_array_map_impl *map;
		CUdeviceptr device;
		uint64_t shards;
	};
	static std::vector<made_copy> made;
	std::lock_guard<std::mutex> g(mu);
	for (auto &mc : made)
		if (mc.map == this)
			return mc.device;
	// one delivering process per map: the first agent owns it; another
	// one's programs get NULL (its adds would never be delivered)
	{
		uint64_t mine = (uint64_t)getpid();
		uint64_t *owner = status_word(GPU_MODEL_STATUS_OWNER_PID);
		uint64_t cur = __atomic_load_n(owner, __ATOMIC_ACQUIRE);
		while (cur != mine) {
			if (cur != 0 && kill((pid_t)cur, 0) == 0) {
				SPDLOG_ERROR(
					"GPU device array map: delivered by process {}; this process ({}) gets no device copy (its lookups return NULL)",
					cur, mine);
				return 0;
			}
			if (__atomic_compare_exchange_n(owner, &cur, mine,
							false, __ATOMIC_ACQ_REL,
							__ATOMIC_ACQUIRE))
				break;
		}
	}
	uint64_t s = 1;
	if (!per_thread) {
		CUdevice dev = 0;
		int sms = 0;
		if (cuCtxGetDevice(&dev) != CUDA_SUCCESS ||
		    cuDeviceGetAttribute(
			    &sms, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT,
			    dev) != CUDA_SUCCESS ||
		    sms <= 0)
			sms = 1;
		uint64_t pow2 = 1;
		while (pow2 < (uint64_t)sms)
			pow2 <<= 1;
		s = pow2 * 32; // (SM & (pow2 - 1)) * 32 + lane
		shards = s;
	}
	uint64_t bytes = max_entries * s * thread_count * value_size;
	CUdeviceptr d = 0;
	CUstream zs = nullptr;
	// zeroed on a stream of our own, waited for before the address is
	// published: no later work can see the allocation's old contents
	// (relaxed capture mode: this may run on an application thread)
	CUstreamCaptureMode mode = CU_STREAM_CAPTURE_MODE_RELAXED;
	cuThreadExchangeStreamCaptureMode(&mode);
	CUresult r = cuMemAlloc(&d, bytes);
	if (r == CUDA_SUCCESS)
		r = cuStreamCreate(&zs, CU_STREAM_NON_BLOCKING);
	if (r == CUDA_SUCCESS)
		r = cuMemsetD8Async(d, 0, bytes, zs);
	if (r == CUDA_SUCCESS) {
		// the agent's own synchronization: not counted as the
		// application's (agent_sync_scope)
		attach::probed_launch_activity::agent_sync_scope agent_call;
		r = cuStreamSynchronize(zs);
	}
	if (zs)
		cuStreamDestroy(zs);
	cuThreadExchangeStreamCaptureMode(&mode);
	if (r != CUDA_SUCCESS) {
		SPDLOG_ERROR(
			"GPU device array map: cannot allocate and zero {} bytes ({})",
			bytes, (int)r);
		if (d)
			cuMemFree(d);
		return 0;
	}
	SPDLOG_INFO(
		"GPU device array map ({}): {} bytes of device memory at {:x}, {} shards",
		per_thread ? "1532" : "1531", bytes, (uintptr_t)d, s);
	made.push_back({ this, d, s });
	gpu_model::register_device_array(this, d, max_entries, s,
					 thread_count * value_size / 8);
	return d;
}
