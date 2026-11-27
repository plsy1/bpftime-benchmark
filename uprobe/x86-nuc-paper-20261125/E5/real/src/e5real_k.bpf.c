// E5-real kernel side. Program "truth": tracepoint syscalls/sys_enter_write (the truth for every leg).
// Program "probe_k": uprobe on libc write (control leg Kc only). Both record only tasks in the cgroup whose id is
// cfg[0] (the workload's cgroup v2; set by the loader after load, before attach). The loader attaches ONE program
// per process, so each role has its own recs map.
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "e5real.h"

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, __u32);
	__type(value, __u64);
} cfg SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, E5REAL_MAX);
	__type(key, struct rkey);
	__type(value, struct rrec);
} recs SEC(".maps");

// 0: calls recorded (in cgroup), 1: insert failed (map full or key clash)
struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 2);
	__type(key, __u32);
	__type(value, __u64);
} stats SEC(".maps");

static __always_inline void bump(__u32 idx)
{
	__u64 *v = bpf_map_lookup_elem(&stats, &idx);
	if (v)
		__sync_fetch_and_add(v, 1);
}

static __always_inline int in_cgroup(void)
{
	__u32 z = 0;
	__u64 *c = bpf_map_lookup_elem(&cfg, &z);
	return c && *c && bpf_get_current_cgroup_id() == *c;
}

static __always_inline void record(__u64 fd, __u64 buf, __u64 count)
{
	struct rrec r = {};
	struct rkey k = {};
	r.t = bpf_ktime_get_ns();
	r.pid_tgid = bpf_get_current_pid_tgid();
	r.uid_gid = bpf_get_current_uid_gid();
	bpf_get_current_comm(r.comm, sizeof(r.comm));
	r.fd = fd;
	r.buf = buf;
	r.count = count;
	k.t = r.t;
	k.pid_tgid = r.pid_tgid;
	bump(0);
	if (bpf_map_update_elem(&recs, &k, &r, BPF_NOEXIST) != 0)
		bump(1);
}

SEC("tracepoint/syscalls/sys_enter_write")
int truth(struct trace_event_raw_sys_enter *ctx)
{
	if (!in_cgroup())
		return 0;
	record(ctx->args[0], ctx->args[1], ctx->args[2]);
	return 0;
}

SEC("uprobe")
int BPF_UPROBE(probe_k, __u64 fd, __u64 buf, __u64 count)
{
	if (!in_cgroup())
		return 0;
	record(fd, buf, count);
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
