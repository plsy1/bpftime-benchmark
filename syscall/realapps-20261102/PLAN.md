# System call rewriting on real runtimes: Go, JIT runtimes, temporary masks (2026-11-02)

User request 11-02: check the correctness of the aarch64 system call rewriting in three
scenarios left open by the coverage round (`../coverage-20261030`): Go goroutine stacks, real
JIT runtimes (Node, Java) in default and lazy mode, and the temporary signal masks of
ppoll-style calls in lazy mode. Survey done on dev a4ac0d31 (`build-jetson-daily`; the
transformer is unchanged since 10-31), Go 1.22.2, Node 18.19.1, OpenJDK 17.0.20.1.

## Findings

### 1. Go: every program crashes once the agent is loaded (confirmed)

| Run (`go/`) | dummy agent | real agent, no programs | real agent + syscount |
|---|---|---|---|
| `smoke-dyn` (64 goroutines: getpid, pipe, stat) | 3/3 ok | 3/3 crash | 3/3 crash |
| `deep/deep` (system call at every call depth, canaries) | 5/5 ok | 3/3 crash | 3/3 crash |
| `grow/grow` (= smoke, each goroutine grows its stack to ~128 KiB first, GC off) | - | 3/3 ok | 3/3 ok, traced (syscount: getpid 2.7M, write 3.2M) |

Crash signatures: `unexpected return pc ... fatal error: unknown caller pc`, `unexpected fault
address`, `bad sequence number`, SIGSEGV (rc 139): a neighbouring goroutine's frames
overwritten.

Mechanism (confirmed by the grow variant): Go issues system calls from its own runtime
(`runtime/internal/syscall.Syscall6`, NOSPLIT, frame 0, `svc` in the binary's text; 37 sites,
rewritten) on the goroutine stack. Goroutine stacks start at 2 KiB, have no guard page, and
guarantee only about `stackGuard` = 800 + 128 = 928 bytes below sp minus the NOSPLIT chain
above `Syscall6`. The hook writes below the application's sp (`stackuse/`, a getpid with sp
in a pattern-filled buffer):

| Hook configuration | bytes below the caller's sp |
|---|---:|
| dummy agent | 880 |
| real agent, no program matches | 1,376 |
| real agent, syscount attached | 18,144 (first call 18,208) |

The 18 KB is the LLVM JIT's per-invocation stack: outside `kernel_compatible_mode` (never
enabled by bpftime; that mode also changes the function signature, map_val and tail calls, so it
is not a stack switch) it allocas `64 * MAX_LOCAL_FUNC_DEPTH (32) + 10` u64 (16.5 KB) plus a
320-pointer call stack (2.5 KB) on every run. CLAUDE.md ("about 1.3 KiB with the agent's
callback, plus the BPF stack") understates it: the "plus" is 17 KB.

Not affected: static Go binaries (`CGO_ENABLED=0`, `smoke-static`) are never rewritten, since
LD_PRELOAD does not load into them (untraced, as on x86). cgo calls into libc run on the
system stack (g0, the pthread stack).

Likely also affected (not tested): x86 (zpoline frame with XSAVE plus the same JIT stack), Frida
uprobes on Go functions (programs run on the goroutine stack), and any traced call from a
signal handler on a small sigaltstack (16 KiB SIGSTKSZ-sized stacks < 18 KB).

### 2. Node and Java: pass (default and lazy mode)

`jit/io.js` (JIT hot loop, 2000 statSync, 200 readFile on the libuv pool) and `jit/Io.java`
(JIT hot loop, 8 threads x 2000 file reads), real agent + syscount, output checked:

| Run | default | lazy |
|---|---|---|
| node | ok; statx 2114, pread 203 traced | ok; statx 2010 |
| java | ok; openat 15878, read 46637 traced | ok; openat 15863 |

Their threads have large stacks, and V8/HotSpot make system calls through libc (rewritten).
Neither JIT emits raw `svc`, so lazy mode's trap-and-rewrite of real JIT code was not exercised:
the lazy pass shows only that the filter does not break them (no execve in these runs; Java's
ProcessBuilder via jspawnhelper would be refused, as documented).

### 3. Lazy mode, temporary masks (code reading; reproducer is step 3)

`sanitize_signal_call` strips SIGSYS from `rt_sigprocmask` sets and `rt_sigaction` masks only.
Not handled: `ppoll` (arg 4), `epoll_pwait` / `epoll_pwait2` (arg 5), `rt_sigsuspend` (arg 1),
`pselect6` (arg 6: a pointer to `{sigset_t *, size_t}`), `io_uring_enter` (arg 5, or inside the
`IORING_ENTER_EXT_ARG` struct). A handler interrupting such a wait that reaches a never-rewritten
`svc` traps with SIGSYS blocked: the kernel kills the process. Also: the sanitizer reads the
`rt_sigprocmask` set and the `rt_sigaction` struct without checking the pointer; a bad pointer is
EFAULT natively and SIGSEGV in the hook under lazy mode. Lazy mode is off by default (research).

## Plan

### Step 1. Go: run the hook on its own stack (default mode)

`syscall_hooker_asm` switches to a per-thread hook stack before saving anything: the gate has
already saved x16/x17, so they are free; read the thread's hook-stack bounds from initial-exec
TLS; if sp is already inside them (a nested entry: the hook's own calls, or a signal handler on
that stack whose call is traced) stay there, otherwise switch to the top; save the application
sp in the frame and switch back after restoring. Details to settle:

- Allocation: a 64 KiB mapping per thread (JIT 19 KB + agent + nesting), allocated by the C++
  side when a thread's pointer is null; until then the call runs on the caller's stack as today
  (a new thread's first system calls run on its system stack: glibc's thread start, Go's g0 /
  pthread stack under cgo). Reuse after thread exit through a free list keyed by tid liveness
  (as the ringbuf owner slots), or leak 64 KiB per exited thread: decide by measuring thread
  churn (nginx workers, Go Ms are long-lived).
- Unwinding: the CFA (application sp) is then loaded from the frame (`DW_CFA_def_cfa_expression`
  deref), the return-address expression unchanged. The existing tests (`pthread_cancel` through a
  hooked call, backtraces from a signal handler, the namespace unwind) must still pass.
- Signals: a signal arriving while the hook runs is delivered on the hook stack (or the
  application's sigaltstack, which Go always sets); Go's handler sees a non-Go pc, which is never
  an async-preemption point. Verify with SIGURG-heavy Go runs.
- The fork child inherits the hook stack mapping and the TLS pointer (same thread): fine. vfork
  / CLONE_VM children share it while the parent is suspended: fine (clone takes the direct path).

Tests (transformer, each with a mutation that must fail):
- a call made with sp 256 bytes above an unmapped page (mmap'd guard page below a small stack)
  must not fault; mutation: no switch.
- nested: a signal handler interrupting a blocking hooked call makes a traced call; both return
  correct results and the outer frame is intact; mutation: always switch to the top.
- the existing unwind tests unchanged.
End to end: `go/smoke-dyn`, `go/deep` (canaries), a Go HTTP server under syscount (a few minutes,
many goroutines, GC on), 20 runs each with the agent and syscount: 0 crashes, 0 corrupted
canaries, counts against strace. Cost: hook dispatch with `run_layers` (no program, write),
expected +1-2 ns; nginx under syscount unchanged.

Fallback if the switch cannot be made safe: leave Go modules' text unrewritten (detect
`.note.go.buildid`) with a warning naming them: no crash, but Go's own system calls (most of a Go
program's) are untraced.

### Step 2. JIT stack size (optional, after step 1)

Allocate the per-invocation stack the program needs (512 bytes times the deepest local-call chain
found at load, plus its call-stack entries) instead of 32 levels. Reduces 18 KB to about 1 KB for
programs without local calls, which also helps Frida uprobes on small stacks and handlers on
sigaltstacks. llvmbpf change: through the parent-owned overlay (like
`local-call-return-block.patch`). Test: the JIT's frame size for a program with 0, 1, 3 local-call
levels; the existing local-call tests. Only if step 1 lands or the user wants the Frida side.

### Step 3. Lazy mode: temporary masks and bad pointers

Reproducer first (`masks/`): lazy mode, an `svc` in an anonymous page (never rewritten), `ppoll`
with SIGSYS in its mask, a SIGALRM handler issuing that `svc` during the wait: killed by SIGSYS
today. Then in `sanitize_signal_call`: copy the mask without SIGSYS for `ppoll`, `epoll_pwait`,
`epoll_pwait2`, `rt_sigsuspend`, `pselect6` (copy the `{ptr, len}` struct too) and
`io_uring_enter` (both forms); read every application pointer with `process_vm_readv`-free
checks (a fault-safe copy, or leave the call unchanged when the pointer is not readable, so the
kernel returns EFAULT). Tests: one per call (handler reaches an unrewritten svc during the wait)
and a bad-pointer `rt_sigprocmask` returning EFAULT; mutations: drop each case.

### Step 4. Documentation

CLAUDE.md: the hook's stack use (numbers above), the Go result, the hook stack, the lazy-mode
masks; remove "not tested with Go". Results README with all runs.

## Order and size

Step 1 (2-3 days, the real default-mode crash) -> step 3 (1 day, lazy only) -> step 4; step 2 only
on request. Each fix: test with mutation check, candidate gate, hook cost A/B; push and the daily
patch list only after asking. Out of scope: making lazy mode general-purpose, Go static binaries,
Frida uprobes on Go (recorded), x86 (ask the NUC to run `go/` once step 1 exists).
