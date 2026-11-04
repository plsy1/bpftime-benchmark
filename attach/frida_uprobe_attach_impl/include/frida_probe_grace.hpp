#ifndef _BPFTIME_FRIDA_PROBE_GRACE_HPP
#define _BPFTIME_FRIDA_PROBE_GRACE_HPP
#include <atomic>
#include <cstdint>
#include <sys/types.h>

namespace bpftime
{
namespace attach
{
namespace probe_grace
{
// Probe callbacks run between enter() and leave() on their thread. When
// synchronize() returns, no callback that may have loaded something
// unpublished before the call is still running: a grace period, as userspace
// RCU's membarrier flavor. enter() and leave() write only the calling
// thread's own counter (odd inside); synchronize() makes the other threads
// execute a barrier with membarrier() and waits for each counter it finds odd
// to move.
struct reader {
	std::atomic<uint64_t> count{ 0 };
	uint32_t depth = 0;
	pid_t tid = 0;
};
reader *enter();
void leave(reader *self);
void synchronize();
} // namespace probe_grace
} // namespace attach
} // namespace bpftime
#endif
