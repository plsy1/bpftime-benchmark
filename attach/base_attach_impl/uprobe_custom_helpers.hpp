#ifndef _BPFTIME_UPROBE_CUSTOM_HELPERS_HPP
#define _BPFTIME_UPROBE_CUSTOM_HELPERS_HPP

namespace bpftime::attach
{
// The helpers the uprobe attach impls (Frida, trap) register for every
// program, each with its own functions. The syscall server registers the
// same indices and names when it compiles the AOT objects agents load
// (runtime/src/bpftime_prog_cache.cpp): an object's key holds the index and
// name of every helper registered for it, so the impls must take their
// indices and names from here, or no agent would use the cached objects.
struct uprobe_custom_helper {
	int index;
	const char *name;
};

inline constexpr uprobe_custom_helper uprobe_get_func_arg = {
	183, "bpf_get_func_arg"
};
inline constexpr uprobe_custom_helper uprobe_get_func_ret = {
	184, "bpf_get_func_ret_id"
};
inline constexpr uprobe_custom_helper uprobe_get_retval = { 186,
							    "bpf_get_retval" };

inline constexpr uprobe_custom_helper uprobe_custom_helpers[] = {
	uprobe_get_func_arg, uprobe_get_func_ret, uprobe_get_retval
};
} // namespace bpftime::attach

#endif
