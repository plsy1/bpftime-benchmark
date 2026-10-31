#define _GNU_SOURCE
#include <linux/bpf.h>
#include <linux/perf_event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/syscall.h>
static long bpf(int cmd, union bpf_attr *a) { return syscall(__NR_bpf, cmd, a, sizeof(*a)); }
int main(int argc, char **argv) {
	const char *target = argc > 1 ? argv[1] : "/bin/true";
	unsigned long off = argc > 2 ? strtoul(argv[2], 0, 0) : 0x1000;
	int delay = getenv("DELAY") ? atoi(getenv("DELAY")) : 0;
	int secs = getenv("SECS") ? atoi(getenv("SECS")) : 30;
	union bpf_attr a; memset(&a, 0, sizeof(a));
	a.map_type = BPF_MAP_TYPE_ARRAY; a.key_size = 4; a.value_size = 8; a.max_entries = 1;
	strcpy(a.map_name, "cnt");
	int mfd = bpf(BPF_MAP_CREATE, &a);
	printf("map fd=%d errno=%d\n", mfd, errno); fflush(stdout);
	if (delay) { printf("sleep %d before prog/link\n", delay); fflush(stdout); sleep(delay); }
	struct bpf_insn insns[] = {
		{0x62, 10, 0, -4, 0},            /* *(u32*)(r10-4)=0 */
		{0xbf, 2, 10, 0, 0},             /* r2=r10 */
		{0x07, 2, 0, 0, -4},             /* r2+=-4 */
		{0x18, 1, 1, 0, mfd}, {0,0,0,0,0},/* lddw r1, map fd */
		{0x85, 0, 0, 0, 1},              /* call 1 */
		{0x15, 0, 0, 3, 0},              /* if r0==0 goto +3 */
		{0x79, 1, 0, 0, 0},              /* r1=*(u64*)(r0) */
		{0x07, 1, 0, 0, 1},              /* r1+=1 */
		{0x7b, 0, 1, 0, 0},              /* *(u64*)(r0)=r1 */
		{0xb7, 0, 0, 0, 0},              /* r0=0 */
		{0x95, 0, 0, 0, 0},
	};
	memset(&a, 0, sizeof(a));
	a.prog_type = BPF_PROG_TYPE_KPROBE; a.insns = (uintptr_t)insns; a.insn_cnt = sizeof(insns)/sizeof(insns[0]);
	a.license = (uintptr_t)"GPL"; strcpy(a.prog_name, "count");
	int pfd = bpf(BPF_PROG_LOAD, &a);
	printf("prog fd=%d errno=%d\n", pfd, errno); fflush(stdout);
	int type = -1; FILE *f = fopen("/sys/bus/event_source/devices/uprobe/type", "r");
	if (f) { fscanf(f, "%d", &type); fclose(f); }
	printf("uprobe type=%d\n", type);
	struct perf_event_attr pa; memset(&pa, 0, sizeof(pa));
	pa.size = sizeof(pa); pa.type = type; pa.config1 = (uintptr_t)target; pa.config2 = off;
	int efd = syscall(__NR_perf_event_open, &pa, -1, 0, -1, PERF_FLAG_FD_CLOEXEC);
	printf("perf fd=%d errno=%d\n", efd, errno);
	memset(&a, 0, sizeof(a));
	a.link_create.prog_fd = pfd; a.link_create.target_fd = efd; a.link_create.attach_type = BPF_PERF_EVENT;
	int lfd = bpf(BPF_LINK_CREATE, &a);
	printf("link fd=%d errno=%d pid=%d\n", lfd, errno, getpid()); fflush(stdout);
	for (int i = 0; i < secs; i++) {
		uint32_t k = 0; uint64_t v = 0;
		memset(&a, 0, sizeof(a)); a.map_fd = mfd; a.key = (uintptr_t)&k; a.value = (uintptr_t)&v;
		long r = bpf(BPF_MAP_LOOKUP_ELEM, &a);
		printf("t=%d lookup r=%ld counter=%lu\n", i, r, v); fflush(stdout);
		sleep(1);
	}
	return 0;
}
