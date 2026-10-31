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
`bpftime_perf_wakeup_state` with a 1 ms timeout: register in `waiters`,
fence, re-check, then `FUTEX_WAIT`. A producer asks for a wakeup only when
its shard was at least half full right after publishing (measured from the
tail read before publishing) or a sample was dropped, like the kernel's
`wakeup_watermark`; smaller output is delivered by the timeout. Waking on
every sample costs a futex syscall per event (measured -8% req/s on
ssl-nginx with 16 B responses). The producer fences and issues `FUTEX_WAKE`
only when `waiters` is non-zero; keep both fences, or a wakeup can be missed
until the timeout. SIGINT/SIGTERM stay
blocked while sleeping and are collected with a zero-timeout `sigtimedwait`
afterwards. Ring buffers do not notify and are found by the timeout.
Segments without the wakeup object (older servers) fall back to polling.

A producer finds its shard by (pid, tid) on every sample. Both are cached per
thread (getpid and gettid are system calls); the atfork child handler clears
the cache, and the shard match still compares pid and tid. Keep both, or a
fork child would write into its parent's shard.

Idle shards of dead threads are reclaimed by probing (pid, tid) with tgkill.
Each shard records its producer's pid namespace (inode of `/proc/self/ns/pid`);
a reclaimer probes only shards of its own namespace. A producer in a container
sharing `/dev/shm` with the tool used to lose its live, idle shard (tgkill from
the host: ESRCH) and the lost-sample count in it. Test: "Software perf event
shards of another pid namespace are not reclaimed".

The shard search compares the namespace too (cached per process, cleared in
fork children: a child forked after `unshare(CLONE_NEWPID)` is in another
one). Matching (pid, tid) alone put two containers' pid 1 into one
single-producer ring: torn samples delivered as valid, garbage LOST counts,
shards dropped as invalid. The per-thread shard cache needs no namespace: an
entry is made by its thread for its own namespace, and a reused shard has a
new generation. Tests: "Software perf event shards are matched within a pid
namespace", "Producers in two pid namespaces with the same pid write apart"
(root: two `unshare` children's pid 1 writing at once).

Shards of other pid namespaces are never reclaimed (above), so shared memory
can run out. A shard that cannot be allocated makes the output fail with
ENOSPC (no shard, so no lost count); Boost's `bad_alloc` used to escape the
helper into the traced process. Safely reclaiming a live foreign producer's
idle shard would need an atomic flag on every output. Test: "Software perf
event output fails cleanly when shared memory is full".

Each drain starts one shard further (`drain_start`). When the consumer ring
fills up, the shards drained last keep their samples and then drop new ones
(ENOSPC, LOST); a fixed order starved the same threads every time. Test:
"Software perf event drains start at a different shard each time". Not
handled: a tool that calls `perf_buffer__consume()` without epoll never
triggers a drain, and mapping the consumer buffer a second time with a
larger size reallocates it under libbpf (tools map it once).

A ringbuf reservation records its owner (pid, tid, pid namespace) in a
256-slot table in the ring (`reservation_owner`); the record header's second
word (the kernel's pg_off, unread by libbpf) holds the slot, -1 when the
table is full. Submit and discard free the slot. The tool's epoll wait calls
`recover_dead_reservation()` before `has_data()`: if the next record is still
busy and its owner is a thread of the tool's pid namespace that tgkill finds
gone, the record is marked discarded and the ones behind it are delivered.
A producer killed between `bpf_ringbuf_reserve` and submit used to hold
back every later record forever, with no error (the kernel cannot have
this). Still wedged: an owner in another pid namespace, a full table, and a
tool that calls `ring_buffer__consume()` without polling (libbpf reads the
ring itself). The owner identity comes from `producer_identity.cpp`, shared
with perf output. Test: "Ringbuf skips the record of a producer that died
before submitting".

Not handled (documented): a process killed while holding `shard_lock`, the
ringbuf's `reserve_spin_lock` or a map's `map_lock` (per-CPU hash: every
operation) leaves every later user spinning, like the hash map stripes. A
consumer killed between delivering a record and releasing the shard tail
redelivers it on the next drain, and one killed while waiting leaves
`waiters` above zero (every watermark output then pays a futex wake).
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
The existence check runs only for `BPF_NOEXIST`/`BPF_EXIST`, under the
implementation's writer lock; `BPF_ANY` stays a single probe. Lookup-then-update counters
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

Lookups take no lock and write nothing shared. Filled buckets never move,
and each bucket's 4-byte word holds the state (low two bits) and a
generation that every delete increments. Inserts write key and value after a
release fence and publish the word with a release store; a lookup reads the
word (acquire), compares the key, fences (acquire) and accepts the match only
if the word is unchanged, otherwise it starts again. Without the re-check a
lookup matched keys half overwritten by the next insert into the bucket
(18k-291k false matches in 1.5 s). The bucket stride is rounded to 4 bytes
so the word is aligned.

Writers lock stripes of buckets (a power of two, at least 64, at most 4096
stripes). An operation locks every stripe from its key's home bucket
through the last bucket it may write or rely on (the probe bound, plus one
for a delete's empty-successor check), in increasing stripe order with the
wrapped part from stripe 0 first; when it finds it needs more it unlocks
everything and starts again with the larger range. Never take a stripe lock
while holding a higher one. Each stripe keeps the probe bound of the keys
whose home bucket is in it (stored only when it changes; lookups read it).
A delete turns tombstones back into empty buckets only inside its locked
range. The element count is `max_entries` tokens: each stripe holds some
under its lock, the rest are in a shared atomic pool. An insert takes one
from its home stripe (refilling 32 from the pool when it has none), a delete
returns it there (giving 32 back at 64); only when the pool is empty too
does an insert lock every stripe and take any stripe's token, so the limit
is exact (E2BIG only when all `max_entries` are used). `get_elem_count` is
exact only without concurrent writers.

A stripe's whole state (lock, probe bound, tokens, displacement counts up to
distance 10) is one 64-byte line, and every operation resolves the two
shared-memory vectors to plain pointers once (`view`). The single-thread
cost of striping came from extra lines and instructions, not from atomics:
per-CPU token slots, a separate bound array and a 320-byte stripe state
added cache/TLB misses with keys spread over many stripes (memleak's
addresses), and offset_ptr resolution per access added ~300 instructions to
an insert + lookup + delete. Keep the stripe state one line and the hot path
free of offset_ptr arithmetic.

Values are still updated in place (readers may see a partly written value,
as with the kernel without `BPF_F_LOCK`), and a value pointer is only valid
while its key stays, as in the kernel's preallocated htab. A process killed
inside a writer leaves its stripes locked (writers to them hang, lookups do
not). Tests: `test_hash_map_lockfree_lookup.cpp` (torn keys; values read
while no delete runs), `test_hash_map_striped_writers.cpp` (threads against a reference
with clustered keys, exact capacity, probe runs wrapping past the last
stripe, processes mapping the segment at different addresses).

Element layout: state word at 0, key at 4, value at `round_up(4 + key_size,
A)`, element size a multiple of A, where A is 8 for values of 8 bytes or
more and 4 otherwise. The value used to follow the key directly in elements
rounded to 4: with an 8-byte key and value a quarter of the values crossed a
16-byte boundary, and a BPF atomic add on one (the JIT emits `ldaddal`) died
with SIGBUS on aarch64 (x86 tolerates it). u32 keys with 4- or 8-byte values
keep their size; u64 key and value grew from 20 to 24 bytes. Arrays, per-CPU
arrays and per-CPU hash values keep a `value_size` stride: a naturally aligned
C struct with an 8-byte member has a size that is a multiple of 8, so only
packed structs could misalign there. Test: `[alignment]` (key sizes 1-64,
value sizes 1-8, 12, 16, 20, 24, an atomic add on every value of 4 bytes or
more, every value written to its last byte without reaching a neighbour).

A stripe without tokens, once the pool is empty, takes a batch (up to
`TOKEN_BATCH`) from other stripes while it holds every stripe, instead of one
token per insert (which locked every stripe on each insert near capacity).
Test: `[tokens]`.

Known, not changed: `hash_func` (4 bytes: 29791, 961, 31, 1 per byte; others
byte-wise times 31) maps keys with fixed low bytes (packed fields, `id << 16`)
to few adjacent homes: 10,000 such u32 keys in 20,000 entries gave a probe
bound of 9,945 (an independent review's stress run: 170,707), and every miss
homing there probes the whole run. MurmurHash3's 32-bit finalizer brought it
under 64 but made 6-thread churn 1.2-1.4% slower in two runs; its 64-bit
finalizer made a 900K-entry table of u64 address keys 3.1% slower (they rely
on `hash_func`'s locality). Neither was committed (results
`reviews-20261031/fixes/hash-ab`, `hash-ab2`).

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

## aarch64 System Call Rewriting

`attach/text_segment_transformer` intercepts system calls by rewriting code
(the `bpftime start -s` / transformer path). x86 uses zpoline (`syscall` ->
`call *%rax` into a page at address 0); aarch64 cannot (instructions must be
4-byte aligned and `bl` overwrites x30), so each `svc #imm` becomes `b gate`
to its own 32-byte gate in a trampoline region mapped within the +-128 MiB
reach of `b` (`MAP_FIXED_NOREPLACE`, closest free gap), after svc-hook
(Middleware 2025) and ASC-Hook. The gate saves only x16/x17 on the stack and
passes its own address (`adr`); `syscall_hooker_asm` saves x0-x30, NZCV, FPSR,
FPCR and q0-q31 (a 784-byte frame below the gate's 16 bytes), calls
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
Sites are found per executable mapping by its ELF file's `SHF_EXECINSTR`
sections (no constants of a shared segment taken for code). Mappings without
section headers (anonymous code, unreadable files) are skipped and counted in
the log unless `BPFTIME_SYSCALL_REWRITE_ANON=1`: scanned word by word, a
literal-pool constant equal to `svc #0` was rewritten into a branch.
The section table comes from the mapped file itself: a file renamed over or
unlinked under a running process (a package upgrade, then `bpftime attach`)
shows as "path (deleted)" and is opened through `/proc/self/map_files`
(needs `CAP_CHECKPOINT_RESTORE`, so root; otherwise skipped with a warning
naming the mapping); any other path must still have the mapped inode. Before,
"(deleted)" mappings were always skipped (untraced). `MAP_SHARED` executable
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
for SIGSYS itself, keeping the application's action without installing it:
a trap with SIGSYS blocked or ignored kills the process. Costs: about
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
  masks (`sanitize_signal_call`), counting refusals for the next hooked call
  to log.
- `patch_lock` yields every 64 spins (a SCHED_FIFO spinner on the holder's
  CPU).

Known limits of the aarch64 rewriting (documented, not fixed):
- A hooked call pushes 16 + 784 bytes before the C dispatch, about 1.3 KiB
  with the agent's callback, plus the BPF stack when programs run; `svc`
  needs none. Code on very small stacks (Go goroutine stacks, a sigaltstack
  near MINSIGSTKSZ) can overflow; not tested with Go.
- The gate's `stp x16, x17, [sp, #-16]!` needs a 16-byte aligned sp: hand
  written code issuing `svc` with a misaligned sp gets SIGBUS once rewritten.
- `mremap` moving rewritten code breaks the PC-relative `b` of its sites.
- `in_hook` (below) stays set if a signal handler `siglongjmp`s out of a hook
  that is running programs, or a vfork child sharing the thread's TLS is
  killed inside the hook: that thread's calls are then untraced. Jumping out
  of running programs is unsupported anyway (map locks may be held); a
  stack-pointer check instead would let handlers on an alternate stack
  re-enter the hook.

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
pthread_atfork only). The array binding and map usage handlers used to
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

Only the values of `per_cpu_hash_map_impl` are per CPU: the key index (Boost
`unordered_map`, which rehashes on insert), the four-byte-key value slot and
recycled-node free lists, and the key scratch buffers are shared by every
thread and process. Every operation therefore runs under the handler's map
lock (`should_lock = true`, plus the typed helper entry points and the
diagnostic direct paths). It used to take none: two threads inserting new keys
crashed or hung the traced process (shared counters, 64 keys, 2-6 threads: 6 of
9 runs). The program path follows the kernel percpu htab like the syscall path:
`BPF_NOEXIST` on an existing key fails with `EEXIST` (it used to overwrite the
calling CPU's value), `BPF_EXIST` on a missing key with `ENOENT`, a new key in a
full map with `E2BIG` (eight-byte and larger keys had no limit). Lookups still
return a pointer used after the lock is released, as for hash maps. Values of
every key size live in slots of shared chunks (`acquire_uint32_value`, a free
list): a delete returns the slot and frees nothing, so a stale pointer writes
into a slot, as with the kernel's preallocated htab. Keys other than 4 bytes
used to own a separately allocated value that a delete freed at once: the
program's next write landed in memory the segment allocator had handed out
again (reproduced on the old code: a block allocated after the delete was
overwritten, on every CPU). Test: "Per-CPU hash values outlive a delete
without freeing memory".

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
convert; a lookup returns a per-thread buffer unless the storage already is
the syscall layout (`value_size` a multiple of 8, no holes). They used to copy `value_size` times the online CPUs packed: a
tool reading 4-byte per-CPU counters took CPU 1's value from CPU 0's padding.
`KERNEL_USER_PERCPU_ARRAY` already used the kernel layout. Tests:
`[percpu-layout]` (with an injected "0,2-3" mask), "Possible CPUs are counted
as libbpf does" (the parser on masks with holes).

`BPF_MAP_LOOKUP_AND_DELETE_ELEM` (`bpftime_map_lookup_and_delete_elem`) used
to ignore the key and pop: hash maps returned ENOTSUP, so libbpf's
`bpf_map_lookup_and_delete_elem` failed on them. It now pops queues and
stacks and removes the key's element of hash, per-CPU hash and LRU hash maps
in one step (the striped hash copies the value under its stripe lock, the
others under the handler lock); other types return ENOTSUP like the kernel.
Test: "bpf() lookup and delete takes the key of hash maps".

The per-CPU hash's `bpf()` update compares the low 32 bits of the flags with
`BPF_NOEXIST`/`BPF_EXIST`, as its program path does (bpftime's own
operations use the high bits); it compared the whole value, so a flag with
a high bit set skipped the checks there. Test: "Per-CPU hash update flags
mean the same through bpf()".

Map creation (`bpf_map_handler::map_init`) checks sizes as the kernel's
`map_alloc_check` does for hash, per-CPU hash, LRU hash, array, per-CPU
array, queue and stack: 0 entries, 0-byte values (and keys for hashes) are
EINVAL; keys over 512 bytes, per-CPU values over 32 KiB and a lower bound of
the size above the segment are E2BIG. An allocation that throws is ENOMEM.
The handler is removed and creation returns -1 with errno (`set_handler`
used to ignore `map_init`'s result and keep an empty handler). Before, a
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
128 MB (was 50): memleak's 1M-entry hash map needs 64-96 MB with the doubled
hash buckets. When a `bpf()` or `perf_event_open()` that bpftime handles fails
in userspace (Boost `bad_alloc`, Boost container `length_error`: "allocator's
max size reached"), the interposer returns -1 with `ENOMEM` (`EINVAL` for other
exceptions) and logs a hint to raise the size. It must never fall back to the
kernel there: the kernel then created a real map the runtime did not know, the
tool loaded without error, and its programs could not resolve the map ("Expected
fd 5 to be a map fd") and traced nothing.

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

## Agent Work Is Invisible To Probes

The agent runs inside the traced process; its own calls must not reach the
probes (memleak reported ~250 agent allocations and two 8 MB thread stacks).
`agent_internal_section` (RAII `gum_interceptor_ignore_current_thread`) covers
`init_attach_ctx_from_handlers`, thread creation and `setenv`, and every agent
background thread (IPC, auto-refresh, detach) ignores probes for its lifetime.
Wrap any new agent-side work that allocates or calls hooked functions on the
host's threads. Allocations made before the agent takes over (process start-up
before `__libc_start_main`) stay invisible, unlike kernel uprobes armed at exec.

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
