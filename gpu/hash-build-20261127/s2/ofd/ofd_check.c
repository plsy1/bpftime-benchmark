// OFD lock check for D8/D11 liveness (RULES-ofd.md). CPU only.
//   ofd_check            items 1-6, 8, 9
//   sudo ofd_check 7     item 7 (pid namespace)
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define PATH "/dev/shm/bpftime-ofd-check"
#define B(i) ((off_t)1 << 40) + (i)

static int fails;
static void result(const char *item, int ok, const char *what)
{
	printf("%-4s %s  %s\n", item, ok ? "PASS" : "FAIL", what);
	fflush(stdout);
	if (!ok)
		fails++;
}

static int open_f(void)
{
	int fd = open(PATH, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		perror("open");
		exit(2);
	}
	return fd;
}
static int lk(int fd, off_t off, short type, int cmd)
{
	struct flock fl;
	memset(&fl, 0, sizeof fl);
	fl.l_type = type;
	fl.l_whence = SEEK_SET;
	fl.l_start = off;
	fl.l_len = 1;
	return fcntl(fd, cmd, &fl);
}
// held by someone (as seen through a fresh description of this process)
static int held(off_t off)
{
	int fd = open_f();
	struct flock fl;
	memset(&fl, 0, sizeof fl);
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = off;
	fl.l_len = 1;
	if (fcntl(fd, F_OFD_GETLK, &fl) != 0) {
		perror("getlk");
		exit(2);
	}
	close(fd);
	return fl.l_type != F_UNLCK;
}
// free: a fresh description can take it (and gives it back)
static int takeable(off_t off)
{
	int fd = open_f();
	int ok = lk(fd, off, F_WRLCK, F_OFD_SETLK) == 0;
	int err = errno;
	if (ok)
		lk(fd, off, F_UNLCK, F_OFD_SETLK);
	close(fd);
	if (!ok && err != EAGAIN && err != EACCES)
		printf("     (setlk errno %d)\n", err);
	return ok;
}
static int takeable_within(off_t off, int ms)
{
	for (int i = 0; i < ms; i++) {
		if (takeable(off))
			return 1;
		usleep(1000);
	}
	return 0;
}

static _Atomic uint64_t *counter;
static void *spin(void *a)
{
	(void)a;
	for (;;)
		atomic_fetch_add(counter, 1);
	return NULL;
}
static void start_threads(int n)
{
	for (int i = 0; i < n; i++) {
		pthread_t t;
		pthread_create(&t, NULL, spin, NULL);
	}
}
static void say(int fd, char c)
{
	if (write(fd, &c, 1) != 1)
		_exit(3);
}
static char hear(int fd)
{
	char c = 0;
	if (read(fd, &c, 1) != 1)
		return 0;
	return c;
}
static int comm_is(pid_t pid, const char *name)
{
	char p[64], b[64] = { 0 };
	snprintf(p, sizeof p, "/proc/%d/comm", pid);
	FILE *f = fopen(p, "r");
	if (!f)
		return 0;
	if (!fgets(b, sizeof b, f))
		b[0] = 0;
	fclose(f);
	b[strcspn(b, "\n")] = 0;
	return strcmp(b, name) == 0;
}
static void reap(pid_t pid)
{
	kill(pid, SIGKILL);
	waitpid(pid, NULL, 0);
}

static void item1(void)
{
	int p[2];
	pipe(p);
	pid_t h = fork();
	if (h == 0) {
		int fd = open_f();
		if (lk(fd, B(1), F_WRLCK, F_OFD_SETLK) != 0)
			_exit(4);
		say(p[1], 'L');
		pause();
		_exit(0);
	}
	hear(p[0]);
	int fd = open_f();
	int r = lk(fd, B(1), F_WRLCK, F_OFD_SETLK);
	int e = errno;
	close(fd);
	result("1a", r != 0 && (e == EAGAIN || e == EACCES), "another process's try fails while the holder lives");
	result("1b", held(B(1)), "F_OFD_GETLK reports the holder's lock");
	reap(h);
	result("1c", takeable_within(B(1), 1000), "free after the holder is killed");
}

static void item2(void)
{
	int fd1 = open_f(), fd2 = open_f();
	lk(fd1, B(2), F_WRLCK, F_OFD_SETLK);
	int r = lk(fd2, B(2), F_WRLCK, F_OFD_SETLK);
	result("2a", r != 0, "a second description of the same process conflicts");
	result("2b", held(B(2)), "GETLK through a third description reports it");
	close(fd1);
	close(fd2);
}

static void item3(void)
{
	int p[2];
	pipe(p);
	pid_t h = fork();
	if (h == 0) {
		int fd = open_f();
		if (lk(fd, B(3), F_WRLCK, F_OFD_SETLK) != 0)
			_exit(4);
		start_threads(4);
		say(p[1], 'L');
		pause();
		_exit(0);
	}
	hear(p[0]);
	usleep(50000);
	result("3a", held(B(3)), "held while four threads run");
	reap(h);
	result("3b", takeable_within(B(3), 1000), "free after SIGKILL with threads running");
}

static void item4(void)
{
	int p[2];
	pipe(p);
	pid_t h = fork();
	if (h == 0) {
		int fd = open_f();
		if (lk(fd, B(4), F_WRLCK, F_OFD_SETLK) != 0)
			_exit(4);
		start_threads(4);
		say(p[1], 'L');
		usleep(50000);
		execl("/bin/sleep", "sleep", "3", (char *)NULL);
		_exit(5);
	}
	hear(p[0]);
	int ok = 0;
	for (int i = 0; i < 1000 && !ok; i++) {
		ok = comm_is(h, "sleep");
		if (!ok)
			usleep(1000);
	}
	result("4a", ok, "the holder exec'd /bin/sleep");
	int free_now = takeable_within(B(4), 500);
	result("4b", free_now && kill(h, 0) == 0 && comm_is(h, "sleep"),
	       "free while the exec'd image runs (same pid)");
	reap(h);
}

static void item5(int child_closes)
{
	int p[2], q[2];
	pipe(p);
	pipe(q);
	const off_t b = B(child_closes ? 50 : 51);
	pid_t h = fork();
	if (h == 0) {
		int fd = open_f();
		if (lk(fd, b, F_WRLCK, F_OFD_SETLK) != 0)
			_exit(4);
		pid_t c = fork();
		if (c == 0) {
			if (child_closes)
				close(fd);
			say(q[1], 'C');
			sleep(5);
			_exit(0);
		}
		hear(q[0]);
		if (write(p[1], &c, sizeof c) != sizeof c)
			_exit(3);
		pause();
		_exit(0);
	}
	pid_t c = 0;
	if (read(p[0], &c, sizeof c) != sizeof c)
		exit(2);
	const char *tag = child_closes ? "5a" : "5b";
	char what[160];
	snprintf(what, sizeof what, "%s: held while the parent lives",
		 child_closes ? "child closed its copy" : "child kept its copy");
	result(child_closes ? "5a1" : "5b1", held(b), what);
	reap(h);
	usleep(20000);
	int child_alive = kill(c, 0) == 0;
	if (child_closes)
		result("5a2", child_alive && takeable_within(b, 1000),
		       "child closed its copy: free once the parent is gone, child alive");
	else
		result("5b2", child_alive && held(b),
		       "child kept its copy: still held after the parent is gone (a fork handler must close it)");
	kill(c, SIGKILL);
	usleep(20000);
	result(child_closes ? "5a3" : "5b3", takeable_within(b, 1000), "free once the child is gone too");
	(void)tag;
}

static void item6(void)
{
	int p[2];
	pipe(p);
	pid_t h = fork();
	if (h == 0) {
		int fd = open_f();
		if (lk(fd, B(6), F_WRLCK, F_OFD_SETLK) != 0)
			_exit(4);
		pid_t v = vfork();
		if (v == 0) {
			execl("/bin/true", "true", (char *)NULL);
			_exit(127);
		}
		int st;
		waitpid(v, &st, 0);
		say(p[1], WIFEXITED(st) && WEXITSTATUS(st) == 0 ? 'V' : 'X');
		pause();
		_exit(0);
	}
	char c = hear(p[0]);
	result("6", c == 'V' && held(B(6)), "held after a vfork child exec'd /bin/true");
	reap(h);
}

static void item7(void)
{
	int p[2];
	pipe(p);
	pid_t mid = fork();
	if (mid == 0) {
		if (unshare(CLONE_NEWPID) != 0) {
			perror("unshare");
			_exit(6);
		}
		pid_t h = fork(); // pid 1 of the new namespace
		if (h == 0) {
			int fd = open_f();
			if (lk(fd, B(7), F_WRLCK, F_OFD_SETLK) != 0)
				_exit(4);
			pid_t me = getpid();
			say(p[1], me == 1 ? '1' : 'n');
			pause();
			_exit(0);
		}
		if (write(p[1], &h, sizeof h) != sizeof h)
			_exit(3);
		waitpid(h, NULL, 0);
		_exit(0);
	}
	pid_t h = 0;
	if (read(p[0], &h, sizeof h) != sizeof h)
		exit(2);
	char c = hear(p[0]);
	result("7a", c == '1', "holder is pid 1 of its own namespace");
	result("7b", held(B(7)), "held as seen from the parent namespace");
	kill(h, SIGKILL);
	waitpid(mid, NULL, 0);
	result("7c", takeable_within(B(7), 1000), "free after the namespace's holder is killed");
}

static void item8(void)
{
	int p[2];
	pipe(p);
	pid_t h = fork();
	if (h == 0) {
		int fd = open_f();
		if (lk(fd, B(8), F_WRLCK, F_OFD_SETLK) != 0)
			_exit(4);
		say(p[1], 'L');
		close_range(3, ~0U, 0);
		sleep(3);
		_exit(0);
	}
	hear(p[0]);
	usleep(200000);
	result("8", kill(h, 0) == 0 && takeable_within(B(8), 500),
	       "close_range in the holder releases the lock (the limit to document)");
	reap(h);
}

static uint64_t ns(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint64_t)t.tv_sec * 1000000000ull + t.tv_nsec;
}
static int cmp(const void *a, const void *b)
{
	double x = *(const double *)a, y = *(const double *)b;
	return x < y ? -1 : x > y;
}
static void item9(void)
{
	const int N = 100000, R = 5;
	int p[2];
	pipe(p);
	pid_t h = fork();
	if (h == 0) {
		int fd = open_f();
		lk(fd, B(91), F_WRLCK, F_OFD_SETLK);
		say(p[1], 'L');
		pause();
		_exit(0);
	}
	hear(p[0]);
	int fd = open_f();
	double take[R], gfree[R], gheld[R];
	for (int r = 0; r < R; r++) {
		uint64_t t0 = ns();
		for (int i = 0; i < N; i++) {
			lk(fd, B(90), F_WRLCK, F_OFD_SETLK);
			lk(fd, B(90), F_UNLCK, F_OFD_SETLK);
		}
		take[r] = (double)(ns() - t0) / N / 2;
		struct flock fl;
		t0 = ns();
		for (int i = 0; i < N; i++) {
			memset(&fl, 0, sizeof fl);
			fl.l_type = F_WRLCK;
			fl.l_start = B(92);
			fl.l_len = 1;
			fcntl(fd, F_OFD_GETLK, &fl);
		}
		gfree[r] = (double)(ns() - t0) / N;
		t0 = ns();
		for (int i = 0; i < N; i++) {
			memset(&fl, 0, sizeof fl);
			fl.l_type = F_WRLCK;
			fl.l_start = B(91);
			fl.l_len = 1;
			fcntl(fd, F_OFD_GETLK, &fl);
		}
		gheld[r] = (double)(ns() - t0) / N;
	}
	qsort(take, R, sizeof(double), cmp);
	qsort(gfree, R, sizeof(double), cmp);
	qsort(gheld, R, sizeof(double), cmp);
	printf("9    COST setlk or unlock %.0f ns, getlk free %.0f ns, getlk held %.0f ns (medians of %d x %d)\n",
	       take[R / 2], gfree[R / 2], gheld[R / 2], R, N);
	close(fd);
	reap(h);
}

int main(int argc, char **argv)
{
	counter = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (argc > 1 && strcmp(argv[1], "7") == 0) {
		item7();
		printf("fails %d\n", fails);
		return fails != 0;
	}
	int fd = open(PATH, O_RDWR | O_CREAT | O_CLOEXEC, 0666);
	if (fd < 0 || ftruncate(fd, 4096) != 0) {
		perror("create");
		return 2;
	}
	fchmod(fd, 0666);
	close(fd);
	item1();
	item2();
	item3();
	item4();
	item5(1);
	item5(0);
	item6();
	item8();
	item9();
	printf("fails %d\n", fails);
	return fails != 0;
}
