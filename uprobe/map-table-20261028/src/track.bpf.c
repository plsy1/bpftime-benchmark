// memleak-like tracking table: alloc_event(addr) inserts addr -> {size, timestamp, stack id}
// (8 B key, 24 B value, as memleak's allocs map, 1M entries); free_event(addr) looks it up and
// deletes it. CONTROL=1: the same probes and helpers without map operations.
#include <linux/bpf.h>
#include <asm/ptrace.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

struct alloc_info {
	__u64 size;
	__u64 timestamp_ns;
	__s64 stack_id;
};

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 1048576);
	__type(key, __u64);
	__type(value, struct alloc_info);
} allocs SEC(".maps");

// Per-thread result slots (see attach.c): 0 allocs seen, 1 frees found, 2 frees not found.
struct slot {
	__u64 allocs;
	__u64 hits;
	__u64 misses;
	__u64 pad[5];
};
struct slot slots[64];

SEC("uprobe")
int BPF_UPROBE(on_alloc, __u64 addr)
{
	__u32 tid = (__u32)bpf_get_current_pid_tgid();
	struct slot *slot = &slots[(tid & 31) * 2];
	struct alloc_info info = { .size = 64, .timestamp_ns = bpf_ktime_get_ns(), .stack_id = -1 };
#if CONTROL
	slot->pad[0] += info.timestamp_ns ^ addr;
#else
	bpf_map_update_elem(&allocs, &addr, &info, BPF_ANY);
#endif
	slot->allocs++;
	return 0;
}

SEC("uprobe")
int BPF_UPROBE(on_free, __u64 addr)
{
	__u32 tid = (__u32)bpf_get_current_pid_tgid();
	struct slot *slot = &slots[(tid & 31) * 2];
#if CONTROL
	slot->pad[0] += addr;
	slot->hits++;
#else
	struct alloc_info *info = bpf_map_lookup_elem(&allocs, &addr);
	if (!info) {
		slot->misses++;
		return 0;
	}
	slot->pad[0] += info->size;
	slot->hits++;
	bpf_map_delete_elem(&allocs, &addr);
#endif
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
