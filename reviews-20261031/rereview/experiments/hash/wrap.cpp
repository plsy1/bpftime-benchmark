#include <bpf_map/bpftime_hash_map.hpp>
using namespace bpftime;
__attribute__((noinline)) void *lk(bpftime_hash_map &m, const void *k) { return m.elem_lookup(k); }
__attribute__((noinline)) bool up(bpftime_hash_map &m, const void *k, const void *v) { return m.elem_update(k, v); }
__attribute__((noinline)) bool dl(bpftime_hash_map &m, const void *k) { return m.elem_delete(k); }
