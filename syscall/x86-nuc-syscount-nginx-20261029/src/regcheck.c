// x86-64: a system call preserves every register except rax (result), rcx and
// r11. Set rdi, rsi, rdx, r10, r8, r9, rbx, rbp, r12-r15 to known values, issue
// getppid with a raw `syscall` (rewritten by the transformer like any other),
// and report which registers changed; then the same for xmm0-15. glibc relies
// on the general registers: __libc_sigaction
// keeps the oact pointer in r8 across its rt_sigaction `syscall`.
#include <stdint.h>
#include <stdio.h>

int main(void)
{
	static const char *names[] = { "rdi", "rsi", "rdx", "r10", "r8", "r9",
				       "rbx", "rbp", "r12", "r13", "r14", "r15" };
	enum { N = 12 };
	uint64_t want[N], got[N];
	int bad = 0;
	for (int i = 0; i < N; i++)
		want[i] = 0x1111111111111111ULL * (uint64_t)(i + 1);
	for (int round = 0; round < 1000; round++) {
		uint64_t *w = want, *g = got; // rax and rcx are overwritten
		__asm__ volatile(
			"push %%rbp\n\t"
			"push %%rbx\n\t"
			"push %%r12\n\t"
			"push %%r13\n\t"
			"push %%r14\n\t"
			"push %%r15\n\t"
			"push %[got]\n\t"
			"mov 0(%[want]), %%rdi\n\t"
			"mov 8(%[want]), %%rsi\n\t"
			"mov 16(%[want]), %%rdx\n\t"
			"mov 24(%[want]), %%r10\n\t"
			"mov 32(%[want]), %%r8\n\t"
			"mov 40(%[want]), %%r9\n\t"
			"mov 48(%[want]), %%rbx\n\t"
			"mov 56(%[want]), %%rbp\n\t"
			"mov 64(%[want]), %%r12\n\t"
			"mov 72(%[want]), %%r13\n\t"
			"mov 80(%[want]), %%r14\n\t"
			"mov 88(%[want]), %%r15\n\t"
			"mov $110, %%eax\n\t" // getppid
			"syscall\n\t"
			"pop %%rax\n\t" // got
			"mov %%rdi, 0(%%rax)\n\t"
			"mov %%rsi, 8(%%rax)\n\t"
			"mov %%rdx, 16(%%rax)\n\t"
			"mov %%r10, 24(%%rax)\n\t"
			"mov %%r8, 32(%%rax)\n\t"
			"mov %%r9, 40(%%rax)\n\t"
			"mov %%rbx, 48(%%rax)\n\t"
			"mov %%rbp, 56(%%rax)\n\t"
			"mov %%r12, 64(%%rax)\n\t"
			"mov %%r13, 72(%%rax)\n\t"
			"mov %%r14, 80(%%rax)\n\t"
			"mov %%r15, 88(%%rax)\n\t"
			"pop %%r15\n\t"
			"pop %%r14\n\t"
			"pop %%r13\n\t"
			"pop %%r12\n\t"
			"pop %%rbx\n\t"
			"pop %%rbp\n\t"
			: [want] "+a"(w), [got] "+c"(g)
			:
			: "rdi", "rsi", "rdx", "r8", "r9", "r10", "r11", "memory");
		for (int i = 0; i < N; i++)
			if (got[i] != want[i]) {
				if (round == 0)
					printf("%s: expected %#llx, got %#llx\n", names[i],
					       (unsigned long long)want[i],
					       (unsigned long long)got[i]);
				bad++;
			}
	}
	printf("1000 system calls, %d register values changed\n", bad);
	// The kernel also preserves the vector registers (xmm0-15 here).
	uint64_t xwant[32], xgot[32];
	int xbad = 0;
	for (int i = 0; i < 32; i++)
		xwant[i] = 0x0101010101010101ULL * (uint64_t)(i + 1);
	for (int round = 0; round < 1000; round++) {
		uint64_t *w = xwant, *g = xgot;
		__asm__ volatile(
			"push %[got]\n\t"
			"movdqu 0(%[want]), %%xmm0\n\t"
			"movdqu 16(%[want]), %%xmm1\n\t"
			"movdqu 32(%[want]), %%xmm2\n\t"
			"movdqu 48(%[want]), %%xmm3\n\t"
			"movdqu 64(%[want]), %%xmm4\n\t"
			"movdqu 80(%[want]), %%xmm5\n\t"
			"movdqu 96(%[want]), %%xmm6\n\t"
			"movdqu 112(%[want]), %%xmm7\n\t"
			"movdqu 128(%[want]), %%xmm8\n\t"
			"movdqu 144(%[want]), %%xmm9\n\t"
			"movdqu 160(%[want]), %%xmm10\n\t"
			"movdqu 176(%[want]), %%xmm11\n\t"
			"movdqu 192(%[want]), %%xmm12\n\t"
			"movdqu 208(%[want]), %%xmm13\n\t"
			"movdqu 224(%[want]), %%xmm14\n\t"
			"movdqu 240(%[want]), %%xmm15\n\t"
			"mov $110, %%eax\n\t" // getppid
			"syscall\n\t"
			"pop %%rax\n\t"
			"movdqu %%xmm0, 0(%%rax)\n\t"
			"movdqu %%xmm1, 16(%%rax)\n\t"
			"movdqu %%xmm2, 32(%%rax)\n\t"
			"movdqu %%xmm3, 48(%%rax)\n\t"
			"movdqu %%xmm4, 64(%%rax)\n\t"
			"movdqu %%xmm5, 80(%%rax)\n\t"
			"movdqu %%xmm6, 96(%%rax)\n\t"
			"movdqu %%xmm7, 112(%%rax)\n\t"
			"movdqu %%xmm8, 128(%%rax)\n\t"
			"movdqu %%xmm9, 144(%%rax)\n\t"
			"movdqu %%xmm10, 160(%%rax)\n\t"
			"movdqu %%xmm11, 176(%%rax)\n\t"
			"movdqu %%xmm12, 192(%%rax)\n\t"
			"movdqu %%xmm13, 208(%%rax)\n\t"
			"movdqu %%xmm14, 224(%%rax)\n\t"
			"movdqu %%xmm15, 240(%%rax)\n\t"
			: [want] "+a"(w), [got] "+c"(g)
			:
			: "r11", "memory", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4",
			  "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11",
			  "xmm12", "xmm13", "xmm14", "xmm15");
		for (int i = 0; i < 16; i++)
			if (xgot[2 * i] != xwant[2 * i] ||
			    xgot[2 * i + 1] != xwant[2 * i + 1]) {
				if (round == 0)
					printf("xmm%d: expected %#llx, got %#llx (low half)\n", i,
					       (unsigned long long)xwant[2 * i],
					       (unsigned long long)xgot[2 * i]);
				xbad++;
			}
	}
	printf("1000 system calls, %d xmm register values changed\n", xbad);
	return bad != 0 || xbad != 0;
}
