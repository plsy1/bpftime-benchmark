#ifndef _BPFTIME_FRIDA_HOOK_SLOT_HPP
#define _BPFTIME_FRIDA_HOOK_SLOT_HPP
#include <atomic>
#include <cstdint>
#include <frida-gum.h>
#include <vector>

namespace bpftime
{
namespace attach
{
class frida_attach_entry;
class frida_internal_attach_entry;

// What a slot's listeners and replacement run. Replaced as a whole, and the
// old one freed after a grace period (frida_probe_grace.hpp).
struct frida_hook_dispatch {
	std::vector<const frida_attach_entry *> uprobes;
	std::vector<const frida_attach_entry *> uretprobes;
	frida_internal_attach_entry *override_owner = nullptr;
};

// One per function bpftime hooks, for the life of the process. Gum is never
// asked to detach or revert while the process runs: Gum 16.1.2 frees a
// detached listener's entry while other threads iterate it, and restores a
// function's first instructions over its 16-byte redirect while other
// threads may be executing them (a thread that fetched the redirect's
// `ldr x16` then loads the restored instructions as its jump target). The
// listeners (and a replacement, once an override came) stay; bpftime swaps
// `dispatch`. With nothing attached the slot is parked: the redirect's
// target (its literal, or a `b`) points at Gum's copy of the displaced first
// instructions, so calls bypass Gum; both targets are valid at any moment.
struct frida_hook_slot {
	void *function = nullptr;
	GumInvocationListener *enter_listener = nullptr;
	GumInvocationListener *leave_listener = nullptr;
	bool replaced = false;
	std::vector<frida_internal_attach_entry *> owners;
	std::atomic<const frida_hook_dispatch *> dispatch{ nullptr };

	enum parking_kind { cannot_park, literal64, literal_low32, branch };
	parking_kind parking = cannot_park;
	void *patch_address = nullptr;
	uint64_t enter_value = 0;
	uint64_t invoke_value = 0;
	bool parked = false;
};

// The slot of `function` hooked for `attach_type`, made on first use, with
// `owner` added. Returns a negative Gum attach or replace error, the slot
// unchanged.
int frida_hook_add_owner(void *function, int attach_type,
			 frida_internal_attach_entry *owner,
			 frida_hook_slot **slot);
// Hooks the slot for one more attach type (a uretprobe needs the leave
// listener, an override the replacement).
int frida_hook_ensure(frida_hook_slot *slot, int attach_type);
// Rebuilds the dispatch from the owners' attaches. When it returns, no
// callback can still run what the owners no longer hold.
void frida_hook_republish(frida_hook_slot *slot);
// Removes `owner` and republishes.
void frida_hook_remove_owner(frida_hook_slot *slot,
			     frida_internal_attach_entry *owner);
// Tests: whether calls to `function` bypass Gum.
bool frida_hook_is_parked(void *function);

void frida_slot_on_enter(GumInvocationContext *ctx, gpointer user_data);
void frida_slot_on_leave(GumInvocationContext *ctx, gpointer user_data);
} // namespace attach
} // namespace bpftime

extern "C" void *__bpftime_frida_attach_manager__override_handler();
#endif
