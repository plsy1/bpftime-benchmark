#ifndef _BPFTIME_FRIDA_INTERNAL_ATTACH_ENTRY_HPP
#define _BPFTIME_FRIDA_INTERNAL_ATTACH_ENTRY_HPP
#include "frida_attach_entry.hpp"
#include <frida-gum.h>
#include <vector>
#include "frida_uprobe_attach_impl.hpp"
namespace bpftime
{
namespace attach
{
struct frida_hook_slot;
struct frida_hook_dispatch;
// Used to represent an attach entry faced to frida. On a certain function, we
// can have either one replace attach, or one filter attach, or many uprobe
// attaches or uretprobe attaches. This entry was related with a function
// address. Gum's hooks belong to the function's slot (frida_hook_slot.hpp);
// after changing user_attaches, republish the slot before freeing what was
// removed.
class frida_internal_attach_entry {
	void *function;
	std::vector<frida_attach_entry *> user_attaches;
	frida_hook_slot *slot = nullptr;

	friend class frida_attach_impl;

    public:
	bool has_override() const;
	bool has_uprobe_or_uretprobe() const;
	void ensure_listener(int attach_type);
	// Adds this entry's attaches to a dispatch being built.
	void collect(frida_hook_dispatch &dispatch);
	// Publishes user_attaches: when it returns, no callback runs an attach
	// removed before the call.
	void republish();
	static void run_uprobes(const frida_hook_dispatch &dispatch,
				const pt_regs &regs);
	static void run_uretprobes(const frida_hook_dispatch &dispatch,
				   const pt_regs &regs);
	static void run_override(const frida_attach_entry &attach,
				 const pt_regs &regs);
	frida_internal_attach_entry(const frida_internal_attach_entry &) =
		delete;
	frida_internal_attach_entry &
	operator=(const frida_internal_attach_entry &) = delete;
	~frida_internal_attach_entry();
	frida_internal_attach_entry(void *function, int basic_attach_type);
};
} // namespace attach
} // namespace bpftime
#endif
