// Parking forms other than the aligned 64-bit literal: a function whose
// entry is 4 mod 8 (literal_low32), one that only allows a 4-byte redirect
// (b form), one that allows 8 (adrp; br: cannot_park). Four threads call the
// function while probes attach and detach; then a fork child detaches while
// the parent's threads are inside callbacks.
#include <frida_uprobe_attach_impl.hpp>
#include <frida_hook_slot.hpp>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <signal.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace bpftime;
using namespace bpftime::attach;

extern "C" uint64_t f_unaligned(uint64_t x);
extern "C" uint64_t f_bform(uint64_t x);
extern "C" uint64_t f_adrpform(uint64_t x);

asm(R"(
	.text
	.balign 16
	nop
	.globl f_unaligned
	.type f_unaligned, %function
f_unaligned:
	mov x1, #0x1234
	eor x0, x0, x1
	add x0, x0, x1
	mul x0, x0, x1
	eor x0, x0, x0, lsr #17
	ret
	.size f_unaligned, .-f_unaligned

	.balign 16
	.globl f_bform
	.type f_bform, %function
f_bform:
	mov x1, #8
1:	subs x1, x1, #1
	add x0, x0, x1
	mul x0, x0, x0
	b.ne 1b
	ret
	.size f_bform, .-f_bform

	.balign 16
	.globl f_adrpform
	.type f_adrpform, %function
f_adrpform:
	mov x1, #8
	mov x2, #3
1:	subs x1, x1, #1
	add x0, x0, x2
	mul x0, x0, x0
	b.ne 1b
	ret
	.size f_adrpform, .-f_adrpform
)");

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

static void dump(const char *tag, void *fn)
{
	const auto *p = (const uint32_t *)fn;
	printf("%-32s %p words: %08x %08x %08x %08x parked=%d\n", tag, fn, p[0],
	       p[1], p[2], p[3], (int)frida_hook_is_parked(fn));
	fflush(stdout);
}

static int cycle(frida_attach_impl &man, const char *name,
		 uint64_t (*fn)(uint64_t), int seconds)
{
	printf("== %s at %p (mod 8 = %lu)\n", name, (void *)fn,
	       (unsigned long)((uintptr_t)fn & 7));
	callers c(fn);
	std::atomic<uint64_t> hits{ 0 };
	int cycles = 0;
	const auto until = std::chrono::steady_clock::now() +
			   std::chrono::seconds(seconds);
	while (std::chrono::steady_clock::now() < until) {
		int enter = man.create_uprobe_at((void *)fn, [&](const pt_regs &) {
			hits.fetch_add(1, std::memory_order_relaxed);
		});
		int leave = man.create_uretprobe_at(
			(void *)fn, [&](const pt_regs &) {
				hits.fetch_add(1, std::memory_order_relaxed);
			});
		if (enter < 0 || leave < 0) {
			printf("attach failed %d %d\n", enter, leave);
			return 2;
		}
		if (cycles == 0)
			dump("attached", (void *)fn);
		std::this_thread::sleep_for(std::chrono::microseconds(200));
		man.detach_by_id(enter);
		man.detach_by_id(leave);
		if (cycles == 0)
			dump("detached", (void *)fn);
		cycles++;
	}
	c.join();
	printf("%s: cycles=%d hits=%lu calls=%lu parked_now=%d\n", name, cycles,
	       (unsigned long)hits.load(), (unsigned long)c.calls.load(),
	       (int)frida_hook_is_parked((void *)fn));
	fflush(stdout);
	return 0;
}

static int fork_child_detaches()
{
	frida_attach_impl man;
	std::atomic<uint64_t> hits{ 0 };
	int id = man.create_uprobe_at((void *)f_unaligned, [&](const pt_regs &) {
		hits.fetch_add(1, std::memory_order_relaxed);
		for (volatile int i = 0; i < 2000; i++)
			;
	});
	if (id < 0)
		return 2;
	callers c(f_unaligned);
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	pid_t pid = fork();
	if (pid == 0) {
		// Only this thread exists here; the parent's callers' counters
		// are odd in our copy and never move.
		man.detach_by_id(id);
		uint64_t (*volatile t)(uint64_t) = f_unaligned;
		t(7);
		_exit(frida_hook_is_parked((void *)f_unaligned) ? 0 : 6);
	}
	int status = 0;
	bool exited = false;
	for (int i = 0; i < 100 && !exited; i++) {
		if (waitpid(pid, &status, WNOHANG) == pid)
			exited = true;
		else
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	if (!exited) {
		kill(pid, SIGKILL);
		waitpid(pid, &status, 0);
		printf("fork child: detach HUNG (killed after 10 s)\n");
		c.join();
		return 7;
	}
	printf("fork child: status %d (0 = detached and parked)\n",
	       WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status));
	c.join();
	man.detach_by_id(id);
	return 0;
}

static int body()
{
	frida_attach_impl man;
	int rc = 0;
	rc |= cycle(man, "f_unaligned", f_unaligned, 2);
	rc |= cycle(man, "f_bform", f_bform, 2);
	rc |= cycle(man, "f_adrpform", f_adrpform, 2);
	return rc;
}

int main(int argc, char **argv)
{
	int which = argc > 1 ? atoi(argv[1]) : 0;
	pid_t pid = fork();
	if (pid == 0)
		_exit(which == 1 ? fork_child_detaches() : body());
	int status = 0;
	waitpid(pid, &status, 0);
	if (WIFEXITED(status))
		printf("child exited %d\n", WEXITSTATUS(status));
	else
		printf("child killed by signal %d (%s)\n", WTERMSIG(status),
		       strsignal(WTERMSIG(status)));
	return 0;
}
