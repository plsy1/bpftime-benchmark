// E5 audit probe: one uprobe program, loaded twice (kernel tool on audit_point_k, bpftime tool
// on audit_point_u). Records {pid_tgid, uid_gid, comm} per audit sequence number (first
// argument) into a hash map keyed by seq. A second hit on the same seq increments `hits`.
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

struct rec {
	__u64 pid_tgid;
	__u64 uid_gid;
	char comm[16];
	__u64 hits;
};

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 262144);
	__type(key, __u64);
	__type(value, struct rec);
} recs SEC(".maps");

// 0: calls, 1: duplicate seq, 2: update failed and no entry found
struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 4);
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
int BPF_UPROBE(audit, __u64 seq)
{
	struct rec r = {};
	r.pid_tgid = bpf_get_current_pid_tgid();
	r.uid_gid = bpf_get_current_uid_gid();
	bpf_get_current_comm(r.comm, sizeof(r.comm));
	r.hits = 1;
	bump(0);
	if (bpf_map_update_elem(&recs, &seq, &r, BPF_NOEXIST) != 0) {
		struct rec *old = bpf_map_lookup_elem(&recs, &seq);
		if (old) {
			old->hits++;
			bump(1);
		} else {
			bump(2);
		}
	}
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
