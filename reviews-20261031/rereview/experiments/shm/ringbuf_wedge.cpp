// A producer dying between bpf_ringbuf_reserve and submit leaves a BUSY
// header; does the consumer ever get past it?
#include "bpf_map/userspace/ringbuf_map.cpp"
#include <sys/wait.h>
#include <unistd.h>
#include <cstdio>
using namespace boost::interprocess;
int main()
{
	const char *name = "fable2_ringbuf_wedge";
	shared_memory_object::remove(name);
	managed_shared_memory shm(create_only, name, 4 << 20);
	auto *rb = shm.construct<bpftime::ringbuf_map_impl>("rb")(4096, shm);
	pid_t child = fork();
	if (child == 0) {
		void *p = rb->reserve(16, 1); // program killed here: no submit
		_exit(p ? 0 : 2);
	}
	int st; waitpid(child, &st, 0);
	// Later producers keep working...
	for (int i = 0; i < 10; i++) {
		void *p = rb->reserve(16, 1);
		if (!p) { printf("reserve %d failed errno %d\n", i, errno); break; }
		memset(p, 0x5a, 16);
		rb->submit(p, false);
	}
	// ...but the consumer delivers nothing, forever.
	auto impl = rb->create_impl_shared_ptr();
	int got = 0;
	for (int round = 0; round < 5; round++)
		got += impl->fetch_data([](void *, void *, size_t) { return 0; }, nullptr);
	printf("child exit %d; records delivered after 10 submits: %d; has_data=%d\n",
	       WEXITSTATUS(st), got, (int)impl->has_data());
	shared_memory_object::remove(name);
	return 0;
}
