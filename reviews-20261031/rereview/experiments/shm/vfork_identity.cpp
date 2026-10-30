// posix_spawn/vfork children share the parent thread's TLS. If the thread's
// producer identity cache is still empty, the child's first perf output fills
// it with the child's (pid, tid), and the parent keeps that identity.
#include "handler/perf_event_handler.cpp"
#include "fork_handlers.cpp"
#include <sys/wait.h>
#include <cstdio>
using namespace boost::interprocess;
int main()
{
	const char *name = "fable2_vfork_identity";
	shared_memory_object::remove(name);
	managed_shared_memory shm(create_only, name, 16 << 20);
	auto *perf = shm.construct<bpftime::software_perf_event_data>("perf")(0, 0, 0, shm);
	perf->ensure_mmap_buffer(getpagesize() + 64 * 1024);
	uint64_t payload = 1;
	pid_t child = vfork();
	if (child == 0) {
		// e.g. a traced execve() in the spawn child before exec
		perf->output_data(&payload, sizeof(payload));
		_exit(0);
	}
	int st; waitpid(child, &st, 0);
	// parent produces afterwards
	perf->output_data(&payload, sizeof(payload));
	perf->output_data(&payload, sizeof(payload));
	printf("parent pid %d, child pid %d\n", getpid(), child);
	for (auto &s : perf->producer_shards)
		printf("  shard pid=%d tid=%ld gen=%lu has_data=%d\n", s.pid, (long)s.tid,
		       (unsigned long)s.generation, (int)s.buffer.has_data());
	// consumer drains (130 drains include two reclaim passes)
	for (int i = 0; i < 130; i++) (void)perf->has_data();
	printf("after drains+reclaim: %zu shards\n", perf->producer_shards.size());
	// parent produces again
	perf->output_data(&payload, sizeof(payload));
	for (auto &s : perf->producer_shards)
		printf("  shard pid=%d tid=%ld gen=%lu\n", s.pid, (long)s.tid, (unsigned long)s.generation);
	shared_memory_object::remove(name);
	return 0;
}
