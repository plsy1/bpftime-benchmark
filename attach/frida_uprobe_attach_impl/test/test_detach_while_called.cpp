#include <frida_uprobe_attach_impl.hpp>
#include <frida_hook_slot.hpp>
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <exception>
#include <signal.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace bpftime;
using namespace bpftime::attach;

extern "C" __attribute__((noinline)) uint64_t
detach_while_called_target(uint64_t x)
{
	uint64_t h = 0x9e3779b97f4a7c15ULL;
	for (int i = 0; i < 4; i++) {
		h ^= x + i;
		h *= 0xff51afd7ed558ccdULL;
	}
	asm volatile("" : "+r"(h));
	return h;
}

extern "C" __attribute__((noinline)) uint64_t
detach_reattach_target(uint64_t x)
{
	uint64_t h = 0xc2b2ae3d27d4eb4fULL;
	for (int i = 0; i < 4; i++) {
		h ^= x + i;
		h *= 0x165667b19e3779f9ULL;
	}
	asm volatile("" : "+r"(h));
	return h;
}

namespace
{
// Four threads call `target` until stopped.
struct callers {
	uint64_t (*volatile target)(uint64_t);
	std::atomic<bool> stop{ false };
	std::atomic<uint64_t> calls{ 0 };
	std::vector<std::thread> threads;
	explicit callers(uint64_t (*fn)(uint64_t), int count = 4) : target(fn)
	{
		for (int i = 0; i < count; i++)
			threads.emplace_back([this, i] {
				uint64_t n = 0, sink = 0;
				while (!stop.load(std::memory_order_relaxed)) {
					sink += target(n + i);
					n++;
				}
				calls.fetch_add(n + (sink & 0));
			});
	}
	void join()
	{
		stop.store(true);
		for (auto &t : threads)
			t.join();
		threads.clear();
	}
	~callers()
	{
		if (!threads.empty())
			join();
	}
};

// Runs a body in a child, bounded at 30 s: a crash or hang must not take the
// test binary with it. Returns the child's exit status, or 128 + signal.
int run_in_child(int (*body)())
{
	pid_t pid = fork();
	if (pid == 0) {
		// Never back into Catch in the child.
		try {
			_exit(body());
		} catch (const std::exception &e) {
			fprintf(stderr, "child: %s\n", e.what());
		} catch (...) {
		}
		_exit(5);
	}
	int status = 0;
	bool exited = false;
	for (int i = 0; i < 300 && !exited; i++) {
		if (waitpid(pid, &status, WNOHANG) == pid)
			exited = true;
		else
			std::this_thread::sleep_for(
				std::chrono::milliseconds(100));
	}
	if (!exited) {
		kill(pid, SIGKILL);
		waitpid(pid, &status, 0);
		return 255;
	}
	return WIFEXITED(status) ? WEXITSTATUS(status) :
				   128 + WTERMSIG(status);
}

int detach_while_threads_call_the_function()
{
	frida_attach_impl man;
	callers c(detach_while_called_target);
	std::atomic<uint64_t> hits{ 0 };
	int cycles = 0;
	const auto until =
		std::chrono::steady_clock::now() + std::chrono::seconds(3);
	while (std::chrono::steady_clock::now() < until) {
		int enter = man.create_uprobe_at(
			(void *)detach_while_called_target,
			[&](const pt_regs &) {
				hits.fetch_add(1, std::memory_order_relaxed);
			});
		int leave = man.create_uretprobe_at(
			(void *)detach_while_called_target,
			[&](const pt_regs &) {
				hits.fetch_add(1, std::memory_order_relaxed);
			});
		if (enter < 0 || leave < 0)
			return 2;
		std::this_thread::sleep_for(std::chrono::microseconds(200));
		man.detach_by_id(enter);
		man.detach_by_id(leave);
		cycles++;
	}
	c.join();
	// Probes ran, and the threads kept calling through the detaches.
	return hits.load() > 0 && c.calls.load() > 0 && cycles > 100 ? 0 : 3;
}

// Each cycle's callback checks, after a short spin, whether its detach has
// already returned.
int callbacks_after_detach_returns()
{
	frida_attach_impl man;
	callers c(detach_while_called_target);
	std::deque<std::atomic<bool>> detached;
	std::atomic<uint64_t> late{ 0 }, hits{ 0 };
	const auto until =
		std::chrono::steady_clock::now() + std::chrono::seconds(3);
	while (std::chrono::steady_clock::now() < until) {
		auto &done = detached.emplace_back(false);
		int id = man.create_uprobe_at(
			(void *)detach_while_called_target,
			[&](const pt_regs &) {
				for (volatile int i = 0; i < 100; i++)
					;
				hits.fetch_add(1, std::memory_order_relaxed);
				if (done.load(std::memory_order_acquire))
					late.fetch_add(1);
			});
		if (id < 0)
			return 2;
		std::this_thread::sleep_for(std::chrono::microseconds(200));
		man.detach_by_id(id);
		done.store(true, std::memory_order_release);
	}
	c.join();
	if (hits.load() == 0)
		return 3;
	return late.load() == 0 ? 0 : 4;
}
} // namespace

// Detaching the last probe of a function used to copy its saved first
// instructions back over Gum's 16-byte redirect while other threads ran it
// (a thread that had fetched the redirect's `ldr x16` loaded the restored
// instructions as the jump address), and Gum freed the detached listener's
// entry while they iterated it (SIGBUS/SIGSEGV; problem 7 of
// uprobe/realtools-20261104).
TEST_CASE("Detaching probes while threads call the function keeps them running")
{
	int status = run_in_child(detach_while_threads_call_the_function);
	// 2: attach failed; 3: no probe ran or too few cycles; 5: an
	// exception; 255: hang; above 128: killed by a signal
	INFO("child status " << status);
	REQUIRE(status == 0);
}

TEST_CASE("No probe callback runs after its detach returned")
{
	int status = run_in_child(callbacks_after_detach_returns);
	// 4: a callback saw its detach already returned
	INFO("child status " << status);
	REQUIRE(status == 0);
}

TEST_CASE("Probes attached again after a detach run")
{
	frida_attach_impl man;
	uint64_t (*volatile target)(uint64_t) = detach_reattach_target;
	int hits = 0;
	int id = man.create_uprobe_at((void *)detach_reattach_target,
				      [&](const pt_regs &) { hits++; });
	REQUIRE(id >= 0);
	target(1);
	REQUIRE(hits == 1);
	REQUIRE_FALSE(frida_hook_is_parked((void *)detach_reattach_target));
	man.detach_by_id(id);
	target(2);
	REQUIRE(hits == 1);
#if defined(__aarch64__)
	// Gum's 16-byte redirect: calls bypass Gum while nothing is attached.
	REQUIRE(frida_hook_is_parked((void *)detach_reattach_target));
#endif
	int leave_hits = 0;
	id = man.create_uprobe_at((void *)detach_reattach_target,
				  [&](const pt_regs &) { hits++; });
	int leave = man.create_uretprobe_at(
		(void *)detach_reattach_target,
		[&](const pt_regs &) { leave_hits++; });
	REQUIRE(id >= 0);
	REQUIRE(leave >= 0);
	REQUIRE_FALSE(frida_hook_is_parked((void *)detach_reattach_target));
	target(3);
	REQUIRE(hits == 2);
	REQUIRE(leave_hits == 1);
	man.detach_by_id(id);
	man.detach_by_id(leave);
	target(4);
	REQUIRE(hits == 2);
	REQUIRE(leave_hits == 1);
}

// Calls per second of a hooked function: native, attached (an empty uprobe),
// and parked after the detach. Hidden; run by name.
TEST_CASE("Hook cost attached and parked", "[.][hook-cost]")
{
	auto rate = [](int threads) {
		callers c(detach_while_called_target, threads);
		std::this_thread::sleep_for(std::chrono::seconds(2));
		c.join();
		return c.calls.load() / 2.0;
	};
	frida_attach_impl man;
	for (int t : { 1, 6 })
		printf("native threads=%d calls/s=%.0f\n", t, rate(t));
	int id = man.create_uprobe_at((void *)detach_while_called_target,
				      [](const pt_regs &) {});
	for (int t : { 1, 6 })
		printf("attached threads=%d calls/s=%.0f\n", t, rate(t));
	man.detach_by_id(id);
	printf("parked=%d\n",
	       (int)frida_hook_is_parked((void *)detach_while_called_target));
	for (int t : { 1, 6 })
		printf("detached threads=%d calls/s=%.0f\n", t, rate(t));
}
