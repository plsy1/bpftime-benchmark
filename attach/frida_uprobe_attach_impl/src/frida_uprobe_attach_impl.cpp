/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#include "frida_uprobe_attach_impl.hpp"
#include "frida-gum.h"
#include "frida_attach_private_data.hpp"
#include "frida_attach_utils.hpp"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <pthread.h>
#include <string>
#include <typeinfo>
#include <utility>
#include <unistd.h>
#include "frida_internal_attach_entry.hpp"
#include "frida_attach_entry.hpp"
#include <variant>
#include <vector>

using namespace bpftime::attach;

thread_local std::optional<void *>
	bpftime::attach::current_thread_gum_cpu_context;

#if defined(__aarch64__)
namespace
{
constexpr size_t MAX_USER_STACK_FRAMES = 127; // kernel perf_event_max_stack
constexpr uint64_t MAX_USER_ADDRESS = 1ULL << 52; // arm64 user VA limit

struct thread_stack_range {
	uintptr_t low = 0;
	uintptr_t high = 0;
};
thread_local thread_stack_range cached_stack_range;

// The current thread's stack mapping, cached per thread. False when sp is
// outside it (signal alternate stack, user-level stacks): no walk then.
bool current_stack_range(uintptr_t sp, thread_stack_range &range)
{
	auto &cached = cached_stack_range;
	if (cached.low <= sp && sp < cached.high) {
		range = cached;
		return true;
	}
	pthread_attr_t attr;
	if (pthread_getattr_np(pthread_self(), &attr) != 0)
		return false;
	void *addr = nullptr;
	size_t size = 0;
	const int err = pthread_attr_getstack(&attr, &addr, &size);
	pthread_attr_destroy(&attr);
	if (err != 0)
		return false;
	thread_stack_range fresh{ (uintptr_t)addr, (uintptr_t)addr + size };
	if (!(fresh.low <= sp && sp < fresh.high))
		return false;
	cached = fresh;
	range = fresh;
	return true;
}

// User stack the way arm64 perf_callchain_user (bpf_get_stackid) builds it:
// the pc first, then the return address of every frame record on the x29
// chain with its pointer authentication code stripped. Records are read only
// inside the current thread's stack and must move strictly upwards, so an x29
// used as a general register ends the walk instead of faulting. At function
// entry the immediate caller is not in the chain yet, as in the kernel.
std::vector<uint64_t> *walk_frame_pointers(const GumCpuContext &cpu,
					   uint64_t first_pc)
{
	auto result = new std::vector<uint64_t>;
	result->reserve(16);
	result->push_back(gum_strip_code_address(first_pc));
	thread_stack_range range;
	if (!current_stack_range(cpu.sp, range))
		return result;
	uintptr_t fp = cpu.fp;
	while (result->size() < MAX_USER_STACK_FRAMES && fp != 0 &&
	       (fp & 0x7) == 0 && fp >= range.low &&
	       fp + 2 * sizeof(uint64_t) <= range.high) {
		const auto *record = reinterpret_cast<const uint64_t *>(fp);
		const uint64_t ret = gum_strip_code_address(record[1]);
		// Past the outermost frame the chain can reach the initial
		// stack (argc, argv); such values are not code addresses.
		if (ret < 0x1000 || ret >= MAX_USER_ADDRESS)
			break;
		result->push_back(ret);
		const uintptr_t next = record[0];
		if (next <= fp)
			break;
		fp = next;
	}
	return result;
}
} // namespace
#endif

frida_attach_impl::~frida_attach_impl()
{
	gum_object_unref((GumInterceptor *)interceptor);
}

frida_attach_impl::frida_attach_impl(bool use_fuzzy_backtracer)
	: use_fuzzy_backtracer(use_fuzzy_backtracer)
{
	SPDLOG_DEBUG("Initializing frida attach manager");
	gum_init_embedded();
	interceptor = gum_interceptor_obtain();
}
int frida_attach_impl::attach_at_with_ebpf_callback(void *func_addr,
						    ebpf_callback_args &&cb)
{
	return attach_at(func_addr, cb);
}
int frida_attach_impl::attach_at(void *func_addr,
				 frida_attach_entry_callback &&cb)
{
	if (func_addr == nullptr) {
		SPDLOG_ERROR("Unable to attach uprobes to address 0");
		return -EINVAL;
	}
	auto itr = internal_attaches.find(func_addr);
	int current_attach_type;
	if (std::holds_alternative<callback_variant>(cb)) {
		current_attach_type = from_cb_idx_to_attach_type(
			std::get<callback_variant>(cb).index());

	} else {
		current_attach_type =
			std::get<ebpf_callback_args>(cb).attach_type;
	}
	if (itr == internal_attaches.end()) {
		// Create a frida attach entry
		itr = internal_attaches
			      .emplace(func_addr,
				       std::make_unique<
					       frida_internal_attach_entry>(
					       func_addr, current_attach_type,
					       (GumInterceptor *)interceptor))
			      .first;
		SPDLOG_DEBUG("Created frida attach entry for func addr {:x}",
			     (uintptr_t)func_addr);
	} else if (itr->second->has_override()) {
		SPDLOG_ERROR(
			"Function {} was already attached with replace or filter, cannot attach anything else");
		return -EEXIST;
	}

	auto &inner_attach = itr->second;
	inner_attach->ensure_listener(current_attach_type);
	int used_id = this->allocate_id();
	frida_attach_entry ent(used_id, std::move(cb), func_addr);
	int result = ent.self_id;
	auto inserted_attach_entry =
		this->attaches
			.emplace(ent.self_id,
				 std::make_unique<frida_attach_entry>(
					 std::move(ent)))
			.first;
	inner_attach->user_attaches.push_back(
		inserted_attach_entry->second.get());
	inserted_attach_entry->second->internal_attach = inner_attach.get();
	return result;
}

int frida_attach_impl::create_uprobe_at(void *func_addr, uprobe_callback &&cb)
{
	return attach_at(
		func_addr,
		callback_variant(std::in_place_index_t<ATTACH_UPROBE_INDEX>(),
				 cb));
}

int frida_attach_impl::create_uretprobe_at(void *func_addr,
					   uretprobe_callback &&cb)
{
	return attach_at(
		func_addr,
		callback_variant(
			std::in_place_index_t<ATTACH_URETPROBE_INDEX>(), cb));
}

int frida_attach_impl::create_uprobe_override_at(void *func_addr,
						 uprobe_override_callback &&cb)
{
	return attach_at(
		func_addr,
		callback_variant(
			std::in_place_index_t<ATTACH_UPROBE_OVERRIDE_INDEX>(),
			cb));
}

int frida_attach_impl::detach_by_id(int id)
{
	void *drop_func_addr = nullptr;
	if (auto itr = attaches.find(id); itr != attaches.end()) {
		auto p = itr->second->internal_attach;

		auto &user_attaches = p->user_attaches;
		auto tail =
			std::remove_if(user_attaches.begin(),
				       user_attaches.end(),
				       [&](const auto &v) -> bool {
					       return v == itr->second.get();
				       });
		user_attaches.resize(tail - user_attaches.begin());
		attaches.erase(itr);
		if (p->user_attaches.empty()) {
			drop_func_addr = p->function;
		}
	} else {
		SPDLOG_ERROR("Unable to find attach id {}", id);
		errno = -ENOENT;
	}
	if (drop_func_addr)
		internal_attaches.erase(drop_func_addr);
	return 0;
}
void frida_attach_impl::iterate_attaches(attach_iterate_callback cb)
{
	for (const auto &[k, v] : attaches) {
		cb(k, v->function, v->get_type());
	}
}

int frida_attach_impl::detach_by_func_addr(const void *func)
{
	if (auto itr = internal_attaches.find((void *)func);
	    itr != internal_attaches.end()) {
		auto uattaches = itr->second->user_attaches;
		for (auto attach_entry : uattaches) {
			attaches.erase(attach_entry->self_id);
		}
		internal_attaches.erase(itr);
		return 0;
	} else {
		return -ENOENT;
	}
}

extern "C" uint64_t bpftime_set_retval(uint64_t value);
int frida_attach_impl::create_attach_with_ebpf_callback(
	ebpf_run_callback &&cb, const attach_private_data &private_data,
	int attach_type)
{
	SPDLOG_DEBUG("Attaching with private_data type {}",
		     typeid(private_data).name());
	try {
		auto &sub = dynamic_cast<const frida_attach_private_data &>(
			private_data);
		SPDLOG_DEBUG(
			"Attaching with ebpf callback, private data offset={:x}, module name={}",
			sub.addr, sub.module_name);
		// Check if module path exists in the current process's map
		// Only check if the module_name is not empty. If it's empty, it
		// means we won't rely on module_name
		if (!sub.module_name.empty()) {
			bool ok = false;
			std::ifstream ifs("/proc/self/maps");
			std::string line;
			while (ifs) {
				std::getline(ifs, line);
				SPDLOG_DEBUG("Checking map line {}", line);
				char *module_path;
				if (sscanf(line.c_str(), "%*s%*s%*s%*s%*s%ms",
					   &module_path) == 1) {
					std::string curr_module(module_path);
					free(module_path);
					SPDLOG_DEBUG("Checking {}",
						     curr_module);
					if (std::filesystem::exists(
						    curr_module)) {
						bool matched = std::filesystem::
							equivalent(
								sub.module_name,
								curr_module);
						SPDLOG_DEBUG(
							"Checked {}, matched={}",
							curr_module, matched);
						if (matched) {
							ok = true;
							break;
						}
					} else {
						SPDLOG_DEBUG(
							"{} doesn't exist, skipped",
							curr_module);
					}
				}
			}
			if (!ok) {
				SPDLOG_INFO(
					"Unable to attach: module name {} doesn't exist in current process's memory maps",
					sub.module_name);
				return -EINVAL;
			}
		}
		ebpf_callback_args args{ .ebpf_cb = cb,
					 .attach_type = attach_type };
		if (attach_type == ATTACH_UPROBE ||
		    attach_type == ATTACH_URETPROBE ||
		    attach_type == ATTACH_UPROBE_OVERRIDE) {
			return attach_at_with_ebpf_callback(
				(void *)(uintptr_t)sub.addr, std::move(args));
		} else if (attach_type == ATTACH_UREPLACE) {
			return attach_at_with_ebpf_callback(
				(void *)(uintptr_t)sub.addr,
				ebpf_callback_args{
					.ebpf_cb = [=](void *memory,
						       size_t memory_size,
						       uint64_t *return_value)
						-> int {
						if (int err = cb(memory,
								 memory_size,
								 return_value);
						    err < 0) {
							SPDLOG_ERROR(
								"Failed to run ebpf callback at frida attach manager for attach type {}, err={}",
								attach_type,
								err);
							return err;
						} else {
							SPDLOG_DEBUG(
								"Override return value in ureplace: {}",
								*return_value);
							bpftime_set_retval(
								*return_value);
							return err;
						}
					},
					.attach_type =
						ATTACH_UPROBE_OVERRIDE });

		} else {
			SPDLOG_ERROR(
				"Unsupported attach type by frida attach manager: {}",
				attach_type);
			return -ENOTSUP;
		}
	} catch (const std::bad_cast &ex) {
		SPDLOG_ERROR(
			"Frida attach manager expected a private data of type frida_attach_private_data: {}",
			ex.what());
		return -EINVAL;
	}
}
static constexpr int BPF_FUNC_get_func_arg = 183;
static constexpr int BPF_FUNC_get_func_ret = 184;
static constexpr int BPF_FUNC_get_retval = 186;
void frida_attach_impl::register_custom_helpers(
	ebpf_helper_register_callback register_callback)
{
	register_callback(BPF_FUNC_get_func_arg, "bpf_get_func_arg",
			  (void *)bpftime_get_func_arg);
	register_callback(BPF_FUNC_get_func_ret, "bpf_get_func_ret_id",
			  (void *)bpftime_get_func_ret);
	register_callback(BPF_FUNC_get_retval, "bpf_get_retval",
			  (void *)bpftime_get_retval);
}

void *frida_attach_impl::call_attach_specific_function(const std::string& name,
						       void *data)
{
	SPDLOG_DEBUG("Calling frida attach impl specified feature: {}", name);
	if (name == "generate_stack") {
		GumCpuContext *cpu_context = nullptr;
		if (current_thread_gum_cpu_context.has_value()) {
			cpu_context =
				(GumCpuContext *)*current_thread_gum_cpu_context;
		} else if (auto *invocation =
				   gum_interceptor_get_current_invocation()) {
			// Uretprobe callbacks run in on_leave, which leaves the
			// thread-local unset to keep returns cheap.
			cpu_context = invocation->cpu_context;
		}
		if (cpu_context == nullptr) {
			SPDLOG_ERROR("There is no frida hook running");
			return nullptr;
		}
		SPDLOG_DEBUG("Getting stack trace...");
#if defined(__aarch64__)
		if (!use_fuzzy_backtracer) {
			// The accurate gum backtracer re-reads ELF unwind data
			// on every call (memleak: ~50 us per allocation) and
			// stops at PAC-signed return addresses.
			uint64_t first_pc = cpu_context->pc;
			auto *invocation = gum_interceptor_get_current_invocation();
			if (invocation != nullptr &&
			    gum_invocation_context_get_point_cut(invocation) ==
				    GUM_POINT_LEAVE) {
				// At return the kernel's pc is the return address.
				first_pc = (uint64_t)(uintptr_t)
					gum_invocation_context_get_return_address(
						invocation);
			}
			return walk_frame_pointers(*cpu_context, first_pc);
		}
#endif

		struct {
			guint len;
			GumReturnAddress items[127];
		} array;
		array.len = 0;
		auto tracer = use_fuzzy_backtracer ?
				      gum_backtracer_make_fuzzy() :
				      gum_backtracer_make_accurate();
		gum_backtracer_generate(tracer, cpu_context,
					(GumReturnAddressArray *)&array);
		if (array.len == 0) {
			SPDLOG_ERROR("Unable to get stack trace");
			g_object_unref(tracer);
			return nullptr;
		}
		auto result = new std::vector<uint64_t>;
		for (guint i = 0; i < array.len; i++) {
			auto frame_addr = array.items[i];
			result->push_back((uint64_t)(uintptr_t)frame_addr);
		}
		SPDLOG_DEBUG("Got {} stack traces", result->size());
		g_object_unref(tracer);
		return result;

	} else {
		SPDLOG_ERROR("Invalid frida attach impl feature: {}", name);
		return nullptr;
	}
}
