// Entry/exit churn, the dominant real-tool pattern (map-usage-20261027): the uprobe inserts a start
// record keyed by the thread, the uretprobe looks it up and deletes it. KEY_SIZE 4 keys by tid,
// 8 by pid_tgid; VALUE_SIZE is the record size. CONTROL=1 builds the same probes without map
// operations, to subtract everything that is not the map.
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#ifndef KEY_SIZE
#define KEY_SIZE 4
#endif
#ifndef VALUE_SIZE
#define VALUE_SIZE 8
#endif

#if KEY_SIZE == 4
typedef __u32 churn_key;
#elif KEY_SIZE == 8
typedef __u64 churn_key;
#else
#error "KEY_SIZE must be 4 or 8"
#endif

struct start {
	__u64 ts;
	__u8 rest[VALUE_SIZE - 8];
};

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 10240);
	__type(key, churn_key);
	__type(value, struct start);
} starts SEC(".maps");

// Results per thread: 64-byte slots, threads use every other one ((tid & 31) * 2) so no two
// threads share a cache line whatever the alignment of the map value (run1 shared lines).
struct slot {
	__u64 hits;
	__u64 misses;
	__u64 total_ns;
	__u64 pad[5];
};
struct slot slots[64];

SEC("uprobe")
int BPF_UPROBE(enter)
{
	__u64 id = bpf_get_current_pid_tgid();
	churn_key key = (churn_key)id;
	struct start s = {};
	s.ts = bpf_ktime_get_ns();
#if CONTROL
	slots[((__u32)id & 31) * 2].total_ns -= s.ts + key;
#else
	bpf_map_update_elem(&starts, &key, &s, BPF_ANY);
#endif
	return 0;
}

SEC("uretprobe")
int BPF_URETPROBE(leave)
{
	__u64 id = bpf_get_current_pid_tgid();
	churn_key key = (churn_key)id;
	struct slot *slot = &slots[((__u32)id & 31) * 2];
	__u64 now = bpf_ktime_get_ns();
#if CONTROL
	slot->total_ns += now + key;
	slot->hits++;
#else
	struct start *s = bpf_map_lookup_elem(&starts, &key);
	if (!s) {
		slot->misses++;
		return 0;
	}
	slot->total_ns += now - s->ts;
	slot->hits++;
	bpf_map_delete_elem(&starts, &key);
#endif
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
