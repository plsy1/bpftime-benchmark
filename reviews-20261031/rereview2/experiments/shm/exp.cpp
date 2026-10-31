// Standalone experiments against libruntime.a (HEAD 9aeb2863).
#include "handler/perf_event_handler.hpp"
#include "bpf_map/userspace/ringbuf_map.hpp"
#include "producer_identity.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sched.h>
#include <signal.h>
#include <string>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace bip = boost::interprocess;

static int zombie()
{
	const std::string name = "fable3_rb_zombie_" + std::to_string(getpid());
	bip::shared_memory_object::remove(name.c_str());
	int rc = 0;
	{
		bip::managed_shared_memory shm(bip::create_only, name.c_str(),
					       4 << 20);
		auto *map = shm.construct<bpftime::ringbuf_map_impl>("rb")(4096,
									    shm);
		auto rb = map->create_impl_shared_ptr();
		pid_t child = fork();
		if (child == 0) {
			map->reserve(16, 1);
			_exit(0);
		}
		// Wait until the child is a zombie (not yet reaped).
		for (int i = 0; i < 2000; i++) {
			char path[64];
			snprintf(path, sizeof path, "/proc/%d/stat", child);
			FILE *f = fopen(path, "r");
			char st = 0;
			if (f) {
				int p;
				char comm[64];
				if (fscanf(f, "%d %63s %c", &p, comm, &st) != 3)
					st = 0;
				fclose(f);
			}
			if (st == 'Z')
				break;
			usleep(1000);
		}
		bool before = rb->recover_dead_reservation();
		int status;
		waitpid(child, &status, 0);
		bool after = rb->recover_dead_reservation();
		printf("zombie: recover before reap=%d after reap=%d\n", before,
		       after);
		rc = before ? 1 : 0;
	}
	bip::shared_memory_object::remove(name.c_str());
	return rc;
}

static int headeronly()
{
	const std::string name = "fable3_perf_hdr_" + std::to_string(getpid());
	bip::shared_memory_object::remove(name.c_str());
	int rc = 0;
	{
		bip::managed_shared_memory shm(bip::create_only, name.c_str(),
					       1 << 20);
		auto *perf = shm.construct<bpftime::software_perf_event_data>(
			"perf")(0, 0, 0, shm);
		const size_t ring = 64 * 1024;
		if (perf->ensure_mmap_buffer(getpagesize() + ring) == nullptr) {
			printf("consumer ring alloc failed\n");
			return 2;
		}
		// Leave about 24 KiB: enough for a list node and the 4 KiB
		// header page, not for the 64 KiB data area.
		std::vector<void *> filler;
		while (shm.get_free_memory() > 24 * 1024) {
			size_t want = shm.get_free_memory() - 24 * 1024;
			if (want > 64 * 1024)
				want = 64 * 1024;
			try {
				filler.push_back(shm.allocate(want));
			} catch (...) {
				break;
			}
		}
		printf("free before output: %zu\n", shm.get_free_memory());
		char payload[16] = {};
		errno = 0;
		int r1 = perf->output_data(payload, sizeof payload);
		int e1 = errno;
		printf("output while full: ret=%d errno=%d shards=%zu", r1, e1,
		       perf->producer_shards.size());
		if (!perf->producer_shards.empty())
			printf(" shard mmap_size=%zu",
			       perf->producer_shards.front().buffer.mmap_size());
		printf("\n");
		for (void *p : filler)
			shm.deallocate(p);
		printf("free after release: %zu\n", shm.get_free_memory());
		errno = 0;
		int r2 = perf->output_data(payload, sizeof payload);
		int e2 = errno;
		printf("output after memory freed: ret=%d errno=%d shards=%zu\n",
		       r2, e2, perf->producer_shards.size());
		rc = (r2 == 0) ? 0 : 1;
	}
	bip::shared_memory_object::remove(name.c_str());
	return rc;
}

static int rawclone()
{
	int pid, tid_dummy;
	int64_t tid;
	bpftime::current_producer_identity(pid, tid); // cache in parent
	(void)tid_dummy;
	// A fork-like clone that runs no atfork handler (raw syscall).
	long child = syscall(SYS_clone, SIGCHLD, 0, 0, 0, 0);
	if (child == 0) {
		int cpid;
		int64_t ctid;
		bpftime::current_producer_identity(cpid, ctid);
		_exit(cpid == getpid() ? 0 : 1);
	}
	int status;
	waitpid((pid_t)child, &status, 0);
	printf("rawclone: child sees its own pid=%s\n",
	       WEXITSTATUS(status) == 0 ? "yes" : "NO (parent's cached ids)");
	return WEXITSTATUS(status);
}

int main(int argc, char **argv)
{
	if (argc < 2)
		return 2;
	std::string which = argv[1];
	if (which == "zombie")
		return zombie();
	if (which == "headeronly")
		return headeronly();
	if (which == "rawclone")
		return rawclone();
	return 2;
}
