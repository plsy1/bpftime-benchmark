/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026, eunomia-bpf org
 * All rights reserved.
 */
#define _GNU_SOURCE
#include <linux/bpf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

void close_at_exit_set_fd(int fd);

/* Run under the syscall server: a bpf() call starts it (it creates the
 * segment), then the library closes the map's fd in its destructor at exit,
 * after the server unmapped the segment. */
int main(void)
{
	union bpf_attr attr;
	memset(&attr, 0, sizeof(attr));
	attr.map_type = BPF_MAP_TYPE_ARRAY;
	attr.key_size = 4;
	attr.value_size = 8;
	attr.max_entries = 1;
	long fd = syscall(__NR_bpf, BPF_MAP_CREATE, &attr, sizeof(attr));
	if (fd < 0) {
		perror("bpf(BPF_MAP_CREATE)");
		return 2;
	}
	/* The map must be the server's, or the exit path is not exercised. */
	const char *shm_name = getenv("BPFTIME_GLOBAL_SHM_NAME");
	char path[512];
	if (shm_name == NULL ||
	    snprintf(path, sizeof(path), "/dev/shm/%s", shm_name) >=
		    (int)sizeof(path) ||
	    access(path, F_OK) != 0) {
		fprintf(stderr, "no bpftime segment: the server is not active\n");
		return 3;
	}
	close_at_exit_set_fd((int)fd);
	printf("map fd %ld\n", fd);
	/* stdio is flushed after the destructors: a crash there loses it. */
	fflush(stdout);
	return 0;
}
