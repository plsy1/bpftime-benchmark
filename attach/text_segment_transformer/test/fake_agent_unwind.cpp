// Stands in for the agent's syscall callback in its own link-map namespace
// (its own libstdc++ and libgcc_s): a try/catch around the real system call,
// as syscall_callback has. A forced unwind (pthread_cancel) is driven by the
// application's libgcc_s, but this frame's personality calls this
// namespace's _Unwind_SetGR, which aborts until this copy has unwound once.
#include <cstdint>
#include <cxxabi.h>
#include <unwind.h>

using hook_t = int64_t (*)(int64_t, int64_t, int64_t, int64_t, int64_t,
			   int64_t, int64_t);
static hook_t next_hook;

extern "C" void fake_agent_set_next(hook_t next)
{
	next_hook = next;
}

extern "C" void fake_agent_init_unwinder()
{
	_Unwind_Backtrace([](_Unwind_Context *, void *) { return _URC_NO_REASON; },
			  nullptr);
}

extern "C" int64_t fake_agent_hook(int64_t nr, int64_t a1, int64_t a2,
				   int64_t a3, int64_t a4, int64_t a5,
				   int64_t a6)
{
	try {
		return next_hook(nr, a1, a2, a3, a4, a5, a6);
	} catch (abi::__forced_unwind &) {
		throw;
	} catch (...) {
		return -1;
	}
}
