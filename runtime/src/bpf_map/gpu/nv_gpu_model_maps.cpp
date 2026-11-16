#include "nv_gpu_model_maps.hpp"
#include "bpftime_shm.hpp"
#include "bpftime_shm_internal.hpp"
#include "spdlog/spdlog.h"
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
	status_buffer.resize(GPU_MODEL_STATUS_WORDS * 8 +
			     this->thread_count * value_size);
	std::memset(status_buffer.data(), 0, status_buffer.size());
	SPDLOG_INFO(
		"GPU device array map ({}): value_size={}, max_entries={}, threads per key={}, host copy {} bytes",
		per_thread ? "1532 per-thread" : "1531 sum", value_size,
		max_entries, this->thread_count, data_buffer.size());
}

void *nv_gpu_device_array_map_impl::elem_lookup(const void *key)
{
	auto k = *(const uint32_t *)key;
	if (k >= max_entries && k < max_entries + GPU_MODEL_STATUS_WORDS) {
		// status word k - max_entries, then zeros up to a value
		std::atomic_thread_fence(std::memory_order_acquire);
		return status_buffer.data() + 8 * (k - max_entries);
	}
	if (k >= max_entries) {
		errno = ENOENT;
		return nullptr;
	}
	// pairs with the delivery's release increment of the status
	(void)__atomic_load_n(&status_words()[GPU_MODEL_STATUS_DELIVERIES],
			      __ATOMIC_ACQUIRE);
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
// ld.relaxed.gpu (generic addressing, as the programs' atom.add.u64).
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
	ld.relaxed.gpu.u64 %rd15, [%rd14];
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

struct registered_map {
	nv_gpu_device_array_map_impl *map;
	CUdeviceptr device;
	uint64_t entries, shards, words_per_entry;
	uint64_t *staging = nullptr; // pinned host memory, snapshot output
	CUdeviceptr staging_device = 0;
};

struct delivery_state {
	std::mutex reg_mutex; // the list
	std::mutex run_mutex; // one delivery at a time
	std::vector<registered_map> maps;
	CUcontext ctx = nullptr;
	CUmodule module = nullptr;
	CUfunction fn = nullptr;
	CUstream stream = nullptr;
	bool broken = false;
	std::thread thread;
	std::mutex wake_mutex;
	std::condition_variable wake;
	bool stop = false;
	bool final_done = false;
	uint64_t deliveries = 0;
	uint64_t snapshot_ns_total = 0;
	uint64_t setup_ns = 0;
};

delivery_state &state()
{
	// never destroyed: the exit handler and the thread may outlive
	// static destructors
	static auto *s = new delivery_state();
	return *s;
}

uint64_t now_ns()
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(
		       std::chrono::steady_clock::now().time_since_epoch())
		.count();
}

// with run_mutex held, the context current and relaxed capture mode
bool prepare_locked(delivery_state &s)
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
			r = cuStreamSynchronize(s.stream);
	}
	if (r != CUDA_SUCCESS) {
		SPDLOG_ERROR("GPU map delivery: cannot set up ({})", (int)r);
		s.broken = true;
		return false;
	}
	return true;
}

// one delivery of every map; returns whether any word changed
bool deliver_locked(delivery_state &s, bool final, int *delivered)
{
	std::vector<registered_map> maps;
	{
		std::lock_guard<std::mutex> g(s.reg_mutex);
		maps = s.maps;
	}
	if (maps.empty())
		return false;
	if (!s.fn) {
		const uint64_t t = now_ns();
		if (!prepare_locked(s))
			return false;
		s.setup_ns = now_ns() - t;
	}
	bool changed = false;
	const uint64_t t0 = now_ns();
	uint64_t t_launch = 0, t_sync = 0, t_copy = 0;
	for (auto &m : maps) {
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
			r = cuStreamSynchronize(s.stream);
		t_launch += b - a;
		t_sync += now_ns() - b;
		if (r != CUDA_SUCCESS) {
			SPDLOG_ERROR("GPU map delivery: snapshot failed ({})",
				     (int)r);
			s.broken = true;
			return changed;
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
		uint64_t *st = m.map->status_words();
		__atomic_store_n(&st[GPU_MODEL_STATUS_PID], (uint64_t)getpid(),
				 __ATOMIC_RELAXED);
		__atomic_fetch_add(&st[GPU_MODEL_STATUS_DELIVERIES], 1,
				   __ATOMIC_RELEASE);
		if (final)
			__atomic_store_n(&st[GPU_MODEL_STATUS_FINAL], 1,
					 __ATOMIC_RELEASE);
		if (delivered)
			(*delivered)++;
	}
	s.deliveries++;
	const uint64_t took = now_ns() - t0;
	s.snapshot_ns_total += took;
	if (s.deliveries <= 3 || final || took > 20000000)
		SPDLOG_INFO(
			"GPU map delivery {}{}: {} us (launch {}, wait {}, copy {})",
			s.deliveries, final ? " (final)" : "", took / 1000,
			t_launch / 1000, t_sync / 1000, t_copy / 1000);
	return changed;
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

uint64_t env_ms(const char *name, uint64_t dflt)
{
	const char *v = getenv(name);
	if (!v || !*v)
		return dflt;
	return strtoull(v, nullptr, 10);
}

void drain_loop()
{
	auto &s = state();
	context_scope scope(s.ctx);
	// while values change: every BUSY ms; otherwise every IDLE ms (graph
	// replays and launches the hooks miss are found within that)
	const uint64_t busy_ms = env_ms("BPFTIME_GPU_DELIVERY_BUSY_MS", 20);
	const uint64_t idle_ms = env_ms("BPFTIME_GPU_DELIVERY_IDLE_MS", 200);
	uint64_t wait_ms = busy_ms;
	for (;;) {
		{
			std::unique_lock<std::mutex> lk(s.wake_mutex);
			s.wake.wait_for(lk, std::chrono::milliseconds(wait_ms),
					[&] { return s.stop; });
			if (s.stop)
				return;
		}
		bool changed;
		{
			std::lock_guard<std::mutex> g(s.run_mutex);
			if (s.final_done)
				return;
			changed = deliver_locked(s, false, nullptr);
		}
		wait_ms = changed ? busy_ms : idle_ms;
	}
}

void final_delivery_at_exit()
{
	auto &s = state();
	{
		std::lock_guard<std::mutex> lk(s.wake_mutex);
		s.stop = true;
	}
	s.wake.notify_all();
	if (s.thread.joinable())
		s.thread.join();
	int n = gpu_model::deliver_now(true);
	SPDLOG_INFO(
		"GPU map delivery: final delivery of {} maps; {} deliveries in all, {} us of snapshots, {} us of set-up",
		n, s.deliveries, s.snapshot_ns_total / 1000, s.setup_ns / 1000);
}
} // namespace

namespace bpftime::gpu_model
{
void register_device_array(nv_gpu_device_array_map_impl *map,
			   CUdeviceptr device, uint64_t entries,
			   uint64_t shards, uint64_t words_per_entry)
{
	auto &s = state();
	registered_map m{ map, device, entries, shards, words_per_entry };
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
		first = s.maps.empty();
		if (first && cuCtxGetCurrent(&s.ctx) != CUDA_SUCCESS)
			s.ctx = nullptr;
		s.maps.push_back(m);
	}
	if (first) {
		if (!s.ctx) {
			SPDLOG_ERROR(
				"GPU map delivery: no current context at registration");
			return;
		}
		// after the application's cudart registered its own exit
		// handler? Not guaranteed; the final delivery uses the driver
		// API and the agent's retained primary context only.
		atexit(final_delivery_at_exit);
		s.thread = std::thread(drain_loop);
	}
}

int deliver_now(bool final)
{
	auto &s = state();
	if (!s.ctx)
		return 0;
	context_scope scope(s.ctx);
	std::lock_guard<std::mutex> g(s.run_mutex);
	if (s.final_done)
		return 0;
	int n = 0;
	deliver_locked(s, final, &n);
	if (final)
		s.final_done = true;
	return n;
}
} // namespace bpftime::gpu_model

CUdeviceptr nv_gpu_device_array_map_impl::get_gpu_mem_buffer()
{
	if (shm_holder.global_shared_memory.get_open_type() !=
	    shm_open_type::SHM_OPEN_ONLY)
		return 0; // the server never runs GPU code
	// one device copy per map per process
	static std::mutex mu;
	static std::vector<std::pair<nv_gpu_device_array_map_impl *,
				     CUdeviceptr> > made;
	std::lock_guard<std::mutex> g(mu);
	for (auto &[m, d] : made)
		if (m == this)
			return d;
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
	if (cuMemAlloc(&d, bytes) != CUDA_SUCCESS ||
	    cuMemsetD8(d, 0, bytes) != CUDA_SUCCESS) {
		SPDLOG_ERROR("GPU device array map: cannot allocate {} bytes",
			     bytes);
		if (d)
			cuMemFree(d);
		return 0;
	}
	SPDLOG_INFO(
		"GPU device array map ({}): {} bytes of device memory at {:x}, {} shards",
		per_thread ? "1532" : "1531", bytes, (uintptr_t)d, s);
	made.emplace_back(this, d);
	gpu_model::register_device_array(this, d, max_entries, s,
					 thread_count * value_size / 8);
	return d;
}
