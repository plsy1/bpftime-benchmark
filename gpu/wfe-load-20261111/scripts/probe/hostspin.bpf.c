// hostspin: at the exit of spinKernel, the latency of one GPU-to-CPU host call, timed on the GPU:
// before and after a bpf_map_update_elem on an ordinary hash map (which every
// build serves through the host-call slot), stored per call in a GPU array.
#define BPF_NO_GLOBAL_DATA
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>

#define BPF_MAP_TYPE_GPU_ARRAY_MAP 1503 // dev creates it as 1513 on the Orin
#define SLOTS 4096

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 16);
	__type(key, u32);
	__type(value, u64);
} host_hash SEC(".maps");

// lat[0]: number of calls recorded; lat[1..SLOTS]: ns per call
struct {
	__uint(type, BPF_MAP_TYPE_GPU_ARRAY_MAP);
	__uint(max_entries, SLOTS + 1);
	__type(key, u32);
	__type(value, u64);
} lat SEC(".maps");

static const u64 (*bpf_get_globaltimer)(void) = (void *)502;

SEC("kretprobe/_Z10spinKernelyPy")
int cuda__probe()
{
	u32 zero = 0;
	u64 one = 1;
	u64 t0 = bpf_get_globaltimer();
	bpf_map_update_elem(&host_hash, &zero, &one, BPF_ANY);
	u64 t1 = bpf_get_globaltimer();
	u64 *n = bpf_map_lookup_elem(&lat, &zero);
	if (!n)
		return 0;
	u64 i = *n + 1;
	if (i > SLOTS)
		return 0;
	*n = i;
	u32 k = (u32)i;
	u64 *slot = bpf_map_lookup_elem(&lat, &k);
	if (slot)
		*slot = t1 - t0;
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
