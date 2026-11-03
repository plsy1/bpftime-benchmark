// R1 in bpf's own hash map: two keys of one bucket updated at once while
// other CPUs look them up. Each program runs under BPF_PROG_TEST_RUN with a
// repeat count and does LOOPS operations per run (bpf_loop), so that the
// test-run overhead does not set the rate; readers count lookups that return
// NULL (a miss of a key that is present throughout) and values that are not
// {key, x, x}.
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

struct value {
	__u64 key, a, b;
};

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 64);
	__uint(map_flags, BPF_F_ZERO_SEED);
	__type(key, __u32);
	__type(value, struct value);
} table SEC(".maps");

struct stats {
	__u64 reads, misses, torn, updates;
};

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(max_entries, 1);
	__type(key, __u32);
	__type(value, struct stats);
} counts SEC(".maps");

const volatile __u32 key0 = 0, key1 = 0;
#define LOOPS 1000

static long update_cb(__u64 i, void *ctx)
{
	__u32 *key = ctx, zero = 0;
	struct stats *s = bpf_map_lookup_elem(&counts, &zero);
	if (!s)
		return 1;
	s->updates++;
	struct value v = { *key, s->updates, s->updates };
	bpf_map_update_elem(&table, key, &v, BPF_ANY);
	return 0;
}

SEC("xdp")
int update0(struct xdp_md *ctx)
{
	__u32 key = key0;
	bpf_loop(LOOPS, update_cb, &key, 0);
	return XDP_PASS;
}

SEC("xdp")
int update1(struct xdp_md *ctx)
{
	__u32 key = key1;
	bpf_loop(LOOPS, update_cb, &key, 0);
	return XDP_PASS;
}

static __always_inline void read_one(struct stats *s, __u32 key)
{
	struct value *v = bpf_map_lookup_elem(&table, &key);
	s->reads++;
	if (!v) {
		s->misses++;
		return;
	}
	__u64 k = *(volatile __u64 *)&v->key, a = *(volatile __u64 *)&v->a,
	      b = *(volatile __u64 *)&v->b;
	if (k != key || a != b)
		s->torn++;
}

static long read_cb(__u64 i, void *ctx)
{
	__u32 zero = 0;
	struct stats *s = bpf_map_lookup_elem(&counts, &zero);
	if (!s)
		return 1;
	read_one(s, key0);
	read_one(s, key1);
	return 0;
}

SEC("xdp")
int reader(struct xdp_md *ctx)
{
	bpf_loop(LOOPS, read_cb, NULL, 0);
	return XDP_PASS;
}

char LICENSE[] SEC("license") = "GPL";
