// Stand-in for libbpftime-agent.so (AGENT_SO) to measure the layers below the
// agent: the transformer rewrites the code and calls
// _bpftime__setup_syscall_trace_callback from its dlmopen namespace. By default
// the hook stays the transformer's own (issue the system call directly); with
// DUMMY_PASSTHROUGH set, a function in this namespace is installed that just
// calls the previous hook (one indirect call across the namespace).
#include <stdint.h>
#include <stdlib.h>

typedef int64_t (*hook_t)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t,
			  int64_t);

_Bool injected_with_frida; // the transformer writes it unconditionally
static hook_t orig;

static int64_t passthrough(int64_t nr, int64_t a1, int64_t a2, int64_t a3,
			   int64_t a4, int64_t a5, int64_t a6)
{
	return orig(nr, a1, a2, a3, a4, a5, a6);
}

void _bpftime__setup_syscall_trace_callback(hook_t *hook)
{
	if (getenv("DUMMY_PASSTHROUGH")) {
		orig = *hook;
		*hook = passthrough;
	}
}
