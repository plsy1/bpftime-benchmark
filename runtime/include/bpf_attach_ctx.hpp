#ifndef _BPF_ATTACH_CTX
#define _BPF_ATTACH_CTX

#include "attach_private_data.hpp"
#include "base_attach_impl.hpp"
#include "bpftime_config.hpp"
#include "bpftime_helper_group.hpp"
#include "handler/link_handler.hpp"
#include "handler/perf_event_handler.hpp"
#include "handler/prog_handler.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <mutex>
#include <string_view>
#include <utility>
#include <vector>

#ifdef BPFTIME_ENABLE_CUDA_ATTACH
#include "cuda.h"
#include "nv_attach_impl.hpp"
#include "trampoline/gpu_helper_protocol.h"
#endif
typedef struct _GumInterceptor GumInterceptor;
typedef struct _GumInvocationListener GumInvocationListener;

namespace bpftime
{

#ifdef BPFTIME_ENABLE_CUDA_ATTACH
namespace cuda
{
// The memory the GPU trampoline shares with the host:
// attach/nv_attach_impl/trampoline/gpu_helper_protocol.h.
using bpftime_gpu::CommSharedMem;
using bpftime_gpu::GPU_HELPER_MAX_BUF;
using bpftime_gpu::HelperCallRequest;
using bpftime_gpu::HelperCallResponse;
using bpftime_gpu::HelperOperation;

struct CUDAContext {
	// Indicate whether cuda watcher thread should stop
	std::shared_ptr<std::atomic<bool>> cuda_watcher_should_stop =
		std::make_shared<std::atomic<bool>>(false);

	// What the host-call watcher does, for tests and diagnostics
	struct watcher_state {
		// waits on the request flag (WFE, with BPFTIME_CUDA_WATCHER_WFE=1)
		// because a noted probed launch may run
		std::atomic<bool> waiting_on_flag{ false };
		// requests from launches no hook noted keep it from parking
		// (cuda_watcher::unseen_launch_latch)
		std::atomic<bool> unseen_latched{ false };
		// parks (no probed kernel may run), sleeps of slow_sleep_us (one
		// may), requests caught by the poll window
		std::atomic<uint64_t> parks{ 0 }, sleeps{ 0 }, window_catches{ 0 };
	};
	std::shared_ptr<watcher_state> cuda_watcher =
		std::make_shared<watcher_state>();

	// Shared memory region for CUDA
	cuda::CommSharedMem *cuda_shared_mem;
	// Mapped device pointer
	uintptr_t cuda_shared_mem_device_pointer;

	// device_pointer: mem's address for the GPU (create_cuda_context
	// obtains it first and makes no context without one).
	CUDAContext(cuda::CommSharedMem *mem, uintptr_t device_pointer);

	CUDAContext(CUDAContext &&) = default;
	CUDAContext &operator=(CUDAContext &&) = default;
	CUDAContext(const CUDAContext &) = delete;
	CUDAContext &operator=(const CUDAContext &) = delete;

	virtual ~CUDAContext();
};

// Empty when this process has no CUDA attach: CommSharedMem is missing from
// the segment or has no device address (no usable GPU, or the segment was not
// registered with CUDA). The attach context then runs without it.
std::optional<std::unique_ptr<cuda::CUDAContext>> create_cuda_context();

// The address a GPU thread uses for a map value the host found: device and
// managed memory as they are, host memory through the device mapping of the
// registered range it lies in; nullptr when the GPU cannot reach it.
void *gpu_address_of_map_value(const void *value);

inline std::atomic<uintptr_t> &cuda_shared_mem_device_pointer_storage() noexcept
{
	static std::atomic<uintptr_t> value{ 0 };
	return value;
}

inline void set_cuda_shared_mem_device_pointer(uintptr_t ptr) noexcept
{
	cuda_shared_mem_device_pointer_storage().store(ptr,
						      std::memory_order_release);
}

inline uintptr_t get_cuda_shared_mem_device_pointer() noexcept
{
	return cuda_shared_mem_device_pointer_storage().load(
		std::memory_order_acquire);
}

} // namespace cuda
#endif

class base_attach_manager;

class handler_manager;
class array_map_binding;
class bpftime_prog;

using syscall_hooker_func_t = int64_t (*)(int64_t sys_nr, int64_t arg1,
					  int64_t arg2, int64_t arg3,
					  int64_t arg4, int64_t arg5,
					  int64_t arg6);

class bpf_attach_ctx {
public:
	bpf_attach_ctx();
	~bpf_attach_ctx();

	// create bpf_attach_ctx from handler_manager in shared memory
	int init_attach_ctx_from_handlers(const handler_manager *manager,
					  const runtime_config &config);
	// create bpf_attach_ctx from handler_manager in global_shared_memory
	int init_attach_ctx_from_handlers(const runtime_config &config);
	// Register an attach implementation. Attach manager will take its
	// ownership. The third argument is a function that initializes a
	// corresponding attach private data with the given string.
	void register_attach_impl(
		std::initializer_list<int> &&attach_types,
		std::unique_ptr<attach::base_attach_impl> &&impl,
		std::function<std::unique_ptr<attach::attach_private_data>(
			const std::string_view &, int &)>
			private_data_creator);
	// Destroy a specific attach link
	int destroy_instantiated_attach_link(int link_id);
	// Destroy all instantiated attach links
	int destroy_all_attach_links();
	// Clear all instantiated state (programs/perf events/link bookkeeping).
	// Attach implementations stay registered, so the context can be reused
	// for a new tracing session without destroying the whole object.
	void reset_instantiated_state();
	std::optional<attach::base_attach_impl *>
	get_attach_impl_by_attach_type(int attach_type)
	{
		if (auto itr = attach_impls.find(attach_type);
		    itr != attach_impls.end()) {
			return itr->second.first;
		} else {
			return {};
		}
	}
	bpftime_prog *find_instantiated_prog(int handler_id) const;

#ifdef BPFTIME_ENABLE_CUDA_ATTACH
	std::optional<attach::nv_attach_impl *> find_nv_attach_impl() const;
	// The map table the trampoline reads (map_info), one entry per fd below
	// filled_size: GPU maps' device buffers, ordinary arrays' device
	// address of their values.
	std::vector<attach::MapBasicInfo>
	create_map_basic_info(int filled_size);
	// Whether the host-call watcher now waits on the request flag (WFE
	// mode, a probed kernel may run) rather than sleeping between requests.
	bool cuda_watcher_waits_on_flag() const
	{
		return cuda_ctx && cuda_ctx->cuda_watcher->waiting_on_flag.load(
					   std::memory_order_acquire);
	}
	// Whether requests from unnoted launches keep the watcher from parking.
	bool cuda_watcher_unseen_latched() const
	{
		return cuda_ctx && cuda_ctx->cuda_watcher->unseen_latched.load(
					   std::memory_order_acquire);
	}
	// How often the watcher parked, slept while a probed kernel could run,
	// and caught a request in its poll window, so far.
	uint64_t cuda_watcher_parks() const
	{
		return cuda_ctx ? cuda_ctx->cuda_watcher->parks.load(
					  std::memory_order_relaxed) :
				  0;
	}
	uint64_t cuda_watcher_sleeps() const
	{
		return cuda_ctx ? cuda_ctx->cuda_watcher->sleeps.load(
					  std::memory_order_relaxed) :
				  0;
	}
	uint64_t cuda_watcher_window_catches() const
	{
		return cuda_ctx ? cuda_ctx->cuda_watcher->window_catches.load(
					  std::memory_order_relaxed) :
				  0;
	}
#endif

private:
#ifdef BPFTIME_ENABLE_CUDA_ATTACH
	void *gpu_address_of_array_values(int fd);
	// Array maps whose values patched modules address directly, pinned
	// for this context's life (by value storage).
	std::map<void *, std::shared_ptr<array_map_binding>> gpu_array_bindings;
#endif
	mutable std::mutex ctx_mutex;
	// Stable shm epoch_seq last seen (even). Used to detect session switch and
	// rebind GPU resources/maps accordingly.
	std::uint64_t last_epoch_seq_seen = 0;
	int destroy_instantiated_attach_link_unlocked(int link_id);
	int destroy_all_attach_links_unlocked();
	void reset_instantiated_state_unlocked();

#ifdef BPFTIME_ENABLE_CUDA_ATTACH
	// Start host thread for handling map requests from CUDA
	void start_cuda_watcher_thread();
	void stop_cuda_watcher_thread();
	std::unique_ptr<cuda::CUDAContext> cuda_ctx;
	std::thread cuda_watcher_thread;
#endif

	constexpr static int CURRENT_ID_OFFSET = 65536;
	volatile int current_id = CURRENT_ID_OFFSET;

	// Helpers provided by attach impls
	std::map<int, bpftime_helper_info> helpers;

	// handler_id -> instantiated programs
	std::map<int, std::unique_ptr<bpftime_prog>> instantiated_progs;
	// handler_id -> (instantiated attaches id, attach_impl*)
	std::map<int, std::pair<int, attach::base_attach_impl *>>
		instantiated_attach_links;
	// handler_id -> instantiated attach private data & attach type
	std::map<int,
		 std::pair<std::unique_ptr<attach::attach_private_data>, int>>
		instantiated_perf_events;
	// attach_type -> attach impl
	std::map<int, std::pair<attach::base_attach_impl *,
				std::function<std::unique_ptr<
					attach::attach_private_data>(
					const std::string_view &, int &)>>>
		attach_impls;
	// Holds the ownership of all attach impls
	std::vector<std::unique_ptr<attach::base_attach_impl>>
		attach_impl_holders;
	// Record which handlers were already instantiated
	std::set<int> instantiated_handlers;

	int instantiate_handler_at(const handler_manager *manager, int id,
				   std::set<int> &stk,
				   const runtime_config &config,
				   bool handle_nv_attach_impl);
	int instantiate_prog_handler_at(const handler_manager *manager, int id,
					const bpf_prog_handler &handler,
					const runtime_config &config);
	int instantiate_bpf_link_handler_at(int id,
					    const bpf_link_handler &handler,
					    bool handle_nv_attach_impl);
	int instantiate_perf_event_handler_at(
		int id, const bpf_perf_event_handler &perf_handler);

};

int load_prog_and_helpers(
	bpftime_prog *prog, const runtime_config &config,
	const bpftime_helper_info *map_lookup_override = nullptr,
	const bpftime_helper_info *map_update_override = nullptr,
	const std::map<int, bpftime_helper_info> *custom_helpers = nullptr,
	const bpftime_helper_info *map_delete_override = nullptr);

// load_prog_and_helpers without the load: registers the configured helper
// groups, the overrides and the custom helpers.
int register_prog_helpers(
	bpftime_prog *prog, const runtime_config &config,
	const bpftime_helper_info *map_lookup_override = nullptr,
	const bpftime_helper_info *map_update_override = nullptr,
	const std::map<int, bpftime_helper_info> *custom_helpers = nullptr,
	const bpftime_helper_info *map_delete_override = nullptr);

// Registers the helpers a traced process gives the program of insns: the
// configured groups, the typed map helpers chosen from the maps the
// instructions name (shared memory only), and custom_helpers. The syscall
// server registers the same set to compile the AOT object agents load, so
// both must go through this function.
int register_prog_handler_helpers(
	const handler_manager *manager, const ebpf_inst *insns, size_t insn_cnt,
	bpftime_prog *prog, const runtime_config &config,
	const std::map<int, bpftime_helper_info> *custom_helpers);

} // namespace bpftime

#endif
