#include "common.hpp"
#include <sys/mman.h>
int main()
{
	setenv("BPFTIME_SYSCALL_LAZY", "1", 1);
	bpftime::setup_syscall_tracer();
	if (!bpftime::enable_lazy_rewriting({})) return 10;
	uint32_t code[3] = { mov_x8(SYS_getppid), SVC_0, RET };
	// P: a JIT's private RWX page.
	auto *P = (uint32_t *)mmap(nullptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	memcpy(P, code, sizeof(code));
	__builtin___clear_cache((char *)P, (char *)P + 4096);
	// S: a MAP_SHARED RWX file mapping with the same code (mapped before the
	// first trap, so its mmap does not clear the cache afterwards).
	int fd = memfd_create("code", 0);
	ftruncate(fd, 4096);
	pwrite(fd, code, sizeof(code), 0);
	auto *S = (uint32_t *)mmap(nullptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, fd, 0);
	if (P == MAP_FAILED || S == MAP_FAILED) return 11;
	// First trap at P: rewritten, the handler caches P's (private) mapping.
	long r = ((code_fn)P)();
	auto st = bpftime::get_lazy_rewrite_stats();
	printf("P first call: getppid=%ld (%d) word1=%08x traps=%lu rewritten=%lu shared_skipped=%lu\n", r, getppid(), P[1], st.traps, st.rewritten, st.shared_skipped);
	// Move the shared mapping over P (mremap through libc: a rewritten
	// site, hooked, not a 'special' call: the cache is not cleared).
	void *moved = mremap(S, 4096, 4096, MREMAP_MAYMOVE | MREMAP_FIXED, P);
	if (moved != P) { perror("mremap"); return 12; }
	printf("after mremap: P word1=%08x (svc again, from the shared file)\n", P[1]);
	r = ((code_fn)P)();
	uint32_t file_word = 0;
	pread(fd, &file_word, 4, 4);
	st = bpftime::get_lazy_rewrite_stats();
	printf("P second call: getppid=%ld word1=%08x FILE word at 4 = %08x (%s) traps=%lu rewritten=%lu shared_skipped=%lu\n", r, P[1], file_word,
	       file_word == SVC_0 ? "svc: file intact" : "NOT svc: THE SHARED FILE WAS PATCHED", st.traps, st.rewritten, st.shared_skipped);
	return file_word == SVC_0 ? 0 : 1;
}
