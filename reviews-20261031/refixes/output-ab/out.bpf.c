// Output paths after the second review's fixes: count_event(key) sends key as
// an 8-byte record. KIND selects the path:
//   5 bpf_perf_event_output into a PERF_EVENT_ARRAY (BPF_F_CURRENT_CPU)
//   6 bpf_ringbuf_output into a RINGBUF
//   0 control: the probe and bpf_get_current_pid_tgid only
// Every kind calls bpf_get_current_pid_tgid (the slot index), as count.bpf.c.
#include <linux/bpf.h>
#include <asm/ptrace.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#ifndef KIND
#define KIND 5
#endif

#if KIND == 5
struct {
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__uint(key_size, sizeof(__u32));
	__uint(value_size, sizeof(__u32));
} events SEC(".maps");
#elif KIND == 6
struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 1 << 22);
} events SEC(".maps");
#endif

// Per-thread result slots (see attach.c): 0 probe hits, 1 failed outputs.
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
	__u64 key = arg;
	slot->hits++;
#if KIND == 5
	if (bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &key,
				  sizeof(key)))
		slot->failed++;
#elif KIND == 6
	if (bpf_ringbuf_output(&events, &key, sizeof(key), 0))
		slot->failed++;
#else
	slot->pad[0] += key;
#endif
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
