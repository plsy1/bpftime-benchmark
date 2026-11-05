// Shared harness for the hook-stack experiments: rewrites this process's own
// code (like the transformer tests) and installs a hook that does a little
// work before the real system call, as the agent's dispatch would.
#pragma once
#include "text_segment_transformer.hpp"
#include <cstdint>
#include <cstring>
#include <sys/syscall.h>
#include <unistd.h>

static syscall_hooker_func_t next_hook;

__attribute__((noinline)) static void some_work()
{
	char buf[256];
	memset(buf, 0x77, sizeof(buf));
	__asm__ volatile("" : : "r"(buf) : "memory");
}

static int64_t recording_hook(int64_t nr, int64_t a1, int64_t a2, int64_t a3,
			      int64_t a4, int64_t a5, int64_t a6)
{
	some_work();
	return next_hook(nr, a1, a2, a3, a4, a5, a6);
}

static void install_hook()
{
	bpftime::setup_syscall_tracer();
	next_hook = bpftime::get_call_hook();
	bpftime::set_call_hook(&recording_hook);
}

// getpid issued with sp at `sp`, then back on the thread's stack (from the
// transformer test "a hooked call uses only a few bytes of the caller's
// stack").
__attribute__((noinline)) static long getpid_with_sp(uint8_t *sp)
{
	register long x8 __asm__("x8") = SYS_getpid;
	register long x0 __asm__("x0");
	register uint8_t *x9 __asm__("x9") = sp;
	__asm__ volatile("mov x10, sp\n\t"
			 "mov sp, x9\n\t"
			 "svc #0\n\t"
			 "mov sp, x10\n\t"
			 : "=r"(x0)
			 : "r"(x8), "r"(x9)
			 : "x10", "memory");
	return x0;
}

static uint8_t probe_stack[64 << 10] __attribute__((aligned(16)));

// Bytes a hooked getpid writes below the caller's sp: 32 when the hook
// switched to its own stack, ~880 when its frame stayed on the caller's.
static size_t bytes_used_below_sp()
{
	memset(probe_stack, 0xa5, sizeof(probe_stack));
	getpid_with_sp(probe_stack + sizeof(probe_stack));
	for (size_t i = 0; i < sizeof(probe_stack); i++)
		if (probe_stack[i] != 0xa5)
			return sizeof(probe_stack) - i;
	return 0;
}
