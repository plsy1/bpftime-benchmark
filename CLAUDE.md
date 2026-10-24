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
