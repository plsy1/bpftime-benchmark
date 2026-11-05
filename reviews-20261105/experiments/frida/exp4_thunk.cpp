// Gum resolves a function whose first instruction is `b target` to the
// target: a probe on the thunk is a listener on the target's Gum context.
// The thunk's slot is keyed by the thunk address and cannot park (its first
// word is a `b`), while the target's slot can. Parking the target's slot
// bypasses Gum for the target, so the thunk's probes stop firing although
// they are attached.
#include <frida_uprobe_attach_impl.hpp>
#include <frida_hook_slot.hpp>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace bpftime;
using namespace bpftime::attach;

extern "C" uint64_t thunk_target(uint64_t x);
extern "C" uint64_t thunk(uint64_t x);
asm(R"(
	.text
	.balign 16
	.globl thunk_target
	.type thunk_target, %function
thunk_target:
	mov x1, #0x55
	eor x0, x0, x1
	add x0, x0, x1
	mul x0, x0, x1
	ret
	.size thunk_target, .-thunk_target
	.balign 16
	.globl thunk
	.type thunk, %function
thunk:
	b thunk_target
	.size thunk, .-thunk
)");

static int body()
{
	frida_attach_impl man;
	uint64_t (*volatile tgt)(uint64_t) = thunk_target;
	uint64_t (*volatile thk)(uint64_t) = thunk;
	std::atomic<int> hits_target{ 0 }, hits_thunk{ 0 };
	int id_target = man.create_uprobe_at(
		(void *)thunk_target, [&](const pt_regs &) { hits_target++; });
	int id_thunk = man.create_uprobe_at(
		(void *)thunk, [&](const pt_regs &) { hits_thunk++; });
	printf("ids: target=%d thunk=%d\n", id_target, id_thunk);
	thk(1);
	printf("after one call of thunk: hits_target=%d hits_thunk=%d (Gum follows the b: both 1)\n",
	       hits_target.load(), hits_thunk.load());
	tgt(2);
	printf("after one call of target: hits_target=%d hits_thunk=%d\n",
	       hits_target.load(), hits_thunk.load());
	man.detach_by_id(id_target);
	printf("target probe detached: target parked=%d thunk parked=%d\n",
	       (int)frida_hook_is_parked((void *)thunk_target),
	       (int)frida_hook_is_parked((void *)thunk));
	thk(3);
	printf("after one call of thunk with its probe still attached: hits_thunk=%d (expect 2)\n",
	       hits_thunk.load());
	man.detach_by_id(id_thunk);
	fflush(stdout);
	return 0;
}

int main()
{
	pid_t pid = fork();
	if (pid == 0) {
		int rc = body();
		fflush(stdout);
		_exit(rc);
	}
	int status = 0;
	waitpid(pid, &status, 0);
	if (WIFEXITED(status))
		printf("child exited %d\n", WEXITSTATUS(status));
	else
		printf("child killed by signal %d (%s)\n", WTERMSIG(status),
		       strsignal(WTERMSIG(status)));
	return 0;
}
