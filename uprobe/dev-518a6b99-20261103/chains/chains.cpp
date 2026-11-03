// Chain shape of the key-pattern matrix's live sets, seeded (as built) vs a seed forced to 0.
#include <bpf_map/bpftime_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <cstdio>
using namespace bpftime;
using namespace boost::interprocess;
int bpftime_cpu_index_limit() { return 6; }
static unsigned long make_key(int pattern, unsigned long gid)
{
	switch (pattern) {
	case 0: return ((gid * 0x9e3779b97f4a7c15ul) & 0x00000ffffffffffful) << 4 | 0x10;
	case 1: return (4242ul << 32) | (unsigned)gid;
	case 2: { unsigned k = (unsigned)gid * 2654435761u; return k ^ (k >> 16); }
	default: return (unsigned)(gid << 12);
	}
}
int main(int argc, char **argv)
{
	const char *names[] = { "addr", "seq64", "rand32", "shl12" };
	for (size_t cap : { 10240ul, 102400ul })
	for (int pat = 0; pat < 4; pat++)
	for (double load : { 0.5, 0.9 }) {
		shared_memory_object::remove("chains_stat");
		managed_shared_memory seg(create_only, "chains_stat", 64 << 20);
		size_t ks = pat < 2 ? 8 : 4;
		bpftime_hash_map m(seg, cap, ks, 8);
		size_t live = (size_t)(cap * load);
		unsigned long base = 1000000; // a window of consecutive ids, as keyloop after churn
		uint64_t v = 0;
		for (size_t i = 0; i < live; i++) { unsigned long k = make_key(pat, base + i); m.elem_update(&k, &v); }
		double hit = 0; size_t multi = 0;
		for (size_t i = 0; i < live; i++) { unsigned long k = make_key(pat, base + i); size_t c = m.chain_length_of_key(&k); hit += c; }
		printf("%6zu %3.0f%% %-6s mean chain at a hit %.3f\n", cap, load * 100, names[pat], hit / live);
		shared_memory_object::remove("chains_stat");
	}
}
