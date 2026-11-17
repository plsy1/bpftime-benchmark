/* The floor: probe functions that do nothing.
 *   PT_EXIT_ONLY: an exit probe only (hist's shape); else entry and exit
 *   (lat's shape). No map. */
#include "pt_common.h"
#ifdef PT_EXIT_ONLY
#define DEF(fn, kidx, kname) \
	SEC("kretprobe/" kname) int cuda__out_##fn() { return 0; }
#else
#define DEF(fn, kidx, kname) \
	SEC("kprobe/" kname) int cuda__in_##fn() { return 0; } \
	SEC("kretprobe/" kname) int cuda__out_##fn() { return 0; }
#endif
PT_PAIRS(DEF)
char LICENSE[] SEC("license") = "GPL";
