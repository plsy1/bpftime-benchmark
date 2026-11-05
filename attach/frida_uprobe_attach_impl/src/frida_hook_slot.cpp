#include "frida_hook_slot.hpp"
#include "frida_internal_attach_entry.hpp"
#include "frida_probe_grace.hpp"
#include "frida_uprobe_attach_impl.hpp"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <cstring>
#include <mutex>
#include <unordered_map>

using namespace bpftime::attach;

// Gum's readers of a leading jump (`b`, PLT-style stubs), in libfrida-gum
// but not declared by the devkit header.
extern "C" gpointer
gum_arm64_reader_try_get_relative_jump_target(gconstpointer address);
extern "C" gpointer
gum_x86_reader_try_get_relative_jump_target(gconstpointer address);

namespace
{
// Guards the slots, their Gum calls and their code patches.
std::mutex slots_mutex;
// Never destroyed: Gum's listeners point at the slots until the process ends.
std::unordered_map<void *, frida_hook_slot *> &slots()
{
	static auto *map = new std::unordered_map<void *, frida_hook_slot *>();
	return *map;
}

#if defined(__aarch64__)
constexpr uint32_t LDR_X16_LITERAL_AT_8 = 0x58000050;
constexpr uint32_t BR_X16 = 0xd61f0200;
constexpr uint32_t B_MASK = 0xfc000000;
constexpr uint32_t B_OPCODE = 0x14000000;
// Gum's on-invoke code follows the on-enter and on-leave code in the slice
// it allocates for the function.
constexpr uint64_t SLICE_SPAN = 1024;

bool is_b(uint32_t insn)
{
	return (insn & B_MASK) == B_OPCODE;
}
uint64_t b_target(const void *at, uint32_t insn)
{
	int64_t imm = (int32_t)(insn << 6) >> 6;
	return (uint64_t)(uintptr_t)at + imm * 4;
}
#endif

// The address Gum instruments for `function`, as gum_interceptor_resolve
// finds it: a function that starts with a jump (a `b` thunk, a PLT-style
// stub) is hooked at the jump's target, unless the function is hooked itself.
// Slots are keyed by it: a thunk's probes are listeners on its target's Gum
// context, and parking a separate target slot silenced them (review
// 2026-11-05).
void *resolve_locked(void *function)
{
	for (int hops = 0; hops < 8 && !slots().count(function); hops++) {
#if defined(__aarch64__)
		gpointer target =
			gum_arm64_reader_try_get_relative_jump_target(function);
#elif defined(__x86_64__)
		gpointer target =
			gum_x86_reader_try_get_relative_jump_target(function);
#else
		gpointer target = nullptr;
#endif
		if (target == nullptr)
			break;
		function = target;
	}
	return function;
}

bool looks_redirected(void *function)
{
#if defined(__aarch64__)
	const auto *code = (const uint32_t *)function;
	return (code[0] == LDR_X16_LITERAL_AT_8 && code[1] == BR_X16) ||
	       is_b(code[0]);
#else
	(void)function;
	return true;
#endif
}

// Hands out the on-invoke code only: the redirect is written when the
// transaction that installs and reverts it ends, so no thread can get here.
__attribute__((noinline)) void unreachable_replacement()
{
	__builtin_trap();
}

// Parking rewrites one aligned word that a running thread reads whole: the
// redirect's literal (data loaded by its `ldr`), or its single `b`, which the
// architecture allows to be replaced by another `b` while other cores may
// execute it. Gum writes the literal right after the two instructions,
// aligned or not; unaligned, only its low word may change.
void find_parking(frida_hook_slot &s, void *original)
{
#if defined(__aarch64__)
	if (original == nullptr || !gum_query_is_rwx_supported())
		return;
	const auto *code = (const uint32_t *)s.function;
	const uint64_t invoke = (uint64_t)(uintptr_t)original;
	if (code[0] == LDR_X16_LITERAL_AT_8 && code[1] == BR_X16) {
		uint64_t enter;
		memcpy(&enter, code + 2, sizeof(enter));
		if (invoke <= enter || invoke - enter >= SLICE_SPAN)
			return;
		s.patch_address = (void *)(code + 2);
		s.enter_value = enter;
		s.invoke_value = invoke;
		if (((uintptr_t)s.patch_address & 7) == 0)
			s.parking = frida_hook_slot::literal64;
		else if ((enter >> 32) == (invoke >> 32))
			s.parking = frida_hook_slot::literal_low32;
		return;
	}
	if (is_b(code[0])) {
		const uint64_t enter = b_target(code, code[0]);
		const int64_t delta =
			(int64_t)invoke - (int64_t)(uintptr_t)s.function;
		if (invoke <= enter || invoke - enter >= SLICE_SPAN ||
		    delta < -(int64_t(1) << 27) || delta >= (int64_t(1) << 27))
			return;
		s.patch_address = (void *)code;
		s.enter_value = code[0];
		s.invoke_value = B_OPCODE | ((uint64_t)(delta >> 2) & ~B_MASK);
		s.parking = frida_hook_slot::branch;
	}
	// Gum's 8-byte form (adrp, br) and deflectors cannot be retargeted in
	// one write: such functions keep Gum's path while nothing is attached.
#else
	(void)s;
	(void)original;
#endif
}

struct patch_request {
	frida_hook_slot::parking_kind kind;
	uint64_t value;
};
void apply_patch(gpointer mem, gpointer data)
{
	auto *request = (const patch_request *)data;
	if (request->kind == frida_hook_slot::literal64)
		std::atomic_ref<uint64_t>(*(uint64_t *)mem)
			.store(request->value, std::memory_order_relaxed);
	else
		std::atomic_ref<uint32_t>(*(uint32_t *)mem)
			.store((uint32_t)request->value,
			       std::memory_order_relaxed);
}

void set_parked(frida_hook_slot &s, bool park)
{
	if (s.parking == frida_hook_slot::cannot_park || s.parked == park)
		return;
	patch_request request{ s.parking,
			       park ? s.invoke_value : s.enter_value };
	const size_t size = s.parking == frida_hook_slot::literal64 ? 8 : 4;
	if (gum_memory_patch_code(s.patch_address, size, apply_patch,
				  &request))
		s.parked = park;
	else
		SPDLOG_WARN("Unable to {} the hook at {:x}",
			    park ? "park" : "unpark", (uintptr_t)s.function);
}

int create_slot(void *function, frida_hook_slot **out)
{
	ensure_gum_thread_context();
	auto *s = new frida_hook_slot;
	s->function = function;
	s->enter_listener =
		gum_make_probe_listener(frida_slot_on_enter, s, nullptr);
	const bool fresh = !looks_redirected(function);
	GumInterceptor *interceptor = gum_interceptor_obtain();
	gum_interceptor_begin_transaction(interceptor);
	int err = gum_interceptor_attach(interceptor, function,
					 s->enter_listener, nullptr);
	gpointer original = nullptr;
	if (err == GUM_ATTACH_OK && fresh) {
		if (gum_interceptor_replace(
			    interceptor, function,
			    (gpointer)unreachable_replacement, nullptr,
			    &original) == GUM_REPLACE_OK)
			gum_interceptor_revert(interceptor, function);
		else
			original = nullptr;
	}
	gum_interceptor_end_transaction(interceptor);
	g_object_unref(interceptor);
	if (err != GUM_ATTACH_OK) {
		g_object_unref(s->enter_listener);
		delete s;
		return err;
	}
	find_parking(*s, original);
	SPDLOG_DEBUG("Hooked {:x}, parking kind {}", (uintptr_t)function,
		     (int)s->parking);
	slots().emplace(function, s);
	*out = s;
	return 0;
}

int ensure_locked(frida_hook_slot &s, int attach_type)
{
	if (attach_type == ATTACH_URETPROBE && s.leave_listener == nullptr) {
		ensure_gum_thread_context();
		auto *listener = gum_make_call_listener(
			nullptr, frida_slot_on_leave, &s, nullptr);
		GumInterceptor *interceptor = gum_interceptor_obtain();
		int err = gum_interceptor_attach(interceptor, s.function,
						 listener, nullptr);
		g_object_unref(interceptor);
		if (err != GUM_ATTACH_OK) {
			g_object_unref(listener);
			return err;
		}
		s.leave_listener = listener;
	} else if (attach_type == ATTACH_UPROBE_OVERRIDE && !s.replaced) {
		ensure_gum_thread_context();
		GumInterceptor *interceptor = gum_interceptor_obtain();
		int err = gum_interceptor_replace(
			interceptor, s.function,
			(gpointer)__bpftime_frida_attach_manager__override_handler,
			&s, nullptr);
		g_object_unref(interceptor);
		if (err != GUM_REPLACE_OK)
			return err;
		s.replaced = true;
	}
	return 0;
}

void republish_locked(frida_hook_slot &s)
{
	auto *next = new frida_hook_dispatch;
	for (auto *owner : s.owners)
		owner->collect(*next);
	const bool empty = next->uprobes.empty() &&
			   next->uretprobes.empty() &&
			   next->override_attach == nullptr;
	if (empty) {
		delete next;
		next = nullptr;
		// New calls bypass Gum before the empty dispatch is seen.
		set_parked(s, true);
	}
	const frida_hook_dispatch *old =
		s.dispatch.exchange(next, std::memory_order_acq_rel);
	if (old != nullptr) {
		probe_grace::synchronize();
		delete old;
	}
	if (!empty)
		set_parked(s, false);
}
} // namespace

int bpftime::attach::frida_hook_add_owner(void *function, int attach_type,
					  frida_internal_attach_entry *owner,
					  frida_hook_slot **slot)
{
	std::lock_guard guard(slots_mutex);
	function = resolve_locked(function);
	frida_hook_slot *s;
	if (auto itr = slots().find(function); itr != slots().end()) {
		s = itr->second;
	} else if (int err = create_slot(function, &s); err < 0) {
		return err;
	}
	if (int err = ensure_locked(*s, attach_type); err < 0) {
		if (s->owners.empty())
			set_parked(*s, true);
		return err;
	}
	s->owners.push_back(owner);
	*slot = s;
	return 0;
}

int bpftime::attach::frida_hook_ensure(frida_hook_slot *slot, int attach_type)
{
	std::lock_guard guard(slots_mutex);
	return ensure_locked(*slot, attach_type);
}

void bpftime::attach::frida_hook_republish(frida_hook_slot *slot)
{
	std::lock_guard guard(slots_mutex);
	republish_locked(*slot);
}

void bpftime::attach::frida_hook_remove_owner(frida_hook_slot *slot,
					      frida_internal_attach_entry *owner)
{
	std::lock_guard guard(slots_mutex);
	auto &owners = slot->owners;
	owners.erase(std::remove(owners.begin(), owners.end(), owner),
		     owners.end());
	republish_locked(*slot);
}

bool bpftime::attach::frida_hook_is_parked(void *function)
{
	std::lock_guard guard(slots_mutex);
	auto itr = slots().find(resolve_locked(function));
	return itr != slots().end() && itr->second->parked;
}
