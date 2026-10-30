#include "fork_handlers.hpp"
#include <atomic>
#include <cerrno>
#include <pthread.h>

namespace bpftime
{
namespace
{
struct fork_handler_entry {
	void (*prepare)();
	void (*parent)();
	void (*child)();
};

// Constant-initialized: static initializers register before any
// constructor of this file could run. Entries are published in order, so
// a reader of `published` sees complete entries only. Registration is
// rare (static init, call_once); running takes no lock, so a fork while
// another thread registers cannot deadlock the child.
constexpr int max_fork_handlers = 32;
fork_handler_entry fork_handlers[max_fork_handlers];
std::atomic<int> reserved{ 0 };
std::atomic<int> published{ 0 };
} // namespace

int register_fork_handlers(void (*prepare)(), void (*parent)(),
			   void (*child)())
{
	const int index = reserved.fetch_add(1, std::memory_order_relaxed);
	if (index >= max_fork_handlers)
		return ENOMEM;
	fork_handlers[index] = { prepare, parent, child };
	int expected = index;
	while (!published.compare_exchange_weak(expected, index + 1,
						std::memory_order_release,
						std::memory_order_relaxed))
		expected = index;
	return pthread_atfork(prepare, parent, child);
}

void run_fork_handlers(fork_phase phase)
{
	const int count = published.load(std::memory_order_acquire);
	if (phase == fork_prepare) {
		for (int i = count - 1; i >= 0; i--)
			if (fork_handlers[i].prepare)
				fork_handlers[i].prepare();
		return;
	}
	for (int i = 0; i < count; i++) {
		auto handler = phase == fork_parent ? fork_handlers[i].parent :
						      fork_handlers[i].child;
		if (handler)
			handler();
	}
}
} // namespace bpftime
