#ifndef _BPFTIME_AGENT_NAMESPACE_HPP
#define _BPFTIME_AGENT_NAMESPACE_HPP

#include <cstdint>
#include <utility>
#include <vector>

namespace bpftime
{
// Load the agent into a new link-map namespace (dlmopen), so that its libc is
// not rewritten. The two libcs allocate pthread key numbers from separate
// tables but store values in the same per-thread array; this reserves a range
// of the first 32 keys (stored inline in the thread, never freed) for the
// agent's libc and keeps the process's libc out of it. Returns the handle, or
// nullptr with dlerror() set.
void *load_agent_in_new_namespace(const char *path);
// The application forks through the main libc, which does not run the fork
// handlers the agent registered with its own libc: register, with the main
// libc, handlers that call the agent's bpftime_agent_fork_hook. Returns false
// when the agent does not export it.
bool install_agent_fork_hook(void *handle);
// The executable segments of every object in the namespace of `handle`.
std::vector<std::pair<uint64_t, uint64_t> > namespace_code_ranges(void *handle);
} // namespace bpftime

#endif
