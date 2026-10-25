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
The existence check runs only for `BPF_NOEXIST`/`BPF_EXIST`, under the map lock
the callers hold; `BPF_ANY` stays a single probe. Lookup-then-update counters
depend on `BPF_EXIST`: when a reader deletes the key in between, ignoring the
flag re-inserted the stale count (the malloc example counted up to 20% too many
calls).

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
hash. `bpf_get_stackid` returns errors negative (it used to cast them to a
valid-looking `uint32_t` id).

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
