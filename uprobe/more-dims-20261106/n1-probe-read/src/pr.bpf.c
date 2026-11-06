// bpf_probe_read cost and errors (more-dims-20261106/n1-probe-read): a uprobe on probe_me(s).
// KIND 1: bpf_probe_read_user_str(buf, SIZE, s) (opensnoop, execsnoop, bashreadline),
//      2: bpf_probe_read_user(buf, SIZE, s) (sslsniff's buffers), 0: control (no read).
// The buffer is a per-CPU array value of SIZE bytes. Slots (first .bss object, per thread):
// 0 hits, 1 reads that failed, 2 the last error (positive), 3 bytes returned.
#include <linux/bpf.h>
#include <asm/ptrace.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#ifndef KIND
#define KIND 1
#endif
#ifndef SIZE
#define SIZE 256
#endif

struct big {
	char b[SIZE];
};
struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(max_entries, 1);
	__type(key, __u32);
	__type(value, struct big);
} heap SEC(".maps");

struct slot {
	__u64 hits;
	__u64 failed;
	__u64 last_err;
	__u64 bytes;
	__u64 pad[4];
};
struct slot slots[64];

SEC("uprobe")
int BPF_UPROBE(on_probe, const char *s)
{
	__u32 tid = (__u32)bpf_get_current_pid_tgid();
	struct slot *slot = &slots[(tid & 31) * 2];
	slot->hits++;
#if KIND != 0
	__u32 zero = 0;
	struct big *b = bpf_map_lookup_elem(&heap, &zero);
	if (!b)
		return 0;
#if KIND == 1
	long r = bpf_probe_read_user_str(b->b, SIZE, s);
#else
	long r = bpf_probe_read_user(b->b, SIZE, s);
#endif
	if (r < 0) {
		slot->failed++;
		slot->last_err = -r;
	} else {
		slot->bytes += r;
	}
#endif
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
