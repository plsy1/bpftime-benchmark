#include <frida_uprobe_attach_impl.hpp>
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <chrono>
#include <dlfcn.h>
#include <latch>
#include <signal.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace bpftime;
using namespace bpftime::attach;

namespace
{
thread_local int mmap_hits = 0;

void map_and_unmap_a_page()
{
	void *p = mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p != MAP_FAILED)
		munmap(p, 4096);
}

// Runs in a child: a hang must not take the test binary with it.
int ignore_probes_on_fresh_threads_with_mmap_probed()
{
	frida_attach_impl man;
	void *mmap_addr = dlsym(RTLD_DEFAULT, "mmap");
	if (mmap_addr == nullptr)
		return 2;
	if (man.create_uprobe_at(mmap_addr,
				 [](const pt_regs &) { mmap_hits++; }) < 0)
		return 3;
	// All threads stay alive until each has its context (about 100 KB
	// each, mostly the invocation stack), so Gum's heap has to grow while
	// the contexts are made.
	constexpr int thread_count = 32;
	std::latch all_done(thread_count);
	std::atomic<int> wrong{ 0 };
	std::vector<std::thread> threads;
	for (int i = 0; i < thread_count; i++) {
		threads.emplace_back([&] {
			ignore_probes_on_this_thread();
			map_and_unmap_a_page();
			int hits_while_ignored = mmap_hits;
			unignore_probes_on_this_thread();
			map_and_unmap_a_page();
			if (hits_while_ignored != 0 || mmap_hits < 1)
				wrong++;
			all_done.arrive_and_wait();
		});
	}
	for (auto &t : threads)
		t.join();
	return wrong.load() == 0 ? 0 : 1;
}
} // namespace

TEST_CASE("Fresh threads can ignore probes while mmap is probed")
{
	pid_t pid = fork();
	REQUIRE(pid >= 0);
	if (pid == 0)
		_exit(ignore_probes_on_fresh_threads_with_mmap_probed());
	int status = 0;
	bool exited = false;
	for (int i = 0; i < 200 && !exited; i++) {
		if (waitpid(pid, &status, WNOHANG) == pid)
			exited = true;
		else
			std::this_thread::sleep_for(
				std::chrono::milliseconds(100));
	}
	if (!exited) {
		kill(pid, SIGKILL);
		waitpid(pid, &status, 0);
	}
	// A hang is a thread spinning on the Gum heap lock it holds itself.
	INFO("child exited within 20 s: " << exited);
	REQUIRE(exited);
	REQUIRE(WIFEXITED(status));
	// 1: a probe hit an ignored thread or missed an unignored one
	REQUIRE(WEXITSTATUS(status) == 0);
}
