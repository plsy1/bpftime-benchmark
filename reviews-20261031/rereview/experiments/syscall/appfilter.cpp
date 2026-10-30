// lazy mode: the application installs its own seccomp RET_TRAP filter for
// getppid; the SIGSYS it produces comes from the hook's own svc.
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <linux/audit.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <csignal>
#include <cstdio>
#include <cstddef>
#include <cstdlib>
int main() {
	printf("getppid before: %ld\n", (long)getppid()); fflush(stdout);
	sock_filter f[] = {
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, nr)),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_getppid, 0, 1),
		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_TRAP),
		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
	};
	sock_fprog prog = { 4, f };
	prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	if (syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, 0, &prog) != 0) { perror("seccomp"); return 2; }
	printf("app filter installed; calling getppid\n"); fflush(stdout);
	long r = getppid();
	printf("getppid after: %ld (still alive)\n", r); fflush(stdout);
	printf("write after: ok\n"); fflush(stdout);
	return 0;
}
