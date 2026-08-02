// SPDX-License-Identifier: GPL-2.0
// Matched kernel-BPF control/real programs for array update value-size sweeps.

#define BPF_NO_GLOBAL_DATA
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>

#define DEFINE_VALUE_TYPE(bits, bytes)                                        \
	struct value_##bits {                                                   \
		u8 data[bytes];                                                   \
	}

DEFINE_VALUE_TYPE(8, 8);
DEFINE_VALUE_TYPE(16, 16);
DEFINE_VALUE_TYPE(32, 32);
DEFINE_VALUE_TYPE(64, 64);
DEFINE_VALUE_TYPE(128, 128);
DEFINE_VALUE_TYPE(256, 256);

#define DEFINE_SIZE_CASE(bits)                                                \
	struct {                                                               \
		__uint(type, BPF_MAP_TYPE_ARRAY);                                \
		__uint(max_entries, 1024);                                       \
		__type(key, u32);                                                 \
		__type(value, struct value_##bits);                               \
	} kaus_map_##bits SEC(".maps");                                        \
	                                                                         \
	SEC("uprobe")                                                           \
	int kaus_##bits##_ctrl(struct pt_regs *ctx)                              \
	{                                                                        \
		struct value_##bits value = {};                                   \
		for (int i = 0; i < 1000; ++i) {                                  \
			u32 key = i;                                                \
			value.data[0] = (u8)i;                                      \
			void *key_pointer = &key;                                   \
			void *value_pointer = &value;                               \
			barrier_var(key_pointer);                                   \
			barrier_var(value_pointer);                                 \
			barrier();                                                  \
		}                                                                \
		return 0;                                                        \
	}                                                                        \
	                                                                         \
	SEC("uprobe")                                                           \
	int kaus_##bits##_real(struct pt_regs *ctx)                              \
	{                                                                        \
		struct value_##bits value = {};                                   \
		for (int i = 0; i < 1000; ++i) {                                  \
			u32 key = i;                                                \
			value.data[0] = (u8)i;                                      \
			bpf_map_update_elem(&kaus_map_##bits, &key, &value,          \
					    BPF_ANY);                                  \
		}                                                                \
		return 0;                                                        \
	}

DEFINE_SIZE_CASE(8);
DEFINE_SIZE_CASE(16);
DEFINE_SIZE_CASE(32);
DEFINE_SIZE_CASE(64);
DEFINE_SIZE_CASE(128);
DEFINE_SIZE_CASE(256);

char LICENSE[] SEC("license") = "GPL";
