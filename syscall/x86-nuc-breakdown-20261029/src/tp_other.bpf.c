// An empty program on a system call the workload never makes (getppid): the
// agent's dispatch runs for every system call but finds no program for write.
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

SEC("tracepoint/syscalls/sys_enter_getppid")
int on_other(void *ctx)
{
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
