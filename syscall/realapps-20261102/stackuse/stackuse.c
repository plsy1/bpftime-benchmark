// How much stack below the caller's sp does a hooked system call use? Runs
// getpid with sp inside a pattern-filled buffer, then finds the lowest byte
// that changed. The svc is in this file, so the transformer rewrites it.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>

static uint8_t buf[64 * 1024] __attribute__((aligned(16)));

static long call_on(uint8_t *sp)
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

int main(void)
{
	for (int round = 0; round < 6; round++) {
		memset(buf, 0xa5, sizeof(buf));
		long pid = call_on(buf + sizeof(buf));
		size_t low = sizeof(buf);
		for (size_t i = 0; i < sizeof(buf); i++)
			if (buf[i] != 0xa5) {
				low = i;
				break;
			}
		printf("call %d pid %ld, bytes used below sp: %zu\n", round, pid,
		       sizeof(buf) - low);
	}
	return 0;
}
