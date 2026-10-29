#ifndef _BPFTIME_AGENT_NAMESPACE_HPP
#define _BPFTIME_AGENT_NAMESPACE_HPP

namespace bpftime
{
// Load the agent into a new link-map namespace (dlmopen), so that its libc is
// not rewritten. The two libcs allocate pthread key numbers from separate
// tables but store values in the same per-thread array; this reserves a range
// of the first 32 keys (stored inline in the thread, never freed) for the
// agent's libc and keeps the process's libc out of it. Returns the handle, or
// nullptr with dlerror() set.
void *load_agent_in_new_namespace(const char *path);
} // namespace bpftime

#endif
