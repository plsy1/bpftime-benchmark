# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

bpftime is a high-performance userspace eBPF runtime and general extension framework. It enables running eBPF programs in userspace for observability, networking, GPU, and other use cases, achieving up to 10x better performance than kernel eBPF for certain operations.

## Common Development Commands

### Build Commands

```bash
# Build with tests and all components (Debug mode)
make build

# Build release version
make release

# Build with LLVM JIT support
make release-with-llvm-jit

# Build with static library
make release-with-static-lib

# Clean build artifacts
make clean

# Build specific components
cmake --build build --target <target_name>
```

### Testing Commands

```bash
# Run all unit tests
make unit-test

# Run runtime tests only
make unit-test-runtime

# Run daemon tests only
make unit-test-daemon

# Build eBPF test programs (from runtime/test/bpf)
make -C runtime/test/bpf

# Run specific test binary
./build/runtime/unit-test/bpftime_runtime_tests
./build/daemon/test/bpftime_daemon_tests
```

### Running Examples

```bash
# Build example programs
make -C example/malloc

# Load and run eBPF program
export PATH=$PATH:~/.bpftime/
bpftime load ./example/malloc/malloc
bpftime start ./example/malloc/victim

# Attach to running process (requires sudo)
sudo bpftime attach <pid>
```

### Development Tools

```bash
# Install to system (after build)
sudo cmake --install build

# Build with specific features
cmake -Bbuild -DBPFTIME_ENABLE_IOURING_EXT=1  # IO uring support
cmake -Bbuild -DBPFTIME_LLVM_JIT=1           # LLVM JIT backend
cmake -Bbuild -DBPFTIME_ENABLE_MPK=1         # Memory Protection Keys
```

## Architecture & Code Structure

### Component Organization

The codebase is organized into distinct components that interact through well-defined interfaces:

1. **`vm/`** - Virtual Machine implementations
   - `vm-core/`: Core VM interface and abstractions
   - `llvm-jit/`: LLVM-based JIT/AOT compiler (high performance)
   - Supports multiple backends (LLVM JIT, ubpf)

2. **`runtime/`** - Core runtime functionality
   - `include/`: Public APIs and interfaces
   - `src/handler/`: Handler pattern for managing eBPF objects (programs, maps, links)
   - `src/bpf_map/`: Map implementations (hash, array, ringbuf, etc.)
   - `syscall-server/`: Intercepts eBPF syscalls for compatibility
   - `agent/`: Library injected into target processes

3. **`attach/`** - Event attachment mechanisms
   - `base_attach_impl/`: Abstract interface for all attach types
   - `frida_uprobe_attach_impl/`: Dynamic instrumentation via Frida
   - `syscall_trace_attach_impl/`: Syscall interception
   - `nv_attach_impl/`: CUDA/GPU event attachment

4. **`daemon/`** - System monitoring and kernel interaction
   - Monitors kernel eBPF operations
   - Can redirect to userspace runtime

5. **`bpftime-verifier/`** - Safety verification
   - Wraps PREVAIL verifier for userspace
   - Optional kernel verifier integration

### Key Architectural Patterns

1. **Shared Memory Architecture**: Uses boost::interprocess for zero-copy IPC between processes
   - Central `handler_manager` registry in shared memory
   - File descriptor abstraction for kernel compatibility

2. **Handler Pattern**: All eBPF objects (programs, maps, links) managed through handlers
   - Consistent interface for create/destroy/access
   - Enables cross-process object sharing

3. **Modular Attach System**: New event sources added by implementing `base_attach_impl`
   - Each attach type provides its own helpers
   - Supports custom context preparation

4. **VM Abstraction**: Multiple VM backends through common interface
   - Switch between interpreter/JIT/AOT transparently
   - Extensible helper function registration

### Important Files and Interfaces

- `runtime/include/bpftime.hpp`: Main runtime API
- `runtime/include/bpftime_shm.hpp`: Shared memory management
- `vm/vm-core/include/ebpf-vm.h`: VM interface
- `attach/base_attach_impl/base_attach_impl.hpp`: Attach interface
- `runtime/src/handler/handler_manager.hpp`: Central object registry

### Development Workflow

1. **Adding new functionality**: Check existing patterns in similar components
2. **Testing**: Add unit tests in component's test directory
3. **eBPF programs**: Use standard clang/libbpf toolchain, test with both userspace and kernel
4. **Performance**: Consider shared memory overhead, prefer batch operations

The architecture prioritizes performance (bypassing kernel), compatibility (same APIs as kernel eBPF), and extensibility (modular design for new features).

## Injected Agent Host-Process Contract

Code loaded into a third-party process through `LD_PRELOAD` or runtime injection
must remain a guest of that process:

- Do not call `exit`, `_exit`, `abort`, `terminate`, or otherwise end the host
  because bpftime initialization, configuration, allocation, or execution fails.
  Return or record the error and disable only the affected bpftime operation.
- Do not write to, close, redirect, or flush the host's stdin, stdout, or stderr
  unless the user explicitly selected that behavior. Route diagnostics and
  `bpf_trace_printk` output through an explicitly configured bpftime sink.
- Do not replace host signal behavior without restoring or correctly chaining
  the previous handler.
- Add regression coverage for failure paths and for the default absence of host
  termination and stdio side effects.

The syscall-trace transformer (`agent-transformer.cpp`) follows it too: an
agent that cannot be loaded, or lacks `injected_with_frida`, leaves the host
running untraced (it called `exit(1)` and dereferenced the missing symbol), and
its entry message is debug level (spdlog's default logger, before the stderr
logger is set, wrote it to the host's stdout). Test: "The transformer leaves
the host running when the agent is unusable".

## Repository Documentation

Trap callbacks execute inside SIGTRAP. Runtime-managed BPF links must reject
programs whose concrete helper implementations have not been audited as
async-signal-safe, and must finish JIT compilation before arming a probe.
Other attach backends retain their existing helper support. Never replace a
rejected helper with a silent no-op. Native/opaque callbacks are responsible
for satisfying the same signal-safety contract themselves.

RISC-V trap validation must include a real 4-byte instruction at an address
congruent to 2 modulo 4, with assertions on both instruction width and the
three-phase write counter. Test late dlopen after worker threads already exist,
and permission failures during arm, disarm and rollback. QEMU results do not
establish native concurrent patch/restore behavior.

Do not create a root `docs/` directory or commit pull-request review artifacts.
Keep repository maintenance and design constraints in this file.

## Ordinary Array JIT Specialization

Array lookup specialization is automatic for eligible LLVM JIT programs; the
`BPFTIME_ARRAY_JIT_INLINE` enable switch is obsolete and not read. Descriptor
kind is per map (explicit even when a per-CPU allocation has one CPU), and LLVM
selects individual constant-handle lookup sites. Mixed ordinary/per-CPU arrays,
hash maps, updates and unrelated helpers may coexist. Unknown handles and
non-array calls retain the exact original helper. Never bypass custom or
diagnostic helpers. Programs with map-index, BTF-ID, function or other
unsupported LDDW relocations, CUDA, MPK or indirect helper calls remain on the
original path. Interpreter and AOT do not acquire JIT bindings; bound programs
still reject signal use.

Direct map-value LDDWs (libbpf global data: .rodata/.data/.bss/.kconfig) no
longer disqualify a program, but only on the kernel's terms: the target is a
single-element ordinary array and the offset lies inside its value. Each such
map is bound and owned exactly like an inlined lookup target, and the loaded
copy of the program replaces the LDDW with a plain 64-bit constant of the
retained element-0 address plus offset; the stored instructions keep their
relocations. A bound program must never resolve global data through
`map_val(fd)`, eagerly or through lazy compilation. Any other direct value
target (multi-element, per-CPU, hash, out-of-range offset) keeps the whole
program on the original path. Global data alone, without an array lookup to
specialize, does not create a binding.

Generated addresses must be owned, not cached from a reusable FD. Program
bindings share the existing implementation reference count and retain the
exact local shared-memory mapping. Miss fallbacks must resolve the same pinned
map as inlined hits. Handler mutation and reference acquisition use the named
map-binding mutex. No mutex or reference increment belongs in the inlined hit
path. Abrupt process termination can strand shared references until the shared
segment is removed, as with the existing shared-reference machinery; do not
claim crash-recovery or mix incompatible runtime versions in one segment.
The private helper names `bpftime_bound_array_lookup_v1`,
`bpftime_bound_percpu_array_lookup_v1` and `bpftime_bound_array_lookup_v2` are
reserved for this standard-helper contract. Only for these may the JIT canonicalize a
proven out-of-range lookup to FD -1, whose result is ENOENT/null without another
FD-table or binding lookup. Custom helpers keep their original arguments.
Management APIs still require their existing external serialization, and this
feature does not add concurrent-detach/quiescence guarantees to attach backends.
Fork children execute inherited bound programs (forking servers such as
nginx attach in the master and run probes in workers). The atfork prepare
handler takes one shared reference per live binding for the child before the
fork, under a process-local lock that also serializes binding creation and
release. The counter is shared, so the parent must not return them. The child
owns its inherited copies and releases them normally. A failed fork, or a child
that execs or _exits without destructors, strands those references, like
abrupt termination; do not
take the shared map-binding mutex in atfork handlers. Do not claim support for
fork from a signal handler interrupting an active JIT invocation.

Standard update/delete calls in bound programs must resolve the same owned
array identity as lookup, including after FD reuse; other map kinds keep their
original helper. Custom helper pointers are selected BEFORE compilation and
never overridden by this optimization. Dynamic array bindings are retained
for an invocation; this does not extend non-array map lifetime guarantees.

The llvmbpf safety changes are parent-owned patches applied to a generated
translation unit and descriptor header in the build directory, not an
uncommitted submodule edit. All native VM consumers must use the same generated
descriptor header; do not mix objects built with different definitions.
Patch conflicts on upstream updates must fail configuration for explicit review.

Use workloads that consume lookup values and check results when measuring JIT
specialization. An unused lookup result can simplify after inlining; those
timings must not replace or be compared directly with the historical C++ map
operation table. Test close/reuse/reset, mapping replacement, duplicate handles,
last-reference reclamation and concurrent/cross-process lifetimes.

The per-CPU extension is selected per eligible map. Its descriptor stride is value_size times
the implementation CPU count. JIT code must call the known standard helper 8
once per in-range lookup, including within a single invocation; never cache its result,
mark it pure, or assume benchmark affinity is an execution guarantee. A custom
or missing CPU helper disables per-CPU specialization, not ordinary arrays in
the same program. Check the CPU against the bound
allocation's CPU count before forming a usable result. This preserves existing
userspace selection-at-lookup behavior; it does not provide kernel-style
preemption disabling or make per-CPU values safe against thread migration while
the caller subsequently uses a returned pointer. CPU topology/hotplug support
is not added by this experiment.

The LLVM overlay may weakly resolve `bpftime_resolve_array_cpu_query_v1` at
compile time. The runtime supplies callbacks only for its exact standard CPU
helper pointer. The direct query uses the existing public CPU API once per
in-range lookup. On raw return -1, invoke the same cold error handler as helper
8 (log once through the selected sink, return CPU 0), then apply the same CPU
bounds check. Other negative values retain signed extension and fail bounds.
Keep legacy helper 8 and custom/helper-only VM behavior intact; absence of the
optional resolver retains the original helper call. Do not introduce CPU
caching, private TLS offsets, new BPF helper numbers, query purity attributes
or weaker map-lifetime, null/key/CPU bounds, fork, AOT or signal restrictions.
The sched_getcpu wrapper used to inject failures belongs only to unit tests.

## Software Perf Event Output

A sample that does not fit in its producer shard is dropped the kernel way:
`bpf_perf_event_output` returns `-ENOSPC`, and the shard counts the drop.
The count is reported by a 24-byte `PERF_RECORD_LOST` (no `sample_id`)
published atomically with the next sample that fits, never on its own and
never silently discarded. Output without a mapped ring also returns `-ENOSPC`
(the kernel's behavior); it is not counted as lost. A pending count is lost
with its shard only when the shard is reclaimed (dead thread or consumer
remap), as kernel counts are lost with their ring.

Consumers in `bpftime_epoll_wait` sleep on the shared futex word
`bpftime_perf_wakeup_state`: register in `waiters`, fence, re-check, then
`FUTEX_WAIT`, up to the caller's timeout and at most 100 ms (libbpf's tools
poll with 100 ms). What no producer wakes for is found then: ring buffer
records submitted with `BPF_RB_NO_WAKEUP`, perf events made without wakeup
settings below the watermark, and a dead producer's reservation (recovered
up to 100 ms later). Until 11-06 the timeout was 1 ms, and an idle tool
scanned every ring and shard a thousand times a second (2-7 ms of CPU a
second; with 493 shards and their sweeps 36% of a core;
`uprobe/more-dims-20261106`, n5-footprint). Test "An idle consumer sleeps
through the caller's timeout" (572 drains in 300 ms at 1 ms, under 10 now). A producer asks for a wakeup as the
kernel decides from the consumer's `perf_event_attr` (`watermark`,
`wakeup_events` / `wakeup_watermark`, passed from `perf_event_open` into the
event: `software_perf_event_wakeup`): when a sample was dropped or its shard
is past the watermark right after publishing (measured from the tail read
before publishing; `wakeup_watermark` bytes with `watermark` set, else half
the shard), and without `watermark` every `wakeup_events` samples, counted
per shard. libbpf's perf_buffer asks for every sample (`wakeup_events =
sample_period = 1`). From db3d475f to 11-06 only the half-shard rule
existed (waking on every sample had cost ssl-nginx 8-9% of its requests per
second), and libbpf's tools got their samples by the 1 ms timeout: 0.5 ms
late on average (gethostlatency 540-572 us against bpf's 19-44,
`uprobe/output-dims-20261106`, findings 6 and 15). An event made without
the settings (`bpftime_add_software_perf_event`) keeps the half-shard rule
only. Tests "Software perf event wakes as the consumer's attr asks", "Small
output wakes a consumer that asks for every sample" (both fail with the
settings ignored).

Waking the consumer for every sample costs the traced process a
`FUTEX_WAKE` system call per sample (the kernel raises an irq_work
instead): ssl-nginx under sslsniff lost 10% of its requests per second,
`futex_wake` alone 4.7% of nginx's time (`uprobe/improve-20261106`, item 3).
So for a while after it found records (`BPFTIME_CONSUMER_NAP_WINDOW_US`,
2 ms) the consumer naps (`BPFTIME_CONSUMER_NAP_US`, 100 us, a futex wait
with that timeout) registered in `nappers`, not `waiters`: a perf producer
wakes a napping consumer only past the watermark or on a drop (`urgent`), so
under a steady stream it makes no system call and the nap bounds the delay;
ring buffer records wake it by bpf's rule as a waiting one (woken only at
half a ring, a shared ring lost records in bursts: 8,240-byte records on 4
threads, 89%). Idle longer than the window, the consumer sleeps until woken,
so the first record of a burst wakes it at once. Measured (checks on):
ssl-nginx 12,673 req/s against 11,505 waking for every sample and 12,854
waking at half a shard (kernel 12,150), sslsniff 10% of a core (21%, 5%;
kernel 12%); gethostlatency's lines p50 90 us, p99 170 us (every sample:
21 / 47-150 us; half a shard: 540 / 1,070 us; kernel 19 / 44-79 us).
`BPFTIME_CONSUMER_NAP_US=0` turns the naps off (every sample wakes the
consumer, as the kernel). Opt-in `BPFTIME_CONSUMER_POLL_US`: poll that long
after records instead (unregistered, `sched_yield` between polls): the
latency was below the kernel's (p50 13 us) with the throughput of the naps,
but the consumer kept a core busy (100% under ssl-nginx); polling without
yielding the CPU delayed the traced processes sharing it (gethostlatency p99
1 ms in 2 of 3 runs). Tests "Napping consumers are woken only when a buffer
fills" (mutations: perf output ignoring `nappers`, ring buffers ignoring
them, no nap). Asking for a wakeup on every sample costs every output the
fence and the `waiters` load (perf 8 B on one thread 538 -> 597 ns, kernel
2,759); on aarch64 the store-release that publishes the record already orders a
following load-acquire, so the fence could be dropped there (not done; x86
needs it). The producer fences and issues `FUTEX_WAKE`
only when `waiters` is non-zero; keep both fences, or a wakeup can be missed
until the timeout. SIGINT/SIGTERM stay
blocked while sleeping and are collected with a zero-timeout `sigtimedwait`
afterwards. Not handled (found 11-07): that `sigtimedwait` can dequeue a
process-directed SIGINT meant for another thread of the tool; the handler then
runs in the polling thread, and a thread waiting for it in `pause()` (outattach's
main thread) waits for ever, the tool never exits (`uprobe/improve-20261106`,
investigations; single-threaded tools are not affected). Fix: leave the signals
unblocked, return EINTR when a futex wait (the sleep or the nap) is
interrupted (a handler ran in this thread), and sleep with nanosleep where no
wakeup object exists. Ring buffer records follow bpf's rule (`bpf_ringbuf_commit`): a
consumer is woken when it has caught up with the record (it may be asleep), or
with `BPF_RB_FORCE_WAKEUP`, never with `BPF_RB_NO_WAKEUP`; discards too, since
the consumer must pass them. The data area is mapped once (bpf maps it twice),
so a record that would cross the end goes to offset 0 behind a padding record
(a discard filling the end), published with it; a consumer standing at that
padding has caught up too (`consumer_at` reads the header it stands at: a
discard ending exactly at the end, before a record at 0). Comparing with the
record alone woke nobody at a wrap, and the consumer slept to its timeout
while the ring filled: 4 KB records in bursts of 100 on 4 threads delivered
60% (bpf 99%), variable sizes 71-96%, and opensnoop's 8,248-byte records pad
every 31 (`uprobe/output-dims-20261106`, finding 5). Test "A record behind the
end padding wakes a consumer at the padding" (fails without the padding case).
After publishing, the producer fences and loads
`waiters` (acquire), and only with a consumer waiting reads `consumer_pos`
(`notify_ringbuf_consumers`): a running consumer writes that line for every
record, and reading it on every submit (with a second fence) made single-thread
ringbuf output 9.7% slower (639 -> 701 ns per call; the futex wakes, 964 in a
6 s run, cost nothing measurable). As it is: 640 -> 683 ns per call on one
thread (+6.7%; fencing and loading `waiters` without ever waking, 657: the rest
is the consumer draining as records come instead of in 1 ms batches), and
faster on six (6,617-6,770 -> 6,294-6,361); results
`reviews-20261105/fixes/output-ab`. Every change of `waiters` is a
read-modify-write, so a load that sees a registration also sees the position
the consumer stored before it. Nothing more is read before the publishing
exchange (see the submit's note on owner-slot loads). Before, ring buffers were found
by the 1 ms timeout only: a burst larger than the ring within it lost the rest
at reserve (opensnoop: 8,240-byte records, 31 per 256 KiB ring, 48% of the
opens delivered in bursts of 100 against bpf's 99.8%;
`uprobe/opensnoop-loss-20261105`). Test: "Ring buffer records wake a consumer
that caught up" (fails with the notify removed). Segments without the wakeup
object (older servers) fall back to polling.

A producer finds its shard by (pid, tid) on every sample. Both are cached per
thread (getpid and gettid are system calls); the atfork child handler clears
the cache, and the shard match still compares pid and tid. Keep both, or a
fork child would write into its parent's shard. The cache
(`producer_identity.cpp`) also serves `bpf_get_current_pid_tgid` and ringbuf
reservations.

A vfork or posix_spawn child (CLONE_VM without CLONE_SETTLS) shares its parent
thread's memory and TLS, and no fork handler runs for it: it reported its
parent thread's ids, or filled an empty cache with its own, which the parent
then kept (its perf shard keyed to the dead child was reclaimed while it wrote
to it). On aarch64 the syscall-trace hook calls `note_clone_syscall` before
clone and clone3 take the direct path (the direct path restores every
register from the frame, so the call is invisible to the child); for such a
clone it bumps the agent's exported `bpftime_identity_epoch` (found with
dlsym after loading). It also bumps for any clone without `CLONE_THREAD`: a
raw `clone(SIGCHLD)` or glibc's `clone()` runs no fork handler, and its child
kept the forking thread's ids (its perf output went to that thread's shard,
and once that thread exited the tool recovered the child's live ringbuf
reservation as a dead thread's); glibc's fork bumps too (one re-read per
thread). The hook makes no system call there: clone3's flags are read
directly when `clone_args` lies 8-byte aligned within the 1 KiB above the
application's sp (the caller's frame, where glibc keeps it; live, mapped
stack), and anywhere else the epoch is bumped (a bad pointer stays the
kernel's EFAULT). It used `getpid` and `process_vm_readv`, which an
application's seccomp filter sees: a filter killing on `process_vm_readv`
killed the process at `pthread_create`, one denying it (Docker's default
profile) lost the bump (test "the clone hook makes no system call an
application may forbid"). A cached identity older than the epoch
is read again, and cached only if its pid is the process's (`process_pid`,
refreshed in fork children only, so in a vfork child still the parent's):
the child gets its own ids uncached (a raw-clone child: on every call),
the parent refreshes once. The fast
path is inline (`producer_identity.hpp`) on one `__thread` struct: in the
dlmopen-ed agent each TLS variable needs a descriptor call, and three
separate `thread_local`s in another file cost 8 ns per
`bpf_get_current_pid_tgid` (results `reviews-20261031/refixes/output-ab`).
Costs: about 4.5 ns per identity read on the Jetson (the epoch load and
compare). Not covered: uprobe (Frida) mode and x86, which have no
such marker; a vfork or raw-clone child there still uses its parent
thread's identity.
Tests: "bpf_get_current_pid_tgid in a vfork child and its parent after it"
(runtime, the bump by hand), "clones that share the thread's TLS bump the
identity epoch" (transformer).

Idle shards of dead threads are reclaimed by probing (pid, tid) with tgkill,
at most every 100 ms per event (`last_reclaim_ns`), and at most every 10 ms
when a new producer finds shared memory full. From 64 shards on, every drain
and every new producer swept them all: an idle tool with 493 shards made about
a million tgkill calls a second, 36% of a core (`uprobe/more-dims-20261106`,
n5-footprint; test "Software perf event looks for dead threads' shards at most
every 100 ms", "Software perf event output reclaims dead threads' shards when
memory runs out"). Each shard records its producer's pid namespace (inode of `/proc/self/ns/pid`;
without /proc a random id with the top bit set, drawn per process: 0 for
every such process made two namespaces' producers share shards, test "An
unknown pid namespace equals no other process's");
a reclaimer probes only shards of its own namespace. A producer in a container
sharing `/dev/shm` with the tool used to lose its live, idle shard (tgkill from
the host: ESRCH) and the lost-sample count in it. Test: "Software perf event
shards of another pid namespace are not reclaimed".

The shard search compares the namespace too (cached per process, cleared in
fork children: a child forked after `unshare(CLONE_NEWPID)` is in another
one). Matching (pid, tid) alone put two containers' pid 1 into one
single-producer ring: torn samples delivered as valid, garbage LOST counts,
shards dropped as invalid. The per-thread shard cache is matched by pid and
tid, so its entries carry a fork generation that the fork child handler
bumps: a pid 1 forking after `unshare(CLONE_NEWPID)` has a pid 1 child, and
the inherited entry put it into its parent's ring (test "Software perf event
shard caches are not inherited by fork children"). Tests: "Software perf event shards are matched within a pid
namespace", "Producers in two pid namespaces with the same pid write apart"
(root: two `unshare` children's pid 1 writing at once).

Shards of other pid namespaces are never reclaimed (above), so shared memory
can run out; so can many live threads (a shard per thread per CPU it ran on,
each the consumer's mapping in size: about 500 libbpf-sized shards fill the
default 128 MB). A thread that gets no shard writes to the event's overflow
shard, made with the consumer's buffer (and again when it is mapped with
another size), under `overflow_lock`, one producer at a time, and tries for
a shard of its own again after 10 ms (the decision is cached per thread). It
used to fail with ENOSPC (600 threads: 107 of 600 samples, n5-footprint);
test "Software perf event output without a shard goes to the overflow
shard". Only with no overflow shard (the segment was full already when the
consumer mapped its buffer) does output fail with ENOSPC (no shard, so no
lost count); Boost's `bad_alloc` used to escape the helper into the traced
process. A shard whose data area does not fit (the
shard catches that allocation itself) is removed at once and the next
output tries again: it was kept and cached without data, and its thread got
ENOSPC for ever (test "Software perf event output retries a shard whose
data did not fit"). Safely reclaiming a live foreign producer's
idle shard would need an atomic flag on every output. Test: "Software perf
event output survives a full shared memory".

Each drain starts one shard further (`drain_start`). When the consumer ring
fills up, the shards drained last keep their samples and then drop new ones
(ENOSPC, LOST); a fixed order starved the same threads every time. Test:
"Software perf event drains start at a different shard each time". Not
handled: a tool that calls `perf_buffer__consume()` without epoll never
triggers a drain, and mapping the consumer buffer a second time with a
larger size reallocates it under libbpf (tools map it once).

A ringbuf reservation records its owner (pid, tid, pid namespace) in a
256-slot table in the ring (`reservation_owner`), searched from the slot of
the thread's tid (a shared next-slot cursor made 6-thread output 7% slower);
the record header's second
word (the kernel's pg_off, unread by libbpf) is a tag (`ringbuf_record_tag`):
the map's fd above the low 9 bits, the slot plus one in them (0 when the table
is full). `bpf_ringbuf_submit` and `bpf_ringbuf_discard` take the fd from the
sample's tag (`bpftime_ringbuf_record_fd`); when the slot alone was stored
there (5920d5f9), they looked the slot up as a map fd, so every record reserved
with `bpf_ringbuf_reserve` stayed busy until the reclaim discarded it
(get_stack_id delivered 0-70 of ~1,300 events; test "Ringbuf helpers submit
and discard through the sample's map"). The tool's epoll wait calls
`recover_dead_reservation()` before `has_data()`: if the next record is still
busy and its owner is a thread of the tool's pid namespace that is gone
(`is_thread_gone`: tgkill ESRCH, or Z/X in `/proc/<pid>/task/<tid>/stat`,
since tgkill finds an unreaped thread-group leader), the record is marked
discarded and the ones behind it are delivered. Submit reads the slot from
the header before publishing (afterwards, a lap later, the bytes may be
another record's header or padding, and it freed that slot), publishes
with one exchange and releases the slot only if the record was still BUSY
(else recovery took it and freed the slot). A slot whose record was
consumed a lap ago is free (a producer killed between publishing and
releasing leaked it); a lap, because submit releases with a plain store
right after publishing. Keep submit to the header exchange and that store:
an ownership check reading the owner slot before the exchange (a646180d)
made single-thread output 12% slower in about 40% of runs with the
consumer on the other CPU cluster (results `reviews-20261031/refixes2`).
Tests: "Ringbuf submit leaves a recovered record alone", "Ringbuf owner
slots of consumed records are reused", "Ringbuf recovers a dead producer
before it is reaped". Not covered: a live producer whose record was
recovered (a stale identity) and lapped before its submit clears another
record's BUSY, as before the owner table.
A producer killed between `bpf_ringbuf_reserve` and submit used to hold
back every later record forever, with no error (the kernel cannot have
this). Still wedged: an owner in another pid namespace, a full table, and a
tool that calls `ring_buffer__consume()` without polling (libbpf reads the
ring itself). The owner identity comes from `producer_identity.cpp`, shared
with perf output. Test: "Ringbuf skips the record of a producer that died
before submitting".

Reserve takes its lock with glibc's protocol inline (`spin_lock_guard`,
`ringbuf_map.cpp`), and a waiter calls `sched_yield` after 2,048 polls of the
held lock: the kernel holds its ring buffer lock with preemption off, and with
64 producer processes on 6 CPUs the waiters on a descheduled holder's CPU spun
through their time slices (a call waited 40 ms at p99.9 and the workload made
15% fewer calls; now 0.2-1 ms and as many calls as under the kernel). The lock
word and the two positions reserve keeps sit on one 64-byte aligned line of
their own (`reserve_line()`, the aligned part of `reserve_area`; mappings are
page aligned, so it is the same bytes in every process): the producer
position as reserve last stored it, and the consumer position it read last.
The consumer only moves forward, so space computed from the cached position is
never more than there is, and the lock orders it after the acquire that read
it; `consumer_pos` (written by the consumer for every record) is read again
only when that space is too small or the owner table looks full (slots of
records consumed a lap ago are free). Any new check in reserve that depends on
the consumer's position must allow for a stale one. `try_get_ringbuf_map_impl`
reads the handler once (`get_handler_at_valid_fd`); through
`is_ringbuf_map_fd` every reserve and submit passed `get_handler`'s
static-local init guard three times. Costs (`uprobe/improve-20261106`, item
4; ns per call): one thread 719 -> 518 (kernel 2,041), four processes 4,040 ->
2,991 (kernel 3,272), four threads of one process 3,990 -> 3,168. Tests:
"Ringbuf producers in several processes deliver every record once" (no lock:
torn and missing records), "Ringbuf reserve lock has a cache line of its
own", "Ringbuf reserve finds space the consumer freed" (never re-reading the
consumer position: nothing reserved after the first lap), section "Only ring
buffer fds reserve".

Not handled (documented): a process killed while holding `shard_lock`, the
ringbuf's reserve lock (`reserve_line().lock`), a map's `map_lock` or the per-CPU hash's
`pool_lock` (inserts and deletes) leaves every later user spinning, like the hash map stripes. A
consumer killed between delivering a record and releasing the shard tail
redelivers it on the next drain, and one killed while waiting leaves
`waiters` (or `nappers`) above zero (every output that asks for a wakeup
then pays a futex wake).
`map_lock` is initialized `PTHREAD_PROCESS_SHARED` (glibc ignores the flag for
spin locks; the lock is shared between processes).

## Credential Cache

`bpf_get_current_uid_gid` may cache the real uid/gid only while every change of
them invalidates the cache. The agent exports interposers for `setuid`,
`setgid`, `seteuid`, `setegid`, `setreuid`, `setregid`, `setresuid` and
`setresgid` (`runtime/agent/credential_hooks.cpp`, listed in `agent.version`);
each calls the real libc function, keeps its return value and errno, then
invalidates. The cache is enabled only if `dlsym(RTLD_DEFAULT, name)` resolves
all eight to the agent's own hooks (the agent was LD_PRELOADed); a late dlopen
or another interposer leaves the helper querying `getuid`/`getgid` on every
call. Fork children invalidate in the existing atfork child handler. The fast
path is one atomic load; a filler re-checks the invalidation generation after
storing so a racing change cannot leave a stale value. Credentials changed by
raw system calls that bypass libc are not observed; do not claim otherwise,
and do not add the helper to the async-signal-safe list for this reason alone.
Forking servers such as nginx change uid in the worker after fork, so keep
both invalidation paths.

## Uretprobe Stacks

`generate_stack` (behind `bpf_get_stackid` and `bpf_get_stack`) uses
`current_thread_gum_cpu_context` when `on_enter` set it and otherwise the CPU
context of `gum_interceptor_get_current_invocation()`, so stacks work in
uretprobes (memleak records its stacks there). `on_leave` does not set the
thread-local: in the agent each TLS access is a `__tls_get_addr` call, and
setting and resetting it cost every return about 5 ns (uretprobe +1.9%).

On aarch64 the non-fuzzy backtracer is a frame-pointer walk with the kernel's
`perf_callchain_user` result: the pc first (the probed function at entry, the
return address at return, as `regs->pc` in a kernel uprobe/uretprobe), then the
return address of each x29 frame record with its PAC stripped. At entry the
immediate caller is missing, as in the kernel. Records are read only within the
current thread's stack (`pthread_getattr_np`, cached per thread), must move
strictly upwards and 8-byte aligned, and a return address below the first page
or above the user VA limit ends the walk; never read through an unchecked x29.
Return addresses are stripped with `strip_code_pointer_auth` (XPACI when
`HWCAP_PACA`): `gum_strip_code_address` returns signed addresses unchanged on
Linux, and libc and the agent sign theirs. Do not write it as a naked asm
function: interprocedural optimization dropped the "unused" argument.
The gum accurate backtracer re-read ELF unwind data on every call (memleak
20k allocations/s) and stopped at PAC-signed return addresses. Other
architectures and the fuzzy option keep gum.

## Process Exit

Probes stay attached until the process ends, while the exit-time destructor
`__destruct_shm` unmaps the global shm. `bpftime_destroy_global_shm` sets
`global_shm_released` before unmapping and `bpftime_prog_exec` then returns
`-ENODEV` without running (re-initialization clears it). Hooked calls made by
later destructors (gum/GLib teardown calling `free` under memleak) used to run
programs against the unmapped segment and crash the host at exit. The Frida
callback does not log `-ENODEV`, because the logger may already be destroyed.
Threads still inside a program when the main thread unmaps are not covered.

The default logger is destroyed with spdlog's registry at exit, while probes
still run: a probe that logged after that wrote through a freed formatter and
crashed the traced process (`timeout` under the get_stack_id example, 1-2 of 4
runs, `uprobe/harness-gaps-20261104`). `bpftime_set_logger` registers
`bpftime_quiet_logger_at_exit` once the registry exists; exit handlers run in
reverse order, so it runs before the registry's destructor, keeps the logger
alive (a reference never released, reached through the registry's storage
afterwards) and turns it off: logs from exit onwards are dropped. Test "A log
after spdlog's teardown at exit is dropped" (a helper process logs from an exit
handler registered before the registry; exits 3 without the handler). Loggers
installed with `spdlog::set_default_logger` alone are not covered.

## Syscall Server Fork Children

A child forked before the syscall server started (memleak `-c` forks the
traced command before its first `bpf()` call) passes every call to libc: an
atfork child handler marks the interposer disabled when `start_up` has not
been entered. Starting a server in the child began a new session, whose
`reset_server_state` dropped the maps the parent was creating, and the child's
diverged fd table no longer matches the shared handler table. Children of a
started server keep the inherited state: the same atfork handler records the
fds that hold bpftime objects at fork time (`inherited_bpftime_fds`), and the
child's `close` of one closes only its fd, as the kernel drops just that
reference (it used to destroy the object for every process). Objects the
child creates itself are destroyed on close as before.

## Hash Map Update Flags

`fix_size_hash_map_impl` (the default `BPF_MAP_TYPE_HASH`) follows the kernel
htab: `BPF_NOEXIST` on an existing key fails with `EEXIST`, `BPF_EXIST` on a
missing key with `ENOENT`, a new key in a full map with `E2BIG`, flags above
`BPF_EXIST` with `EINVAL`, and deleting a missing key with `ENOENT` (all as -1
plus errno, the convention of the other maps). `BPF_F_LOCK` is still ignored.
Every mode walks the key's chain once under its stripe lock, which also does
the existence check of `BPF_NOEXIST`/`BPF_EXIST`. Lookup-then-update counters
depend on `BPF_EXIST`: when a reader deletes the key in between, ignoring the
flag re-inserted the stale count (the malloc example counted up to 20% too many
calls).

## Hash Map Concurrency

`fix_size_hash_map_impl` locks itself (`should_lock = false`); callers, the
typed helper entry points included, must not take the handler's map lock for
it. The handler's single lock used to serialize every operation, lookups
included: with one key per thread the map cost per probe hit went from 0.12
us (1 thread) to 10.7 us (6 threads, both clusters), lookup-only counters to
1.4 us.

`bpftime_hash_map` (11-03) is the kernel's preallocated htab in offsets:
`max_entries` preallocated elements `{word, next, key, value}` plus a spare
per stripe, and a power of two (at least `max_entries`) of u32 chain heads, each the index of its
chain's first element or a terminator naming the bucket (`NULLS | bucket`).
The bucket is the top bits of one multiply of the key (4 and 8 B keys are
their own hash XORed with a per-map random seed, longer ones mixed 8 bytes
at a time from the seed; the kernel's `hashrnd`). Without the seed, keys
computed to share one chain were trivial (1000 keys `j * phi^-1`: one chain,
2.2 us per insert); test "hash map chains stay short for keys made to
collide" (mutation: seed 0, 1000-element chains). The seed costs ~2 ns per
lookup (suite hash lookup +12.7%, per-CPU hash +4.5%; kept): it is loaded
from the map object, whose pointer arrives late, so load, XOR, multiply now
precede the head load, where the multiply used to overlap that pointer's load
(results `uprobe/dev-b7ded27a-20261103`). More for structured keys: unseeded,
the Fibonacci multiply spreads keys built from an arithmetic progression
(addresses, ids) better than at random; seeded they spread at random, as with
the kernel's jhash. Key-pattern matrix on 518a6b99: 8-byte addresses 11-28%
slower than unseeded (0.245 -> 0.313 us at 102400 / 90%), still 0.28-0.68x
the kernel; random keys unchanged (`uprobe/dev-518a6b99-20261103`). Seed 0
with the kernel's `BPF_F_ZERO_SEED` (map flag; the kernel's is for tests and
CAP_SYS_ADMIN only) or with `BPFTIME_HASH_ZERO_SEED` set to anything but 0
in the process that creates the map (bpftime's, for trusted keys); hash and
per-CPU hash (`hash_zero_seed`, `map_handler.cpp`). Test "Hash maps without
a seed on BPF_F_ZERO_SEED or BPFTIME_HASH_ZERO_SEED" (next_key orders of two
maps; mutations: flag ignored, variable ignored). One buffer holds
stripe locks, free lists, heads and elements, resolved once per operation
(`view`). It replaced the upstream open-addressing table (linear probing,
key and value inline in twice `max_entries` buckets, tombstones, the
per-byte `hash_func`): keys with fixed low bits (`id << 12`) clustered into
runs of thousands of buckets (1.7 us lookups, 6.9 us misses at 90% full),
tombstones piled up under churn (empty buckets 55% -> 21-43%), every value
was stored twice, and its striped writers needed probe-range locking and
exact-capacity bookkeeping whose token version had a cliff near capacity
(results `uprobe/hash-redesign-survey-20261102`, `uprobe/hash-keypattern-20261103`).

Lookups take no lock and write nothing shared. An element's word holds a
generation (every delete adds one step) and the in-use bit. A lookup walks
its chain: at each element it reads the word (acquire), restarts unless the
element is in use, compares the key (a four-byte key with an acquire load,
otherwise memcmp then an acquire fence) and accepts a match only if the word
is unchanged (`lookup_read`'s reader runs before that re-check). Past a
mismatch it loads `next` (acquire for four-byte keys; relaxed and an acquire
fence after a memcmp) and follows it only if the link it came through (the
head or the previous element's `next`, reloaded with acquire) still points
to the element and the word is still unchanged: updates reuse a retired
element in place of another element of the same chain, so an element
reached through a link read before its retirement but read after its reuse
further down the chain (its word alone looks fine), or retired while the
lookup stands on it, would skip the elements in between. Without the link
check 1-25 lookups per 2 s round still missed on the NUC (7f35edec); with
it 0 in 3.5 billion. Cost against 518a6b99 (link at head, no checks): suite
hash lookup -2.8%, update -7.9%, delete +1.4% (chains of about one); key
pattern matrix through BPF programs 0.97-1.04x, up to +11 ns at 90% load
(rand32, 102400), one cell 1.14x (10240 / 50% / shl12); NUC full ten within
noise (results `uprobe/dev-7ad5a152-20261104`, x86:
`uprobe/x86-nuc-r1-miss-20261104`). A `next` read before a retirement leads into
a free list (not in use; the lists end in `NULLS | n_heads`, a bucket that
does not exist), the rest of its old chain, or another chain, which ends in
another bucket's terminator; the lookup then starts again. A step cap
restarts runaway walks. A key's element is only in its own bucket's chain,
so a wrong chain cannot produce a false hit. (The per-step re-check had
been dropped in e5ba4aa7, when nothing reused an element in place and no
test could tell; it came back with updates in place.) Inserts write key and
value after a release
fence (a lookup that read the element's previous key and sees new bytes
also sees the delete's generation), then `next`, the word (release), the
head (release). A delete unlinks (release store), then sets the word's new
generation (release), and gives the element back after unlocking the
stripe. In the other order (e5ba4aa7) a deleter stopped between the two
stores left an element in its chain that was not in use, and every lookup
reaching it restarted: until the deleter ran again, forever if it was
killed (review `uprobe/dev-e5ba4aa7-20261103/REVIEW.md`, 1, reproduced).
Now such an element is out of the chain and still in use; test "hash map
lookups pass a delete stopped halfway" (mutation: the stores swapped).

Writers lock the stripe of the key's head (16 heads per stripe, at most 4096
stripes, one cache line per lock): one lock per operation, no ranges or
retries. Free elements are in per-CPU lists (the kernel's `pcpu_freelist`,
one lock and line each, at most 64): an insert pops from its thread's list
(chosen from the thread pointer: no system call, no TLS variable), else
from the others one at a time (try-locked first, then waiting for each
lock, since a busy list may hold the last elements); a delete pushes to its
thread's list. The free elements are the capacity: E2BIG when an insert
found none, so no count is kept. Exact without concurrent deletes; with
them an insert can miss an element pushed to a list it already passed and
fail with elements left (the window of the kernel's `pcpu_freelist`; not
reproduced in 22-40 million inserts with 1-28 free) (`get_elem_count` sums the lists, exact without concurrent
writers). Lock order: stripe, then list; a list is never held while taking
a stripe. Step 1 of the redesign (42595149) had replaced the tokens with one
shared atomic count, which cost an atomic per insert and delete (suite hash
delete 33.9 -> 40.6 ns). Against the table before both steps (a4ac0d31;
results `uprobe/dev-e5ba4aa7-20261103`): suite hash lookup -28%, update -9%,
delete +8% (the stripe lock, then a list lock); per-CPU hash lookup -8%,
update -6%, delete +12%. Delete + insert through BPF programs at 90% load,
one thread: 0.14-0.28 us, 0.27-0.62x the kernel (a4ac0d31: 2.9-34 us).

The stripe and free-list locks and the handler's `map_lock`
(`bpftime_lock_guard`) are taken with `bpftime_spin_lock`
(`map_common_def.hpp`; the ring buffer's reserve lock uses the same protocol
inline with a yield, above: 4-6 producers spent 67-75% of their samples in
glibc's lock, `uprobe/output-dims-20261106` finding 13): glibc's protocol on the
`pthread_spinlock_t` word (exchange 1 to take, release store 0), inline, so
pthread_spin_lock on the same word still excludes (test "bpftime spin locks
exclude each other and pthread spin locks"). Through glibc each lock was a
PLT call, glibc's function and its out-of-line atomic: 1-4 ns per map
operation. The exchange stays `__atomic` (with -moutline-atomics, libgcc's
LSE helper): an inline `swpa` was no faster on the Orin and an
`ldaxr`/`stxr` loop 5 ns slower. Only on aarch64 glibc (its generic
protocol); everything else calls pthread_spin_lock: x86-64 glibc keeps its
own asm with the opposite protocol (pthread_spin_init stores 1 = free, lock
is `lock decl`, held at 0), and with the inline exchange there every map
operation spun forever on a lock pthread_spin_init had made (found on the
NUC; the test above hung).
The lock is not fair (glibc's protocol is not either): one thread updating a
stripe in a tight loop (about 4M updates/s, 35 ns under the lock) starves
another writer of that stripe for 10-100+ ms per update (pthread_spin_lock,
before 0ffe368b, was worse; review 2026-11-05). Probe rates (about 1 us per
Frida hit, 0.4 us per hooked system call) stay well below that; a ticket lock
would fix it at one more word per stripe if a workload ever reaches it.
The table takes it inline; `bpftime_lock_guard` calls the out-of-line
copies (`bpftime_spin_lock_outlined`): forced inline there, the per-CPU
hash helpers ran 30 more instructions per update and lookup (LTO kept the
guard's constructor out of line around it), and left to the compiler the
stripes lost 2 ns per hash update.

An update of an existing key does not write in place (review 3): as the
kernel's preallocated htab with its per-CPU `extra_elems`, it takes the
stripe's spare element (one per stripe, outside `max_entries`, on the lock's
line, under the lock), writes key and value there and links it in the old
element's place (one release store of the old element's link: the head or
its predecessor's `next`), then retires the old element (new generation) and
leaves it as the spare. A lookup on the predecessor loads the old element
(its value, whole) or the new one. A `lookup_read` copy holds one value
whole (the word re-check catches reuse) only if the reader reads with acquire
loads on 4-byte-key maps (the header says so): that path has no fence between
the reader and the re-check, which d16fcbf5 removed (4 ns per lookup), and a
memcpy reader there got torn copies (1 of 80M at 64 B, 575 of 39M at 256 B;
review 2026-11-05). The in-tree readers (the per-CPU hash's slot number, the
tests) use acquire loads; a fence for every non-empty reader would charge each
per-CPU lookup for nothing. The kernel links the new element at
the head and then unlinks the old one, so a lookup past the head, on the old
element's predecessor, reaches the end and misses a key present throughout
(review of 3cb3b630..7670cee5, R1). Measured on the NUC (x86, results
`uprobe/x86-nuc-r1-miss-20261104` on the x86-nuc branch), two keys of one
bucket updated at once from two threads, looked up from three: the kernel's
htab (BPF_PROG_TEST_RUN, bpf_loop) missed 0-0.63% of lookups per 2 s round
depending on the CPUs, this map linked at the head (d5279469) 0.001-0.31%;
on the Orin 4-8 in 61 million. Tests "hash map lookups of keys updated at
once never miss" (two updaters, three readers; misses and torn copies must
be 0; d5279469's code: 2-7 misses per run on the Orin) and "hash map updates
keep the chain's order" (next_key order unchanged across updates; d5279469:
the tail key moved to the head); "hash map lookups never return a value
reused for another key" stands a lookup on an element while its key is
updated and the spare reused for another key (mutation: no retire, the
lookup takes the other key's value). In place, 16 B values
`{x, x}` with two updaters and three readers mixed halves in 995,658 copies
and 391,877 of 364 million pointer reads in 5 s; now 0 of 55 million copies
and 75 pointer reads in 1.5 s (the old element reused by a later
update while the reader holds it; the kernel has the same window). Test
"hash map copies of a value being updated are whole" (mutation: in place,
torn copies); "hash map updates of existing keys replace their element" (a
key alone in its chain stays found, mutation: the old head kept as next,
lookups never return; a full map still takes updates; spares are not
capacity). Cost: suite hash update (existing keys) 25.0 -> 34.6 ns, lookup
and delete within 3%, kernel 95.8 ns; spares are up to 1/8 more elements
(heads / 16, at most 4096). One spare per free list taken by an exchange
cost 47.2 ns (the LSE helper and its barriers, half the added samples).
Changes against the in-place table: a value pointer follows the element,
not the key, so it is valid until the key is updated or deleted (a
BPF `*v += 1` racing an update of that key goes to the old element, lost;
kernel the same). An updated key keeps its place in its chain (the kernel
moves it to the head, so there next_key during updates returns keys again
and a tool that updates each key it visits can cycle between two keys of a
bucket). A value pointer kept
longer writes into a free or reused element, as in the kernel.

A process killed inside a writer leaves its stripe locked
(writers of that stripe and `next_key` calls reaching it hang, lookups do
not); a tool killed inside `next_key` is such a holder too. Lookup-and-delete
copies the value out after unlocking (a tool's bad buffer, the kernel's
EFAULT, faulted under the lock and left the stripe locked until 11-05; test
"bpftime_hash_map lookup-and-delete copies out after unlocking", the
traced process's writer hung without it). One killed holding a free list's lock hangs that list's deletes and
every insert that runs out of its own list, and with it that insert's
stripe (reproduced: the inserter and another writer of its stripe stopped,
a lookup returned); the kernel waits there too, but its holders cannot die.
One killed between taking and linking an element, or between unlinking and
giving it back, loses that element (E2BIG one entry early). Tests:
"bpftime_hash_map replaces long keys in place without misses" (8- and 16-byte
keys: the memcmp path under replacements; removing that path's two acquire
fences does not fail it, its readers' acquire loads cover them on this core),
`test_hash_map_chains.cpp` (writers lock only the key's stripe, lookups no
lock; lookups of keys present throughout never miss while elements around
them move through the free lists into other chains, mutation: any
terminator taken as the end; inserts take elements from every list up to
exactly `max_entries`, also with 2 entries and 6 lists, mutation: no
stealing; `lookup_read` with 8 and 16 B keys), `test_hash_map_value_access.cpp`
(a lookup stalled on an element freed and reused by another key misses,
mutation: no re-check after the read), `test_hash_map_lockfree_lookup.cpp`
(torn keys; values read while no delete runs), `test_hash_map_striped_writers.cpp`
(threads against a reference, exact capacity, two chains churning apart,
processes mapping the segment at different addresses), "bpftime_hash_map
churn near capacity locks only the keys' stripes".

Element layout: word at 0, next at 4, key at 8, value at `round_up(8 +
key_size, A)`, element size a multiple of A, where A is 8 for values of 8
bytes or more and 4 otherwise; the elements start 16-aligned. A misaligned
value used to kill a BPF atomic add (`ldaddal`) with SIGBUS on aarch64 (an
8-byte key and value crossing a 16-byte boundary in the old layout). Arrays,
per-CPU arrays and per-CPU hash values keep a `value_size` stride: a
naturally aligned C struct with an 8-byte member has a size that is a
multiple of 8, so only packed structs could misalign there. Test:
`[alignment]` (key sizes 1-64, value sizes 1-8, 12, 16, 20, 24, an atomic add
on every value of 4 bytes or more, every value written to its last byte
without reaching a neighbour).

`bpftime_hasher::hash_func` (4 bytes: 29791, 961, 31, 1 per byte; others
byte-wise times 31) stays for the GPU shared hash map; the table no longer
uses it.

## llvmbpf Local Call Overlay

`vm/compat/llvm-vm/local-call-return-block.patch` generates
`compiler_local_call.cpp` in the build tree, replacing upstream
`src/compiler.cpp` the same way as the array JIT overlay. Upstream registered
the return block of a bpf-to-bpf call only for calls after pc 0, so a program
starting with a call (libbpf-tools funclatency's kretprobe) stored a null block
address and crashed the traced process while compiling. Conflicts on upstream
updates must fail configuration.

## Failed JIT Compiles

The LLVM VM has no interpreter, and its `exec` retried the compile on every
invocation after a failed one (about 1 ms each, logged each time: 1,122
calls/s for funclatency with the unsupported helper 37). `bpftime_prog_load`
now marks such a program `jit_failed`, logs once that it is disabled, and
`bpftime_prog_exec` returns `-ENOEXEC` without running; the Frida callback
does not log it again. VMs with an interpreter (ubpf) still fall back to it.
A disabled program is not a stub for the missing helper: the tool loads but
records nothing.


libbpf turns instructions of a branch it knows is dead into calls of helper
2001000000 + map index (the load of a map it did not create) or 2002000000 +
extern index (a kfunc it did not find), and an instruction whose CO-RE
relocation failed (a field this kernel lacks, read behind
`bpf_core_field_exists`) into a call of 195896080 (0xbad2310, "bad relo", both
halves of an ldimm64); the kernel's verifier removes the dead code. bpftime has no verifier: such a program did not compile and was disabled
(opensnoop, whose libbpf-tools compat buffer creates only its ringbuf or only
its perf map). `bpftime_prog_load` points those calls at a private slot
(`BPFTIME_LIBBPF_POISON_SLOT`, 8188) whose helper reports that it ran and
returns -EINVAL. The CO-RE form was missed until 11-05: memleak's tracepoint
programs were disabled for it (`Ext func not found: _bpf_helper_ext_195896080`).
Tests "Calls libbpf poisoned in dead code do not disable the program", "Calls
libbpf poisoned for a failed CO-RE relocation do not disable the program"
(disabled, -ENOEXEC, without the rewrite).

libbpf-tools keep their options in `.rodata`, a `BPF_F_RDONLY_PROG` array that
libbpf freezes before loading the programs, and the code of an option that is
off may call helpers bpftime does not have (opensnoop's `full_path`:
`bpf_get_current_task_btf`, `bpf_probe_read_kernel_str`; syscount's `-P`:
`bpf_get_current_task`). The kernel's verifier treats a frozen read-only map's
value as constant and drops that code; bpftime disabled the whole program.
`BPF_MAP_FREEZE` is now recorded (`bpf_map_handler::freeze`: later syscall
updates, deletes, pushes, pops and lookup-and-deletes are EPERM, as is a
writable mmap of a frozen array through the syscall server (a read-only one
still points into the shared value, which stays writable memory), and a second
freeze EBUSY; it was a no-op; push, pop, lookup-and-delete and mmap were left
out until 11-05, test "A frozen map refuses every syscall write"), and when
a program calls a helper nobody registered, `bpftime_prog_load` runs
`reachable_instructions` (`bpftime_prog_reachability.cpp`): a forward
analysis that tracks scalars and pointers into frozen read-only array values
and follows a branch decided by them one way only. Calls of missing helpers it
finds unreachable go to a private slot (`BPFTIME_DEAD_CALL_SLOT`, 8187) whose
helper reports that it ran; a reachable one still fails the compile. A kfunc
call (`BPF_PSEUDO_KFUNC_CALL`, src_reg 2, imm a BTF id) is never registered and
counts as missing (before 11-05 one in dead code kept the program disabled). It is
conservative: unmodeled instructions leave their registers unknown, joins turn
differing registers unknown, calls clobber r0-r5, bpf-to-bpf callees and
callbacks (ldimm64 `BPF_PSEUDO_FUNC`, target `pc + imm + 1`) are entries; the
stack is not tracked, so a constant spilled and reloaded is unknown. Tests:
"Branches on read-only constants are followed one way", "Reachability keeps
subprograms, callbacks and joined paths", "A missing helper behind a
read-only constant does not disable the program" (no constants, calls keeping
r1-r5, or joins keeping a value each fail one). Sound for programs the kernel
accepts: nothing stops a program from writing a `BPF_F_RDONLY_PROG` map in
bpftime (no verifier), and then the constant would be stale. Not applied to
AOT-loaded programs (they skip `bpftime_prog_load`'s rewrites), nor to maps
restored by `bpftimetool import` (not frozen again). `bpftime aot` compiles
after the rewrites, so its objects call the two private slots:
`load_aot_object` registers them too (it did not, and such an object failed to
load). Test "An AOT object of a program with poisoned calls loads" (the
branch is on the input: LLVM drops a call behind a constant one).
## Stack Trace Map

`stack_trace_map_impl::fill_stack_trace` follows the kernel `__bpf_get_stackid`:
frames beyond `value_size / 8` are dropped, stored traces are zero-padded to the
whole value (lookups copy `value_size` bytes; a shorter stored vector was read
out of bounds and showed heap garbage as frames), the same trace returns its id,
a different trace in an occupied bucket returns `-EEXIST` unless
`BPF_F_REUSE_STACKID` replaces it, and `BPF_F_FAST_STACK_CMP` compares the full
hash. `bpf_get_stackid` returns errors negative in its 64-bit result (it used
to cast them to a valid-looking `uint32_t` id; `ret < 0 ? ret : (uint32_t)ret`
still did, both branches being unsigned int: -EEXIST came back as 0xffffffef).
No frame left after the skip is `-EFAULT`, as the kernel (it stored an empty
trace).

A stored trace is found without the map lock (`find_stack_trace`), as the
kernel's buckets: the map was a boost `unordered_map` of shared-memory vectors
filled through one shared `key_buf`, so every call took the map's spin lock,
and memleak on six threads allocating from one stack spent 42% of its cycles
spinning on it (0.78x bpf; `uprobe/vs-pre-mt-20261104`). Now each id has a
slot holding the offset of its record (hash, frame count, frames), allocated
under the lock on the slot's first fill and kept until the map is destroyed (a
delete clears the count), so a reader never follows a freed record. Records
are not preallocated: bpftrace's `ustack` map is 131,072 x 1,016 bytes, more
than the default 128 MB segment. A writer clears the count, writes the frames
and the hash, then stores the count (release); a reader loads the count
(acquire), then compares the hash before the frames. The hash is what keeps a
torn read from matching: a mix of two traces read during a replacement can
have a third trace's frames, never its hash (the test "never take a mix of two
traces for a third" fails within 2 s without the hash comparison). Anything
but a match takes the lock and `fill_stack_trace` decides, so `-EEXIST` on an
occupied slot still takes the lock every time (the kernel returns it without
one). With `BPF_F_FAST_STACK_CMP` a reader may match a trace that is being
replaced, as in the kernel. Userspace lookups and deletes keep the lock; a
lookup returns a pointer that the caller copies after the lock is released,
so a concurrent `BPF_F_REUSE_STACKID` replacement can still tear that copy
(as before).

## aarch64 System Call Rewriting

`attach/text_segment_transformer` intercepts system calls by rewriting code
(the `bpftime start -s` / transformer path). x86 uses zpoline (`syscall` ->
`call *%rax` into a page at address 0); aarch64 cannot (instructions must be
4-byte aligned and `bl` overwrites x30), so each `svc #imm` becomes `b gate`
to its own 32-byte gate in a trampoline region mapped within the +-128 MiB
reach of `b` (`MAP_FIXED_NOREPLACE`, closest free gap), after svc-hook
(Middleware 2025) and ASC-Hook. The gate saves only x16/x17 on the stack and
passes its own address (`adr`); `syscall_hooker_asm` saves x0-x30, NZCV, FPSR,
FPCR and q0-q31 (a 784-byte frame, on the thread's hook stack: below), calls
`syscall_hooker_cxx`, restores everything (x0 = result; FPCR written back
only if the hook changed it, as the kernel preserves it and a write can
serialize) and branches to the gate's return stub, which restores x16/x17 and
branches to the site + 4. The kernel preserves every register but x0 across `svc`, so nothing
may be clobbered: svc-hook's x6/x14/x15 scratch registers and unsaved SIMD
registers would corrupt callers. `rt_sigreturn`, `clone` and `clone3` take the
gate's direct stub: all registers and sp restored, `svc` issued in the gate,
plain `b` back, so a child on a fresh stack never returns into C frames (they
are not traced, as on x86). glibc returns from signal handlers through the
vDSO's `__kernel_rt_sigreturn` (never rewritten); musl and Go use their own restorer, covered by a test.

The hook runs on a stack of its own per thread (`bpftime_hook_stack`, initial-
exec TLS read by the asm: 128 KiB above a 64 KiB guard, mapped with the hook's
own system calls at the thread's first traced call, which still runs on the
caller's stack). A thread exiting through the hook (`SYS_exit`, in the
special-call bitmap) is on its stack until the kernel takes it down, so it only
offers it (a lock-free table of 256): a later thread's allocation takes it once
the exited thread is gone (`tgkill(pid, tid, 0)`: ESRCH; a reused tid only
delays it). Before, every thread ever created left two mappings and at least a
page behind (16,384 threads: 84 -> 32,854 mappings; at the stock
`vm.max_map_count` of 65530 the process's own mmap and pthread_create fail at
about 32k; review 2026-11-05). Not offered: a stack marked `shared`, and one
the exiting thread did not map (`owner_tid`): a vfork-style child exits on its
parent's TLS, and a clone with `CLONE_VM` and no `CLONE_SETTLS` maps the
parent's stack first, so it is the parent's. Threads exiting through code
that is not rewritten, or with the table full, leave theirs mapped. Tests:
"hook stacks of exited threads are used again" (2,000 threads, 110 mappings
before and after; 4,110 without the reuse), "a vfork child's exit leaves its
parent's hook stack alone" (SIGSEGV without the owner check). Below
the application's sp it writes 32 bytes (the gate's x16/x17, then x0/x1 while
it switches). It wrote 880 bytes with no agent callback, 1,376 with the agent,
18,144 with a program attached (the LLVM JIT allocas 32 local-call levels of
512 bytes plus a call stack on every run), while Go makes its system calls on
goroutine stacks (2 KiB to start, about 900 bytes guaranteed below sp, no
guard page): every dynamically linked Go program crashed once the agent was
loaded, programs or not (neighbouring goroutines' frames overwritten:
"unknown caller pc", "unexpected fault address", SIGSEGV). The frame stays on
the current stack when the thread has no hook stack yet, when sp is already on
it (the hook's own calls, a handler without an alternate stack interrupting a
hooked blocking call: below the live frames) and when the top frame is live
and sp is elsewhere (a handler on the application's sigaltstack, as Go's
always are: the top holds that frame). The top frame is live while bit 0 of
its slot (the stack's last word, `hi - 8`) is set: the slot is written after sp
moves onto the hook stack and cleared after the frame's last read, while sp is
still on it, so a handler arriving in between runs below on the hook stack or
finds the frame live, and one arriving before or after destroys nothing in use.
A TLS flag (`busy`) cleared before the epilogue read the frame back let a
sigaltstack handler's hooked call, in those 22 instructions, build its frame
over the one being read: the thread hung, or went on on the signal stack
(review 2026-11-05; a hang within 30 s in the test below with the clear moved
back). The tests before the switch use no flags (NZCV is saved later). The
frame's slot at 776 holds the application's sp (bit 0: switched, bit 3:
direct path), from which the unwind information computes the CFA; the saved
registers are described relative to sp, and the CFA from x0 for the
instruction before the slot is written and from x17 after it is cleared.
A frame can be left marked without its epilogue: a vfork-style child
(`posix_spawn`, `system`, Go's `os/exec`: `CLONE_VM` without `CLONE_SETTLS`)
shares its parent's TLS and hook stack and execs or exits inside a hooked
call, and a handler can `siglongjmp` out of one. Afterwards every call of that
thread stayed on the caller's stack (32 -> 1272 bytes below sp; for Go with
programs, the 18 KB crash again). Now a marked top with sp off the hook stack
costs a `sigaltstack(NULL, &old)` (through `orig_syscall_direct`, the one
`svc` the scanner skips; 96 bytes below sp): on the signal stack
(`SS_ONSTACK`) the frame is live and the call stays; otherwise the frame was
abandoned and the call switches over it. It stays on an error, and on
`SS_DISABLE` in a thread that set an `SS_AUTODISARM` signal stack: inside such a
handler the kernel reports the stack disabled and drops the `SS_AUTODISARM` bit
too (`sas_ss_reset`), exactly as with no signal stack, so the hook records the
flag itself (`autodisarm` in the TLS block: from the thread's hooked
`sigaltstack` calls, now in the special-call bitmap, and a query at its first
hooked call); with that flag missing the case crashed (SIGSEGV). An
`SS_AUTODISARM` stack set by code that is not rewritten, after the thread's
first hooked call, stays unknown. It also stays, for good, once the thread made
a clone whose child runs at the same time on its TLS (`CLONE_VM` without
`CLONE_VFORK` or `CLONE_SETTLS`, or clone3 flags not readable: `shared`). The cost of telling: a handler without an
alternate stack that moves to a stack of its own (`swapcontext`) while a frame
is live and makes a hooked call there is taken for an abandoned frame and
overwrites it (with `busy` it stayed); no runtime checked here does that (Go
returns from its handler before switching). Cost: 39 instructions, 14-15 cycles per hooked call (write: 397 -> 406
ns rewritten, agent dispatch 426 -> 436 ns; results
`syscall/realapps-20261102/hook-perf`). Tests: "a hooked call uses only a few bytes of the caller's stack", "a
handler on an alternate stack leaves the hook's live frame alone", "a handler
on the hook stack builds its frame below the live one", "handlers on an
alternate stack interrupting the hook leave its frame intact" (about 340k
signals, nearly all landing in the hook, each unwound from the handler back to
the interrupted loop; it cannot single out the few instructions with their own
CFA rows, which were checked with `llvm-dwarfdump --eh-frame`), "the hook stack
is used again after a frame on it is abandoned" (`posix_spawn`, `system`,
`vfork`, `siglongjmp` from either stack), "a handler on an auto-disarmed signal
stack leaves the hook's live frame alone"; end to end with Go,
results `syscall/realapps-20261102`. A raw `CLONE_VM` thread without
`CLONE_SETTLS` shares its creator's TLS and hook stack (not covered).
Frida uprobes on Go functions still run programs (18 KB) on the goroutine
stack.
Sites are found per executable mapping by its ELF file's `SHF_EXECINSTR`
sections (no constants of a shared segment taken for code). Mappings without
section headers (anonymous code, unreadable files) are skipped and counted in
the log unless `BPFTIME_SYSCALL_REWRITE_ANON=1`: scanned word by word, a
literal-pool constant equal to `svc #0` was rewritten into a branch.
The section table comes from the mapped file itself: a file renamed over or
unlinked under a running process (a package upgrade, then `bpftime attach`)
shows as "path (deleted)" and is opened through `/proc/self/map_files`
(needs `CAP_CHECKPOINT_RESTORE`, so root; otherwise skipped with a warning
naming the mapping); any other path must still have the mapped device and
inode (the device too: an inode number alone may exist on another filesystem
mounted over the path; test "a mapped file is the same file only with its
device and inode"). On btrfs the inode alone (`same_mapped_file`): maps
shows the superblock's device and stat the subvolume's anonymous one, so
every library there was skipped (test "a mapped file on btrfs is recognized
by its inode"). Not handled: overlayfs on kernels before `file_user_inode`
(6.8), whose maps show the underlying file's device and inode. Before, "(deleted)" mappings were always skipped
(untraced). `MAP_SHARED` executable
mappings are skipped: rewriting a writable file's mapping stored branches to
this process's gates into the file. Tests: "a deleted library is rewritten
from the mapped file when allowed" (rewritten as root, skipped otherwise),
"shared executable file mappings are not rewritten".
The vDSO's fallback `svc` sites (clock_gettime, clock_getres, gettimeofday:
CPU-time clocks always take them) are rewritten from its in-memory ELF image
(the write makes the process's own copy of the page), except
`__kernel_rt_sigreturn`, found by its dynamic symbol: unwinders recognize
signal frames by its code. `[stack]` and the transformer's own
`syscall_addr` are skipped.

`syscall_hooker_asm` and `call_orig_syscall` carry unwind information. The
hook frame's CFA is the application's sp at the svc; its return address
column is x17 (the gate's scratch register), a DWARF expression decoding the
gate's `b site + 4` at +16 from the gate address (x16, then its slot, then
x16 - x17 after the return offset is added), so the caller's frame is the
function that issued the svc with its own x30. Without it, unwinding stopped
at the hook: `pthread_cancel` of a thread blocked in a hooked read skipped
every destructor up to the thread start (glibc then longjmps there), and
backtraces from signal handlers stopped. The agent (dlmopen) has its own
libgcc_s: its frames' personality called that copy's `_Unwind_SetGR`, which
aborted on its never-filled register size table, so
`_bpftime__setup_syscall_trace_callback` unwinds once at setup, and
`syscall_callback` rethrows `abi::__forced_unwind` from its catch-all
(swallowing it aborts). Not covered: a cancel arriving while a program runs
stops at the JIT frames (no unwind information) and longjmps as before; the
gate stub itself has none either. Tests: "a thread cancelled in a hooked
blocking call runs its cleanups", "a cancel unwinds through a hook in another
namespace" (stand-in agent `test/fake_agent_unwind.cpp`; without the setup
unwind it aborts), "backtraces from a signal handler pass the hook", "the
hook's floating-point control changes do not reach the caller"; end to end
with the real agent: results `reviews-20261031/fixes/cancel-e2e`.
Each site is one aligned word store plus an I-cache flush; `svc` -> `b` is a
permitted concurrent modification. Failures (including restoring a page's
protection) skip the site or mapping and log; they never end the host. The asm lives in file-scope blocks
and `syscall_hooker_cxx` is `used`: LTO dropped them from executables.
Code that becomes executable later is rewritten when it is mapped:
after a hooked `mmap` of a file succeeds with `PROT_EXEC`,
`after_hooked_syscall` rescans just that range (inside the re-entrancy
guard, under `rescan_mutex`, which an atfork handler keeps from being
inherited locked), so a library loaded with dlopen is rewritten before it
runs (ld.so maps code executable from its file). Anonymous `mmap` and every
`mprotect`/`pkey_mprotect` to `PROT_EXEC` are rescanned only with
`BPFTIME_SYSCALL_REWRITE_ANON=1` (JIT code; a file mapped first without
`PROT_EXEC`, such as a text relocation, is not covered otherwise). The
decision is taken from the call's arguments: reading `/proc/self/maps` for
every JIT `mprotect` cost 190 us per call. The rescan runs inside ld.so's dlopen with
the loader lock held: never call `dl_iterate_phdr`, `dlsym` or `dlopen`
there. Trampoline regions are recorded and never scanned. The agent's
namespace is loaded inside an `unhooked_scope`, so its own libc stays
unrewritten; a library the agent itself dlopens later in its namespace
would be rewritten (it does not today). Gates of an unloaded library are
left mapped. Tests:
`bpftime_text_segment_transformer_tests` (aarch64; each case rewrites a
forked child): register preservation, arguments/results/override, threads,
fork/clone/posix_spawn, signals, own restorer.

## aarch64 Lazy System Call Rewriting (seccomp, research)

`BPFTIME_SYSCALL_LAZY=1` adds a fallback for `svc` the rewriting never saw
(JIT code without anonymous scanning, sites without trampoline space): after
the agent is loaded, `enable_lazy_rewriting` sets `no_new_privs` and installs
a seccomp filter on all threads (TSYNC) that allows `rt_sigreturn`, `clone`
and `clone3` by number (the gate's direct path), calls from
`call_orig_syscall`'s page and from the agent's namespace
(`namespace_code_ranges`, which must stay unrewritten), and turns every
other system call into SIGSYS. `on_sigsys` takes the site at `pc - 4` (the
kernel reports the address after `svc` and restores x0), gives it a gate
from a fixed pool of 64 KiB regions mapped near it with raw
`MAP_FIXED_NOREPLACE` probes, writes gate and branch through
`/proc/self/mem` (no protection change or `/proc/self/maps` read: with them
a first trap cost 90 us, now about 13 us), and sets `pc` back to the site,
which runs again through the gate and the normal dispatch; no BPF program
runs in the handler. With no gate it makes the call untraced and counts it.
Everything the handler does is raw system calls via `orig_syscall_direct`
(allowed by the filter) and fixed storage; code patching is serialized with
rescans by `patch_owner`, a spin lock that refuses its own thread (the call
is then made untraced). While active, the hook removes SIGSYS from
`rt_sigprocmask` sets and `rt_sigaction` masks and answers `rt_sigaction`
for SIGSYS itself, keeping the application's action without installing it (it runs for
SIGSYS that is not ours, below): a trap with SIGSYS blocked or ignored kills
the process. Costs: about
+60 ns per system call (the filter compares the instruction pointer, so
the kernel's per-number cache cannot skip it). Not for general use: the
filter cannot be removed and survives execve, where nothing handles SIGSYS,
so an exec-ed program would be killed at its first system call (a preloaded
`taskset` exec-ing its target died this way, rc 159). So while active the
hook refuses `execve` and `execveat` with EPERM and a warning naming the
path (read with `process_vm_readv`, a bad pointer logs "?"): `system`,
`posix_spawn` and shells fail visibly instead. Making exec work needs a
fixed-address allowed page plus a loader or a ptrace helper re-arming the
new image (as K23 does on x86, where SUD is cleared on exec); arm64 6.8 has
no SUD. Not done. Non-seccomp SIGSYS and the application's own SIGSYS handler
are not run; temporary masks of `ppoll`, `pselect6`, `epoll_pwait` and
`rt_sigsuspend` are not stripped of SIGSYS (a handler interrupting the wait
that reaches a never-rewritten `svc` is killed). Tests: `[lazy]` cases (first
trap rewrites once, threads trapping on one site, SIGSYS kept deliverable,
fork children, execve/fexecve/posix_spawn refused).

Fixed after an independent review (results `reviews-20261031`), each with a
`[lazy]` test that fails without it:
- `patch_owner` has atfork handlers (prepare takes it unless this thread
  holds it, the child clears it): a fork while another thread patched left
  the child's lock owned by a thread it lacks, and its first trap spun forever
  (8 of 20 children in the review).
- Enabling is refused (warning naming the thread) while any thread has SIGSYS
  in its `SigBlk` (`/proc/self/task/*/status`): the filter covers every
  thread, the unblock only the caller, and a thread that had blocked all
  signals died at its first trap.
- The handler has `SA_ONSTACK`: a trap's frame is about 5 KiB, and a thread
  with an alternate signal stack (every Go thread) takes it there.
- Allowed code ranges are compared as `[lo + 4, hi + 4)` (the kernel reports
  the address after the svc), and `call_orig_syscall`'s own svc is allowed
  exactly, not its page: an svc in an allowed range's last word trapped, and
  a relink putting `syscall_addr` in a page's last word (1 in 1024) would
  have trapped the hook's own calls into unbounded recursion.
- A process that may not write `/proc/self/mem` (not dumpable: setuid
  programs, daemons after `PR_SET_DUMPABLE 0`) rewrites with `mprotect`
  around the store instead, the page's protection read from
  `/proc/self/maps` with raw system calls; every trap was emulated untraced.
  The emulation path (no gate space) refuses execve too and sanitizes signal
  masks (`sanitize_signal_call`), counting refusals (reported by the next
  rescan; see below).
- `patch_lock` yields every 64 spins (a SCHED_FIFO spinner on the holder's
  CPU).

Fixed after the second review (results `reviews-20261031/rereview`):
- One set of transformer fork handlers (`install_transformer_fork_handlers`,
  registered once at setup) takes `rescan_mutex` and then the patch lock, the
  rescan's order. The patch lock's handlers were a second, later set, whose
  prepare glibc runs first: a fork racing a dlopen (rescan holding
  `rescan_mutex`, waiting for the patch lock in `install_region`) deadlocked
  within a second. Test: "lazy mode survives a dlopen racing a fork".
- Our filter's traps carry `SECCOMP_RET_DATA` `LAZY_TRAP_DATA` (0xb7e5,
  reported in `si_errno`; with several filters returning TRAP the kernel
  keeps the newest one's data). Any other SIGSYS goes to the application's
  action (`forward_sigsys`): its handler runs with the original siginfo and
  context (the x0 it sets is the call's result), `SA_RESETHAND` and its mask
  applied, SIGSYS left deliverable; with `SIG_DFL` (or `SIG_IGN` for a
  seccomp trap, which the kernel forces) SIGSYS is restored to its default
  and raised, ending the process as without bpftime. An action set before
  lazy mode starts is read then. `lazy_rewrite_site` refuses the hook's own
  svc and the allowed ranges. The handler took every seccomp trap as its
  own: the application's filter trapping the hook's own svc made it rewrite
  that svc into a branch to a gate, and the hook recursed until the stack
  overflowed (SIGSEGV instead of SIGSYS). Either install order works: the
  application's filter newer, its data wins at a new site (forwarded, the
  site stays unrewritten); ours newer, the site is rewritten and the hook's
  svc then traps with the application's data. An application filter using
  0xb7e5 as its data would be taken for ours. Tests: "lazy mode passes the
  application's seccomp traps to its handler" (both orders), "lazy mode
  leaves the application's untrapped seccomp SIGSYS fatal".
  The kernel decides from the installed action, ours, whether a call
  interrupted by a SIGSYS sent with kill restarts: whenever the
  application's action changes our handler is reinstalled (raw
  rt_sigaction, from its installed form) with the application's SA_RESTART
  (always for SIG_DFL and SIG_IGN); without it a read blocked under an
  SA_RESTART handler, or with SIGSYS ignored, returned EINTR (test "lazy
  mode restarts calls across a SIGSYS as the application's action"). Still
  different: with SIG_IGN natively nothing is interrupted, while calls that
  never restart (poll, epoll_wait, a relative nanosleep) return EINTR under
  our handler. The application's action is published whole (written into
  the next of eight copies, then made current): a plain array copied by one
  thread's rt_sigaction while another's handler read it could pair a new
  handler with old flags. `rescan_mutex` records its holder, and the fork
  prepare handler does not take it when the holder is the forking thread: a
  fork from the application's SIGSYS handler that interrupted a rescan (its
  filter trapping the rescan's open of /proc/self/maps) deadlocked (test
  "lazy mode lets the application's SIGSYS handler fork during a rescan").
- MAP_SHARED code is not patched (`site_in_shared_mapping`, counted in
  `shared_skipped` and made untraced each time): through `/proc/self/mem`
  (FOLL_FORCE) or `mprotect`, the branch to this process's gate landed in
  the file and in every process mapping it (the default mode skipped shared
  mappings since 3d7ada3d). The handler keeps the mapping it looked up last
  (a maps read is about 75 us, so a JIT mapping's first trap pays it once);
  every call that changes mappings (`mmap`, `mprotect`, `pkey_mprotect`,
  `munmap`, `mremap`, `shmat`, `remap_file_pages`) forgets it after the
  change is made, through the hook's out-of-line path (a bitmap of special
  numbers), its in_hook pass-through and the emulation path. Only
  mmap/mprotect used to reach the hook's: a MAP_SHARED file mremap-ed over a
  patched private JIT page was taken for that page and the next trap wrote
  the branch into the file. Tests: "lazy mode leaves shared code mappings
  unpatched" (memfd, RWX, MAP_SHARED), "lazy mode notices a shared mapping
  moved over patched code".
- execve calls the handler refused are reported by the next `rescan` (after
  an mmap or mprotect, which are not async-signal-safe) and counted in
  `execs_refused`. The next hooked call reported them: possibly an
  `rt_sigaction` in a signal handler that interrupted malloc, while logging
  allocates and takes locks. Test: "lazy mode reports refused execve outside
  signal handlers" (the signal sent from untraced code, so the handler's
  sigaction is the first hooked call after the refusal). A process that
  never maps code again after a refusal logs nothing (the count stays in
  `execs_refused`).
- The `mprotect` fallback (not dumpable) writes a gate that crosses a page
  (every 128th: header 16 bytes, gates 32) by unprotecting both pages, each
  restored to its own protection; it refused such gates, and their sites
  were made untraced. Test: "lazy mode rewrites in processes that are not
  dumpable" (130 sites).
- When enabling fails after the SIGSYS handler was installed (a thread
  blocks SIGSYS, the filter cannot be installed), the previous SIGSYS action
  is put back. Test: "lazy mode is refused while a thread blocks SIGSYS".
  `no_new_privs`, set before the filter, cannot be undone and stays.

Known limits of the aarch64 rewriting (documented, not fixed):
- A thread's first traced call, and a hooked call from a signal handler on
  an alternate stack while a hooked call of the same thread is live, still
  run on the current stack (about 1.4 KiB with the agent, 18 KB with a
  program): a sigaltstack near MINSIGSTKSZ can overflow.
- The gate's `stp x16, x17, [sp, #-16]!` needs a 16-byte aligned sp: hand
  written code issuing `svc` with a misaligned sp gets SIGBUS once rewritten.
- `mremap` moving rewritten code breaks the PC-relative `b` of its sites.
- `in_hook` (below) stays set if a signal handler `siglongjmp`s out of a hook
  that is running programs, or a vfork child sharing the thread's TLS is
  killed inside the hook: that thread's calls are then untraced. Jumping out
  of running programs is unsupported anyway (map locks may be held); a
  stack-pointer check instead would let handlers on an alternate stack
  re-enter the hook.
- Section-based scanning still matches data inside `SHF_EXECINSTR` sections
  (literal pools, constant tables some libraries keep in .text): a word
  equal to an `svc` encoding there (1 in 65536 random words) is rewritten.
- `run_callbacks` (syscall_trace_attach_impl.cpp) is `noexcept` and catches
  `...`: a forced unwind reaching it aborts ("exception not rethrown"). The
  LLVM JIT's frames have no unwind information, so with the default backend
  the unwind stops before it and the cancel ends the thread by longjmp, as
  documented above; an interpreter backend with full CFI would abort.
- `lazy_rewrite_site` reads the trapping word without checking it is
  readable: on Armv8.7 EPAN hardware an execute-only mapping would fault in
  the handler (Orin has no EPAN).

## System Call Hook Re-entrancy

`syscall_hooker_cxx` (both architectures) runs the hook at most once per
thread at a time: a thread-local, initial-exec flag (`in_hook`) sends any
system call the hook's own work makes through rewritten code straight to
the kernel, untraced, like the agent's other work. The system call itself
is made through `call_unguarded_syscall` (the "original" the agent gets
from `get_call_hook`), which clears the flag around it, so a signal handler
interrupting a blocking call is traced as usual. Without it a new thread's
first traced call crashed with programs attached: the agent's dynamic TLS is
allocated by ld.so with the main namespace's malloc, whose new arena reads
`/sys/devices/system/cpu/online` through the rewritten libc, which entered
the hook again before the TLS existed, until the stack overflowed (python3
with four threads under syscount). Keep the flag initial-exec: a dynamic TLS
access in the hook would allocate. Test: `system calls the hook itself makes
are not hooked again`. The guard costs 31 instructions per system call
(+5 ns rewritten, +7 ns with the agent and no program; dispatch without
programs 28 -> 35 ns over the untraced call). Keep `syscall_hooker_cxx`'s
common path a few instructions long: `mmap`, `mprotect` and `pkey_mprotect`
(the rescan) go to the out-of-line `hook_mapping_call`, and the real call is
`orig_syscall_direct` (hidden, no PLT). With the rescan inlined every call
saved ten registers and checked a stack canary (+43 instructions, +10 ns).

## x86-64 System Call Hook Registers

The zpoline hook (`syscall_hooker_asm`, x86-64) must leave every register as
the kernel would: only rax (the result), rcx and r11 change across `syscall`.
It saves rdi, rsi, rdx, r10, r8, r9 and restores them after the C entry, and
saves the vector state with XSAVE/XRSTOR (x87, SSE, AVX, opmask and AVX-512
components, mask 0xe7; area size from CPUID at `setup_syscall_tracer`, which
does not rewrite without OS-enabled XSAVE). It used to push the argument
registers only to pass them to C and return with whatever the C code left:
nginx under syscount crashed in `__libc_sigaction`, which reads its oact
pointer from r8 after the inline system call (the dispatch fast path also left
rdx changed with no program). The frame starts below the caller's 128-byte red
zone; only the 8 bytes the rewritten `call *%rax` stores at its top are lost.
`bpftime_xsave_area_size` is defined in the asm block: only the asm reads it,
and GCC's LTO removed a C++ definition with no C++ read (the link failed).
`setup_syscall_tracer` registers capstone's x86 architecture itself (frida-gum
opens only registered ones); callers such as the tests need not.
Tests: `test_syscall_rewrite_x86.cpp` with `regs_check_x86_64.S` (patterns in
the argument, callee-saved and xmm registers and the red zone, with and
without a hook; skipped without root and `vm.mmap_min_addr=0`).

## Agent Namespace And pthread Keys

The transformer loads the agent with `dlmopen(LM_ID_NEWLM)` so that the
agent's own libc is not rewritten. That libc numbers pthread keys from its
own table, but values live in the thread's shared `specific` array: the
agent's keys (GLib/Frida) and the process's got the same numbers, and a key
the process created later read the agent's pointer instead of NULL (nginx
under syscount aborted in OpenSSL's `ERR_clear_error`, "free(): invalid
pointer"). `load_agent_in_new_namespace` (`agent_namespace.cpp`) first takes
every free key below 32 in the process's libc and keeps them, loads libc
alone into the new namespace, takes every key there and releases the reserved
ones, then loads the agent into that namespace: the agent can use only the
reserved keys, the process only 32 and above. Keep the agent's range below 32:
those slots are stored in the thread descriptor, while higher keys use
second-level arrays that the thread's own libc frees at exit (an array the
agent's libc allocated would be freed by the process's). If the agent needs
more keys than were free, its `pthread_key_create` fails rather than sharing
a slot. Tests: `test_agent_namespace.cpp` (stand-in agent, keys created
before loading, threads using both).

Fork handlers have the same split: `pthread_atfork` in the agent registers
with the agent namespace's libc (`__register_atfork`), and the application
forks through the main libc, so none ran. Children kept the parent's cached
pid/tid (`bpf_get_current_pid_tgid`), wrote into the parent's perf producer
shard (torn records), used the parent's pid for probe read/write checks, and
kept stale cgroup, array binding and map usage state. Runtime code registers
through `bpftime::register_fork_handlers` (`runtime/src/fork_handlers.cpp`):
pthread_atfork as before plus a table, run by the agent's exported
`bpftime_agent_fork_hook`, which `install_agent_fork_hook` registers with the
main libc after loading (never twice: the namespace libc's copies do not run
on the application's fork). Use it instead of `pthread_atfork` in any code
the agent links, and only from a static initializer (as every caller does):
a child forked while another thread was between reserving and publishing an
entry inherits `published < reserved`, and a registration in that child would
wait for an entry nobody completes. Registering while the runtime loads
means a child never registers; the wait is also bounded (2^20 tries, then
pthread_atfork only: `published` never passes that entry, so it and every
later registration are missing from the table, and a dlmopen-ed agent's
fork hook would not run them). The array binding and map usage handlers used to
register at first use. Tests: "Fork handlers are registered before the first
array binding", "Fork handler registration returns over an unpublished
entry". Not covered: the agent namespace libc's own fork handling (its
malloc and stdio locks); a thread inside the agent's malloc at the fork
leaves the child's agent allocator locked. glibc 2.39 exports none of the
functions that would run it (`__run_prefork_handlers`, the malloc fork
locks are hidden; only `__libc_fork`, fork itself, is GLIBC_PRIVATE). Test:
"Fork handlers of an agent in its own namespace run in fork children"
(stand-in agent `test/fake_agent_fork.cpp`).

## System Call Dispatch

`syscall_trace_attach_impl::dispatch_syscall` runs for every system call of a
rewritten process, traced or not. It checks the per-syscall and global lists
first and issues a system call without programs directly. It used to assign
`curr_thread_override_return_callback` twice per call before checking: two
`std::function` heap allocations (the lambda captured three references, more
than the 16-byte local buffer) and dynamic TLS accesses in the dlmopen-ed
agent, about 200 ns (349 cycles) per untraced call on the Jetson, 221 cycles on
x86. With programs, the callback is set only around them and captures one
pointer, so it never allocates; keep it that small. Numbers outside the
512-entry lists (`syscall(-1)`, x32) run only global programs: indexing with
them read neighbouring members (-1 ran global exit programs at entry) or past
the object (600 crashed). Tests count `operator new` per dispatch.

Syscall tracepoints map names to numbers through `syscall_id_list.h`, made by
`generate_syscall_id_table.sh` from `<sys/syscall.h>`. asm-generic targets
(aarch64, riscv) define some numbers through another macro
(`__NR_fcntl __NR3264_fcntl`): the script keeps only lines ending in a digit,
so aarch64 lacked `fcntl`, `newfstatat`, `fstat`, `lseek`, `mmap`, `statfs`,
`fstatfs`, `truncate`, `ftruncate`, `sendfile` (their programs failed to attach,
logged, the rest attached) and had `fadvise64 __NR3264_fadvise64`. It now lets
cpp expand every `__NR_` name (x86 output unchanged, byte for byte), and the
header depends on the script so existing build directories regenerate it.
Test: "System call names map to their numbers" (needs no tracefs).

## Path Helpers

`bpftime_get_abs_path` (1003) and `bpftime_path_join` (1004) in
`runtime/extension/extension_helper.cpp` are in the ufunc group. From 2bcf61f3
(2023-12) to 2026-10 they sat under `#if BPFTIME_ENABLE_FS_HELPER`, which
nothing defined: a program calling one failed to JIT and was disabled
(benchmark/fuse's fs-cache never cached). Each call of 1003 makes a `getcwd`
system call (`std::filesystem::absolute`). Test: `[helpers]`.

## Per-CPU Hash Maps

`per_cpu_hash_map_impl` keeps its keys in a `bpftime_hash_map` (lookups
take no lock, writers lock stripes) whose four-byte value is the number of the
key's value slot; a slot holds every CPU's value, `value_size` bytes each in CPU
order. Nothing takes the handler's map lock (`should_lock = false`; the typed
helper entry points, the diagnostic direct paths and lookup-and-delete lock only
`if constexpr (should_lock)`). The slots are in chunks allocated as they are first
used (a fixed table of chunk offsets from the map object, the same in every
process) with a free list under one spinlock (`pool_lock`, its own cache line):
taken only to insert a new key or to delete one. `BPF_NOEXIST` looks the key up
(lock-free) before taking a slot: without that (a73f6753 to 11-05) every
`BPF_NOEXIST` on an existing key, the losing side of each `lookup_or_try_init`
race, took the pool lock twice and cleared `value_size * ncpu` bytes, and with
the spare slots held by in-flight inserts of a full map it returned E2BIG where
the kernel returns EEXIST (review 2026-11-05; tests in `[percpu-locks]`). A
key inserted between that lookup and the insert still costs a slot taken and
given back. A delete returns the slot and
frees nothing, so a stale value pointer writes into a slot (as with the kernel's
preallocated htab: possibly another key's value once the slot is reused). Keys
other than 4 bytes used to own a separately allocated value that a delete freed
at once, and the program's next write landed in memory the segment allocator had
handed out again. Capacity is `max_entries + ncpu` slots: an insert that loses a
race to insert the same key holds one for a moment (the kernel's percpu htab has
extra elements too); a nearly full map can still report E2BIG while many inserts
are in flight. The pool lock is one more lock a process killed inside it leaves
held (see the dead-holder note above), and a process killed between taking a
slot and inserting its key (it waits for a stripe lock meanwhile) loses that
slot: after more than `ncpu` such deaths inserts fail with E2BIG below
`max_entries` keys (the hash map's dead holders lose elements the same way). Memory: the key table is preallocated as
the hash map's (a 16-byte element and a 4-byte chain head per entry for 4-byte
keys, plus a 4-byte slot free-list entry), about 24 MB for a million entries
(29 MB on the open-addressing table); the old index grew with the keys. The
values still grow with use.

From 10-28 (cdc05f67) to 11-02 the map was a Boost `unordered_map` index (it
rehashes on insert) with shared slot and node free lists and key scratch buffers,
so every operation ran under the handler's map lock; before 10-28 it took none,
and two threads inserting new keys crashed or hung the traced process. The lock
made the per-CPU hash the only map whose lookups serialized threads: shared
counters at 6 threads paid 0.25-0.56 us per call (kernel ~0), and the suite's
lookup was 1.14x the kernel's.

- Lookup: `lookup_read` reads the slot number inside the table's lookup check
  (after the key compare, before the bucket word is read again), so a delete and
  another key's insert into the same bucket cannot hand out that key's values.
- Update of an existing key: `with_existing_value` writes under the key's stripe
  locks, so a concurrent delete cannot give the slot to another key meanwhile. A
  new key: take a slot, zero it, write this CPU's value (the kernel's
  `pcpu_init_value`), insert with `no_exist`; if another thread inserted the key
  meanwhile, give the slot back and update (BPF_NOEXIST: EEXIST).
- `bpf()` lookups copy the values under the stripe locks (always into the
  per-thread buffer); delete and lookup-and-delete take the slot number out with
  the table's delete and give the slot back afterwards.

The program path follows the kernel percpu htab like the syscall path:
`BPF_NOEXIST` on an existing key fails with `EEXIST`, `BPF_EXIST` on a missing key
with `ENOENT`, a new key in a full map with `E2BIG`. Tests: `[percpu-locks]`
(lookups and existing-key updates complete while the pool lock is held, existing-
key updates wait for the key's stripe lock, no operation waits for the handler's
map lock), `[value-access]` (the table operations), "Per-CPU hash values outlive a
delete without freeing memory", "per-cpu hash stays consistent under concurrent
inserts and deletes".

Per-CPU maps (hash and array) hold `bpftime_cpu_index_limit()` values per
key: the highest number in `/sys/devices/system/cpu/possible` plus one, the
bound of `sched_getcpu()`. It used to be `_SC_NPROCESSORS_ONLN` at creation, and nothing
checked `sched_getcpu()` against it: a CPU numbered at or above the online
count (a core a power mode had offlined, back online) read and wrote past the
values, and a failed `sched_getcpu()` (-1) before them. Every path indexing
by the current CPU now checks it (lookup: ENOENT, update/delete: EINVAL), like
the JIT binding `lookup_bound` already did. Test: `[cpu-bounds]` (maps made for
one CPU, the thread pinned to CPU 1).

Through `bpf()` (the syscall server, `elem_*_userspace`, `lookup_and_delete`)
per-CPU values use libbpf's and the kernel's layout: one slot per possible CPU,
in CPU order, each value rounded up to 8 bytes (`get_userspace_value_size`,
`percpu_values_to_userspace`/`from_userspace` in `map_common_def.hpp`). The
number of slots is `bpftime_num_possible_cpus()`, the CPUs in the mask, as
`libbpf_num_possible_cpus` counts them; with holes ("0,2-3") that is less than
the index limit and slot i holds CPU `bpftime_possible_cpu_ids()[i]`. Counting
highest + 1 there (as 6f1e73d9 did) copied 4 slots into libbpf's 3-slot
buffer, a heap overflow in the tool. The maps keep their values packed at
`value_size` and indexed by CPU number (the JIT paths depend on it) and
convert; a per-CPU array lookup returns a per-thread buffer unless the storage
already is the syscall layout (`value_size` a multiple of 8, no holes), a per-CPU
hash lookup always copies (under the key's stripe locks). They used to copy `value_size` times the online CPUs packed: a
tool reading 4-byte per-CPU counters took CPU 1's value from CPU 0's padding.
`KERNEL_USER_PERCPU_ARRAY` already used the kernel layout. Tests:
`[percpu-layout]` (with an injected "0,2-3" mask), "Possible CPUs are counted
as libbpf does" (the parser on masks with holes).

`BPF_MAP_LOOKUP_AND_DELETE_ELEM` (`bpftime_map_lookup_and_delete_elem`) used
to ignore the key and pop: hash maps returned ENOTSUP, so libbpf's
`bpf_map_lookup_and_delete_elem` failed on them. It now pops queues and
stacks and removes the key's element of hash, per-CPU hash and LRU hash maps
in one step (the striped hash and the per-CPU hash take the value out under
the stripe lock, LRU hash under the handler lock); other types return ENOTSUPP (524) like the
kernel. A null key (hashes) or value is EFAULT and removes nothing (a null
key crashed the hash paths, a null value deleted the key without returning
it), and any flag is EINVAL in the syscall server (the kernel takes only
BPF_F_LOCK, for values with a spin lock, which bpftime's maps lack).
Test: "bpf() lookup and delete takes the key of hash maps".

The per-CPU hash's `bpf()` update compares the low 32 bits of the flags with
`BPF_NOEXIST`/`BPF_EXIST`, as its program path does (bpftime's own
operations use the high bits); it compared the whole value, so a flag with
a high bit set skipped the checks there. Test: "Per-CPU hash update flags
mean the same through bpf()".

Map creation (`bpf_map_handler::map_init`) checks sizes as Linux 6.8's
`htab_map_alloc_check`, `array_map_alloc_check` and
`queue_stack_map_alloc_check` do (checked against v6.8 sources; bf1af650 had
an older kernel's 512-byte key limit and value limits): hashes (plain,
per-CPU, LRU) EINVAL for 0 entries or a 0-byte key or value, E2BIG for key +
value >= 4 MiB - sizeof(htab_elem) (48); arrays and per-CPU arrays EINVAL
for 0 entries, a key other than 4 bytes or a 0-byte value, E2BIG for a value
over INT_MAX; queues and stacks EINVAL for 0 entries, a key or a 0-byte
value, E2BIG for a value over 4 MiB; a per-CPU value over 32 KiB rounded to
8 is ENOMEM (the kernel's percpu allocation fails); a size that overflows
is E2BIG. A lower bound of the size (keys once, values per CPU for per-CPU
maps; the key times the CPUs refused per-CPU hashes with large keys) above
the segment, and an allocation that throws, are ENOMEM (the syscall
server's exhaustion contract, "Syscall server reports map allocation
failure instead of using the kernel"): Boost's container length_error is
not a std::length_error, and escaped map_init leaving a handler without a
map (a lookup on it crashed the tool); other constructor refusals are
EINVAL. The handler is removed whatever map_init throws, creation returns
-1 with errno (EINVAL when an implementation set none: unsupported types
returned errno 0, which libbpf takes as fd 0), and a descriptor the shm
opened for the map is closed (`set_handler` used to ignore `map_init`'s
result and keep an empty handler). Other types (ringbuf, stack trace,
perf event array, map-in-map, LPM trie, bloom filter...) keep their own
checks, which may accept what the kernel refuses. Before, a
size that wrapped allocated a small buffer that accesses overran (a hash of
4294922064 entries of 2 GiB values got 97 MiB and its first lookup SIGSEGV;
the per-CPU array computed `max_entries * ncpu * value_size` in 32 bits),
and a 0-byte hash key matched every key. Tests: `[map-create]`.

## Cgroup Array And bpf_current_task_under_cgroup

`BPF_MAP_TYPE_CGROUP_ARRAY` (`cgroup_array_map_impl`) stores, per slot, the
cgroup v2 path of the directory fd the loader wrote (resolved at update via
`/proc/self/fd` and the cgroup2 entry of `/proc/self/mountinfo`), because the
fd means nothing in the traced process. Kernel rules: `BPF_ANY` only (`EINVAL`),
`E2BIG` out of range, `EBADF` for anything but a cgroup v2 directory, `ENOENT`
deleting an empty slot, `EOPNOTSUPP` for lookups. Helper 37 returns 1 when the
calling thread's cgroup (`/proc/thread-self/cgroup`, the `0::` line) is the
slot's path or below it on a component boundary, else 0; `-E2BIG`/`-EAGAIN` as
the kernel. The slot path is copied under the map spinlock and compared outside
it; never do file I/O under the map lock. Each thread keeps its proc fd and
re-reads it with `pread` at offset 0 (fresh content, one system call: 327k
filtered calls/s vs 85k with open/read/close; kernel 469k); `/proc/thread-self`
is bound at open, so the atfork child handler bumps a generation and inherited
fds are reopened. Loader and traced process must share the cgroup namespace;
cgroup v1 is not supported. `bpf_get_current_cgroup_id` (helper 80) returns
the inode number of the calling thread's cgroup v2 directory (the kernel's
cgroup id on 64-bit), from the same per-thread proc read plus the process's
cgroup2 mount (parsed once) and a `stat`; 0 when unavailable.

## bpf_trace_printk Output

`bpf_trace_printk` never writes the host's stdio by default (it used to
`vprintf` into the host's stdout). Default: a bpftime log record
"bpf_trace_printk: <msg>" through the `BPFTIME_LOG_OUTPUT` sink (a rotating,
size-bounded file unless "console"). `BPFTIME_TRACE_PRINTK_OUTPUT` selects
`stdout`, `stderr`, `none` or a file (appended); those are written with one
`write` per message from a 512-byte stack buffer, so nothing interleaves with
or is lost from the host's stdio buffers. The sink is resolved once per
process. The helper returns the formatted length (kernel), not 0. Cost per
printk (malloc example, Jetson): default 453k calls/s, `stdout` 650k, `none`
3.0M, kernel 534k; the old buffered `vprintf` did ~2M. A kernel-like shared
trace ring (bounded, no system call, read by a tool) would be the fast default.

## Shared Memory Size And bpf() Failures

Boost 1.83 commits the whole segment at creation (`posix_fallocate`), so
`BPFTIME_SHM_MEMORY_MB` is resident memory, not a sparse limit. The default is
128 MB (was 50): memleak's 1M-entry hash map (8-byte keys, 24-byte values)
needs about 44 MB (40-byte elements plus 4-byte heads; 80 MB on the
open-addressing table with twice the buckets). When a `bpf()` or `perf_event_open()` that bpftime handles fails
in userspace (Boost `bad_alloc`, Boost container `length_error`: "allocator's
max size reached"), the interposer returns -1 with `ENOMEM` (`EINVAL` for other
exceptions) and logs a hint to raise the size. It must never fall back to the
kernel there: the kernel then created a real map the runtime did not know, the
tool loaded without error, and its programs could not resolve the map ("Expected
fd 5 to be a map fd") and traced nothing.

## Probe Reads

With `ENABLE_PROBE_READ_CHECK` (the CMake default; the official `make
benchmark` turns it off), `bpf_probe_read`, `_user`, `_kernel` and the string
helpers read through `process_vm_readv` on the process itself: a bad pointer
returns -EFAULT, as in bpf. Without it they are a plain memcpy (strings a byte
loop), and a bad pointer kills the traced process with SIGSEGV: NULL, an
unmapped page and a kernel address, 18 of 18 runs, where bpf returns -EFAULT
(`uprobe/more-dims-20261106/n1-probe-read`). Upstream replaced the SIGSEGV
handler that used to catch such faults (#642, "avoid process-wide signal
interception"); a fault-catching copy would have to interpose the
application's `sigaction` to keep the tests "Test probe access preserves
application SIGSEGV handlers" and "Test default SIGSEGV handler is preserved",
and runtimes that install their own handler with raw system calls (Go) would
still take its faults as theirs.

Strings are read straight into the program's buffer, one system call per
chunk: the first chunk at most 256 bytes (most strings end there), later ones
to the end of the source page; a bad destination fails the same call. The
bytes read past the NUL are the source's memory and are zeroed (bpf zeroes
the rest of the word it copied the NUL in; here up to the chunk's end, within
the buffer the program passed). Through a 256-byte scratch buffer and
`process_vm_writev`, every 256 bytes cost two system calls: a 255-byte string
2.1 us over the probe, bpf 0.12 us; nginx under sslsniff lost 7.5% of its
requests per second to the checked reads. A fault zeroes the whole
destination, as in bpf (`bpftime_probe_read` left a partial copy). A seccomp
filter that denies `process_vm_readv` (EPERM, ENOSYS) makes every probe read
fail with -EFAULT, logged rate-limited; the application goes on. Test
"Checked probe reads zero what is not data" (`[probe-read]`; mutations: no
zeroing after the NUL, none on a fault).

## Rate-Limited Runtime Errors

Errors on paths that run per event (helpers, map access from programs, probe
dispatch, `bpftime_prog_exec`, stack generation, unsupported-map dispatch) use
`BPFTIME_LOG_ERROR_RATELIMITED` / `BPFTIME_LOG_WARN_RATELIMITED`
(`attach/base_attach_impl/bpftime_ratelimited_log.hpp`): each call site logs
its 1st, 10th, 100th, ... occurrence with the count, one relaxed atomic
increment per error. Use them for any new per-event diagnostic; load-time and
control-plane errors keep plain `SPDLOG_*`. The format must be a literal.
The logging is a cold, non-inlined template taking the arguments by value
(and file/line as plain values); never inline it or capture locals by
reference: the inlined version made `bpf_probe_read` take a stack-protector
canary on every call (+31% probe read, +27% probe write in the full ten).

## Agent Start-up

An agent started with the process (`LD_PRELOAD`, or the transformer) looks for
the shared segment once: the tool runs before it. Only an injected agent
retries, 60 times 50 ms, for "spawn loader then inject agent" (fac864e5);
`BPFTIME_AGENT_SHM_WAIT_MS` sets the wait for either. With the retries on
every path, anything that kept `LD_PRELOAD` after the tool exited (a shell
that exported it, children of a traced service, a container's environment)
started every process 3 s late (`uprobe/more-dims-20261106`, n2-startup).
Test "Agent preload without shared memory does not delay the process"
(3,021 ms with the retries).

Each Frida probe resolved its module at every process start: the executable
path (`readlink` and a flushed info line), `gum_module_load` and Gum's module
enumeration, and a scan of `/proc/self/maps` stat-ing its files: 0.42 ms per
probe, 0.44 s for 1,000 (`uprobe/more-dims-20261106`, n4-many-probes). The
executable path is read once per process, a module's base is kept while
`dladdr` still finds a module loaded there (a library unloaded and loaded
elsewhere is looked up again), and a module found in the maps once is not
looked for again. Test "Module offset resolution keeps each module's base"
(two modules in turn; one cache slot for both fails it).

Where a traced process's start-up still goes (`uprobe/improve-20261106`,
investigate/startup, about 23 ms per start with one program): the program's
JIT compile in every process (`bpftime_prog_load`, a fresh LLJIT at O3) 44%,
the agent's static constructors (mostly LLVM's) 8.5%, Gum's first module
lookup 7% (half of it `memmem` in `gum_read_auxv_from_stack`, once per
process, for libc's name), Gum's thread context 3%, attaching 1.6%; most of the
rest is the kernel mapping and relocating the 114 MB agent (202,114 relative
relocations). AOT code kept in the segment would remove the compile. The agent
alone, with no tool: 8.8 ms. 3f8981e4 starts 0.9 ms later than d46ceb8f,
spread evenly over every part (not attributed).

## Agent Work Is Invisible To Probes

The agent runs inside the traced process; its own calls must not reach the
probes (memleak reported ~250 agent allocations and two 8 MB thread stacks).
`agent_internal_section` (RAII `ignore_probes_on_this_thread`, frida attach
impl) covers `init_attach_ctx_from_handlers`, thread creation and `setenv`,
and every agent background thread (IPC, auto-refresh, detach) ignores probes
for its lifetime. Wrap any new agent-side work that allocates or calls hooked
functions on the host's threads. Allocations made before the agent takes over
(process start-up before `__libc_start_main`) stay invisible, unlike kernel
uprobes armed at exec. Gum's ignore skips listeners only: replacements
(override, ureplace) still run on an ignored thread.

A thread's Gum interceptor context (~100 KB, mostly the invocation stack) is
made on first use from Gum's dlmalloc heap, which grows with `mmap` while
holding its spin lock. Inside an invocation Gum sets its guard first, so a
probed `mmap` there goes straight through; `gum_interceptor_ignore_current_thread`,
`attach`, `detach` and `end_transaction` make the context without the guard.
With `mmap` probed (memleak), the probe then makes a context again and spins
on the lock its own thread holds: memleak hung every threaded run (agent
threads ignoring probes at start, 4d9b8a61). `ensure_gum_thread_context` makes
the context through one call of a function of our own that Gum hooks (attached
before the first probe, never detached); `ignore_probes_on_this_thread` and the
hook slot's Gum attach and replace call it first. It is Gum's hazard (any
thread's first unguarded context while `mmap` is probed); bpftime only avoids
it, Gum is not patched. Gum still frees a context at thread exit outside the
guard (if that free unmaps with `munmap` probed, it may recurse the same way;
not seen, not handled). Test:
"Fresh threads can ignore probes while mmap is probed" (32 threads, child
process with a 20 s bound; hangs without the anchor call).

## Detaching Uprobes While The Process Runs

The Frida attach impl never asks Gum to detach or revert while the process
runs. Each of the three steps of a Gum detach crashed a thread calling the
function at the time (`uprobe/detach-crash-20261104`; any SIGUSR1 detach or
tool exit under load, official too):

- Gum restores the first instructions over its 16-byte redirect
  (`ldr x16, [pc+8]; br x16; .quad on_enter`, a `gum_memcpy`) while other
  threads may execute them: a thread that fetched the old `ldr` loads the
  restored instructions as its jump target (SIGBUS). Unchanged in current Gum.
- Gum 16.1.2 frees a detached listener's entry while other threads iterate the
  entry array (`gum_function_context_remove_listener`; deferred since frida-gum
  17.24.0).
- bpftime freed the attach (its callback) and the agent the program while
  callbacks still ran them.

Instead each hooked function has a `frida_hook_slot` (`frida_hook_slot.hpp`)
for the life of the process: Gum's enter listener (and a leave listener once a
uretprobe came, the replacement once an override came) stays attached, and
bpftime swaps the slot's `dispatch`. Republishing waits for a grace period
(`frida_probe_grace.hpp`: per-thread counters odd inside a callback,
`membarrier(MEMBARRIER_CMD_PRIVATE_EXPEDITED)`, wait for odd counters to move),
so `detach_by_id` returns only when no callback can still run the attach; the
agent frees programs after it. With nothing attached the slot is parked: the
redirect's literal (one aligned 8-byte store, or the low word when the literal
is unaligned and both targets share the high word) or its single `b` is
pointed at Gum's on-invoke code (the displaced first instructions, then back
into the function), so calls bypass Gum; both targets are valid at any moment.
The unaligned case relies on the core: the thread's `ldr x16` of an unaligned
literal is an 8-byte load that the architecture makes single-copy atomic only
per byte before FEAT_LSE2 (Armv8.4; the Orin is Armv8.2), so a torn low word
is allowed, though not seen (4,200 park/unpark cycles under four callers;
review 2026-11-05). `parked` follows Gum's store, also when restoring the
page's protection fails afterwards (it did not then, and the next republish
skipped the patch), and the first slot holds an interceptor reference for the
life of the process (Gum disposing it would restore the functions under the
slots).
The on-invoke address comes from `gum_interceptor_replace`'s
`original_function`, for a replacement installed and reverted inside the
transaction that first hooks the function (Gum writes the redirect when that
transaction ends, so the replacement is unreachable). The code page is patched
with `gum_memory_patch_code` (RWX pages only).

Costs: a function once hooked keeps Gum's context; parked it costs the
redirect and the displaced instructions (the test function: 157M calls/s
parked against 231M native on one thread, 1,317M against 1,380M on six; the
Gum path with only an empty listener was 5.2M and 1.1M). Gum's 8-byte form
(`adrp; br`), deflectors and x86 are not parked: they keep Gum's path after a
detach. Unparked with a past uretprobe, every call traps its return even in a
uprobe-only session; a past override keeps the replacement, which calls the
original through `__bpftime_frida_attach_manager__override_handler`'s
forwarder: five integer arguments, no floating-point or struct arguments or
returns. Before, only an active override routed calls through it; now a later
uprobe or uretprobe session on a once-overridden function does too (parked, it
is bypassed). Not tested; a register-preserving forwarder or a checked revert
would remove it. Callbacks pay one TLS access and two stores for the grace period.
The counter's flip is decided on its parity with idempotent stores, so a
signal handler's callback between the depth change and the store runs inside
the grace period (deciding on the depth let it run outside; review
2026-11-05). Known limits of the grace period, not fixed: `synchronize()`
waits without bound on a counter that cannot move (a thread that left a
callback by `siglongjmp`; a raw `clone(CLONE_VM)` child without `CLONE_SETTLS`
killed inside a callback leaves its parent's counter odd), and a reader whose
signal handler forks while `synchronize()` holds `registry_mutex` waiting for
it deadlocks in the fork prepare handler; a callback blocked indefinitely
blocks the detach. `membarrier_registered` is fixed at load: if
`MEMBARRIER_CMD_PRIVATE_EXPEDITED` starts failing later (a seccomp filter
installed after the agent), the writer falls back to a fence while readers
keep only a compiler fence, and the grace period is unsound. And the agent
frees all programs after `destroy_all_attach_links` even when a link failed
to detach (it stops at the first failure; the Frida impl always returns 0,
the syscall impl only fails for unknown ids, so it is not reached today).
The replacement runs the dispatch's `override_attach`, never the owner's
`user_attaches`, which `detach_by_id` shrinks before republishing: a call in
between found no override and threw out of Gum's replacement, whose caller has
no unwind information (SIGABRT, 3/3; review 2026-11-05). What the program sets
(`bpftime_set_retval`) is that call's own state, captured by the thread's
override callback, which is restored afterwards: it was a field of the shared
entry, so a thread could return another's value (68,951 of 4.06M calls with four
threads when the program went on after setting it). Nothing escapes the
replacement: an exception logs and calls the original.
Slots are keyed by the address Gum instruments: `gum_interceptor_resolve`
follows a leading jump (a `b` thunk, a PLT-style stub) unless the address is
hooked already, so a probe on `foo: b bar` is a listener on `bar`'s context.
With a slot per address, detaching `bar`'s probe parked `bar`'s slot and
`foo`'s probe, still attached, never fired again (review 2026-11-05). The slot
lookup now follows the jumps the same way
(`gum_arm64_reader_try_get_relative_jump_target` and its x86 twin, in
libfrida-gum but not in the devkit header, at most 8 hops), so both are owners of one slot and its dispatch holds both
attaches; calling either runs both, as Gum did. Test: "A probe on a thunk
keeps firing after its target's probe is detached" (fails without the
lookup).
The attach-time window remains: Gum writes the redirect over the first
instructions while other threads may run them (bpftime attaches at start).
`frida_attach_impl` is not thread-safe: owners of one slot in two impls must
not change their attaches concurrently. Tests: "Detaching probes while threads
call the function keeps them running" (SIGBUS 3/3 before; with a never-detached
keeper listener but Gum detaches, SIGSEGV 5/5), "Detaching an override while
threads call the function keeps them running" (SIGABRT with the live-vector
lookup), "Concurrent override calls each return their own value" (fails with
the entry's fields), "No probe callback runs after
its detach returned" (SIGSEGV 3/3 with `synchronize` returning at once),
"Probes attached again after a detach run" (fails without the unpark), and the
hidden "Hook cost attached and parked".

## Daemon Tracer Programs

`bpftime::select_tracer_programs` (`daemon/user/bpf_tracer.cpp`) decides
which programs of the daemon's kernel tracer load: the open and auto-attach
programs per the options, and never a `tracepoint/syscalls/*` program whose
tracepoint the running kernel lacks. Architectures with the generic syscall
table (arm64, riscv64) have no `open` or `dup2`; attaching those failed the
whole tracer (`bpftime_daemon -o` on arm64). Presence is read from tracefs
(`/sys/kernel/tracing` or `/sys/kernel/debug/tracing`); without tracefs the
program stays selected. Add new syscall tracepoints to the tracer freely; the
selection drops the absent ones. `[daemon-tracer]` tests load the real tracer
and need root (they skip otherwise).

The tracer's pid/fd table (`bpf_fd_map`) follows descriptor duplication:
`dup`, `dup2`, `dup3` and `fcntl(F_DUPFD[_CLOEXEC])` copy the old fd's entry to
the new fd, or clear the new fd's entry when the old one is not a bpf fd.
libbpf 1.3+ creates each map at a scratch fd, `dup2`s it onto a reserved fd
(arm64: the `dup3` system call) and closes the scratch fd, so programs refer
to the reserved fd; without this the daemon could not relocate any program
built with a current libbpf ("Failed to lookup bpf fd map", the tool's probes
ran nowhere). Perf fds are not copied (closing one copy would detach the
shared event). Closing a non-perf bpf fd still leaves its entry, because events
are resolved asynchronously; a reused number therefore keeps the stale entry
until it is overwritten.

Uprobe redirection writes into the loader's memory: on entry to
`perf_event_open` the tracer replaces the uprobe path with `new_uprobe_path`
(`/a`) and the offset with 0, on exit it restores both. Restore exactly the
original string's bytes (`path_len`, NUL included): writing back the whole
255-byte name buffer ran past short heap strings (DeepFlow's per-probe path
strings: "free(): invalid pointer" in the tracer). Never write more of the
loader's memory than was read from it.

Mirrored kernel maps and programs get handler indexes from
`find_minimal_unused_id`, recorded per kernel id in the driver
(`map_handles`, `prog_handles`, each object mirrored once); program
instructions are relocated to map handles, so a program's maps are mirrored
before it. Kernel ids must never index the handler table: they are global,
grow without bound (past the 6144-entry table after a few DeepFlow runs:
"set_handler: fd 6957 out of range", and agents then ran nothing), and maps
and programs have separate id spaces.

## Kernel-User Proxy Maps

In daemon mode, maps that userspace programs share with kernel programs are
proxies (`runtime/src/bpf_map/shared/*_kernel_user*`, type `1000 + kernel
type`) whose operations are syscalls on the kernel map. The proxy objects live
in shared memory and serve every attached process, so descriptors and value
buffers must not be stored in them: `kernel_map_of_this_process` opens each
kernel map once per process (cached per thread), and lookups return a copy in
`kernel_map_thread_buffer`, valid until that thread's next lookup of the map.
(The percpu proxy never opened its fd; the hash proxy kept one value buffer in
shared memory that concurrent lookups overwrote: 105k of 160k reads torn with
8 threads, as on Go programs whose uprobes run on many threads.) The percpu
array proxy gives a program the calling CPU's value and its update rewrites
that CPU's value only (read all, replace, write back: not atomic against other
writers); syscall access uses the kernel layout, each possible CPU's value
rounded up to 8 bytes. The daemon proxies a kernel `LRU_HASH` as a hash map
(the kernel evicts). The array proxy still keeps its fd and mmap in shared
memory. `[kernel_user]` tests need root.

## Map Usage Counters

`BPFTIME_ENABLE_MAP_USAGE_STATS` (CMake, off by default) counts map
operations per map for surveying how real tools use maps; with it off the
hooks compile to nothing. With it on, counting also needs
`BPFTIME_MAP_USAGE_STATS_FILE`: each process appends one JSON line per map at
exit (lookups hit/miss, inserts, updates of existing keys, failed updates,
deletes hit/miss, distinct threads, first/last time), separately for programs
and for the loader's `bpf()` calls. The hooks sit in the `bpf_map_handler`
entry points (generic, typed and array); the generic implementations are
renamed `*_uncounted` in such builds and wrapped. Updates of hash-like maps
look the key up first to tell inserts from updates (diagnostic cost). The
array lookup JIT is disabled in counting builds (`bind_array_maps`), since
inlined accesses bypass the handlers. Metadata is copied at the first
operation, because the exit dump may run after the shared segment is
unmapped; a fork child drops the parent's counts. Do not measure performance
with a counting build.
