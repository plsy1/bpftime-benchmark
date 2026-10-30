# Independent reviews of the dev branch's concurrent code (2026-10-31)

Three Fable 5.1 subagents reviewed code on dev 5062cb97, each given only the
code, the requirements and the platform (no design reasoning), read-only, with
standalone experiments in a scratch directory (sources in `experiments/`).
Every finding below was checked again here against the code; the ones marked
"reproduced" were also rerun independently (`experiments/verify-*`).

Scopes:
- hash map: lock-free lookups and striped writers (9c3e0345, 33bd3d13,
  cb3f29d2, f4fdcbcd);
- aarch64 system call rewriting, default and lazy (seccomp) modes;
- cross-process producers/consumers: software perf output, ringbuf, per-CPU
  hash.

## Verdict on what was asked

- The hash map's concurrent algorithm held up: no torn-key false match, no
  false negative, no deadlock, exact capacity, in ~600M stress operations over
  1 to 2344 stripes (`experiments/fable-hash/stress.cpp`).
- The perf shard SPSC protocol, the futex wakeup (Dekker store/fence/load on
  both sides), lost-record accounting, the ringbuf protocol and the per-CPU
  hash locking held up.
- The rewriting's concurrent modification (aligned word store, cache
  maintenance, svc to b in the Arm ARM's permitted set, gate before branch),
  register save/restore, the clone/rt_sigreturn direct path and the seccomp
  filter's arithmetic held up.

## Problems found, by impact

### Default paths (everyone using these features)

1. **Agent fork handlers never run in the syscall-trace path** (high).
   The transformer loads the agent with `dlmopen(LM_ID_NEWLM)`; its
   `pthread_atfork` handlers register in the agent namespace's libc, but the
   application forks through the main libc. Reproduced
   (`verify-atfork`: dlopen child ok, dlmopen child STALE, glibc 2.39).
   Children of a traced process then keep the parent's caches:
   - software perf output: the child matches the parent's shard (pid, tid) and
     both write one SPSC ring: torn or mixed records, a shard dropped as
     invalid (prefork servers such as nginx);
   - `bpf_get_current_pid_tgid` returns the parent's pid and tid;
   - with `ENABLE_PROBE_READ_CHECK`/`WRITE_CHECK` (bpftime's CMake default; the
     daily build has them off) `bpf_probe_read*` checks and
     `bpf_probe_write_user` use the parent's pid in `process_vm_readv/writev`:
     the child reads or writes its parent's memory;
   - cgroup array, array map binding, map usage stats caches.
   The existing fork tests load the code in-process, so they cannot see it.
2. **Hash map values are misaligned; an atomic add on one kills the process
   on aarch64** (high, pre-existing upstream). The element stride is rounded
   to 4 and the value sits at `+4 + key_size` (`bpftime_hash_map.hpp:189,538`):
   with an 8-byte key and value about a quarter of the values cross a 16-byte
   boundary. The JIT turns BPF `__sync_fetch_and_add` into `ldaddal`, which
   faults there (SIGBUS); x86 tolerates it. Reproduced (`verify-align`: offsets
   12 and 28 SIGBUS, 0/4/8/20 fine). Shape used by memleak `--combined-only`.
   The kernel rounds keys to 8.
3. **Per-CPU hash with keys other than 4 bytes: use after free** (medium-high).
   A program keeps the value pointer after the map lock drops; a concurrent
   delete erases the node and frees its value to the shared-memory allocator
   (`per_cpu_hash_map.cpp:358`); the program's next write corrupts the
   allocator of every process sharing the segment. The kernel's per-CPU hash
   reuses preallocated elements or frees after an RCU grace period. 4-byte
   keys reuse slots (only a stale value, like the kernel).
4. **Per-CPU hash indexes with an unchecked CPU number** (medium). `ncpu` is
   `_SC_NPROCESSORS_ONLN` at creation; lookups, updates and deletes use
   `sched_getcpu()` unchecked (the per-CPU array checks). A CPU numbered at or
   above the online count at creation (a core offlined by a power mode, then
   back) reads and writes out of bounds; -1 is not handled.
5. **The hook's assembly has no unwind information** (medium). No `.cfi`
   directives (`text_segment_transformer.cpp`, 0 occurrences): `pthread_cancel`
   of a thread blocked in a hooked call skips the C++ destructors between it
   and the thread start (the reviewer's experiment: destructor skipped with
   rewriting, run without); backtraces from signal handlers stop at the hook.
6. **Deleted or replaced library files** (medium). Section tables are read by
   opening the maps line's path: "(deleted)" files (a package upgrade under a
   running server, then `bpftime attach`) are skipped, so untraced; a file
   replaced in place gives the new file's sections for the old bytes. Nothing
   checks dev and inode.
7. **`in_hook` can stick** (medium-low): a signal handler that siglongjmps out
   of the hook while programs run, or a vfork child killed inside the hook,
   leaves the thread's flag set: every later call of that thread is
   untraced, silently.
8. **Shared executable file mappings are rewritten** (low probability): the
   `b` goes into a `MAP_SHARED` file opened read-write, i.e. to disk.
9. Small: FPCR not preserved across the hook (rounding mode leaks);
   `agent-transformer.cpp` calls `exit(1)` when the agent cannot load and
   dereferences `injected_with_frida` right after warning it is NULL
   (host-process contract); `svc` with a misaligned sp gets SIGBUS once
   rewritten (the gate's `stp`); mremap of rewritten code breaks the
   PC-relative branches.
10. Perf shards of a live thread can be reclaimed when the producer and the
    tool are in different pid namespaces (tgkill ESRCH), discarding its lost
    count; low.
11. Known class (problem 5, recorded only): a process killed while holding the
    perf shard lock, the ringbuf reserve lock, a per-CPU hash map lock or a
    hash stripe makes later users spin forever.
12. Hash map robustness, not correctness: the 4-byte-key hash is weak
    (structured keys gave a probe bound of 170,707 in the reviewer's stress
    run); near capacity a stripe without tokens takes all stripes on every
    insert.

### Lazy mode (opt-in research)

- A fork while another thread holds `patch_owner` leaves the child's lock held
  by a thread that does not exist: its first trap spins forever (reviewer:
  8 of 20 children hung). Only `rescan_mutex` has atfork handlers.
- `enable_lazy_rewriting` unblocks SIGSYS only on the calling thread; a thread
  that blocked all signals earlier dies (rc 159) at its first trap.
- SIGSYS runs on the trapping thread's stack (no `SA_ONSTACK`), about 4.9 KiB
  with the kernel's frame: too much for Go goroutine stacks.
- The filter allows the page of `syscall_addr`, but seccomp reports the pc
  after `svc`: if a relink puts it in the last word of a page (1 in 1024), the
  hook's own calls trap and recurse until the stack overflows.
- Not dumpable processes (setuid, `PR_SET_DUMPABLE 0`) cannot write
  `/proc/self/mem`: every trap is emulated untraced, and the emulation path
  skips the execve refusal and the mask sanitizing.

### Inherent to the technique (documented, not a defect to fix)

- One hooked call pushes 816 bytes before the dispatch, about 1.3 KiB with the
  agent's callback, plus the BPF stack when programs run; a `svc` needs none.
  Go goroutines are the risk; not tested (no Go toolchain run).

## Files

- `experiments/fable-hash/`: stress harness, alignment experiments.
- `experiments/fable-syscall/exp.cpp`: the rewriting experiments (numbered
  cases cited above).
- `experiments/fable-shm/`: dlmopen atfork experiment.
- `experiments/verify-atfork/`, `experiments/verify-align/`: the independent
  reruns.
