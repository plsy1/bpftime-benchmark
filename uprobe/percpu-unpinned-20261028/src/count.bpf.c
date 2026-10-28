// Shared counters: count_event(key) increments counters[key], with key in 0..NKEYS-1 shared by
// all threads. KIND selects the map and the update, as real tools write them:
//   1 hash, lookup + atomic add, insert with BPF_NOEXIST on a miss (retry the add if another
//     thread inserted first)
//   2 per-CPU hash, lookup + plain add, insert on a miss
//   3 per-CPU array, lookup + plain add
//   4 ordinary array, lookup + atomic add
//   0 control: the same probe without map operations
#include <linux/bpf.h>
#include <asm/ptrace.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#ifndef KIND
#define KIND 1
#endif

#if KIND == 1
#define MAP_TYPE BPF_MAP_TYPE_HASH
#elif KIND == 2
#define MAP_TYPE BPF_MAP_TYPE_PERCPU_HASH
#elif KIND == 3
#define MAP_TYPE BPF_MAP_TYPE_PERCPU_ARRAY
#else
#define MAP_TYPE BPF_MAP_TYPE_ARRAY
#endif

struct {
	__uint(type, MAP_TYPE);
	__uint(max_entries, 1024);
	__type(key, __u32);
	__type(value, __u64);
} counts SEC(".maps");

// Per-thread result slots (see attach.c): 0 probe hits, 1 failed updates.
struct slot {
	__u64 hits;
	__u64 failed;
	__u64 pad[6];
};
struct slot slots[64];

SEC("uprobe")
int BPF_UPROBE(on_count, __u64 arg)
{
	__u32 tid = (__u32)bpf_get_current_pid_tgid();
	struct slot *slot = &slots[(tid & 31) * 2];
	__u32 key = (__u32)arg;
	slot->hits++;
#if KIND == 0
	slot->pad[0] += key;
#else
	__u64 *v = bpf_map_lookup_elem(&counts, &key);
	if (v) {
#if KIND == 1 || KIND == 4
		__sync_fetch_and_add(v, 1);
#else
		*v += 1;
#endif
		return 0;
	}
#if KIND == 1 || KIND == 2
	__u64 one = 1;
	if (bpf_map_update_elem(&counts, &key, &one, BPF_NOEXIST)) {
		v = bpf_map_lookup_elem(&counts, &key);
		if (v) {
#if KIND == 1
			__sync_fetch_and_add(v, 1);
#else
			*v += 1;
#endif
			return 0;
		}
		slot->failed++;
	}
#else
	slot->failed++;
#endif
#endif
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
