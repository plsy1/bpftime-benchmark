// pthread_cancel of a thread blocked in a hooked read, under the real agent
// (dlmopen namespace): its C++ destructors must run.
#include <atomic>
#include <cstdio>
#include <pthread.h>
#include <unistd.h>
static int p[2];
static std::atomic<bool> cleaned{ false };
struct flag {
	~flag() { cleaned = true; }
};
static void *reader(void *)
{
	flag f;
	char c;
	if (read(p[0], &c, 1) < 0)
		perror("read");
	return nullptr;
}
int main()
{
	if (pipe(p) != 0)
		return 2;
	pthread_t t;
	pthread_create(&t, nullptr, reader, nullptr);
	usleep(200000);
	pthread_cancel(t);
	void *r = nullptr;
	pthread_join(t, &r);
	printf("cancelled=%d cleanup=%s\n", r == PTHREAD_CANCELED,
	       cleaned ? "ran" : "SKIPPED");
	return cleaned ? 0 : 1;
}
