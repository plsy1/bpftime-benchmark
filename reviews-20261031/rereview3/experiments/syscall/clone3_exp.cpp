// clone3 scenarios for note_clone_syscall's direct read of clone_args.
#include "text_segment_transformer.hpp"
#include <linux/sched.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
extern char **environ;
static uint64_t epoch = 0;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s (errno %d)\n", __LINE__, #c, errno); _exit(10); } } while (0)

static int wait_ok(pid_t pid) { int st = 0; return waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0; }

static long fork_like_clone3(struct clone_args *ca) {
	memset(ca, 0, sizeof(*ca));
	ca->exit_signal = SIGCHLD;
	return syscall(SYS_clone3, ca, sizeof(*ca));
}

// clone_args in a frame more than 1 KiB above the sp of the svc.
__attribute__((noinline)) static long far_frame_clone3(struct clone_args *ca) {
	volatile char pad[3000];
	pad[0] = 1; pad[2999] = 2;
	__asm__ volatile("" ::"r"(pad) : "memory");
	long r = fork_like_clone3(ca);
	__asm__ volatile("" ::"r"(pad) : "memory");
	return r;
}

static volatile int handler_result = -1;
static void altstack_handler(int) {
	// posix_spawn's clone3 (CLONE_VM|CLONE_VFORK) with its args on the alt stack
	char a0[] = "true"; char *argv[] = { a0, nullptr };
	pid_t pid; 
	if (posix_spawn(&pid, "/bin/true", nullptr, nullptr, argv, environ) != 0) { handler_result = 1; return; }
	int st = 0;
	if (waitpid(pid, &st, 0) != pid || !WIFEXITED(st) || WEXITSTATUS(st) != 0) { handler_result = 2; return; }
	struct clone_args ca;
	long r = fork_like_clone3(&ca); // args in the handler's frame on the alt stack
	if (r == 0) _exit(0);
	handler_result = (r > 0 && wait_ok(r)) ? 0 : 3;
}

// Issue clone3 with a given sp: sp set to `newsp`, the call made from a tiny frame.
__attribute__((noinline)) static long clone3_at_sp(uint64_t newsp, uint64_t ptr, uint64_t size) {
	long ret;
	__asm__ volatile(
		"mov x9, sp\n\t"
		"mov sp, %[nsp]\n\t"
		"str x9, [sp, #-16]!\n\t"
		"mov x8, #435\n\t"
		"mov x0, %[p]\n\t"
		"mov x1, %[s]\n\t"
		"svc #0\n\t"
		"mov %[ret], x0\n\t"
		"ldr x9, [sp], #16\n\t"
		"mov sp, x9\n\t"
		: [ret] "=r"(ret)
		: [nsp] "r"(newsp), [p] "r"(ptr), [s] "r"(size)
		: "x0", "x1", "x8", "x9", "memory", "cc");
	return ret;
}

int main(int argc, char **argv) {
	bool rewrite = argc < 2 || strcmp(argv[1], "native") != 0;
	if (rewrite) {
		bpftime::setup_syscall_tracer();
		bpftime::set_identity_epoch(&epoch);
	}
	uint64_t e = epoch;
	// (a) pthread_create: clone_args in create_thread's frame, CLONE_THREAD|CLONE_SETTLS -> no bump
	pthread_t th;
	CHECK(pthread_create(&th, nullptr, [](void *) -> void * { return nullptr; }, nullptr) == 0);
	CHECK(pthread_join(th, nullptr) == 0);
	printf("a pthread_create: epoch %lu -> %lu\n", e, epoch); e = epoch;
	// (a2) thread with the minimum stack creating a thread itself
	pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setstacksize(&at, PTHREAD_STACK_MIN);
	CHECK(pthread_create(&th, &at, [](void *) -> void * {
		pthread_t t2; if (pthread_create(&t2, nullptr, [](void *) -> void * { return nullptr; }, nullptr) != 0) return (void *)1;
		pthread_join(t2, nullptr); return nullptr; }, nullptr) == 0);
	void *res; CHECK(pthread_join(th, &res) == 0 && res == nullptr);
	printf("a2 min-stack thread creating a thread: epoch %lu -> %lu\n", e, epoch); e = epoch;
	// (b) args > 1 KiB above sp
	struct clone_args ca;
	long r = far_frame_clone3(&ca);
	if (r == 0) _exit(0);
	CHECK(r > 0 && wait_ok(r));
	printf("b far frame fork-like clone3: epoch %lu -> %lu\n", e, epoch); e = epoch;
	// (c) heap args
	struct clone_args *hca = (struct clone_args *)malloc(sizeof(struct clone_args));
	r = fork_like_clone3(hca);
	if (r == 0) _exit(0);
	CHECK(r > 0 && wait_ok(r));
	printf("c heap fork-like clone3: epoch %lu -> %lu\n", e, epoch); e = epoch;
	// (c2) near frame fork-like (no CLONE_THREAD): bump expected either way
	r = fork_like_clone3(&ca);
	if (r == 0) _exit(0);
	CHECK(r > 0 && wait_ok(r));
	printf("c2 near frame fork-like clone3: epoch %lu -> %lu\n", e, epoch); e = epoch;
	// (d) sigaltstack handler
	static char alt[64 * 1024] __attribute__((aligned(16)));
	stack_t ss = { alt, 0, sizeof(alt) };
	CHECK(sigaltstack(&ss, nullptr) == 0);
	struct sigaction sa = {}; sa.sa_handler = altstack_handler; sa.sa_flags = SA_ONSTACK;
	CHECK(sigaction(SIGUSR1, &sa, nullptr) == 0);
	raise(SIGUSR1);
	CHECK(handler_result == 0);
	printf("d posix_spawn + fork-like clone3 on sigaltstack: epoch %lu -> %lu\n", e, epoch); e = epoch;
	// (e) bogus far pointer -> EFAULT
	errno = 0; r = syscall(SYS_clone3, (void *)0x10, 88);
	CHECK(r == -1 && errno == EFAULT);
	printf("e bogus pointer EFAULT: epoch %lu -> %lu\n", e, epoch); e = epoch;
	// (e2) pointer within the window, garbage on the live stack -> kernel decides (EINVAL expected)
	{
		volatile char garbage[256]; memset((void *)garbage, 0xff, sizeof(garbage));
		errno = 0; r = syscall(SYS_clone3, (void *)garbage, 88);
		printf("e2 garbage in-window pointer: r=%ld errno=%d epoch %lu -> %lu\n", r, errno, e, epoch); e = epoch;
		CHECK(r == -1);
	}
	// (f) PATHOLOGICAL: sp 256 bytes below the end of a mapping, bad pointer in the unmapped page above
	if (argc >= 3 && strcmp(argv[2], "f") == 0) {
		size_t pg = 4096;
		char *reg = (char *)mmap(nullptr, 4 * pg, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		CHECK(reg != MAP_FAILED);
		CHECK(munmap(reg + 3 * pg, pg) == 0); // top page unmapped
		uint64_t top = (uint64_t)reg + 3 * pg;
		fflush(stdout);
		errno = 0;
		r = clone3_at_sp(top - 256, top + 64, 88);
		printf("f bad pointer 320 B above an sp near the stack top: raw r=%ld epoch %lu -> %lu\n", r, e, epoch);
		CHECK(r == -EFAULT);
	}
	printf("clone3_exp OK (%s)\n", rewrite ? "rewritten" : "native");
	return 0;
}
