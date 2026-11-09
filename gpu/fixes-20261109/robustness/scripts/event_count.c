// Counts cuEventCreate calls made through the dynamic linker (bpftime's agent;
// cudart reaches the driver through cuGetProcAddress and is not counted),
// split by CU_EVENT_DISABLE_TIMING (bpftime's launch events), printed at exit.
#define _GNU_SOURCE
#include <cuda.h>
#include <dlfcn.h>
#include <stdatomic.h>
#include <stdio.h>
static atomic_int no_timing, other;
CUresult cuEventCreate(CUevent *event, unsigned int flags)
{
	static CUresult (*real)(CUevent *, unsigned int);
	if (!real)
		real = (CUresult(*)(CUevent *, unsigned int))dlsym(RTLD_NEXT, "cuEventCreate");
	if (flags & CU_EVENT_DISABLE_TIMING)
		atomic_fetch_add(&no_timing, 1);
	else
		atomic_fetch_add(&other, 1);
	return real(event, flags);
}
__attribute__((destructor)) static void report(void)
{
	fprintf(stderr, "event-shim: cuEventCreate disable_timing=%d other=%d\n",
		atomic_load(&no_timing), atomic_load(&other));
}
