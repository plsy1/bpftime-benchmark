// (1) An override attached, detached (parked), then a uprobe session and an
// override session again on the same function: the replacement stays and the
// forwarder calls the original; check results with 5 and 6 integer arguments.
// (2) Hundreds of short-lived threads each hit a probe once (a reader each),
// then detaches: synchronize's collection of exited threads' readers.
// (3) The Gum interceptor's refcount while an impl lives and after it dies.
#include <frida_uprobe_attach_impl.hpp>
#include <frida_hook_slot.hpp>
#include <frida-gum.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <signal.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace bpftime;
using namespace bpftime::attach;

extern "C" uint64_t bpftime_set_retval(uint64_t value);

extern "C" __attribute__((noinline)) uint64_t
six_args(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f)
{
	uint64_t h = a * 3 + b * 5 + c * 7 + d * 11 + e * 13 + f * 17;
	asm volatile("" : "+r"(h));
	return h;
}

extern "C" __attribute__((noinline)) double fp_arg(double x, uint64_t y)
{
	double r = x * 2.0 + (double)y;
	asm volatile("" : "+w"(r));
	return r;
}

static int override_sessions()
{
	frida_attach_impl man;
	uint64_t (*volatile f6)(uint64_t, uint64_t, uint64_t, uint64_t,
				uint64_t, uint64_t) = six_args;
	double (*volatile fd)(double, uint64_t) = fp_arg;
	const uint64_t native = f6(1, 2, 3, 4, 5, 6);
	const double native_d = fd(1.5, 2);
	printf("native six_args=%lu fp_arg=%g\n", (unsigned long)native,
	       native_d);

	// Session 1: override that forces 42.
	int id = man.create_uprobe_override_at((void *)six_args,
					       [](const pt_regs &) {
						       bpftime_set_retval(42);
					       });
	int idd = man.create_uprobe_override_at((void *)fp_arg,
						[](const pt_regs &) {
							bpftime_set_retval(7);
						});
	if (id < 0 || idd < 0)
		return 2;
	printf("session1 override: six_args=%lu (expect 42)\n",
	       (unsigned long)f6(1, 2, 3, 4, 5, 6));
	man.detach_by_id(id);
	man.detach_by_id(idd);
	printf("after detach: parked=%d six_args=%lu (expect %lu) fp_arg=%g (expect %g)\n",
	       (int)frida_hook_is_parked((void *)six_args),
	       (unsigned long)f6(1, 2, 3, 4, 5, 6), (unsigned long)native,
	       fd(1.5, 2), native_d);

	// Session 2: a plain uprobe on the once-overridden functions.
	std::atomic<int> hits{ 0 };
	id = man.create_uprobe_at((void *)six_args,
				  [&](const pt_regs &) { hits++; });
	idd = man.create_uprobe_at((void *)fp_arg,
				   [&](const pt_regs &) { hits++; });
	uint64_t got = f6(1, 2, 3, 4, 5, 6);
	double gotd = fd(1.5, 2);
	printf("session2 uprobe: parked=%d hits=%d six_args=%lu (native %lu) %s; fp_arg=%g (native %g) %s\n",
	       (int)frida_hook_is_parked((void *)six_args), hits.load(),
	       (unsigned long)got, (unsigned long)native,
	       got == native ? "OK" : "WRONG", gotd, native_d,
	       gotd == native_d ? "OK" : "WRONG");
	man.detach_by_id(id);
	man.detach_by_id(idd);

	// Session 3: override again.
	id = man.create_uprobe_override_at((void *)six_args,
					   [](const pt_regs &) {
						   bpftime_set_retval(43);
					   });
	printf("session3 override: parked=%d six_args=%lu (expect 43)\n",
	       (int)frida_hook_is_parked((void *)six_args),
	       (unsigned long)f6(1, 2, 3, 4, 5, 6));
	man.detach_by_id(id);
	printf("end: parked=%d six_args=%lu (expect %lu)\n",
	       (int)frida_hook_is_parked((void *)six_args),
	       (unsigned long)f6(1, 2, 3, 4, 5, 6), (unsigned long)native);
	fflush(stdout);
	return 0;
}

extern "C" __attribute__((noinline)) uint64_t gc_target(uint64_t x)
{
	asm volatile("" : "+r"(x));
	return x * 2654435761u;
}

static int many_threads()
{
	frida_attach_impl man;
	std::atomic<int> hits{ 0 };
	int id = man.create_uprobe_at((void *)gc_target,
				      [&](const pt_regs &) { hits++; });
	if (id < 0)
		return 2;
	uint64_t (*volatile t)(uint64_t) = gc_target;
	for (int round = 0; round < 3; round++) {
		std::vector<std::thread> threads;
		for (int i = 0; i < 300; i++)
			threads.emplace_back([&] { t(1); });
		for (auto &th : threads)
			th.join();
		// Each detach synchronizes; readers of exited threads are
		// collected once the list passes 64.
		man.detach_by_id(id);
		id = man.create_uprobe_at((void *)gc_target,
					  [&](const pt_regs &) { hits++; });
		printf("round %d: hits=%d\n", round, hits.load());
		fflush(stdout);
	}
	man.detach_by_id(id);
	return hits.load() == 900 ? 0 : 3;
}

static int refcount()
{
	{
		frida_attach_impl man;
		GumInterceptor *i = gum_interceptor_obtain();
		printf("refcount with one impl alive: %u\n",
		       (unsigned)G_OBJECT(i)->ref_count - 1);
		g_object_unref(i);
	}
	GumInterceptor *i = gum_interceptor_obtain();
	printf("refcount after the impl died (before this obtain): %u\n",
	       (unsigned)G_OBJECT(i)->ref_count - 1);
	g_object_unref(i);
	fflush(stdout);
	return 0;
}

int main(int argc, char **argv)
{
	int which = argc > 1 ? atoi(argv[1]) : 0;
	pid_t pid = fork();
	if (pid == 0) {
		int rc = which == 1 ? many_threads() :
			 which == 2 ? refcount() :
				      override_sessions();
		fflush(stdout);
		_exit(rc);
	}
	int status = 0;
	waitpid(pid, &status, 0);
	if (WIFEXITED(status))
		printf("child exited %d\n", WEXITSTATUS(status));
	else
		printf("child killed by signal %d (%s)\n", WTERMSIG(status),
		       strsignal(WTERMSIG(status)));
	return 0;
}
