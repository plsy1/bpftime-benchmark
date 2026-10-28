// Counts write(2) calls on fd 42 at sys_enter_write, and their 7-byte results
// at sys_exit_write. Context layouts of the syscalls tracepoints: common
// header (8 bytes), syscall number, then the arguments / the return value.
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

struct sys_enter_ctx {
	unsigned long long common;
	long id;
	unsigned long args[6];
};
struct sys_exit_ctx {
	unsigned long long common;
	long id;
	long ret;
};

// .bss: 0 enters on fd 42, 1 exits returning 7, 2 enters seen at all.
unsigned long long counts[8];

SEC("tracepoint/syscalls/sys_enter_write")
int on_enter(struct sys_enter_ctx *ctx)
{
	__sync_fetch_and_add(&counts[2], 1);
	if (ctx->args[0] == 42 && ctx->args[2] == 7)
		__sync_fetch_and_add(&counts[0], 1);
	return 0;
}

SEC("tracepoint/syscalls/sys_exit_write")
int on_exit(struct sys_exit_ctx *ctx)
{
	if (ctx->ret == 7)
		__sync_fetch_and_add(&counts[1], 1);
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
