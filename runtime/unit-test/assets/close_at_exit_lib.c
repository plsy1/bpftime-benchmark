/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026, eunomia-bpf org
 * All rights reserved.
 */
#include <unistd.h>

/* A library whose ELF destructor closes a descriptor, as libnvrm_mem's closes
 * /dev/nvmap. It is a DT_NEEDED of the helper, unrelated to the preloaded
 * syscall server, so it is finalized after the server's __destruct_shm. */
static int fd_to_close = -1;

void close_at_exit_set_fd(int fd)
{
	fd_to_close = fd;
}

__attribute__((destructor)) static void close_at_exit(void)
{
	if (fd_to_close >= 0)
		close(fd_to_close);
}
