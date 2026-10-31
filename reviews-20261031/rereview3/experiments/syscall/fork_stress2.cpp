// Default mode: forks from several threads and from a signal handler while another thread dlopens (rescans).
// env: FS_NATIVE=1 (no rewriting), FS_HANDLER=0 (no handler forks), FS_CHILD_DLOPEN=0 (children just exit)
#include "text_segment_transformer.hpp"
#include <atomic>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
static const char *MODULE = "/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-jetson-daily/attach/text_segment_transformer/libbpftime_test_raw_svc_module.so";
static std::atomic<bool> stop{ false };
static std::atomic<long> forks{ 0 }, handler_forks{ 0 }, dlopens{ 0 }, bad{ 0 }, joined{ 0 };
static bool child_dlopen = true;
static std::atomic<pid_t> pending[3];
static int module_fd = -1;
static void child_work() {
	// a rescan in the child without the loader lock (inherited locked if the fork hit a dlopen): an executable file mapping
	if (child_dlopen) { void *m = mmap(nullptr, 4096, PROT_READ | PROT_EXEC, MAP_PRIVATE, module_fd, 0); if (m == MAP_FAILED) _exit(3); }
	_exit(0);
}
static void do_fork(std::atomic<long> &ctr, int slot) {
	pid_t p = fork();
	if (p == 0) child_work();
	if (p < 0) { bad++; return; }
	pending[slot] = p;
	int st = 0;
	if (waitpid(p, &st, 0) != p || !WIFEXITED(st) || WEXITSTATUS(st) != 0) bad++;
	pending[slot] = 0;
	ctr++;
}
static void on_usr1(int) { do_fork(handler_forks, 2); }
static void *dlopener(void *) {
	while (!stop) { void *h = dlopen(MODULE, RTLD_NOW); if (!h) { bad++; continue; } dlopens++; dlclose(h); }
	joined++; return nullptr;
}
static void *forker(void *a) { int slot = (int)(long)a; while (!stop) do_fork(forks, slot); joined++; return nullptr; }
static void dump(const char *what, pid_t pid) {
	char d[64]; snprintf(d, sizeof d, "/proc/%d/task", pid);
	DIR *dir = opendir(d); if (!dir) return;
	while (dirent *e = readdir(dir)) {
		if (e->d_name[0] == '.') continue;
		char p[128], buf[256]; snprintf(p, sizeof p, "%s/%s/syscall", d, e->d_name);
		int fd = open(p, O_RDONLY); int n = fd >= 0 ? (int)read(fd, buf, sizeof buf - 1) : 0; if (fd >= 0) close(fd); buf[n > 0 ? n - 1 : 0] = 0;
		char w[128], wb[64] = {}; snprintf(w, sizeof w, "%s/%s/wchan", d, e->d_name);
		fd = open(w, O_RDONLY); if (fd >= 0) { n = (int)read(fd, wb, sizeof wb - 1); close(fd); }
		fprintf(stderr, "  %s tid %s: syscall [%s] wchan %s\n", what, e->d_name, buf, wb);
	}
	closedir(dir);
}
int main(int argc, char **argv) {
	int seconds = argc > 1 ? atoi(argv[1]) : 10;
	bool native = getenv("FS_NATIVE") && !strcmp(getenv("FS_NATIVE"), "1");
	bool handler = !(getenv("FS_HANDLER") && !strcmp(getenv("FS_HANDLER"), "0"));
	child_dlopen = !(getenv("FS_CHILD_DLOPEN") && !strcmp(getenv("FS_CHILD_DLOPEN"), "0"));
	module_fd = open(MODULE, O_RDONLY);
	if (!native) bpftime::setup_syscall_tracer();
	struct sigaction sa = {}; sa.sa_handler = on_usr1; sa.sa_flags = SA_RESTART; sigaction(SIGUSR1, &sa, nullptr);
	pthread_t a, b, c;
	pthread_create(&a, nullptr, dlopener, nullptr);
	pthread_create(&b, nullptr, forker, (void *)0);
	pthread_create(&c, nullptr, forker, (void *)1);
	struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
	for (;;) {
		struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
		if (t.tv_sec - t0.tv_sec >= seconds) break;
		if (handler) { pthread_kill(a, SIGUSR1); usleep(2000); pthread_kill(b, SIGUSR1); }
		usleep(2000);
	}
	stop = true;
	for (int i = 0; i < 200 && joined < 3; i++) usleep(100000); // 20 s
	if (joined < 3) {
		fprintf(stderr, "HUNG: native=%d handler=%d child_dlopen=%d forks=%ld handler_forks=%ld dlopens=%ld\n", native, handler, child_dlopen, forks.load(), handler_forks.load(), dlopens.load());
		dump("parent", getpid());
		for (int s = 0; s < 3; s++) if (pid_t p = pending[s]) { fprintf(stderr, " pending child slot %d pid %d\n", s, p); dump("child", p); kill(p, SIGKILL); }
		_exit(2);
	}
	printf("fork_stress native=%d handler=%d child_dlopen=%d: forks=%ld handler_forks=%ld dlopens=%ld bad=%ld\n", native, handler, child_dlopen, forks.load(), handler_forks.load(), dlopens.load(), bad.load());
	return bad == 0 ? 0 : 1;
}
