// Tracking table with a chosen key size and capacity: insert_event(key) inserts key -> {size,
// timestamp, stack id} (24 B value, as memleak's allocs map); delete_event(key) looks it up and
// deletes it. KEY_SIZE 8 uses the argument, 4 its low half. CONTROL=1: the same probes and
// helpers without map operations.
#include <linux/bpf.h>
#include <asm/ptrace.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#ifndef KEY_SIZE
#define KEY_SIZE 8
#endif
#ifndef MAX_ENTRIES
#define MAX_ENTRIES 10240
#endif
#if KEY_SIZE == 8
typedef __u64 map_key;
#elif KEY_SIZE == 4
typedef __u32 map_key;
#else
#error "KEY_SIZE must be 4 or 8"
#endif

struct info {
	__u64 size;
	__u64 timestamp_ns;
	__s64 stack_id;
};

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, MAX_ENTRIES);
	__type(key, map_key);
	__type(value, struct info);
} table SEC(".maps");

// Per-thread result slots (attach.c sums them): 0 inserts seen, 1 deletes found, 2 not found.
struct slot {
	__u64 inserts;
	__u64 hits;
	__u64 misses;
	__u64 pad[5];
};
struct slot slots[64];

SEC("uprobe")
int BPF_UPROBE(on_insert, __u64 arg)
{
	__u32 tid = (__u32)bpf_get_current_pid_tgid();
	struct slot *slot = &slots[(tid & 31) * 2];
	map_key key = (map_key)arg;
	struct info v = { .size = 64, .timestamp_ns = bpf_ktime_get_ns(), .stack_id = -1 };
#if CONTROL
	slot->pad[0] += v.timestamp_ns ^ key;
#else
	bpf_map_update_elem(&table, &key, &v, BPF_ANY);
#endif
	slot->inserts++;
	return 0;
}

SEC("uprobe")
int BPF_UPROBE(on_delete, __u64 arg)
{
	__u32 tid = (__u32)bpf_get_current_pid_tgid();
	struct slot *slot = &slots[(tid & 31) * 2];
	map_key key = (map_key)arg;
#if CONTROL
	slot->pad[0] += key;
	slot->hits++;
#else
	struct info *v = bpf_map_lookup_elem(&table, &key);
	if (!v) {
		slot->misses++;
		return 0;
	}
	slot->pad[0] += v->size;
	bpf_map_delete_elem(&table, &key);
	slot->hits++;
#endif
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
