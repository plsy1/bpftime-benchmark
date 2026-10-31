#include "text_segment_transformer.hpp"
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cerrno>
#include <stddef.h>
static bool install_filter(int nr, uint32_t action)
{
	sock_filter f[] = {
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, nr)),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, (uint32_t)nr, 0, 1),
		BPF_STMT(BPF_RET | BPF_K, action),
		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
	};
	sock_fprog prog = { sizeof(f) / sizeof(f[0]), f };
	return prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == 0 &&
	       syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, 0, &prog) == 0;
}
constexpr uint32_t SVC_0 = 0xd4000001, RET = 0xd65f03c0;
constexpr uint32_t mov_x8(uint32_t nr) { return 0xd2800008 | (nr << 5); }
typedef long (*code_fn)(void);
