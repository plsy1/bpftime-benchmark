#include "shim.hpp"
#include <bpf_map/bpftime_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <cstdio>
using namespace boost::interprocess;
int main() {
  shared_memory_object::remove("hm_smoke");
  managed_shared_memory seg(create_only, "hm_smoke", 1 << 20);
  bpftime::bpftime_hash_map m(seg, 100, 4, 8);
  uint32_t k = 1; uint64_t v = 2;
  printf("update %d lookup %p lists %zu stripes %zu heads %zu\n", m.elem_update(&k,&v), m.elem_lookup(&k), m.free_list_count(), m.stripe_count(), m.bucket_count());
  shared_memory_object::remove("hm_smoke");
}
