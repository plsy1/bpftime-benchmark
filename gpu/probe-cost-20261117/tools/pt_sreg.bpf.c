/* The timer's call shape with another special register (attribution,
 * 11-17): as pt_timer.bpf.c, but each probe calls helper 511 (%laneid)
 * instead of 502 (%globaltimer). timer minus this is the cost of reading
 * %globaltimer itself; this minus empty2 is the cost of the helper call. */
#include "pt_common.h"
static const u64 (*bpf_get_lane_id)(void) = (void *)511;
#define DEF(fn, kidx, kname) \
	SEC("kprobe/" kname) int cuda__in_##fn() { return bpf_get_lane_id() == 99; } \
	SEC("kretprobe/" kname) int cuda__out_##fn() { return bpf_get_lane_id() == 99; }
PT_PAIRS(DEF)
char LICENSE[] SEC("license") = "GPL";
