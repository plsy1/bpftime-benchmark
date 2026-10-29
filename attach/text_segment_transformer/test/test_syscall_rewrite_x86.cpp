// x86-64 system call rewriting (zpoline): each case forks a child, rewrites
// the child's own code (setup_syscall_tracer maps page zero and is
// irreversible) and reports through the exit status. Page zero needs root
// and vm.mmap_min_addr=0; the cases are skipped otherwise.
#include "text_segment_transformer.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

extern "C" int64_t bpftime_test_x86_syscall_with_patterns(int64_t nr,
							   uint64_t *out);

namespace
{
syscall_hooker_func_t next_hook;
int hook_calls;

// As the agent's dispatch with programs: a C++ call chain that uses the
// argument registers, the vector registers and the flags.
__attribute__((noinline)) int64_t clobbering_hook(int64_t nr, int64_t a1,
						   int64_t a2, int64_t a3,
						   int64_t a4, int64_t a5,
						   int64_t a6)
{
	volatile double acc = 1.0;
	for (int i = 1; i < 32; i++)
		acc = acc * 1.0001 + i;
	char buf[256];
	memset(buf, (int)acc & 0x7f, sizeof(buf));
	asm volatile("" : : "r"(buf) : "memory");
	hook_calls++;
	return next_hook(nr, a1, a2, a3, a4, a5, a6);
}

bool page_zero_available()
{
	void *p = mmap(nullptr, 4096, PROT_READ,
		       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1,
		       0);
	if (p == MAP_FAILED)
		return false;
	munmap(p, 4096);
	return p == nullptr;
}

int in_rewritten_child(bool with_hook)
{
	fflush(nullptr);
	pid_t pid = fork();
	if (pid == 0) {
		alarm(30);
		bpftime::setup_syscall_tracer();
		if (with_hook) {
			next_hook = bpftime::get_call_hook();
			bpftime::set_call_hook(clobbering_hook);
		}
		uint64_t out[58];
		int bad = 0;
		pid_t parent = getppid();
		int64_t ret = bpftime_test_x86_syscall_with_patterns(
			SYS_getppid, out);
		if (ret != parent || (int64_t)out[0] != parent) {
			fprintf(stderr, "result %ld, expected %d\n", (long)ret,
				parent);
			bad++;
		}
		static const char *names[] = { "rdi", "rsi", "rdx", "r10",
					       "r8",  "r9",  "rbx", "r12",
					       "r13", "r14", "r15" };
		for (int i = 1; i <= 11; i++)
			if (out[i] != (0x5a5a000000000000ull | i)) {
				fprintf(stderr, "%s = %#lx\n", names[i - 1],
					(unsigned long)out[i]);
				bad++;
			}
		for (int n = 0; n < 16; n++)
			if (out[12 + 2 * n] != (0x1111000000000000ull | n) ||
			    out[13 + 2 * n] != (0x2222000000000000ull | n)) {
				fprintf(stderr, "xmm%d = %#lx %#lx\n", n,
					(unsigned long)out[12 + 2 * n],
					(unsigned long)out[13 + 2 * n]);
				bad++;
			}
		for (int i = 3; i <= 16; i++)
			if (out[44 + i - 3] != 0x3333000000000000ull + i) {
				fprintf(stderr, "red zone rsp-%d = %#lx\n",
					8 * i, (unsigned long)out[44 + i - 3]);
				bad++;
			}
		if (with_hook && hook_calls == 0) {
			fprintf(stderr, "hook not called\n");
			bad++;
		}
		fflush(stderr);
		_exit(bad > 100 ? 100 : bad);
	}
	int status = 0;
	waitpid(pid, &status, 0);
	return WIFEXITED(status) ? WEXITSTATUS(status) :
				   128 + WTERMSIG(status);
}
} // namespace

TEST_CASE("x86: a rewritten syscall keeps registers and the red zone")
{
	if (!page_zero_available())
		SKIP("page zero not mappable: needs root and vm.mmap_min_addr=0");
	// Without a hook the C entry calls the original system call directly,
	// as the agent's dispatch does for a system call without programs.
	SECTION("no hook")
	{
		REQUIRE(in_rewritten_child(false) == 0);
	}
	SECTION("hook that uses registers")
	{
		REQUIRE(in_rewritten_child(true) == 0);
	}
}
