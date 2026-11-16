/* A probe with no map, on kernels llama.cpp does not have: the agent's own
 * effect (loaded, nothing patched, no delivery thread). */
#include "pt_common.h"
#define DEF(fn, kidx, kname) \
	SEC("kretprobe/" kname) int cuda__out_##fn() { return 0; }
PT_PAIRS(DEF)
char LICENSE[] SEC("license") = "GPL";
