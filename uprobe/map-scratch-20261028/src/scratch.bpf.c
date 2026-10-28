// Large per-CPU scratch value (sslsniff pattern): send_event(buf, len, tag) is probed; the
// program looks up the single 8 KB event record of a per-CPU array, fills the header with the
// same helpers as sslsniff (time, pid/tid, uid, comm) and copies min(len, 8192) bytes of the
// caller's buffer into it with bpf_probe_read_user. It then checks its copy: the first and the
// last 8-byte word must equal tag, which the caller wrote there just before the call (a value
// shared with another thread, or a stale copy, would differ). No output, so only the map and
// the copy are measured. CONTROL: the same header helpers into a stack record, no map, no copy.
#include <linux/bpf.h>
#include <asm/ptrace.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#define MAX_BUF 8192

struct event {
	__u64 timestamp_ns;
	__u64 delta_ns;
	__u32 pid;
	__u32 tid;
	__u32 uid;
	__u32 len;
	__u32 buf_filled;
	__u32 rw;
	char comm[16];
	__u8 buf[MAX_BUF];
};

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(max_entries, 1);
	__type(key, __u32);
	__type(value, struct event);
} scratch SEC(".maps");

// Per-thread result slots (see attach.c): 0 probe hits, 1 failed lookups, 2 bad copies.
struct slot {
	__u64 hits;
	__u64 failed;
	__u64 bad;
	__u64 pad[5];
};
struct slot slots[64];

SEC("uprobe")
int BPF_UPROBE(on_send, const void *ubuf, __u64 len, __u64 tag)
{
	__u64 pid_tgid = bpf_get_current_pid_tgid();
	__u32 tid = (__u32)pid_tgid;
	struct slot *slot = &slots[(tid & 31) * 2];
	slot->hits++;
#ifdef CONTROL
	struct {
		__u64 ts;
		__u32 pid, tid, uid, len;
		char comm[16];
	} h;
	h.ts = bpf_ktime_get_ns();
	h.pid = pid_tgid >> 32;
	h.tid = tid;
	h.uid = bpf_get_current_uid_gid();
	h.len = len;
	bpf_get_current_comm(&h.comm, sizeof(h.comm));
	slot->pad[0] += h.ts + h.uid + h.comm[0];
#else
	__u32 zero = 0;
	struct event *e = bpf_map_lookup_elem(&scratch, &zero);
	if (!e) {
		slot->failed++;
		return 0;
	}
	e->timestamp_ns = bpf_ktime_get_ns();
	e->delta_ns = 0;
	e->pid = pid_tgid >> 32;
	e->tid = tid;
	e->uid = bpf_get_current_uid_gid();
	e->len = len;
	e->buf_filled = 0;
	e->rw = 1;
	bpf_get_current_comm(&e->comm, sizeof(e->comm));
	__u32 n = len > MAX_BUF ? MAX_BUF : len;
	if (n < 8)
		return 0;
	if (bpf_probe_read_user(e->buf, n, ubuf)) {
		slot->failed++;
		return 0;
	}
	e->buf_filled = 1;
	__u32 last = (n - 8) & (MAX_BUF - 8);
	if (*(__u64 *)e->buf != tag || *(__u64 *)(e->buf + last) != tag)
		slot->bad++;
#endif
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
