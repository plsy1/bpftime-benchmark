/* SPDX-License-Identifier: MIT */
#ifndef BPFTIME_DIRECT_MAP_HELPERS_HPP
#define BPFTIME_DIRECT_MAP_HELPERS_HPP

// Private llvmbpf helper slots (not BPF helper numbers) through which a bound
// array JIT program registers its exact original map helpers. For constant
// handles registered with register_direct_map_handle, the JIT calls these
// instead of the program's dispatcher in helper slots 1-3.
#define BPFTIME_DIRECT_MAP_LOOKUP_SLOT 8189
#define BPFTIME_DIRECT_MAP_UPDATE_SLOT 8190
#define BPFTIME_DIRECT_MAP_DELETE_SLOT 8191
#define BPFTIME_DIRECT_MAP_LOOKUP_NAME "bpftime_direct_map_lookup_v1"
#define BPFTIME_DIRECT_MAP_UPDATE_NAME "bpftime_direct_map_update_v1"
#define BPFTIME_DIRECT_MAP_DELETE_NAME "bpftime_direct_map_delete_v1"
#endif
