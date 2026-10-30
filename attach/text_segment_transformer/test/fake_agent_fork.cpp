// Stands in for the agent in test_agent_namespace.cpp: loaded into its own
// link-map namespace, it caches its pid and registers the reset with
// bpftime::register_fork_handlers, as runtime code does.
#include "fork_handlers.hpp"
#include <unistd.h>

namespace
{
pid_t cached_pid = -1;
int prepared, parented;
const int registered = bpftime::register_fork_handlers(
	[] { prepared++; }, [] { parented++; }, [] { cached_pid = -1; });
} // namespace

extern "C" pid_t fake_agent_cached_pid()
{
	if (cached_pid == -1)
		cached_pid = getpid();
	return cached_pid;
}

extern "C" int fake_agent_fork_counts()
{
	return registered == 0 ? prepared * 10 + parented : -1;
}

extern "C" void bpftime_agent_fork_hook(int phase)
{
	bpftime::run_fork_handlers(static_cast<bpftime::fork_phase>(phase));
}
