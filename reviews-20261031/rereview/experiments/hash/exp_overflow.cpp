#include <bpf_map/bpftime_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <cstdio>
using namespace boost::interprocess;
using namespace bpftime;
int main()
{
	const char *name = "fable2_exp_overflow";
	shared_memory_object::remove(name);
	managed_shared_memory seg(create_only, name, 160 << 20);
	// next_prime(2*max_entries) * elem_size wraps to 96.9 MiB.
	const size_t max_entries = 4294922064ULL, key_size = 4,
		     value_size = 2147506256ULL;
	bpftime_hash_map map(seg, max_entries, key_size, value_size);
	printf("constructed: buckets %zu stripes %zu count %zu (true data size %.0f TiB)\n",
	       map.bucket_count(), map.stripe_count(), map.get_elem_count(),
	       (double)map.bucket_count() * 2147506264.0 / 1099511627776.0);
	fflush(stdout);
	uint32_t key = 7;
	void *p = map.elem_lookup(&key); // reads bucket (hash % buckets) * 2 GiB
	printf("lookup returned %p\n", p);
	shared_memory_object::remove(name);
	return 0;
}
