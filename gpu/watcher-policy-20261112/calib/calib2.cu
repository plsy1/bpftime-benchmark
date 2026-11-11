// Cost of the signals the watcher policies read: calib2
// - pread of a held /proc/loadavg fd and of /proc/thread-self/schedstat (opened in this thread)
// - cudaEventQuery on a completed event, and on events recorded by another thread that keeps
//   launching short kernels (the event of the latest launch), as the watcher would see it
// Each item 20,000 times (event queries 2,000 times, 1 every 500 us while launching); prints
// median / p99 / max in us.
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <cuda_runtime.h>

__global__ void spinKernel(unsigned long long ns)
{
	unsigned long long t0, t;
	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t0));
	do {
		asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t));
	} while (t - t0 < ns);
}

static double now_us()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1e6 + ts.tv_nsec / 1e3;
}

static void report(const char *name, std::vector<double> &v)
{
	std::sort(v.begin(), v.end());
	printf("%-44s n=%zu median=%.2f p99=%.2f max=%.2f us\n", name, v.size(),
	       v[v.size() / 2], v[(size_t)(v.size() * 0.99)], v.back());
}

int main()
{
	char buf[256];
	int la = open("/proc/loadavg", O_RDONLY);
	int sd = open("/proc/thread-self/schedstat", O_RDONLY);
	std::vector<double> a, b;
	for (int i = 0; i < 20000; i++) {
		double t0 = now_us();
		pread(la, buf, sizeof(buf), 0);
		double t1 = now_us();
		pread(sd, buf, sizeof(buf), 0);
		double t2 = now_us();
		a.push_back(t1 - t0);
		b.push_back(t2 - t1);
	}
	report("pread /proc/loadavg", a);
	report("pread /proc/thread-self/schedstat", b);

	cudaFree(nullptr);
	cudaEvent_t done;
	cudaEventCreateWithFlags(&done, cudaEventDisableTiming);
	cudaEventRecord(done, 0);
	cudaDeviceSynchronize();
	std::vector<double> c;
	for (int i = 0; i < 20000; i++) {
		double t0 = now_us();
		cudaEventQuery(done);
		c.push_back(now_us() - t0);
	}
	report("cudaEventQuery, completed event, idle", c);

	// another thread launches 200 us kernels back to back, recording an event after each
	cudaStream_t s;
	cudaStreamCreateWithFlags(&s, cudaStreamNonBlocking);
	const int NE = 64;
	cudaEvent_t ev[NE];
	for (int i = 0; i < NE; i++)
		cudaEventCreateWithFlags(&ev[i], cudaEventDisableTiming);
	std::atomic<int> latest{ -1 };
	std::atomic<bool> stop{ false };
	std::vector<double> launch;
	std::thread launcher([&] {
		int i = 0;
		while (!stop.load()) {
			double t0 = now_us();
			spinKernel<<<1, 1, 0, s>>>(200000);
			cudaEventRecord(ev[i % NE], s);
			launch.push_back(now_us() - t0);
			latest.store(i % NE);
			i++;
			cudaStreamSynchronize(s);
		}
	});
	std::vector<double> d;
	int running = 0;
	for (int i = 0; i < 2000; i++) {
		usleep(500);
		int e = latest.load();
		if (e < 0)
			continue;
		double t0 = now_us();
		cudaError_t r = cudaEventQuery(ev[e]);
		d.push_back(now_us() - t0);
		running += r == cudaErrorNotReady;
	}
	stop.store(true);
	launcher.join();
	report("cudaEventQuery while another thread launches", d);
	printf("  (%d of %zu queries saw the kernel running)\n", running, d.size());
	report("launch + event record (launcher thread)", launch);
	return 0;
}
