#include <pthread.h>
#include <unistd.h>
static thread_local int cached_pid = -1;
static const int atfork_status = pthread_atfork(nullptr, nullptr, [] { cached_pid = -1; });
extern "C" int lib_atfork_status() { return atfork_status; }
extern "C" int lib_identity() {
	if (cached_pid == -1) cached_pid = getpid();
	return cached_pid;
}
