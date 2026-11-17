/* The timer alone (attribution, 11-17): entry and exit each read
 * %globaltimer through helper 502 and consume it in a branch that is never
 * taken; no map, no carry. lat minus this is the carry and the merge. */
#include "pt_common.h"
#define DEF(fn, kidx, kname) \
	SEC("kprobe/" kname) int cuda__in_##fn() { return bpf_get_globaltimer() == 1; } \
	SEC("kretprobe/" kname) int cuda__out_##fn() { return bpf_get_globaltimer() == 1; }
PT_PAIRS(DEF)
char LICENSE[] SEC("license") = "GPL";
