// One empty program on sys_exit_write, as the official benchmark/syscall.
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

SEC("tracepoint/syscalls/sys_exit_write")
int on_exit_write(void *ctx)
{
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
