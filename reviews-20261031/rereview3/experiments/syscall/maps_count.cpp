// Default mode: munmap/mremap/shmat/remap_file_pages go out of line now; check they are still traced once, results intact.
#include "text_segment_transformer.hpp"
#include <atomic>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
static std::atomic<long> cnt[512];
static syscall_hooker_func_t next;
static int64_t hook(int64_t nr, int64_t a1, int64_t a2, int64_t a3, int64_t a4, int64_t a5, int64_t a6) {
	if (nr >= 0 && nr < 512) cnt[nr]++;
	return next(nr, a1, a2, a3, a4, a5, a6);
}
int main() {
	bpftime::setup_syscall_tracer();
	next = bpftime::get_call_hook();
	bpftime::set_call_hook(&hook);
	const int N = 2000; int bad = 0;
	for (int i = 0; i < N; i++) {
		char *p = (char *)mmap(nullptr, 8192, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (p == MAP_FAILED) { bad++; continue; }
		memset(p, 0x5a, 8192);
		char *q = (char *)mremap(p, 8192, 16384, MREMAP_MAYMOVE);
		if (q == MAP_FAILED || q[0] != 0x5a || q[8191] != 0x5a) bad++;
		if (munmap(q, 16384) != 0) bad++;
	}
	errno = 0; if (munmap((void *)0x1000, 0) != -1 || errno != EINVAL) bad++; // error path through the hook
	int id = shmget(IPC_PRIVATE, 4096, IPC_CREAT | 0600);
	if (id >= 0) { void *s = shmat(id, nullptr, 0); if (s == (void *)-1) bad++; else { memset(s, 1, 4096); shmdt(s); } shmctl(id, IPC_RMID, nullptr); }
	printf("maps_count: mmap=%ld mremap=%ld munmap=%ld shmat=%ld shmdt=%ld bad=%d\n",
	       cnt[SYS_mmap].load(), cnt[SYS_mremap].load(), cnt[SYS_munmap].load(), cnt[SYS_shmat].load(), cnt[SYS_shmdt].load(), bad);
	return (bad == 0 && cnt[SYS_mremap] == N && cnt[SYS_munmap] == N + 1 && cnt[SYS_shmat] == 1) ? 0 : 1;
}
