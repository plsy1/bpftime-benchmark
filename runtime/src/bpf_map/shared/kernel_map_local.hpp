/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#ifndef _BPFTIME_KERNEL_MAP_LOCAL_HPP
#define _BPFTIME_KERNEL_MAP_LOCAL_HPP
#include <cstddef>
#include <cstdint>

namespace bpftime
{
// Process-local state of a kernel map that a kernel-user proxy map stands
// for. The proxy objects live in shared memory and are used by every process
// attached to it (the daemon creates them, agents run the programs), so a
// descriptor or buffer stored in them is meaningless outside the process
// that made it.
struct kernel_map_local {
	int fd = -1;
	uint32_t key_size = 0;
	uint32_t value_size = 0;
	uint32_t max_entries = 0;
};

// This process's descriptor and geometry of kernel map KERNEL_MAP_ID, opened
// once per process (fork children inherit it). nullptr if the map cannot be
// opened.
const kernel_map_local *kernel_map_of_this_process(int kernel_map_id);

// A buffer of at least SIZE bytes owned by the calling thread for
// KERNEL_MAP_ID. Lookups return pointers into it; it stays valid until the
// thread's next lookup of the same map, which covers the program invocation.
void *kernel_map_thread_buffer(int kernel_map_id, size_t size);
} // namespace bpftime
#endif
