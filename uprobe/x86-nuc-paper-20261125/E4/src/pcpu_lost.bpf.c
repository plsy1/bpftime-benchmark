// E4: per-CPU lost-update microbenchmark, BPF side.
// One uprobe on the target's tick(); the loader enables exactly one program:
//   (a)  prog_a:  v = lookup(pcpu_array, 0); if (v) *v += 1;        (plain non-atomic RMW)
//   (b)  prog_bW: volatile load of *v, then exactly W bpf_ktime_get_ns() calls, then volatile
//                 store of x + 1. W is a compile-time constant per program (W in 0,1,2,4,8), so
//                 prog_b0 is exactly ld / add / st with nothing in between (no rodata loop, no
//                 dead branches: bpftime has no verifier dead-code pruning). NOTE: BPF has no
//                 non-atomic memory add, so prog_a's *v += 1 is the same ld / add / st bytecode
//                 as prog_b0; both JIT (llvmbpf, x86) to one `incq (%rax)` (e4/jit/*-a). A
//                 split RMW at machine level needs something between load and store: prog_b1+.
//   (c)  prog_c:  same RMW as (a) on a BPF_MAP_TYPE_PERCPU_HASH value (key 0, pre-inserted)
// count_runs = 1 (default): every program first counts its own run in a per-thread 64-byte
// .bss slot with an atomic add (exact under any preemption/migration; slot = tid & 127, a
// collision only costs contention, not exactness). NULL lookups are counted in the same slot.
// runs - nulls - sum = updates lost; expected - runs = probe hits that did not run the program.
// count_runs = 0 (loader -R, timing rows): no counter; the program only sets a read-mostly
// "seen" flag once (load, compare, and one store per process lifetime) so that an attach or
// injection failure is still detectable (seen == 0).
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

const volatile int count_runs = 1;

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(max_entries, 1);
	__type(key, __u32);
	__type(value, __u64);
} pcpu_array SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_HASH);
	__uint(max_entries, 16);
	__type(key, __u32);
	__type(value, __u64);
} pcpu_hash SEC(".maps");

struct slot {
	__u64 runs;
	__u64 nulls;
	__u64 pad[6];
} __attribute__((aligned(64)));
struct slot slots[128];
// own line, after the slots; written once (0 -> 1), read on every call in -R mode
struct seen_line {
	__u64 seen;
	__u64 pad[7];
} __attribute__((aligned(64)));
struct seen_line seen_line;

static __always_inline struct slot *my_slot(void)
{
	__u32 tid = (__u32)bpf_get_current_pid_tgid();
	return &slots[tid & 127];
}

static __always_inline void count_run(void)
{
	if (count_runs) {
		__sync_fetch_and_add(&my_slot()->runs, 1);
	} else {
		if (!*(volatile __u64 *)&seen_line.seen)
			*(volatile __u64 *)&seen_line.seen = 1;
	}
}

static __always_inline void count_null(void)
{
	__sync_fetch_and_add(&my_slot()->nulls, 1);
}

SEC("uprobe")
int BPF_UPROBE(prog_a)
{
	__u32 zero = 0;
	count_run();
	__u64 *v = bpf_map_lookup_elem(&pcpu_array, &zero);
	if (v)
		*v += 1;
	else
		count_null();
	return 0;
}

#define KT1 t += bpf_ktime_get_ns();
#define KT2 KT1 KT1
#define KT4 KT2 KT2
#define KT8 KT4 KT4
#define WIDEN_0
#define WIDEN_1 KT1
#define WIDEN_2 KT2
#define WIDEN_4 KT4
#define WIDEN_8 KT8

// t & 0 keeps the helper results used without changing the value written; the helper calls
// themselves are never removed (helpers are not pure for clang).
#define PROG_B(W)                                                          \
	SEC("uprobe")                                                      \
	int BPF_UPROBE(prog_b##W)                                          \
	{                                                                  \
		__u32 zero = 0;                                            \
		count_run();                                               \
		__u64 *v = bpf_map_lookup_elem(&pcpu_array, &zero);        \
		if (!v) {                                                  \
			count_null();                                      \
			return 0;                                          \
		}                                                          \
		__u64 x = *(volatile __u64 *)v;                            \
		__u64 t = 0;                                               \
		WIDEN_##W                                                  \
		*(volatile __u64 *)v = x + 1 + (t & 0);                    \
		return 0;                                                  \
	}

PROG_B(0)
PROG_B(1)
PROG_B(2)
PROG_B(4)
PROG_B(8)

SEC("uprobe")
int BPF_UPROBE(prog_c)
{
	__u32 zero = 0;
	count_run();
	__u64 *v = bpf_map_lookup_elem(&pcpu_hash, &zero);
	if (v)
		*v += 1;
	else
		count_null();
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
