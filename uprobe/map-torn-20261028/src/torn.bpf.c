// Torn reads through a lookup pointer: thread 0 calls write_event(seq), which updates key 0 of a
// hash map with a 112-byte value whose 14 words all equal seq (bpf_map_update_elem, BPF_ANY);
// the other threads call read_event(), which looks the key up and compares the first and the
// last word of the value it points to, with a helper call in between. Different words mean the value changed while the
// program was reading it. NO_PREALLOC: BPF_F_NO_PREALLOC (kernel frees replaced elements after
// an RCU grace period instead of reusing them at once).
#include <linux/bpf.h>
#include <asm/ptrace.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#define WORDS 14

struct value {
	__u64 w[WORDS];
};

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 1024);
	__type(key, __u32);
	__type(value, struct value);
#ifdef NO_PREALLOC
	__uint(map_flags, BPF_F_NO_PREALLOC);
#endif
} vals SEC(".maps");

// Per-thread result slots (see attach.c): 0 reads, 1 lookup misses, 2 torn reads, 3 writes,
// 4 failed writes.
struct slot {
	__u64 reads;
	__u64 misses;
	__u64 torn;
	__u64 writes;
	__u64 failed;
	__u64 pad[3];
};
struct slot slots[64];

SEC("uprobe")
int BPF_UPROBE(on_write, __u64 seq)
{
	__u32 tid = (__u32)bpf_get_current_pid_tgid();
	struct slot *slot = &slots[(tid & 31) * 2];
	struct value v;
	__u32 key = 0;
	for (int i = 0; i < WORDS; i++)
		v.w[i] = seq;
	slot->writes++;
	if (bpf_map_update_elem(&vals, &key, &v, BPF_ANY))
		slot->failed++;
	return 0;
}

SEC("uprobe")
int BPF_UPROBE(on_read)
{
	__u32 tid = (__u32)bpf_get_current_pid_tgid();
	struct slot *slot = &slots[(tid & 31) * 2];
	__u32 key = 0;
	slot->reads++;
	struct value *v = bpf_map_lookup_elem(&vals, &key);
	if (!v) {
		slot->misses++;
		return 0;
	}
	// Read the first word, call a helper (as programs do between lookup and use), then read the
	// last word.
	__u64 first = *(volatile __u64 *)&v->w[0];
	slot->pad[0] += bpf_ktime_get_ns();
	__u64 last = *(volatile __u64 *)&v->w[WORDS - 1];
	if (first != last)
		slot->torn++;
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
