// Does a hook slot survive the destruction of the last frida_attach_impl?
// Gum's interceptor is a refcounted singleton; the impl holds the only
// long-lived reference. When it drops, gum_interceptor_do_dispose removes
// every function context (restores the code, frees the trampolines) while
// the slots still point at them.
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

using namespace bpftime;
using namespace bpftime::attach;

extern "C" __attribute__((noinline)) uint64_t exp_target(uint64_t x)
{
	uint64_t h = 0x9e3779b97f4a7c15ULL;
	for (int i = 0; i < 4; i++) {
		h ^= x + i;
		h *= 0xff51afd7ed558ccdULL;
	}
	asm volatile("" : "+r"(h));
	return h;
}

static void dump(const char *tag)
{
	const auto *p = (const uint32_t *)exp_target;
	printf("%-28s words: %08x %08x %08x %08x  parked=%d\n", tag, p[0], p[1],
	       p[2], p[3], (int)frida_hook_is_parked((void *)exp_target));
	fflush(stdout);
}

static int body(int variant)
{
	uint64_t (*volatile target)(uint64_t) = exp_target;
	uint32_t orig[4];
	memcpy(orig, (void *)exp_target, sizeof(orig));
	dump("original");
	std::atomic<int> hits{ 0 };
	{
		frida_attach_impl A;
		int id = A.create_uprobe_at((void *)exp_target,
					    [&](const pt_regs &) { hits++; });
		if (id < 0)
			return 2;
		target(1);
		dump("A attached");
		printf("hits after 1 call with A attached: %d (expect 1)\n",
		       hits.load());
		if (variant == 1) {
			// Leave the probe attached when A dies.
		} else {
			A.detach_by_id(id);
			dump("A detached (parked)");
			target(2);
			printf("hits after call while parked: %d (expect 1)\n",
			       hits.load());
		}
	}
	dump("after ~A");
	printf("code restored by Gum dispose: %s\n",
	       memcmp(orig, (void *)exp_target, sizeof(orig)) == 0 ? "yes" :
								      "no");
	target(3);
	printf("hits after ~A: %d\n", hits.load());
	{
		frida_attach_impl B;
		int id = B.create_uprobe_at((void *)exp_target,
					    [&](const pt_regs &) { hits++; });
		printf("B attach id=%d\n", id);
		dump("B attached (same slot)");
		fflush(stdout);
		target(4);
		printf("hits after 1 call with B attached: %d (expect 2)\n",
		       hits.load());
		fflush(stdout);
		B.detach_by_id(id);
		dump("B detached");
		target(5);
		printf("hits after B detach: %d\n", hits.load());
	}
	return 0;
}

int main(int argc, char **argv)
{
	int variant = argc > 1 ? atoi(argv[1]) : 0;
	pid_t pid = fork();
	if (pid == 0)
		_exit(body(variant));
	int status = 0;
	waitpid(pid, &status, 0);
	if (WIFEXITED(status))
		printf("child exited %d\n", WEXITSTATUS(status));
	else
		printf("child killed by signal %d (%s)\n", WTERMSIG(status),
		       strsignal(WTERMSIG(status)));
	return 0;
}
