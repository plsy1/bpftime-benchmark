#define BPF_NO_GLOBAL_DATA
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

/* th-hostcall: a CPU-style counter. One lookup on an ordinary HASH map per GPU
 * thread: on bpftime dev that lookup is a synchronous host call. The loader
 * inserts key 0 before attaching, so every lookup hits and the atomic add
 * through the returned pointer counts every thread. */
struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 1);
	__type(key, u32);
	__type(value, u64);
} call_count SEC(".maps");

SEC("kretprobe/_Z12rms_norm_f32ILi512ELi1ELb1ELb1ELb0ELb0EEvPKfPfiiilllfS1_lll5uint3S3_S3_S3_S1_lllS3_S3_S3_S3_f")
int cuda__retprobe()
{
	u32 key = 0;
	u64 *cnt = bpf_map_lookup_elem(&call_count, &key);
	if (cnt)
		__sync_fetch_and_add(cnt, 1);
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
