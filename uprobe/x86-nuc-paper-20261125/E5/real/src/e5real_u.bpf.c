// E5-real bpftime side: program "probe_u", uprobe on libc write in every process that runs the agent. Same record
// as e5real_k.bpf.c, no cgroup filter (only agent processes are hooked; the helper is not needed).
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "e5real.h"

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, E5REAL_MAX);
	__type(key, struct rkey);
	__type(value, struct rrec);
} recs SEC(".maps");

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

SEC("uprobe")
int BPF_UPROBE(probe_u, __u64 fd, __u64 buf, __u64 count)
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
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
