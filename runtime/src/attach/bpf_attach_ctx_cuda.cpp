#include "bpftime_shm_internal.hpp"
#include "cuda_runtime_api.h"
#include "driver_types.h"
#include "nv_attach_impl.hpp"
#include "nv_attach_utils.hpp"
#include "probed_launch_activity.hpp"
#include <sys/auxv.h>
#include "bpf_map/array_map_binding.hpp"
#include <bpf_attach_ctx.hpp>
#include "bpftime_ratelimited_log.hpp"
#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <system_error>
#include <thread>
#include <spdlog/spdlog.h>
#include "cuda.h"

extern "C" {
extern uint64_t bpftime_trace_printk(uint64_t fmt, uint64_t fmt_size, ...);
}

#define CUDART_SAFE_CALL(x, error)                                             \
	do {                                                                   \
		if (auto result = x; result != cudaSuccess) {                  \
			SPDLOG_ERROR("error: {} failed with error code {}",    \
				     #x, (int)result);                         \
			throw std::runtime_error(error);                       \
		}                                                              \
	} while (0)
#define NV_SAFE_CALL(x, error_message)                                         \
	do {                                                                   \
		CUresult result = x;                                           \
		if (result != CUDA_SUCCESS) {                                  \
			SPDLOG_ERROR("error: {} failed with error code {}",    \
				     #x, (int)result);                         \
			throw std::runtime_error(error_message);               \
		}                                                              \
	} while (0)

#define NV_SAFE_CALL_NO_THROW(x, error_message)                                \
	do {                                                                   \
		CUresult result = x;                                           \
		if (result != CUDA_SUCCESS) {                                  \
			SPDLOG_ERROR("error: {} failed with error code {}",    \
				     #x, (int)result);                         \
		}                                                              \
	} while (0)

#define NV_SAFE_CALL_2(x, error_message)                                       \
	do {                                                                   \
		CUresult result = x;                                           \
		if (result != CUDA_SUCCESS) {                                  \
			SPDLOG_ERROR(                                          \
				"error: {} failed with error code {}: {}", #x, \
				(int)result, error_message);                   \
			return -1;                                             \
		}                                                              \
	} while (0)

#define NV_SAFE_CALL_3(x, error_message)                                       \
	do {                                                                   \
		CUresult result = x;                                           \
		if (result != CUDA_SUCCESS) {                                  \
			SPDLOG_ERROR(                                          \
				"error: {} failed with error code {}: {}", #x, \
				(int)result, error_message);                   \
			return {};                                             \
		}                                                              \
	} while (0)

namespace bpftime
{
namespace
{
std::once_flag g_cuda_watcher_atexit_once;
std::mutex g_cuda_watcher_mutex;
std::thread *g_cuda_watcher_thread = nullptr;
std::shared_ptr<std::atomic<bool>> g_cuda_watcher_stop_flag;

void stop_cuda_watcher_thread_at_exit()
{
	std::thread *thread_to_join = nullptr;
	std::shared_ptr<std::atomic<bool>> flag_to_set;
	{
		std::lock_guard<std::mutex> guard(g_cuda_watcher_mutex);
		thread_to_join = g_cuda_watcher_thread;
		flag_to_set = g_cuda_watcher_stop_flag;
		g_cuda_watcher_thread = nullptr;
		g_cuda_watcher_stop_flag.reset();
	}
	if (flag_to_set)
		flag_to_set->store(true, std::memory_order_release);
	if (thread_to_join != nullptr && thread_to_join->joinable())
		thread_to_join->join();
}
} // namespace

void stop_cuda_watcher_before_shm_unmap()
{
	stop_cuda_watcher_thread_at_exit();
}

std::optional<attach::nv_attach_impl *>
bpf_attach_ctx::find_nv_attach_impl() const
{
	for (const auto &entry : this->attach_impl_holders) {
		if (auto p =
			    dynamic_cast<attach::nv_attach_impl *>(entry.get());
		    p)
			return p;
	}
	return {};
}
namespace
{
// Serves the request the GPU posted in ctx's CommSharedMem: reads the
// request, writes the response. The caller signals completion.
void serve_cuda_request(cuda::CUDAContext *ctx)
{
	auto *comm = ctx->cuda_shared_mem;
	const auto req_id = comm->request_id;
	const auto map_fd = comm->map_id;
	SPDLOG_DEBUG("CUDA Received call request id {}, map_fd = {}", req_id,
		     map_fd);
	if (req_id == (int)cuda::HelperOperation::MAP_LOOKUP) {
		const auto &req = comm->req.map_lookup;
		auto &resp = comm->resp.map_lookup;
		auto ptr = bpftime_map_lookup_elem(map_fd, req.key);
		resp.value = nullptr;
		if (ptr) {
			resp.value = cuda::gpu_address_of_map_value(ptr);
			if (resp.value == nullptr)
				BPFTIME_LOG_WARN_RATELIMITED(
					"CUDA: map {} lookup found a value at {:p}, which the GPU cannot reach; the GPU thread gets NULL",
					map_fd, ptr);
		}
		SPDLOG_DEBUG(
			"CUDA: Executing map lookup for {}, key= {:x} result = {:x}",
			map_fd, *(uintptr_t *)(uintptr_t)&req.key,
			(uintptr_t)resp.value);
	} else if (req_id == (int)cuda::HelperOperation::MAP_UPDATE) {
		const auto &req = comm->req.map_update;
		auto &resp = comm->resp.map_update;
		resp.result = bpftime_map_update_elem(map_fd, req.key,
						      req.value, req.flags);
		SPDLOG_DEBUG("CUDA: Executing map update for {}, result = {}",
			     map_fd, resp.result);
	} else if (req_id == (int)cuda::HelperOperation::MAP_DELETE) {
		const auto &req = comm->req.map_delete;
		auto &resp = comm->resp.map_delete;
		resp.result = bpftime_map_delete_elem(map_fd, req.key);
		SPDLOG_DEBUG("CUDA: Executing map delete for {}, result = {}",
			     map_fd, resp.result);
	} else if (req_id == (int)cuda::HelperOperation::TRACE_PRINTK) {
		const auto &req = comm->req.trace_printk;
		auto &resp = comm->resp.trace_printk;
		resp.result = bpftime_trace_printk((uintptr_t)req.fmt,
						   req.fmt_size, req.arg1,
						   req.arg2, req.arg3);
		SPDLOG_DEBUG(
			"CUDA: Executing bpf_printk: {}, arg1={}, arg2={}, arg3={}",
			req.fmt, req.arg1, req.arg2, req.arg3);
	} else if (req_id ==
		   (int)cuda::HelperOperation::GET_CURRENT_PID_TGID) {
		// The value modules get written (attach::trampoline_pid_tgid):
		// the tgid in both halves. The low half used to be this watcher
		// thread's tid, nothing the calling GPU thread has to do with.
		auto &resp = comm->resp.get_tid_pgid;
		resp.result = attach::trampoline_pid_tgid();
		SPDLOG_DEBUG("Called get_current_pid_tgid: {:x}", resp.result);
	} else if (req_id == (int)cuda::HelperOperation::PUTS) {
		const auto &req = comm->req.puts;
		auto &resp = comm->resp.puts;
		SPDLOG_INFO("eBPF: {}", req.data);
		resp.result = 0;
	} else {
		SPDLOG_WARN("Unknown request id {}", req_id);
	}
}

// After serving a request the watcher keeps polling this long before it
// sleeps: the next GPU thread posts its request only after it has read this
// response, a few microseconds later.
constexpr auto cuda_watcher_busy_window = std::chrono::microseconds(200);
// Idle, the watcher sleeps for this long, doubling up to the maximum.
constexpr auto cuda_watcher_min_sleep = std::chrono::microseconds(50);
constexpr auto cuda_watcher_max_sleep = std::chrono::microseconds(1000);

inline void cpu_relax()
{
#if defined(__aarch64__)
	asm volatile("yield" ::: "memory");
#elif defined(__x86_64__) || defined(__i386__)
	__builtin_ia32_pause();
#endif
}

// While a probed kernel may run, the watcher waits for a request with WFE: an
// exclusive load marks flag1's line, and the GPU's write to it ends the wait
// (results gpu/wfe-20261111: about 2 us at any gap, at the power of a sleeping
// core; a sleeping watcher answered 0-1 ms late, gpu/hostwake-20261111). The
// architected timer's event stream ends every wait within about 100 us too, so
// the watcher still sees its stop flag and a kernel's end; without it, or off
// aarch64, the watcher keeps sleeping between requests.
// BPFTIME_CUDA_WATCHER_WFE=0 keeps the old policy (sleeping between requests
// whatever runs): a watcher in WFE counts as running for the scheduler and
// holds its core for as long as any probed kernel runs, with or without host
// calls.
bool cuda_watcher_can_wait_on_flag()
{
#if defined(__aarch64__)
	if (const char *v = getenv("BPFTIME_CUDA_WATCHER_WFE");
	    v != nullptr && strcmp(v, "0") == 0)
		return false;
	return (getauxval(AT_HWCAP) & HWCAP_EVTSTRM) != 0;
#else
	return false;
#endif
}

// Linux arm64's __cmpwait: returns after an event once *p may differ from old.
inline void wait_while_equal(volatile int *p, int old)
{
#if defined(__aarch64__)
	uint32_t tmp;
	asm volatile("sevl\n"
		     "wfe\n"
		     "ldxr %w[tmp], %[v]\n"
		     "eor %w[tmp], %w[tmp], %w[old]\n"
		     "cbnz %w[tmp], 1f\n"
		     "wfe\n"
		     "1:"
		     : [tmp] "=&r"(tmp), [v] "+Q"(*(volatile uint32_t *)p)
		     : [old] "r"((uint32_t)old)
		     : "memory");
#else
	(void)p;
	(void)old;
	cpu_relax();
#endif
}

// How often a watcher in WFE checks whether the probed kernels have ended.
constexpr auto cuda_watcher_kernel_check = std::chrono::microseconds(100);
} // namespace

void bpf_attach_ctx::start_cuda_watcher_thread()
{
	if (!cuda_ctx)
		return;
	if (cuda_watcher_thread.joinable())
		return;
	auto flag = cuda_ctx->cuda_watcher_should_stop;
	cuda_watcher_thread = std::thread([flag, this]() {
		auto *ctx = cuda_ctx.get();
		// The device's primary context (the agent retained it, cudart
		// uses it): map lookups ask the driver for their values' device
		// addresses, which needs a current context.
		CUdevice device = 0;
		CUcontext primary = nullptr;
		if (cuDeviceGet(&device, cuda_device_ordinal()) ==
			    CUDA_SUCCESS &&
		    cuDevicePrimaryCtxRetain(&primary, device) == CUDA_SUCCESS)
			cuCtxSetCurrent(primary);
		else
			SPDLOG_WARN(
				"CUDA watcher: no context for device {}; GPU map lookups through host calls return NULL",
				cuda_device_ordinal());
		using clock = std::chrono::steady_clock;
		auto last_request = clock::now() - cuda_watcher_busy_window;
		auto idle_sleep = std::chrono::duration_cast<
			std::chrono::microseconds>(cuda_watcher_min_sleep);
		auto &activity = attach::launch_activity();
		const bool wfe = cuda_watcher_can_wait_on_flag();
		// The watcher queries events (activity.any_running()) while
		// the application may be capturing a graph in another thread:
		// in the default (global) mode that makes such calls of other
		// threads invalidate the capture. Relaxed for this thread only.
		CUstreamCaptureMode capture_mode = CU_STREAM_CAPTURE_MODE_RELAXED;
		cuThreadExchangeStreamCaptureMode(&capture_mode);
		// launches of probed code seen so far; a newer one makes the
		// watcher wait with WFE until every noted launch has ended
		uint64_t seen_launches = activity.generation();
		bool on_flag = false;
		auto last_kernel_check = clock::now();
		auto *waiting_on_flag = ctx->cuda_watcher_waiting_on_flag.get();

		while (!flag->load(std::memory_order_acquire)) {
			if (ctx != nullptr) {
				auto *comm = ctx->cuda_shared_mem;
				int posted = 1;
				// flag1 1 -> 0 takes the request; acquire: the
				// GPU wrote it before flag1 (membar.sys)
				if (__atomic_load_n(&comm->flag1,
						    __ATOMIC_RELAXED) == 1 &&
				    __atomic_compare_exchange_n(
					    &comm->flag1, &posted, 0, false,
					    __ATOMIC_ACQUIRE,
					    __ATOMIC_RELAXED)) {
					serve_cuda_request(ctx);
					// release: the response before flag2
					__atomic_store_n(&comm->flag2, 1,
							 __ATOMIC_RELEASE);
					last_request = clock::now();
					idle_sleep = cuda_watcher_min_sleep;
					continue;
				}
			}
			if (wfe && !on_flag &&
			    activity.generation() != seen_launches) {
				on_flag = true;
				last_kernel_check = clock::now();
				waiting_on_flag->store(true,
						       std::memory_order_release);
				SPDLOG_DEBUG(
					"CUDA watcher: a probed kernel was launched; waiting on the request flag");
			}
			if (on_flag) {
				if (ctx != nullptr)
					wait_while_equal(&ctx->cuda_shared_mem
								  ->flag1,
							 0);
				const auto now = clock::now();
				if (now - last_kernel_check >=
				    cuda_watcher_kernel_check) {
					last_kernel_check = now;
					// generation first: a launch noted
					// after it brings the watcher back
					const uint64_t g = activity.generation();
					if (!activity.any_running()) {
						seen_launches = g;
						on_flag = false;
						last_request = now;
						idle_sleep = cuda_watcher_min_sleep;
						waiting_on_flag->store(
							false,
							std::memory_order_release);
						SPDLOG_DEBUG(
							"CUDA watcher: the probed kernels have ended; sleeping between requests");
					}
				}
				continue;
			}
			if (clock::now() - last_request <
			    cuda_watcher_busy_window) {
				cpu_relax();
				continue;
			}
			// A launch of probed code ends the sleep at once. Launches
			// the hooks do not see or skip (graph launches, launches
			// into a stream being captured, CUDA 13 kernel handles they
			// cannot resolve) are still served after the sleep. Without WFE a noted launch
			// only ends the current sleep: the watcher then polls for
			// the busy window and sleeps again.
			if (!wfe && activity.generation() != seen_launches) {
				seen_launches = activity.generation();
				last_request = clock::now();
				idle_sleep = cuda_watcher_min_sleep;
				continue;
			}
			activity.wait_for_note(seen_launches, idle_sleep);
			idle_sleep = std::min<std::chrono::microseconds>(
				idle_sleep * 2, cuda_watcher_max_sleep);
		}
		if (primary != nullptr) {
			cuCtxSetCurrent(nullptr);
			cuDevicePrimaryCtxRelease(device);
		}
		SPDLOG_INFO("Exiting CUDA watcher thread");
	});

	std::call_once(g_cuda_watcher_atexit_once, []() {
		std::atexit(stop_cuda_watcher_thread_at_exit);
	});
	{
		std::lock_guard<std::mutex> guard(g_cuda_watcher_mutex);
		g_cuda_watcher_thread = &cuda_watcher_thread;
		g_cuda_watcher_stop_flag = flag;
	}
}

void bpf_attach_ctx::stop_cuda_watcher_thread()
{
	if (!cuda_ctx)
		return;
	auto flag = cuda_ctx->cuda_watcher_should_stop;
	if (flag)
		flag->store(true, std::memory_order_release);
	{
		std::lock_guard<std::mutex> guard(g_cuda_watcher_mutex);
		if (g_cuda_watcher_thread == &cuda_watcher_thread) {
			g_cuda_watcher_thread = nullptr;
			g_cuda_watcher_stop_flag.reset();
		}
	}
	if (cuda_watcher_thread.joinable()) {
		try {
			cuda_watcher_thread.join();
		} catch (const std::system_error &ex) {
			SPDLOG_WARN("Failed to join CUDA watcher thread: {}",
				    ex.what());
			try {
				if (cuda_watcher_thread.joinable())
					cuda_watcher_thread.detach();
			} catch (const std::system_error &detach_ex) {
				SPDLOG_WARN(
					"Failed to detach CUDA watcher thread after join failure: {}",
					detach_ex.what());
			}
		}
	}
}
// An ordinary array map's values lie in the segment, which this process
// registered with CUDA: their device address lets the trampoline use them
// without host calls. The map is pinned (an array_map_binding, as the CPU
// JIT's inlined lookups pin theirs) for as long as this context lives:
// patched modules keep the address after the loader closes the map or a new
// session starts (they cannot be unloaded, and a loader that exits without
// a detach leaves their probes running), and without the pin the GPU would
// write into storage the segment had given to something else. NULL when the
// map cannot be pinned or the GPU cannot reach it: host calls, as before.
// Counting builds keep the host calls, so every operation is counted (the
// CPU JIT's array inlining is off there too).
void *bpf_attach_ctx::gpu_address_of_array_values(int fd)
{
#if BPFTIME_ENABLE_MAP_USAGE_STATS
	(void)fd;
	return nullptr;
#else
	auto memory = shm_holder.global_shared_memory.retain_mapping();
	const auto *manager = shm_holder.global_shared_memory.get_manager();
	if (!memory || !manager ||
	    !memory->find<boost::interprocess::interprocess_recursive_mutex>(
			    "bpftime_map_binding_mutex_v1")
		     .first)
		return nullptr; // a segment without the binding protocol
	auto binding = array_map_binding::acquire(memory, manager, fd, false);
	if (!binding)
		return nullptr;
	void *device = cuda::gpu_address_of_map_value(binding->raw_data());
	if (device == nullptr)
		return nullptr;
	// one pin per array storage, however many links ask
	gpu_array_bindings.emplace(binding->raw_data(), std::move(binding));
	return device;
#endif
}

std::vector<attach::MapBasicInfo>
bpf_attach_ctx::create_map_basic_info(int filled_size)
{
	std::vector<attach::MapBasicInfo> local_basic_info(filled_size);
	for (auto &entry : local_basic_info) {
		entry.enabled = false;
		entry.key_size = 0;
		entry.value_size = 0;
		entry.max_entries = 0;
		entry.map_type = 0;
		entry.extra_buffer = nullptr;
		entry.max_thread_count = 0;
	}
	const auto &handler_manager =
		*shm_holder.global_shared_memory.get_manager();
	for (size_t i = 0; i < handler_manager.size(); i++) {
		const auto &current_handler = handler_manager.get_handler(i);
		if (std::holds_alternative<bpf_map_handler>(current_handler)) {
			auto &local = local_basic_info[i];
			const auto &map =
				std::get<bpf_map_handler>(current_handler);
			auto gpu_buffer = map.get_gpu_map_extra_buffer();
			if (map.type == bpf_map_type::BPF_MAP_TYPE_ARRAY)
				gpu_buffer = gpu_address_of_array_values((int)i);
			SPDLOG_INFO(
				"Copying map fd {} to device, key size={}, value size={}, max ent={}, map_type = {}, gpu_buffer = {:x}",
				i, map.get_key_size(), map.get_value_size(),
				map.get_max_entries(), (int)map.type,
				(uintptr_t)gpu_buffer);

			if (i >= local_basic_info.size()) {
				SPDLOG_ERROR(
					"Too large map fd: {}, max to be {}", i,
					local_basic_info.size());
				return {};
			}

			local.enabled = true;
			local.key_size = map.get_key_size();
			local.value_size = map.get_value_size();
			local.max_entries = map.get_max_entries();
			local.map_type = (int)map.type;
			local.extra_buffer = gpu_buffer;
			local.max_thread_count =
				map.get_gpu_map_max_thread_count();
		}
	}

	return local_basic_info;
}

namespace cuda
{

void *gpu_address_of_map_value(const void *value)
{
	CUmemorytype type = CU_MEMORYTYPE_HOST;
	if (cuPointerGetAttribute(&type, CU_POINTER_ATTRIBUTE_MEMORY_TYPE,
				  (CUdeviceptr)value) == CUDA_SUCCESS &&
	    (type == CU_MEMORYTYPE_DEVICE || type == CU_MEMORYTYPE_UNIFIED))
		return const_cast<void *>(value);
	// Host memory: its address in a registered range's device mapping.
	// The offset from CommSharedMem used here before gave an illegal
	// address for anything outside the registered segment.
	CUdeviceptr device = 0;
	if (cuMemHostGetDevicePointer(&device, const_cast<void *>(value), 0) ==
	    CUDA_SUCCESS)
		return reinterpret_cast<void *>(device);
	return nullptr;
}

void cuda_context_destroyer(CUcontext ptr)
{
	NV_SAFE_CALL(cuCtxDestroy(ptr), "destroy cuda context");
}
void cuda_module_destroyer(CUmodule ptr)
{
	NV_SAFE_CALL(cuModuleUnload(ptr), "Unload CUDA module");
}

std::optional<std::unique_ptr<cuda::CUDAContext>> create_cuda_context()
{
	SPDLOG_INFO("Initializing CUDA shared memory");
	auto *cuda_shared_mem =
		shm_holder.global_shared_memory.get_cuda_comm_shared_mem();
	if (!cuda_shared_mem) {
		SPDLOG_ERROR(
			"CUDA shared communication memory not initialized in shared segment");
		return std::nullopt;
	}
	// The GPU reaches CommSharedMem through the segment's registration
	// (register_cuda_host_memory, in agents). Without a device address (no
	// usable GPU: cuInit failed; a process that did not register the
	// segment) this process has no CUDA attach, and only that: the attach
	// context must still come up for the other probes. It used to throw
	// here, out of bpf_attach_ctx's constructor, and the agent disabled
	// itself (CPU probes included). Driver API: a runtime call would
	// initialize the application's shared cudart before the probes are
	// known (see register_cuda_host_memory).
	CUdeviceptr device_addr = 0;
	if (auto err = cuMemHostGetDevicePointer(
		    &device_addr, (void *)cuda_shared_mem, 0);
	    err != CUDA_SUCCESS) {
		SPDLOG_WARN(
			"CommSharedMem has no device address (cuMemHostGetDevicePointer: {})",
			(int)err);
		return std::nullopt;
	}
	// Only now: the request slot is shared with the processes that can
	// use it.
	memset(cuda_shared_mem, 0, sizeof(*cuda_shared_mem));

	auto cuda_ctx = std::make_optional(std::make_unique<cuda::CUDAContext>(
		cuda_shared_mem, static_cast<uintptr_t>(device_addr)));

	SPDLOG_INFO("CUDA context created");
	return cuda_ctx;
}
CUDAContext::~CUDAContext()
{
	SPDLOG_INFO("Destructing CUDAContext");
}
CUDAContext::CUDAContext(cuda::CommSharedMem *mem, uintptr_t device_pointer)
	: cuda_shared_mem(mem), cuda_shared_mem_device_pointer(device_pointer)

{
	set_cuda_shared_mem_device_pointer(cuda_shared_mem_device_pointer);
	SPDLOG_INFO("CommSharedMem host {:p} mapped to device {:p}",
		    (void *)cuda_shared_mem,
		    reinterpret_cast<void *>(cuda_shared_mem_device_pointer));
}

} // namespace cuda
} // namespace bpftime
