/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026, eunomia-bpf org
 * All rights reserved.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/bpf.h>
#include <linux/perf_event.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

static int global_shm_exists(void)
{
	const char *shm_name = getenv("BPFTIME_GLOBAL_SHM_NAME");
	if (shm_name == NULL || shm_name[0] == '\0') {
		return 0;
	}

	char path[512];
	int written = snprintf(path, sizeof(path), "/dev/shm/%s", shm_name);
	if (written < 0 || (size_t)written >= sizeof(path)) {
		return 0;
	}
	return access(path, F_OK) == 0;
}

static int fd_is_unlinked_mock_tmp_file(int fd)
{
	char proc_path[64];
	int written = snprintf(proc_path, sizeof(proc_path), "/proc/self/fd/%d",
			       fd);
	if (written < 0 || (size_t)written >= sizeof(proc_path)) {
		return 0;
	}

	char target[PATH_MAX];
	ssize_t len = readlink(proc_path, target, sizeof(target) - 1);
	if (len < 0) {
		perror("readlink");
		return 0;
	}
	target[len] = '\0';
	const char *prefix = "/tmp/bpftime-mock.";
	return strncmp(target, prefix, strlen(prefix)) == 0 &&
	       strstr(target, " (deleted)") != NULL;
}

static int trigger_startup(void)
{
	/* The interposer should fall back to the host libc path when bpftime
	 * startup cannot allocate its shared memory.
	 */
	int fd = open("/dev/null", O_RDONLY);
	if (fd < 0) {
		return 101;
	}
	char byte;
	if (read(fd, &byte, 0) != 0) {
		close(fd);
		return 102;
	}
	close(fd);
	fd = syscall(__NR_memfd_create, "bpftime-shm-allocation-test", 0);
	if (fd < 0) {
		return 104;
	}
	close(fd);
	FILE *file = fopen("/dev/null", "r");
	if (file == NULL) {
		return 103;
	}
	fclose(file);
	return 100;
}

static int trigger_startup_ok(void)
{
	int fd = open("/dev/null", O_RDONLY, 0);
	if (fd < 0) {
		return 101;
	}
	close(fd);
	if (getenv("BPFTIME_USED") == NULL || !global_shm_exists()) {
		return 102;
	}
	return 0;
}

static int trigger_startup_wait(void)
{
	int err = trigger_startup_ok();
	if (err != 0) {
		return err;
	}

	const char *ready_file = getenv("BPFTIME_HELPER_READY_FILE");
	const char *release_file = getenv("BPFTIME_HELPER_RELEASE_FILE");
	if (ready_file == NULL || release_file == NULL) {
		return 103;
	}

	FILE *fp = fopen(ready_file, "w");
	if (fp == NULL) {
		perror("fopen ready");
		return 104;
	}
	fclose(fp);

	for (int i = 0; i < 3000; i++) {
		if (access(release_file, F_OK) == 0) {
			return 0;
		}
		usleep(10000);
	}
	return 105;
}

static int trigger_mock_tmp_files(void)
{
	int fd = open("/sys/bus/event_source/devices/uprobe/type",
		      O_RDONLY | O_CLOEXEC, 0);
	if (fd < 0) {
		perror("open");
		return 11;
	}
	if (!fd_is_unlinked_mock_tmp_file(fd)) {
		close(fd);
		return 12;
	}
	if ((fcntl(fd, F_GETFD) & FD_CLOEXEC) == 0) {
		close(fd);
		return 13;
	}
	char buf[64];
	if (read(fd, buf, sizeof(buf)) < 0) {
		perror("read");
		close(fd);
		return 14;
	}
	close(fd);

	FILE *fp = fopen("/sys/bus/event_source/devices/uprobe/format/retprobe",
			 "re");
	if (fp == NULL) {
		perror("fopen");
		return 15;
	}
	fd = fileno(fp);
	if (!fd_is_unlinked_mock_tmp_file(fd)) {
		fclose(fp);
		return 16;
	}
	if ((fcntl(fd, F_GETFD) & FD_CLOEXEC) == 0) {
		fclose(fp);
		return 17;
	}
	if (fgets(buf, sizeof(buf), fp) == NULL && ferror(fp)) {
		perror("fgets");
		fclose(fp);
		return 18;
	}
	fclose(fp);
	return 0;
}

static int trigger_perf_mmap(void)
{
	struct perf_event_attr attr;
	memset(&attr, 0, sizeof(attr));
	attr.type = PERF_TYPE_SOFTWARE;
	attr.size = sizeof(attr);
	attr.config = PERF_COUNT_SW_CPU_CLOCK;
	attr.sample_type = PERF_SAMPLE_RAW;

	int fd = syscall(__NR_perf_event_open, &attr, -1, 0, -1, 0);
	if (fd < 0) {
		perror("perf_event_open");
		return 2;
	}

	size_t length = (size_t)getpagesize() + 8 * 1024 * 1024;
	/* The interposer must not leak the caller's stale errno on failure. */
	errno = E2BIG;
	void *buffer =
		mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (buffer != MAP_FAILED || errno != ENOMEM) {
		fprintf(stderr, "mmap=%p errno=%d\n", buffer, errno);
		return 3;
	}
	return 0;
}

static int check_sigusr1_default(void)
{
	struct sigaction action;
	if (sigaction(SIGUSR1, NULL, &action) != 0) {
		return 105;
	}
	return action.sa_handler == SIG_DFL ? 100 : 106;
}

static long sys_bpf(int cmd, union bpf_attr *attr)
{
	return syscall(__NR_bpf, cmd, attr, sizeof(*attr));
}

/* A loader that forks before its first bpf() call (memleak -c forks the
 * traced command first). The child inherits the interposer without a started
 * server; its first intercepted call must not start a second server session,
 * which would reset the objects the parent created meanwhile.
 */
static int fork_before_startup(void)
{
	int go[2];
	if (pipe(go) != 0)
		return 110;
	pid_t child = fork();
	if (child < 0)
		return 111;
	if (child == 0) {
		char c;
		/* Raw read: passes the interposer without starting anything. */
		if (syscall(SYS_read, go[0], &c, 1) != 1)
			_exit(1);
		int fd = open("/dev/null", O_RDONLY);
		if (fd >= 0)
			close(fd);
		_exit(0);
	}
	union bpf_attr attr;
	memset(&attr, 0, sizeof(attr));
	attr.map_type = BPF_MAP_TYPE_HASH;
	attr.key_size = 4;
	attr.value_size = 8;
	attr.max_entries = 4;
	int map_fd = (int)sys_bpf(BPF_MAP_CREATE, &attr);
	if (map_fd < 0)
		return 112;
	unsigned int key = 1;
	unsigned long long value = 42, out = 0;
	memset(&attr, 0, sizeof(attr));
	attr.map_fd = map_fd;
	attr.key = (unsigned long long)(unsigned long)&key;
	attr.value = (unsigned long long)(unsigned long)&value;
	if (sys_bpf(BPF_MAP_UPDATE_ELEM, &attr) != 0)
		return 113;
	if (write(go[1], "x", 1) != 1)
		return 114;
	int status = 0;
	if (waitpid(child, &status, 0) != child || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != 0)
		return 115;
	memset(&attr, 0, sizeof(attr));
	attr.map_fd = map_fd;
	attr.key = (unsigned long long)(unsigned long)&key;
	attr.value = (unsigned long long)(unsigned long)&out;
	if (sys_bpf(BPF_MAP_LOOKUP_ELEM, &attr) != 0)
		return 116;
	return out == 42 ? 100 : 117;
}

/* A map larger than the shared memory: bpf() must fail with ENOMEM instead
 * of falling back to the kernel (which created a kernel map the runtime could
 * not resolve), and the server must keep working afterwards.
 */
static int map_exhaustion(void)
{
	union bpf_attr attr;
	memset(&attr, 0, sizeof(attr));
	attr.map_type = BPF_MAP_TYPE_HASH;
	attr.key_size = 8;
	attr.value_size = 8;
	attr.max_entries = 8 * 1024 * 1024;
	errno = 0;
	long fd = sys_bpf(BPF_MAP_CREATE, &attr);
	if (fd >= 0)
		return 120;
	if (errno != ENOMEM) {
		fprintf(stderr, "map create errno %d\n", errno);
		return 121;
	}
	attr.max_entries = 4;
	fd = sys_bpf(BPF_MAP_CREATE, &attr);
	if (fd < 0)
		return 122;
	return 100;
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		return 64;
	}
	if (strcmp(argv[1], "startup") == 0) {
		return trigger_startup();
	}
	if (strcmp(argv[1], "startup-ok") == 0) {
		return trigger_startup_ok();
	}
	if (strcmp(argv[1], "startup-wait") == 0) {
		return trigger_startup_wait();
	}
	if (strcmp(argv[1], "mock-tmp") == 0) {
		return trigger_mock_tmp_files();
	}
	if (strcmp(argv[1], "perf-mmap") == 0) {
		return trigger_perf_mmap();
	}
	if (strcmp(argv[1], "map-exhaustion") == 0) {
		return map_exhaustion();
	}
	if (strcmp(argv[1], "fork-before-startup") == 0) {
		return fork_before_startup();
	}
	if (strcmp(argv[1], "check-sigusr1") == 0) {
		return check_sigusr1_default();
	}
	return 64;
}
