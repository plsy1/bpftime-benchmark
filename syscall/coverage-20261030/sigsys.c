// Spike 1: where si_call_addr points on arm64, register access in the handler,
// SIGSYS round trip cost, and the cost of an instruction-pointer filter.
#define _GNU_SOURCE
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <linux/audit.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

extern char svc_site[];
static long raw_getppid(void)
{
	register long x8 __asm__("x8") = SYS_getppid;
	register long x0 __asm__("x0");
	__asm__ volatile(".globl svc_site\nsvc_site: svc #0" : "=r"(x0) : "r"(x8) : "memory");
	return x0;
}
static volatile uintptr_t seen_addr, seen_pc;
static void on_sigsys(int sig, siginfo_t *si, void *ctx)
{
	ucontext_t *uc = ctx;
	seen_addr = (uintptr_t)si->si_call_addr;
	seen_pc = uc->uc_mcontext.pc;
	uc->uc_mcontext.regs[0] = 4242; // the emulated result
}
static double now(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec * 1e-9;
}
static double loop_ns(int n, long nr)
{
	double t = now();
	for (int i = 0; i < n; i++)
		syscall(nr);
	return (now() - t) / n * 1e9;
}
int main(int argc, char **argv)
{
	int mode = argc > 1 ? argv[1][0] - '0' : 0;
	double base = loop_ns(1000000, SYS_getpid);
	if (mode == 0) {
		// Trap getppid only.
		struct sock_filter f[] = {
			BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),
			BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_getppid, 0, 1),
			BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_TRAP),
			BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
		};
		struct sock_fprog prog = { sizeof(f) / sizeof(f[0]), f };
		struct sigaction sa = { 0 };
		sa.sa_sigaction = on_sigsys;
		sa.sa_flags = SA_SIGINFO | SA_NODEFER;
		sigaction(SIGSYS, &sa, NULL);
		if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) || prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog))
			return perror("seccomp"), 1;
		long r = raw_getppid();
		printf("svc at %p, si_call_addr %#lx (svc%+ld), uc pc %#lx (svc%+ld), result %ld\n",
		       (void *)svc_site, seen_addr, (long)(seen_addr - (uintptr_t)svc_site),
		       seen_pc, (long)(seen_pc - (uintptr_t)svc_site), r);
		double trap = loop_ns(200000, SYS_getppid);
		printf("getpid %.1f ns; trapped getppid (SIGSYS round trip) %.1f ns\n", base, trap);
	} else {
		// Instruction-pointer filter: allow from one 4 KiB range, else
		// allow too (what an allowlist costs when the IP check runs on
		// every call; the low word of the IP is compared).
		struct sock_filter f[] = {
			BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, instruction_pointer) + 4),
			BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0x1234, 0, 3),
			BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, instruction_pointer)),
			BPF_JUMP(BPF_JMP | BPF_JGE | BPF_K, 0x10000000, 0, 1),
			BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
			BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
		};
		struct sock_fprog prog = { sizeof(f) / sizeof(f[0]), f };
		if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) || prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog))
			return perror("seccomp"), 1;
		double with = loop_ns(1000000, SYS_getpid);
		printf("getpid without filter %.1f ns, with IP filter %.1f ns (+%.1f)\n", base, with, with - base);
	}
	return 0;
}
