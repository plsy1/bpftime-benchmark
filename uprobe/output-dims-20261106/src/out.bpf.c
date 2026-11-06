// Event output on a uprobe, by path and event size (output-dims-20261106).
// count_event(key) sends a SIZE-byte record whose first word is key.
// VAR=1 (more-dims-20261106): the record size is one of 72, 520, 1480, 3992 bytes chosen from
// the key's hash (bpf_buffer's tools send what they filled: execsnoop, sslsniff); SIZE is then
// the buffer (4096). Reserve takes a constant size, so rbres has a call site per size.
// KIND: 5 bpf_perf_event_output into a PERF_EVENT_ARRAY (BPF_F_CURRENT_CPU),
//       6 bpf_ringbuf_output into a RINGBUF,
//       7 bpf_ringbuf_reserve + bpf_ringbuf_submit (opensnoop's path),
//       0 control: the probe and bpf_get_current_pid_tgid only,
//       8 soak (more-dims-20261106/n6): a 65,536-entry hash map kept about half full by churn
//         (update key k, delete k ^ 0x8000; failed updates in slot 2) and a SIZE-byte record by
//         reserve + submit.
// Records above 256 bytes are built in a per-CPU array, as libbpf-tools'
// bpf_buffer does for perf buffers. RB_SIZE: the ring buffer, 256 KiB as
// libbpf-tools'. Per-thread slots: 0 probe hits, 1 failed outputs.
#include <linux/bpf.h>
#include <asm/ptrace.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#ifndef KIND
#define KIND 5
#endif
#ifndef SIZE
#define SIZE 8
#endif
#ifndef VAR
#define VAR 0
#endif
#ifndef RB_SIZE
#define RB_SIZE (256 * 1024)
#endif

struct record {
	__u64 key;
	__u8 rest[SIZE - 8];
};

#if KIND == 5
struct {
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__uint(key_size, sizeof(__u32));
	__uint(value_size, sizeof(__u32));
} events SEC(".maps");
#elif KIND == 6 || KIND == 7 || KIND == 8
struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, RB_SIZE);
} events SEC(".maps");
#endif
#if KIND == 8
struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 65536);
	__type(key, __u32);
	__type(value, __u64);
} churn SEC(".maps");
#endif

#if SIZE > 256 && (KIND == 5 || KIND == 6)
struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(max_entries, 1);
	__type(key, __u32);
	__type(value, struct record);
} heap SEC(".maps");
#endif

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
	slot->hits++;
#if KIND == 5 || KIND == 6
#if SIZE > 256
	__u32 zero = 0;
	struct record *r = bpf_map_lookup_elem(&heap, &zero);
	if (!r) {
		slot->failed++;
		return 0;
	}
#else
	struct record rec;
	struct record *r = &rec;
	__builtin_memset(r, 0, sizeof(rec));
#endif
	r->key = arg;
#if VAR
	__u64 sel = (arg * 0x9E3779B97F4A7C15ULL) >> 62;
	__u32 len = sel == 0 ? 72 : sel == 1 ? 520 : sel == 2 ? 1480 : 3992;
#else
	__u32 len = sizeof(*r);
#endif
#if KIND == 5
	if (bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, r, len))
		slot->failed++;
#else
	if (bpf_ringbuf_output(&events, r, len, 0))
		slot->failed++;
#endif
#elif KIND == 7 && VAR
	__u64 sel = (arg * 0x9E3779B97F4A7C15ULL) >> 62;
	__u64 *r;
	if (sel == 0)
		r = bpf_ringbuf_reserve(&events, 72, 0);
	else if (sel == 1)
		r = bpf_ringbuf_reserve(&events, 520, 0);
	else if (sel == 2)
		r = bpf_ringbuf_reserve(&events, 1480, 0);
	else
		r = bpf_ringbuf_reserve(&events, 3992, 0);
	if (!r) {
		slot->failed++;
		return 0;
	}
	*r = arg;
	bpf_ringbuf_submit(r, 0);
#elif KIND == 8
	__u32 k = (__u32)(arg >> 5) & 0xffff, k2 = k ^ 0x8000;
	if (bpf_map_update_elem(&churn, &k, &arg, BPF_ANY))
		slot->pad[0]++;
	bpf_map_delete_elem(&churn, &k2);
	struct record *r = bpf_ringbuf_reserve(&events, sizeof(*r), 0);
	if (!r) {
		slot->failed++;
		return 0;
	}
	r->key = arg;
	bpf_ringbuf_submit(r, 0);
#elif KIND == 7
	struct record *r = bpf_ringbuf_reserve(&events, sizeof(*r), 0);
	if (!r) {
		slot->failed++;
		return 0;
	}
	r->key = arg;
	bpf_ringbuf_submit(r, 0);
#else
	slot->pad[0] += arg;
#endif
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
