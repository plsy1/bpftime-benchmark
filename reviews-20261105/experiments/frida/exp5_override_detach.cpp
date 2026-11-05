// Detaching an override (uprobe_override / ureplace) while four threads call
// the function. detach_by_id shrinks the internal entry's user_attaches
// before republishing; the replacement handler takes override_owner from the
// (old) dispatch and then iterates that live vector in run_filter_callback,
// which throws when it finds no override entry.
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

extern "C" uint64_t bpftime_set_retval(uint64_t value);

extern "C" __attribute__((noinline)) uint64_t override_target(uint64_t x)
{
	uint64_t h = 0x9e3779b97f4a7c15ULL;
	for (int i = 0; i < 4; i++) {
		h ^= x + i;
		h *= 0xff51afd7ed558ccdULL;
	}
	asm volatile("" : "+r"(h));
	return h;
}

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

static int body(int seconds)
{
	frida_attach_impl man;
	callers c(override_target);
	std::atomic<uint64_t> hits{ 0 };
	int cycles = 0;
	const auto until = std::chrono::steady_clock::now() +
			   std::chrono::seconds(seconds);
	while (std::chrono::steady_clock::now() < until) {
		int id = man.create_uprobe_override_at(
			(void *)override_target, [&](const pt_regs &) {
				hits.fetch_add(1, std::memory_order_relaxed);
				bpftime_set_retval(42);
			});
		if (id < 0) {
			printf("attach failed %d\n", id);
			return 2;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(200));
		man.detach_by_id(id);
		cycles++;
		if (cycles % 1000 == 0) {
			printf("cycles=%d hits=%lu\n", cycles,
			       (unsigned long)hits.load());
			fflush(stdout);
		}
	}
	c.join();
	printf("done: cycles=%d hits=%lu calls=%lu parked=%d\n", cycles,
	       (unsigned long)hits.load(), (unsigned long)c.calls.load(),
	       (int)frida_hook_is_parked((void *)override_target));
	fflush(stdout);
	return 0;
}

int main(int argc, char **argv)
{
	int seconds = argc > 1 ? atoi(argv[1]) : 3;
	pid_t pid = fork();
	if (pid == 0) {
		int rc = body(seconds);
		fflush(stdout);
		_exit(rc);
	}
	int status = 0;
	waitpid(pid, &status, 0);
	if (WIFEXITED(status))
		printf("child exited %d\n", WEXITSTATUS(status));
	else
		printf("child killed by signal %d (%s)\n", WTERMSIG(status),
		       strsignal(WTERMSIG(status)));
	return 0;
}
