#ifndef _BPFTIME_FORK_HANDLERS_HPP
#define _BPFTIME_FORK_HANDLERS_HPP

namespace bpftime
{
enum fork_phase { fork_prepare = 0, fork_parent = 1, fork_child = 2 };

// pthread_atfork for runtime code. It registers with this library's libc
// as usual and also in bpftime's own table: the syscall-trace transformer
// loads the agent with dlmopen, where pthread_atfork reaches the agent
// namespace's libc, which never sees the application's fork. The
// transformer runs the table from the main libc instead
// (bpftime_agent_fork_hook). Returns pthread_atfork's result, or ENOMEM
// when the table is full (nothing registered then).
int register_fork_handlers(void (*prepare)(), void (*parent)(),
			   void (*child)());

// Runs the table in pthread_atfork's order: prepare handlers last
// registered first, parent and child handlers first registered first.
void run_fork_handlers(fork_phase phase);
} // namespace bpftime

#endif
