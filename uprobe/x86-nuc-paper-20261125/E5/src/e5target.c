// E5 target: identity-helper differential audit.
// At each audit point it records ground truth T1 by raw system calls, calls audit_point_k(seq)
// (kernel uprobe) and audit_point_u(seq) (bpftime uprobe), then records truth T2. Records live
// in a MAP_SHARED array so fork/_Fork/vfork/clone children write into it too; the main process
// writes them to the truth file at the end. The runner starts one target process per scenario.
// seq = scenario_id << 40 | iteration << 4 | point (p0 before, p1 child/changed, p2 after)
// Usage: e5target --scenario S [--scenario S2 ...] --iters N --truth FILE [--churn-seconds S]
//   (--scenarios A,B is also accepted)
// Truth file: "# ..." meta lines, then per record
//   scen seq point  t1pid t1tid t1uid t1gid t1comm_hex  t2pid t2tid t2uid t2gid t2comm_hex
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/futex.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define NOINL __attribute__((noinline, noclone, used))
// Each audit function on its own page, with a body long enough for any inline hook. noipa keeps
// gcc from folding the two identical bodies or propagating anything across the call.
#define AUDIT_FN(name)                                                       \
	__attribute__((noinline, noipa, used, aligned(4096))) void name(uint64_t seq) \
	{                                                                     \
		asm volatile(".rept 48\n\tnop\n\t.endr" : : "r"(seq) : "memory"); \
	}
AUDIT_FN(audit_point_k)
AUDIT_FN(audit_point_u)

struct id {
	int32_t pid, tid, uid, gid;
	char comm[16];
};
struct rec {
	uint64_t seq;
	struct id t1, t2;
};
struct shared {
	_Atomic uint64_t n;
	uint64_t cap;
	struct rec recs[];
};
static struct shared *sh;

// Index = scenario id in seq. Keep in sync with run_e5.py SCEN_POINTS.
static const char *const SCEN[] = { "",   "W1",  "W2",  "W3",  "W3c", "W7g", "W7r",  "W8a",
				    "W8b", "W8c", "W12", "W5",  "W6",  "W8d", "W6t", "W6tc" };
enum { NSCEN = sizeof(SCEN) / sizeof(SCEN[0]) };

static uint64_t seqof(int scen, uint64_t iter, int point)
{
	return ((uint64_t)scen << 40) | (iter << 4) | (uint64_t)point;
}

static void truth(struct id *t)
{
	t->pid = (int32_t)syscall(SYS_getpid);
	t->tid = (int32_t)syscall(SYS_gettid);
	t->uid = (int32_t)syscall(SYS_getuid);
	t->gid = (int32_t)syscall(SYS_getgid);
	memset(t->comm, 0, sizeof(t->comm));
	prctl(PR_GET_NAME, t->comm, 0, 0, 0);
}

// Async-signal-safe and vfork-child-safe: no locks, no allocation, no stdio.
NOINL static void audit(int scen, uint64_t iter, int point)
{
	uint64_t i = atomic_fetch_add(&sh->n, 1);
	uint64_t seq = seqof(scen, iter, point);
	if (i >= sh->cap) {
		audit_point_k(seq);
		audit_point_u(seq);
		return;
	}
	struct rec *r = &sh->recs[i];
	r->seq = seq;
	truth(&r->t1);
	audit_point_k(seq);
	audit_point_u(seq);
	truth(&r->t2);
}

// Meta lines go to a MAP_SHARED text buffer so fork children (W7) can add to it too.
#define META_CAP (1 << 20)
struct metabuf {
	_Atomic uint64_t len;
	char text[META_CAP];
};
static struct metabuf *mb;
static void meta(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void meta(const char *fmt, ...)
{
	char line[512];
	va_list ap;
	va_start(ap, fmt);
	int k = snprintf(line, sizeof(line), "# ");
	k += vsnprintf(line + k, sizeof(line) - k - 1, fmt, ap);
	va_end(ap);
	if (k > (int)sizeof(line) - 2)
		k = sizeof(line) - 2;
	line[k++] = '\n';
	uint64_t off = atomic_fetch_add(&mb->len, (uint64_t)k);
	if (off + k <= META_CAP)
		memcpy(mb->text + off, line, k);
}

static int threads_now(void)
{
	FILE *f = fopen("/proc/self/status", "r");
	char line[256];
	int n = -1;
	while (f && fgets(line, sizeof(line), f))
		if (sscanf(line, "Threads: %d", &n) == 1)
			break;
	if (f)
		fclose(f);
	return n;
}

static double now_s(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void reap(pid_t pid, const char *what)
{
	int st = 0;
	if (pid < 0) {
		meta("error %s: %s", what, strerror(errno));
		return;
	}
	if (waitpid(pid, &st, 0) != pid || !WIFEXITED(st) || WEXITSTATUS(st) != 0)
		meta("error %s child %d status 0x%x", what, pid, st);
}

// ---- W1 fork, W2 _Fork ----
static void w_fork(int scen, uint64_t n, int use_Fork)
{
	for (uint64_t i = 0; i < n; i++) {
		audit(scen, i, 0);
		pid_t pid = use_Fork ? _Fork() : fork();
		if (pid == 0) {
			audit(scen, i, 1);
			_exit(0);
		}
		reap(pid, use_Fork ? "_Fork" : "fork");
		audit(scen, i, 2);
	}
}

// ---- W3 vfork (warm: the parent thread called the helpers just before) ----
NOINL static pid_t vfork_child_audits(int scen, uint64_t i)
{
	pid_t pid = vfork();
	if (pid == 0) {
		audit(scen, i, 1);
		_exit(0);
	}
	return pid;
}

static void w3(int scen, uint64_t n)
{
	for (uint64_t i = 0; i < n; i++) {
		audit(scen, i, 0);
		reap(vfork_child_audits(scen, i), "vfork");
		audit(scen, i, 2);
	}
}

// ---- run fn(arg) on a fresh pthread that has not called any helper yet (cold caches) ----
struct cold_arg {
	int scen;
	uint64_t i;
	void (*fn)(int, uint64_t);
};
static void *cold_thread(void *p)
{
	struct cold_arg *a = p;
	a->fn(a->scen, a->i);
	return NULL;
}
static void on_cold_threads(int scen, uint64_t n, void (*fn)(int, uint64_t))
{
	for (uint64_t i = 0; i < n; i++) {
		struct cold_arg a = { scen, i, fn };
		pthread_t t;
		if (pthread_create(&t, NULL, cold_thread, &a) != 0) {
			meta("error pthread_create");
			return;
		}
		pthread_join(t, NULL);
	}
}

// ---- W3c vfork from a fresh thread ----
static void w3c_one(int scen, uint64_t i)
{
	reap(vfork_child_audits(scen, i), "vfork(cold)");
	audit(scen, i, 2);
}

// ---- W7 credentials: glibc set*id vs raw syscalls, in a single-threaded fork child ----
#define TEST_UID 4242
#define TEST_GID 4343
static int setres(int raw, int uid, unsigned a, unsigned b, unsigned c)
{
	if (raw)
		return (int)syscall(uid ? SYS_setresuid : SYS_setresgid, a, b, c);
	return uid ? setresuid(a, b, c) : setresgid(a, b, c);
}
static int restore_root(int raw)
{
	int r1 = setres(raw, 1, 0, 0, 0); // uid first: needs saved uid 0
	int r2 = setres(raw, 0, 0, 0, 0);
	return r1 || r2;
}
static void w7_child(int scen, uint64_t n, int raw)
{
	int thr = threads_now();
	meta("%s child threads_at_start %d", SCEN[scen], thr);
	for (uint64_t i = 0; i < n; i++) {
		audit(scen, i, 0);
		if (setres(raw, 0, TEST_GID, TEST_GID, 0) || setres(raw, 1, TEST_UID, TEST_UID, 0)) {
			meta("error %s drop: %s", SCEN[scen], strerror(errno));
			if (restore_root(raw))
				meta("error %s restore after failed drop: %s", SCEN[scen], strerror(errno));
			break;
		}
		audit(scen, i, 1);
		if (restore_root(raw)) {
			meta("error %s restore: %s", SCEN[scen], strerror(errno));
			break;
		}
		audit(scen, i, 2);
	}
	meta("%s child threads_at_end %d uid %ld gid %ld", SCEN[scen], threads_now(),
	     syscall(SYS_getuid), syscall(SYS_getgid));
}
static void w7(int scen, uint64_t n, int raw)
{
	if (syscall(SYS_geteuid) != 0) {
		meta("skipped %s: not root", SCEN[scen]);
		return;
	}
	meta("%s parent threads %d (iterations run in a fork child)", SCEN[scen], threads_now());
	fflush(NULL);
	pid_t pid = fork();
	if (pid == 0) {
		w7_child(scen, n, raw);
		_exit(0);
	}
	reap(pid, "w7 child");
}

// ---- W8 comm changes. Base name differs from the exe basename ("e5target") so that "exe
// basename" and "comm cached at first call" can be told apart; p2 follows the restore. ----
static const char *base_name = "w8base";
static void w8a(int scen, uint64_t n)
{
	char name[16];
	prctl(PR_SET_NAME, base_name, 0, 0, 0);
	for (uint64_t i = 0; i < n; i++) {
		audit(scen, i, 0);
		snprintf(name, sizeof(name), "w8a-%llu", (unsigned long long)i);
		prctl(PR_SET_NAME, name, 0, 0, 0);
		audit(scen, i, 1);
		prctl(PR_SET_NAME, base_name, 0, 0, 0);
		audit(scen, i, 2);
	}
}

static int set_comm_proc(pid_t tid, const char *name)
{
	char path[64];
	snprintf(path, sizeof(path), "/proc/self/task/%d/comm", tid);
	int fd = open(path, O_WRONLY);
	if (fd < 0)
		return -1;
	ssize_t r = write(fd, name, strlen(name));
	close(fd);
	return r == (ssize_t)strlen(name) ? 0 : -1;
}

// W8c: a thread renames itself by writing /proc/self/task/<own tid>/comm (procfs, not prctl).
struct w8c_arg {
	int scen;
	uint64_t n;
};
static void *w8c_fn(void *p)
{
	struct w8c_arg *a = p;
	uint64_t n = a->n;
	int scen = a->scen;
	pid_t tid = (pid_t)syscall(SYS_gettid);
	char name[16];
	if (set_comm_proc(tid, base_name)) {
		meta("error W8c base rename: %s", strerror(errno));
		return NULL;
	}
	for (uint64_t i = 0; i < n; i++) {
		audit(scen, i, 0);
		snprintf(name, sizeof(name), "w8c-%llu", (unsigned long long)i);
		if (set_comm_proc(tid, name)) {
			meta("error W8c rename: %s", strerror(errno));
			break;
		}
		audit(scen, i, 1);
		set_comm_proc(tid, base_name);
		audit(scen, i, 2);
	}
	return NULL;
}
static void w8c(int scen, uint64_t n)
{
	struct w8c_arg a = { scen, n };
	pthread_t t;
	if (pthread_create(&t, NULL, w8c_fn, &a) != 0) {
		meta("error pthread_create");
		return;
	}
	pthread_join(t, NULL);
}

// W8b / W8d: the main thread renames a worker (W8b pthread_setname_np, W8d direct write of
// /proc/self/task/<worker tid>/comm; glibc implements the former with the latter).
struct w8_worker {
	sem_t go, done;
	int point, quit, scen;
	uint64_t iter;
	pid_t tid;
};
static void *w8_worker_fn(void *p)
{
	struct w8_worker *w = p;
	w->tid = (pid_t)syscall(SYS_gettid);
	sem_post(&w->done);
	for (;;) {
		sem_wait(&w->go);
		if (w->quit)
			break;
		audit(w->scen, w->iter, w->point);
		sem_post(&w->done);
	}
	return NULL;
}
static int rename_worker(pthread_t t, pid_t tid, int via_proc, const char *name)
{
	if (via_proc)
		return set_comm_proc(tid, name) ? errno : 0;
	return pthread_setname_np(t, name);
}
static void w8_step(struct w8_worker *w, uint64_t i, int point)
{
	w->iter = i;
	w->point = point;
	sem_post(&w->go);
	sem_wait(&w->done);
}
static void w8bd(int scen, uint64_t n, int via_proc)
{
	struct w8_worker w = { .scen = scen };
	sem_init(&w.go, 0, 0);
	sem_init(&w.done, 0, 0);
	pthread_t t;
	if (pthread_create(&t, NULL, w8_worker_fn, &w) != 0) {
		meta("error pthread_create");
		return;
	}
	sem_wait(&w.done);
	int rc = rename_worker(t, w.tid, via_proc, base_name);
	if (rc)
		meta("error %s base rename: %s", SCEN[scen], strerror(rc));
	char name[16];
	for (uint64_t i = 0; i < n && !rc; i++) {
		w8_step(&w, i, 0);
		snprintf(name, sizeof(name), "%s-%llu", via_proc ? "w8d" : "w8b", (unsigned long long)i);
		rc = rename_worker(t, w.tid, via_proc, name);
		if (rc) {
			meta("error %s rename: %s", SCEN[scen], strerror(rc));
			break;
		}
		w8_step(&w, i, 1);
		rename_worker(t, w.tid, via_proc, base_name);
		w8_step(&w, i, 2);
	}
	w.quit = 1;
	sem_post(&w.go);
	pthread_join(t, NULL);
}

// ---- raw thread-like clone helpers (glibc clone(); the child runs fn on its own stack and exits
// with SYS_exit; with CLONE_CHILD_CLEARTID the kernel clears *ctid and wakes it on exit) ----
#define RAW_THREAD_FLAGS                                                                    \
	(CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM | \
	 CLONE_CHILD_CLEARTID)
static void wait_cleartid(_Atomic pid_t *ctid)
{
	pid_t v;
	while ((v = atomic_load(ctid)) != 0)
		syscall(SYS_futex, ctid, FUTEX_WAIT, v, NULL, NULL, 0);
}
static pid_t raw_thread(int (*fn)(void *), void *arg, char *stack_top, _Atomic pid_t *ctid)
{
	atomic_store(ctid, 1);
	pid_t t = clone(fn, stack_top, RAW_THREAD_FLAGS, arg, NULL, NULL, ctid);
	if (t < 0) {
		atomic_store(ctid, 0);
		return -1;
	}
	wait_cleartid(ctid);
	return t;
}

// ---- W12 thread churn with tid reuse ----
// Generation 1: N pthreads created and joined one after another (consecutive tids T0..), each
// audits p0. Then trivial raw threads are spawned (several spawner threads in parallel, then one
// serially) until the kernel's cyclic pid allocator has wrapped around pid_max and stands just
// before T0. Generation 2: N pthreads created and joined one after another, each audits p1; with
// no foreign pid allocation in between, gen-2 thread i gets gen-1 thread i's tid, on the same
// glibc-cached stack. Pids are taken from the whole system; reuse is checked, not assumed.
static uint64_t churn_seconds = 150;
static int churn_fn(void *p)
{
	(void)p;
	return 0;
}
struct churner {
	pthread_t th;
	char *stack;
	_Atomic pid_t ctid;
	uint64_t spawned;
};
static _Atomic int churn_stop, churn_wrapped, churn_overshoot;
static pid_t churn_t0;
static double churn_deadline;
#define CHURN_STACK 16384
#define CHURN_MARGIN 256
static void *churner_fn(void *p)
{
	struct churner *c = p;
	// The allocation cursor started above t0, so t < t0 means it has wrapped. Overshoot is judged
	// on this churner's own sequence (its tids only decrease at the wrap): a global flag could
	// be set by another churner while this one still holds a pre-wrap tid above t0.
	int wrapped_here = 0;
	pid_t prev = 0;
	while (!atomic_load(&churn_stop)) {
		pid_t t = raw_thread(churn_fn, NULL, c->stack + CHURN_STACK, &c->ctid);
		if (t < 0)
			continue;
		c->spawned++;
		if (prev && t < prev)
			wrapped_here = 1;
		prev = t;
		if (t < churn_t0) {
			atomic_store(&churn_wrapped, 1);
			if (t >= churn_t0 - CHURN_MARGIN)
				atomic_store(&churn_stop, 1);
		} else if (wrapped_here) {
			atomic_store(&churn_overshoot, 1);
			atomic_store(&churn_stop, 1);
		}
		if ((c->spawned & 1023) == 0 && now_s() > churn_deadline)
			atomic_store(&churn_stop, 1);
	}
	return NULL;
}
static int tid_in_use(pid_t t)
{
	char path[64];
	struct stat st;
	snprintf(path, sizeof(path), "/proc/%d", t);
	return stat(path, &st) == 0;
}
// Returns 1 if the next pid allocation is expected to be t0.
static int churn_to(pid_t t0, uint64_t *spawned_out, double *secs_out)
{
	enum { NCH = 6 };
	struct churner ch[NCH];
	double t_start = now_s();
	churn_t0 = t0;
	churn_deadline = t_start + (double)churn_seconds;
	atomic_store(&churn_stop, 0);
	atomic_store(&churn_wrapped, 0);
	atomic_store(&churn_overshoot, 0);
	char *stacks = mmap(NULL, NCH * CHURN_STACK, PROT_READ | PROT_WRITE,
			    MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
	if (stacks == MAP_FAILED)
		return 0;
	int started = 0;
	for (int k = 0; k < NCH; k++) {
		ch[k].stack = stacks + k * CHURN_STACK;
		ch[k].spawned = 0;
		atomic_store(&ch[k].ctid, 0);
		if (pthread_create(&ch[k].th, NULL, churner_fn, &ch[k]) != 0)
			break;
		started++;
	}
	uint64_t spawned = 0;
	for (int k = 0; k < started; k++) {
		pthread_join(ch[k].th, NULL);
		spawned += ch[k].spawned;
	}
	int ok = 0;
	if (atomic_load(&churn_wrapped) && !atomic_load(&churn_overshoot)) {
		// Serial approach: stop once every pid between the last one and t0 is in use.
		_Atomic pid_t ctid = 0;
		for (;;) {
			pid_t t = raw_thread(churn_fn, NULL, stacks + CHURN_STACK, &ctid);
			if (t < 0)
				continue;
			spawned++;
			if (t >= t0)
				break; // overshoot (t0 itself was taken)
			pid_t g = t + 1;
			while (g < t0 && tid_in_use(g))
				g++;
			if (g == t0) {
				ok = 1;
				break;
			}
			if (now_s() > churn_deadline + 5)
				break;
		}
	}
	munmap(stacks, NCH * CHURN_STACK);
	*spawned_out = spawned;
	*secs_out = now_s() - t_start;
	return ok;
}
struct w12_arg {
	int scen, point;
	uint64_t i;
	pid_t tid;
};
static void *w12_fn(void *p)
{
	struct w12_arg *a = p;
	a->tid = (pid_t)syscall(SYS_gettid);
	audit(a->scen, a->i, a->point);
	return NULL;
}
static int w12_gen(int scen, uint64_t n, int point, pid_t *tids)
{
	for (uint64_t i = 0; i < n; i++) {
		struct w12_arg a = { scen, point, i, -1 };
		pthread_t t;
		if (pthread_create(&t, NULL, w12_fn, &a) != 0) {
			meta("error pthread_create");
			return -1;
		}
		pthread_join(t, NULL);
		tids[i] = a.tid;
	}
	return 0;
}
static void w12(int scen, uint64_t n)
{
	pid_t *g1 = calloc(n, sizeof(pid_t)), *g2 = calloc(n, sizeof(pid_t));
	if (!g1 || !g2 || w12_gen(scen, n, 0, g1)) {
		meta("error W12 gen1");
		return;
	}
	uint64_t consecutive = 0;
	for (uint64_t i = 1; i < n; i++)
		consecutive += g1[i] == g1[i - 1] + 1;
	uint64_t spawned = 0;
	double secs = 0;
	int ready = churn_to(g1[0], &spawned, &secs);
	meta("W12 churn ready %d wrapped %d overshoot %d spawned %llu seconds %.1f t0 %d", ready,
	     atomic_load(&churn_wrapped), atomic_load(&churn_overshoot),
	     (unsigned long long)spawned, secs, g1[0]);
	if (w12_gen(scen, n, 1, g2)) {
		meta("error W12 gen2");
		return;
	}
	uint64_t reused = 0;
	for (uint64_t i = 0; i < n; i++)
		reused += g1[i] == g2[i];
	meta("W12 tid_reused %llu/%llu gen1_consecutive %llu/%llu gen1 %d..%d gen2 %d..%d",
	     (unsigned long long)reused, (unsigned long long)n, (unsigned long long)consecutive,
	     (unsigned long long)(n ? n - 1 : 0), g1[0], g1[n - 1], g2[0], g2[n - 1]);
	free(g1);
	free(g2);
}

// ---- W5 glibc clone(SIGCHLD) (no CLONE_VM: a fork without atfork handlers, like W2);
// W6 clone(CLONE_VM|CLONE_VFORK|SIGCHLD) on its own stack (posix_spawn-like, like W3) ----
struct clone_arg {
	int scen;
	uint64_t i;
};
static int clone_child(void *p)
{
	struct clone_arg *a = p;
	audit(a->scen, a->i, 1);
	return 0;
}
static void w_clone(int scen, uint64_t n, int vm)
{
	size_t sz = 256 * 1024;
	char *stack = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
	if (stack == MAP_FAILED) {
		meta("error mmap stack");
		return;
	}
	for (uint64_t i = 0; i < n; i++) {
		struct clone_arg a = { scen, i };
		audit(scen, i, 0);
		int flags = SIGCHLD | (vm ? CLONE_VM | CLONE_VFORK : 0);
		pid_t pid = clone(clone_child, stack + sz, flags, &a);
		reap(pid, vm ? "clone(VM|VFORK)" : "clone(SIGCHLD)");
		audit(scen, i, 2);
	}
	munmap(stack, sz);
}

// ---- W6t / W6tc: thread-like raw clone (CLONE_VM|CLONE_THREAD|CLONE_SIGHAND|...) WITHOUT
// CLONE_SETTLS: the live child shares the parent thread's TLS (fs base) while the parent waits.
// W6t: warm (the parent thread audited p0 just before); W6tc: cold (from a fresh pthread). ----
static char *w6t_stack;
#define W6T_STACK (256 * 1024)
static void w6t_one(int scen, uint64_t i, int warm)
{
	struct clone_arg a = { scen, i };
	_Atomic pid_t ctid = 0;
	if (warm)
		audit(scen, i, 0);
	if (raw_thread(clone_child, &a, w6t_stack + W6T_STACK, &ctid) < 0)
		meta("error raw thread clone: %s", strerror(errno));
	audit(scen, i, 2);
}
static void w6tc_one(int scen, uint64_t i)
{
	w6t_one(scen, i, 0);
}
static void w6t(int scen, uint64_t n, int warm)
{
	w6t_stack = mmap(NULL, W6T_STACK, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
	if (w6t_stack == MAP_FAILED) {
		meta("error mmap stack");
		return;
	}
	if (warm)
		for (uint64_t i = 0; i < n; i++)
			w6t_one(scen, i, 1);
	else
		on_cold_threads(scen, n, w6tc_one);
	munmap(w6t_stack, W6T_STACK);
}

static int scen_id(const char *s)
{
	for (int i = 1; i < NSCEN; i++)
		if (!strcmp(SCEN[i], s))
			return i;
	return -1;
}

static int usage(void)
{
	fprintf(stderr, "usage: e5target --scenario S [--scenario S2 ...] --iters N --truth FILE "
			"[--churn-seconds S]\n");
	return 2;
}

int main(int argc, char **argv)
{
	const char *truth_path = NULL;
	uint64_t iters = 0;
	int ids[64], nids = 0;
	char names[1024] = "";
	for (int i = 1; i < argc; i += 2) {
		if (i + 1 >= argc)
			return usage();
		const char *k = argv[i], *v = argv[i + 1];
		if (!strcmp(k, "--scenario") || !strcmp(k, "--scenarios")) {
			char *list = strdup(v);
			for (char *s = strtok(list, ","); s; s = strtok(NULL, ",")) {
				int id = scen_id(s);
				if (id < 0 || nids >= 64) {
					fprintf(stderr, "unknown scenario %s\n", s);
					return 2;
				}
				ids[nids++] = id;
				if (names[0])
					strncat(names, ",", sizeof(names) - strlen(names) - 1);
				strncat(names, s, sizeof(names) - strlen(names) - 1);
			}
			free(list);
		} else if (!strcmp(k, "--iters")) {
			char *end;
			iters = strtoull(v, &end, 10);
			if (*end)
				return usage();
		} else if (!strcmp(k, "--truth")) {
			truth_path = v;
		} else if (!strcmp(k, "--churn-seconds")) {
			churn_seconds = strtoull(v, NULL, 10);
		} else {
			fprintf(stderr, "unknown argument %s\n", k);
			return usage();
		}
	}
	// iteration occupies bits 4..39 of seq
	if (!truth_path || !nids || !iters || iters >= (1ull << 36))
		return usage();
	mb = mmap(NULL, sizeof(*mb), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (mb == MAP_FAILED) {
		perror("mmap");
		return 1;
	}
	uint64_t cap = (uint64_t)nids * (3 * iters + 16);
	size_t bytes = sizeof(struct shared) + cap * sizeof(struct rec);
	sh = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (sh == MAP_FAILED) {
		perror("mmap");
		return 1;
	}
	sh->cap = cap;
	char comm[16] = "";
	prctl(PR_GET_NAME, comm, 0, 0, 0);
	meta("pid %d iters %llu scenarios %s threads %d comm %s", getpid(), (unsigned long long)iters,
	     names, threads_now(), comm);
	for (int k = 0; k < nids; k++) {
		int id = ids[k];
		uint64_t before = atomic_load(&sh->n);
		switch (id) {
		case 1: w_fork(id, iters, 0); break;
		case 2: w_fork(id, iters, 1); break;
		case 3: w3(id, iters); break;
		case 4: on_cold_threads(id, iters, w3c_one); break;
		case 5: w7(id, iters, 0); break;
		case 6: w7(id, iters, 1); break;
		case 7: w8a(id, iters); break;
		case 8: w8bd(id, iters, 0); break;
		case 9: w8c(id, iters); break;
		case 10: w12(id, iters); break;
		case 11: w_clone(id, iters, 0); break;
		case 12: w_clone(id, iters, 1); break;
		case 13: w8bd(id, iters, 1); break;
		case 14: w6t(id, iters, 1); break;
		case 15: w6t(id, iters, 0); break;
		}
		meta("done %s records %llu", SCEN[id], (unsigned long long)(atomic_load(&sh->n) - before));
	}
	uint64_t n = atomic_load(&sh->n);
	if (n > cap)
		meta("error record overflow %llu > %llu", (unsigned long long)n, (unsigned long long)cap);
	FILE *out = fopen(truth_path, "w");
	if (!out) {
		perror(truth_path);
		return 1;
	}
	uint64_t ml = atomic_load(&mb->len);
	fwrite(mb->text, 1, ml < META_CAP ? ml : META_CAP, out);
	if (ml > META_CAP)
		fputs("# error meta overflow\n", out);
	for (uint64_t i = 0; i < n && i < cap; i++) {
		struct rec *r = &sh->recs[i];
		int sc = (int)(r->seq >> 40);
		fprintf(out, "%s %llu %d", sc > 0 && sc < NSCEN ? SCEN[sc] : "?",
			(unsigned long long)r->seq, (int)(r->seq & 15));
		struct id *t[2] = { &r->t1, &r->t2 };
		for (int k = 0; k < 2; k++) {
			fprintf(out, " %d %d %d %d ", t[k]->pid, t[k]->tid, t[k]->uid, t[k]->gid);
			for (int j = 0; j < 16; j++)
				fprintf(out, "%02x", (unsigned char)t[k]->comm[j]);
		}
		fputc('\n', out);
	}
	fclose(out);
	return 0;
}
