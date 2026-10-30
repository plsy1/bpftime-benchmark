// Loaded with dlopen after the process was rewritten: a library with its own
// system call instruction (as Go's runtime or a static libc would have), not
// a call into the already rewritten libc.
#include <sys/syscall.h>

long bpftime_test_raw_getppid(void)
{
	register long nr __asm__("x8") = SYS_getppid;
	register long ret __asm__("x0");
	__asm__ volatile("svc #0" : "=r"(ret) : "r"(nr) : "memory");
	return ret;
}
