#ifndef _TEST_SEGMENT_TRANSFORMER_H
#define _TEST_SEGMENT_TRANSFORMER_H
// C++ standard library could not be found by clangd on my machine.. Will fix in
// a later time
#include <cinttypes>
#include <utility>
#include <vector>
using syscall_hooker_func_t = int64_t (*)(int64_t sys_nr, int64_t arg1,
					  int64_t arg2, int64_t arg3,
					  int64_t arg4, int64_t arg5,
					  int64_t arg6);
namespace bpftime
{
	// Setup userspace syscall trace
void setup_syscall_tracer();
// Get current callback function when a syscall was invoked. Default to be a function that directly calls the syscall
syscall_hooker_func_t get_call_hook();
// Set the syscall callback function
void set_call_hook(syscall_hooker_func_t hook);
// The agent's bpftime_identity_epoch, bumped before every vfork-like clone
// (aarch64; see note_clone_syscall).
void set_identity_epoch(uint64_t *epoch);
struct lazy_rewrite_stats {
	uint64_t traps, rewritten, emulated;
	// Traps in MAP_SHARED code, made untraced (patching would write the
	// file); also counted in emulated.
	uint64_t shared_skipped;
	// execve calls refused in the handler (no gate, so made there).
	uint64_t execs_refused;
};
// Whether BPFTIME_SYSCALL_LAZY=1 was set at setup_syscall_tracer (aarch64).
bool lazy_rewriting_requested();
// Lazy mode (aarch64, research): installs a seccomp filter that turns system
// calls from code never rewritten into SIGSYS, whose handler rewrites that
// site. `allowed` are code ranges whose calls pass (the agent's namespace).
// Irreversible: it sets no_new_privs, and the filter is inherited by children
// and across execve, where nothing handles SIGSYS (an exec-ed program dies at
// its first system call). False when not requested or not installed.
bool enable_lazy_rewriting(
	const std::vector<std::pair<uint64_t, uint64_t> > &allowed);
lazy_rewrite_stats get_lazy_rewrite_stats();
// Tests: opens the file of the mapping [begin, end) of `path` as the
// rewriter does, if it still is the file with this device and inode; -1
// otherwise (aarch64).
int open_mapped_file_for_test(uint64_t begin, uint64_t end, unsigned dev_major,
			      unsigned dev_minor, uint64_t inode,
			      const char *path);
// While one exists, this thread's system calls bypass the hook, and code it
// maps is not rewritten (aarch64): for loading the agent's own namespace.
class unhooked_scope {
	bool previous;

    public:
	unhooked_scope();
	~unhooked_scope();
	unhooked_scope(const unhooked_scope &) = delete;
	unhooked_scope &operator=(const unhooked_scope &) = delete;
};
} // namespace bpftime

#endif
