// Standalone experiments against the aarch64 syscall rewriting (core lib).
#include "text_segment_transformer.hpp"
#include <atomic>
#include <cerrno>
#include <cfenv>
#include <csetjmp>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <dlfcn.h>

static std::atomic<uint64_t> calls[512];
static syscall_hooker_func_t next_hook;
static std::atomic<uint64_t> hook_sp{0};
static std::atomic<int> hook_mode{0};

static int64_t hook(int64_t nr, int64_t a1, int64_t a2, int64_t a3, int64_t a4,
		    int64_t a5, int64_t a6)
{
	if (nr >= 0 && nr < 512)
		calls[nr]++;
	uint64_t sp;
	__asm__ volatile("mov %0, sp" : "=r"(sp));
	hook_sp = sp;
	if (hook_mode == 1) // change the rounding mode and leave it
		fesetround(FE_TOWARDZERO);
	if (hook_mode == 3 && nr == SYS_getppid) // die inside the hook (in_hook = true)
		syscall(SYS_kill, syscall(SYS_getpid), SIGKILL);
	if (hook_mode == 2 && nr == SYS_getppid) { // busy wait inside the hook
		volatile uint64_t x = 0;
		for (uint64_t i = 0; i < 400000000ull; i++)
			x += i;
	}
	return next_hook(nr, a1, a2, a3, a4, a5, a6);
}

static void install()
{
	if (getenv("NO_INSTALL")) return;
	bpftime::setup_syscall_tracer();
	next_hook = bpftime::get_call_hook();
	bpftime::set_call_hook(&hook);
}

constexpr uint32_t SVC_0 = 0xd4000001, RET = 0xd65f03c0;
constexpr uint32_t mov_x8(uint32_t nr) { return 0xd2800008 | (nr << 5); }
using code_fn = long (*)();
static code_fn jit_getpid()
{
	void *page = mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	auto *c = (uint32_t *)page;
	c[0] = mov_x8(SYS_getpid); c[1] = SVC_0; c[2] = RET;
	__builtin___clear_cache((char *)c, (char *)(c + 3));
	mprotect(page, 4096, PROT_READ | PROT_EXEC);
	return (code_fn)page;
}

// 1: bytes pushed below the application's sp by one hooked call
static int exp_stack()
{
	install();
	uint64_t app_sp;
	// sp right before a raw svc (no libc frame in between)
	__asm__ volatile("mov %0, sp\n\tmov x8, %1\n\tsvc #0"
			 : "=r"(app_sp) : "r"((long)SYS_getpid) : "x8", "x0", "memory");
	printf("app sp %#lx, hook entry sp %#lx: %lu bytes below the app's sp at the C hook entry\n",
	       app_sp, hook_sp.load(), app_sp - hook_sp.load());
	return 0;
}

// 2: pthread_cancel of a thread blocked in a hooked read
static int exp_cancel()
{
	install();
	int fds[2];
	pipe(fds);
	pthread_t t;
	pthread_create(&t, nullptr, [](void *p) -> void * {
		char b;
		read(*(int *)p, &b, 1);
		return (void *)1;
	}, &fds[0]);
	usleep(200000);
	pthread_cancel(t);
	void *r = nullptr;
	pthread_join(t, &r);
	printf("joined: %s\n", r == PTHREAD_CANCELED ? "PTHREAD_CANCELED" : "other");
	return r == PTHREAD_CANCELED ? 0 : 3;
}

// 3: FPCR across the hook
static int exp_fpcr()
{
	install();
	fesetround(FE_TONEAREST);
	hook_mode = 1;
	syscall(SYS_getpid);
	hook_mode = 0;
	int r = fegetround();
	printf("rounding mode after hooked call: %s\n",
	       r == FE_TONEAREST ? "FE_TONEAREST (preserved)" : "CHANGED by the hook");
	return r == FE_TONEAREST ? 0 : 3;
}

// 4: siglongjmp out of a signal handler that interrupted the hook (in_hook=true)
static sigjmp_buf jb;
static int exp_longjmp()
{
	install();
	struct sigaction sa = {};
	sa.sa_handler = [](int) { siglongjmp(jb, 1); };
	sigemptyset(&sa.sa_mask);
	sigaction(SIGALRM, &sa, nullptr);
	hook_mode = 2;
	if (sigsetjmp(jb, 1) == 0) {
		alarm(1);
		syscall(SYS_getppid); // hook spins; SIGALRM lands inside the hook
		printf("not interrupted?\n");
		return 2;
	}
	hook_mode = 0;
	uint64_t before = calls[SYS_getpid];
	for (int i = 0; i < 10; i++)
		syscall(SYS_getpid);
	printf("after longjmp out of the hook: %lu of 10 getpid calls traced\n",
	       calls[SYS_getpid] - before);
	return calls[SYS_getpid] - before == 10 ? 0 : 3;
}

// 5: lazy mode; a thread that blocked all signals before lazy mode was enabled
static int exp_lazy_blocked_thread()
{
	setenv("BPFTIME_SYSCALL_LAZY", "1", 1);
	install();
	static std::atomic<int> go{0};
	static code_fn fn;
	pthread_t t;
	pthread_create(&t, nullptr, [](void *) -> void * {
		sigset_t all; sigfillset(&all);
		pthread_sigmask(SIG_BLOCK, &all, nullptr); // before lazy mode
		while (!go) {}
		long r = fn(); // first unrewritten svc of this thread
		printf("thread's trapped call returned %ld\n", r);
		return nullptr;
	}, nullptr);
	usleep(100000);
	if (!bpftime::enable_lazy_rewriting({}))
		return 2;
	fn = jit_getpid();
	go = 1;
	pthread_join(t, nullptr);
	printf("thread survived\n");
	return 0;
}

// 6: lazy mode; fork while another thread holds patch_owner
static int exp_lazy_fork_lock()
{
	setenv("BPFTIME_SYSCALL_LAZY", "1", 1);
	install();
	if (!bpftime::enable_lazy_rewriting({}))
		return 2;
	static std::atomic<bool> stop{false};
	pthread_t t;
	pthread_create(&t, nullptr, [](void *) -> void * {
		while (!stop) {
			code_fn f = jit_getpid(); // a fresh site: traps, takes the lock
			f();
			munmap((void *)f, 4096);
		}
		return nullptr;
	}, nullptr);
	int hung = 0, ok = 0;
	for (int i = 0; i < 20; i++) {
		code_fn f = jit_getpid();
		pid_t pid = fork();
		if (pid == 0) {
			alarm(1);
			f(); // trap in the child: patch_lock spins if inherited locked
			_exit(0);
		}
		int st = 0;
		waitpid(pid, &st, 0);
		if (WIFSIGNALED(st) && WTERMSIG(st) == SIGALRM) hung++;
		else if (WIFEXITED(st) && WEXITSTATUS(st) == 0) ok++;
		munmap((void *)f, 4096);
	}
	stop = true;
	pthread_join(t, nullptr);
	printf("fork children: %d ok, %d hung (killed by alarm)\n", ok, hung);
	return hung ? 3 : 0;
}

// 7: lazy mode with dumpable=0 (/proc/self/mem not writable)
static int exp_lazy_nodump()
{
	setenv("BPFTIME_SYSCALL_LAZY", "1", 1);
	install();
	if (!bpftime::enable_lazy_rewriting({}))
		return 2;
	prctl(PR_SET_DUMPABLE, 0);
	code_fn f = jit_getpid();
	uint64_t before = calls[SYS_getpid];
	for (int i = 0; i < 5; i++)
		if (f() != getpid()) return 3;
	auto s = bpftime::get_lazy_rewrite_stats();
	printf("dumpable=0: traps %lu rewritten %lu emulated %lu; traced %lu of 5 JIT calls\n",
	       s.traps, s.rewritten, s.emulated, calls[SYS_getpid] - before - 5);
	return 0;
}

// 8: hook on a sigaltstack of MINSIGSTKSZ... skip; 8: posix_spawn-like vfork child dying by signal inside hook
static int exp_vfork_signal()
{
	install();
	// vfork child: enters the hook (in_hook = true in the shared TLS) and is killed there
	hook_mode = 3;
	pid_t pid = vfork();
	if (pid == 0) {
		syscall(SYS_getppid);
		_exit(0);
	}
	int st; waitpid(pid, &st, 0);
	hook_mode = 0;
	uint64_t before = calls[SYS_getpid];
	for (int i = 0; i < 10; i++) syscall(SYS_getpid);
	printf("child status %#x; parent afterwards: %lu of 10 getpid traced\n", st, calls[SYS_getpid] - before);
	return 0;
}


// 9: pthread_cancel of a thread blocked in a hooked read: are C++ destructors run?
static std::atomic<int> destroyed{0};
struct Guard { ~Guard() { destroyed = 1; } };
static int exp_cancel_dtor()
{
	install();
	int fds[2];
	if (pipe(fds)) return 2;
	pthread_t t;
	pthread_create(&t, nullptr, [](void *p) -> void * {
		Guard g;
		char b;
		read(*(int *)p, &b, 1);
		return (void *)1;
	}, &fds[0]);
	usleep(200000);
	pthread_cancel(t);
	void *r = nullptr;
	pthread_join(t, &r);
	printf("cancelled thread: result %s, destructor %s\n",
	       r == PTHREAD_CANCELED ? "PTHREAD_CANCELED" : "other", destroyed ? "ran" : "SKIPPED");
	return destroyed ? 0 : 3;
}

// 10: stack high-water mark of one call on a private stack: hook path vs lazy trap path
static long run_on_stack(code_fn f, void *top)
{
	long r;
	__asm__ volatile("mov x19, sp\n\tmov sp, %1\n\tblr %2\n\tmov sp, x19\n\tmov %0, x0"
			 : "=r"(r) : "r"(top), "r"(f)
			 : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10",
			   "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "x19", "x30", "memory", "cc");
	return r;
}
static int exp_stack_mark(bool lazy)
{
	if (lazy) setenv("BPFTIME_SYSCALL_LAZY", "1", 1);
	install();
	if (lazy && !bpftime::enable_lazy_rewriting({})) return 2;
	const size_t size = 64 * 1024;
	char *stack = (char *)mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	memset(stack, 0xa5, size);
	code_fn f = jit_getpid();
	if (!lazy) {
		// rewrite this site through the normal path (scan_anonymous would do it; do it by hand: lazy off)
		// Not rewritten at all without anon scanning: call glibc's syscall() instead, whose svc is rewritten.
		f = []() -> long { return syscall(SYS_getpid); };
	}
	long r = run_on_stack(f, stack + size);
	size_t low = 0;
	while (low < size && (unsigned char)stack[low] == 0xa5) low++;
	printf("%s path: result %ld (pid %d); bytes of stack used below the entry sp: %zu\n",
	       lazy ? "lazy trap (SIGSYS handler on the thread's stack)" : "hook", r, getpid(), size - low);
	return 0;
}

// 13: a library whose file was deleted/replaced on disk before the rewriting
static int exp_deleted_lib(const char *path)
{
	char tmp[] = "/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/fable-syscall/libcopyXXXXXX";
	int fd = mkstemp(tmp);
	int src = open(path, O_RDONLY);
	char buf[65536]; ssize_t n;
	while ((n = read(src, buf, sizeof buf)) > 0) write(fd, buf, n);
	close(src); close(fd);
	void *lib = dlopen(tmp, RTLD_NOW);
	if (!lib) { printf("dlopen: %s\n", dlerror()); return 2; }
	unlink(tmp); // as a package upgrade does
	auto raw_getppid = (long (*)())dlsym(lib, "bpftime_test_raw_getppid");
	install();
	uint64_t before = calls[SYS_getppid];
	raw_getppid();
	printf("svc in a library whose file is '(deleted)': %s\n",
	       calls[SYS_getppid] == before + 1 ? "hooked" : "NOT hooked (silently untraced)");
	return 0;
}

// 17: svc issued with a misaligned sp
static int exp_misaligned_sp()
{
	install();
	long r;
	__asm__ volatile("sub sp, sp, #8\n\tmov x8, %1\n\tsvc #0\n\tadd sp, sp, #8\n\tmov %0, x0"
			 : "=r"(r) : "r"((long)SYS_getpid) : "x0", "x8", "memory");
	printf("svc with sp misaligned by 8: returned %ld\n", r);
	return 0;
}

int main(int argc, char **argv)
{
	int which = argc > 1 ? atoi(argv[1]) : 0;
	alarm(60);
	switch (which) {
	case 1: return exp_stack();
	case 2: return exp_cancel();
	case 3: return exp_fpcr();
	case 4: return exp_longjmp();
	case 5: return exp_lazy_blocked_thread();
	case 6: return exp_lazy_fork_lock();
	case 7: return exp_lazy_nodump();
	case 8: return exp_vfork_signal();
	case 9: return exp_cancel_dtor();
	case 10: return exp_stack_mark(false);
	case 11: return exp_stack_mark(true);
	case 13: return exp_deleted_lib(argv[2]);
	case 17: return exp_misaligned_sp();
	}
	return 1;
}
