# Fixes for the fifth review and problem 13 (2026-11-05, dev 94f9ba8f -> d46ceb8f)

The plan: every high and medium finding of `../README.md`, problem 13 (a ring
buffer submit did not wake the consumer, `../../uprobe/opensnoop-loss-20261105`)
and the low findings, each as one dev commit with a test that fails with the fix
reverted (the mutation check), hot paths measured. 15 commits on
`dev/uprobe-official-177a1dc465ff` after 94f9ba8f, local; migrated onto the
official 25e250d6 candidate as 200 commits (gate below).

## Commits

| # | Commit | Finding | Test (mutation result) |
|---|---|---|---|
| H1 | c6b240a9 frida: run the override from the dispatch, with per-call state | override detach killed the process (SIGABRT 3/3); also the set return value was a field shared by all callers | "Detaching an override while threads call the function keeps them running" (child status 134); "Concurrent override calls each return their own value" (68,951 wrong of 4.06M; in a child since 0a688c3d: 59,783 wrong, status 4) |
| H2 | ce06c39c transformer: mark the hook stack's top frame live in its own slot | `busy` cleared before the frame was read back: sigaltstack handler's hooked call hung the thread | "handlers on an alternate stack interrupting the hook leave its frame intact" (~340k signals, each landing unwound; mutation: hung, alarm 142). CFA rows checked with llvm-dwarfdump (`cfi/`) |
| 13 | 665aaa38 ringbuf: wake a consumer that has caught up with the submitted record | problem 13 | "Ring buffer records wake a consumer that caught up" (notify removed: 4 checks fail) |
| M1 | 5b603bd5 transformer: switch over a hook frame that was left without its epilogue | abandoned frame after vfork-style exec/exit or siglongjmp: 32 -> 1272 bytes below sp | "the hook stack is used again after a frame on it is abandoned" (always stay: 1312); "a handler on an auto-disarmed signal stack leaves the hook's live frame alone" (no autodisarm record: SIGSEGV) |
| - | 0a688c3d frida test: run the concurrent override test in a child | the H1 test in the parent made later fork children crash now and then (finding F1 below) | suite 6/6 after |
| M3 | 795d2545 frida: key hook slots by the address Gum instruments | `b` thunk's probe silenced by parking its target | "A probe on a thunk keeps firing after its target's probe is detached" (no hops: fails) |
| M4 | 54512e46 per-cpu hash: BPF_NOEXIST on an existing key fails before taking a slot | pool lock taken, E2BIG instead of EEXIST | "per-cpu hash BPF_NOEXIST on an existing key takes no slot" (waits; E2BIG) |
| M2 | 76fffb86 transformer: reuse the hook stacks of exited threads | hook stacks never reclaimed | "hook stacks of exited threads are used again" (110 -> 4,110 mappings); "a vfork child's exit leaves its parent's hook stack alone" (no owner check: SIGSEGV 3/3) |
| L1, L2 | 9137661f prog: handle libbpf's CO-RE poison and kfunc calls in dead code | `call 0xbad2310` and dead kfunc calls disabled programs | two tests (both -ENOEXEC with the fix reverted) |
| L3 | e7393eb8 prog: register the private call slots for AOT objects | AOT objects of rewritten programs did not load | "An AOT object of a program with poisoned calls loads" (load -1). Its first version passed without the fix: LLVM folded the call away behind a constant |
| L4 | a4b4142e maps: a frozen map refuses push, pop, lookup-and-delete and writable mmap | freeze checked only update/delete | "A frozen map refuses every syscall write" (9 of 18 fail); the mmap check has no unit test |
| L5-L7 | 91d69897 hash map: copy a deleted value out after unlocking; long-key replace test | tool fault under the stripe lock; memcmp replace path untested; leftover shm; stale wording | "...copies out after unlocking" (writer SIGALRM); "...replaces long keys in place without misses" (coverage: removing the memcmp fences does not fail it, 3/3) |
| L8 | 065528bb frida: decide the grace counter's flip on its parity | nested callback outside the grace period | by reading (no hook on the callback path to hit the window) |
| L9, L10 | acf04c2d frida: parked follows the patch store; slots keep the interceptor | `parked` out of step on an mprotect failure; slots depended on gum_init_embedded's reference | by reading (failures not injectable) |
| M5, C1, docs | d46ceb8f CLAUDE.md: the review's documentation findings and known limits | lookup_read claim; unfair stripe lock; Frida limits | - |

Not done in this round: L11 (the agent frees programs after a partially failed
detach; not reachable today, a test needs a fake attach impl), problem 14
(get_stack in syscall tracepoints), the override forwarder's 5 arguments, x86
parking.

## Verification

- **Unit tests** (build-jetson-daily at d46ceb8f):
  - runtime: every case in its own process as root (`regression/each.sh`): 261
    cases, 258 pass, 3 skipped (`[map-usage]`, built without
    BPFTIME_ENABLE_MAP_USAGE_STATS; Catch2 exits 4); root shm segments cleaned;
  - transformer: 55 cases, 54 pass, 1 fails as expected; `[lazy]` 18 pass;
  - Frida: 25 cases, 3/3 runs (6/6 after 0a688c3d);
  - daemon (root): 10 cases pass;
  - syscall trace (root): 7 of 9; the two failures use `fork`, which aarch64
    does not have (files untouched in this round; finding F3).
- **The review's experiments**, rebuilt or run with the new build:
  `experiments-rerun/results.txt`. Every high and medium one flips (exp5 3/3
  exit 0; e2e_plain race completes 50M iterations where 94f9ba8f stalled at
  58,742; spawn 944 -> 32; busy_stuck 1272 -> 32; thread_churn 32,854 -> 91
  mappings; exp4 thunk probe fires; e1 EEXIST without waiting).
- **Go end to end** (`go-exec/`): goexec (8 execs per round from goroutines,
  deep's canary workload, two spinning goroutines for SIGURG), under the
  transformer with syscount attached, 10 runs per build:

  | build | runs |
  |---|---|
  | dev-b60b99c3 (94f9ba8f) | 10/10 fail: 6 Go fatal errors ("unexpected fault address", "unexpected return pc"), 3 SIGSEGV, 1 exec failure |
  | fix-final (d46ceb8f) | 10/10 exit 0: 160 execs, 0 corrupted canary words |

- **Hook cost** (`hook-perf/`, run_layers, 10 alternating rounds, medians):
  rewrite 405.9 -> 404.8 ns, agent dispatch 436.6 -> 435.9, empty sys_exit
  program 538.9 -> 538.2: unchanged.
- **Ring buffer output** (`output-ab/`, outattach + countloop, consumer in
  `ring_buffer__poll`, 5 reps, ns per call):

  | build | ringbuf 1 thread | ringbuf 6 threads |
  |---|---|---|
  | dev-b60b99c3 | 639.5-640.6 | 6,617-6,802 |
  | fix-rbwake (first version: consumer_pos read on every submit) | 694.9-701.4 (+8.5-9.7%) | 5,963-6,164 |
  | fix-rbwake2 (as committed) | 682.5-683.4 (+6.7%) | 6,294-6,361 (faster) |
  | exp-rbwake-nowake (fence + waiters load, never wakes) | 656.9 (+2.7%) | 6,707 |

  perf and control outputs unchanged (+-3%, ranges overlap). The futex wakes
  are not the cost (964 in a 6 s single-thread run, `output-ab/futex-count`);
  the rest is the consumer draining as records come instead of in 1 ms batches.
- **Problem 13 end to end** (`opensnoop/`, run.py of opensnoop-loss, 2 runs,
  delivered opens):

  | burst | bpf | dev-b60b99c3 | fix-final |
  |---|---|---|---|
  | 100 | 99.9%, 99.9% | 47.9%, 45.7% | 94.7%, 94.0% |
  | 50 | 100%, 99.9% | 66.6%, 66.7% | 93.2%, 92.9% |
  | 25, 10, 1 | 99.8-100% | 99.8-100% | 99.8-100% |

  Short of bpf (finding F4): a burst of 100 takes 391-397 us under fix-final
  against 315-324 us before and 288 us under bpf (the producer's FUTEX_WAKE
  each time the consumer has caught up and slept), and the consumer, woken
  through bpftime_epoll_wait (several system calls per call: sigprocmask,
  futex, sigtimedwait) and scheduling, falls behind a 31-record ring now and
  then.
- **memleak** (`memleak-poison/`, tool first, mallocloop under the agent, 5 s):
  dev-b60b99c3 disables memleak__kmallo, memleak__kfree and memleak__kmem_c
  (8 lines "_bpf_helper_ext_195896080"); fix-final disables none.
- **Gate** on the official 25e250d6 (`tracker.py migrate` with the 15 commits
  appended to a temporary copy of the daily list, then `test-candidate`; 200
  commits, local branch, not pushed; log
  `bpftime-daily-state/candidate-test-candidate-20261105-25e2-200.log`): rc 0.
  Runtime 183 pass + 1 skipped, the root `[kernel_user]` cases, daemon 10,
  Frida 25, transformer 54 + 1 expected failure, the reachability set; smoke:
  funclatency 4,271,000 / 4,271,000, memleak 9 outstanding, SIGUSR1 detach
  rc 0.

## New findings (for the next plan)

- **F1** Frida: a process whose threads ran an override (four threads, 2 s)
  makes a later fork child that attaches a probe while threads call the
  function crash now and then (2-3 of 8, with 94f9ba8f's Frida code too). A
  caller jumped to 0x001f020058000010, the words of an `ldr x16` / `br x16`
  pair: it ran a redirect while Gum was writing it, the documented attach-time
  window, hit more often after the parent's override.
- **F2** `make all` fails on two benchmark diagnostics
  (`per_cpu_hash_path_layers.cpp`, `per_cpu_hash_delete_layers.cpp`): the
  five-argument per_cpu_hash_map_impl call is ambiguous (uint32 ncpu matches
  both constructors; since 7670cee5). The gate's targets do not include them.
- **F3** syscall trace tests "Test syscall dispatch - multiple" and "Test
  resolving private data" assume a `fork` system call (x86); on aarch64 they
  fail as root (and as a user before tracefs is readable).
- **F4** opensnoop delivers 94-95% of bursts of 100 (bpf 99.9%); producer
  bursts 24% slower than before the wakeup. Consumer-side options: spin briefly
  before sleeping, fewer system calls per bpftime_epoll_wait.
- L11 (above) and the review's lows left as documented limits (CLAUDE.md).

`opensnoop/raw/*.tool.txt.xz`: the tools' outputs, xz-compressed.

## Frozen builds

`uprobe/ssl-nginx-2x2-20261023/builds/`: fix-rbwake, fix-rbwake2 (working
trees, `candidate.diff`), exp-rbwake-nowake (diagnosis only), fix-final (dev
d46ceb8f).
