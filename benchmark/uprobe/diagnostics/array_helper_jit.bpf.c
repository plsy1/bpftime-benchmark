// SPDX-License-Identifier: MIT
// The two programs intentionally call the same synthetic helper ID. The host
// harness registers either a no-op function or bpftime's real array helper at
// that ID, keeping the generated BPF instruction stream identical.

typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

#define SEC(name) __attribute__((section(name), used))

static uint64_t (*const diagnostic_helper)(uint64_t, uint64_t, uint64_t,
					    uint64_t, uint64_t) =
	(void *)1000;

SEC("uprobe/jit_lookup")
int jit_lookup(void *ctx)
{
#pragma clang loop unroll(disable)
	for (uint32_t i = 0; i < 1000; ++i) {
		uint32_t key = i;
		diagnostic_helper(3, (uint64_t)&key, 0, 0, 0);
	}
	return ctx != (void *)0 ? 0 : 0;
}

SEC("uprobe/jit_update")
int jit_update(void *ctx)
{
#pragma clang loop unroll(disable)
	for (uint32_t i = 0; i < 1000; ++i) {
		uint32_t key = i;
		uint64_t value = i;
		diagnostic_helper(3, (uint64_t)&key, (uint64_t)&value, 0, 0);
	}
	return ctx != (void *)0 ? 0 : 0;
}

SEC("uprobe/jit_delete")
int jit_delete(void *ctx)
{
#pragma clang loop unroll(disable)
	for (uint32_t i = 0; i < 1000; ++i) {
		uint32_t key = i;
		diagnostic_helper(3, (uint64_t)&key, 0, 0, 0);
	}
	return ctx != (void *)0 ? 0 : 0;
}

char LICENSE[] SEC("license") = "MIT";
