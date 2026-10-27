# DeepFlow on ARM64: kernel vs bpftime daemon mode (2026-10-26/27)

The official `bpftime/benchmark/deepflow` scenario on the Jetson with **upstream DeepFlow v7.2.3**
(native ARM64 support, BPF programs unmodified) instead of eunomia-bpf's x86-only v6.1.1 fork
(which also replaced DeepFlow's goroutine hash map with an array-backed one). Kernel
`6.8.12-1021-tegra-btf`, `jetson_clocks` (cpu5 1728000 before and after), dev branch built from
`build-jit` (daemon, CLI and the fixes below; agent/syscall-server/daemon at `e6bfc3d9`).

## Setup

- DeepFlow eBPF part built in the upstream CI image `ghcr.io/deepflowio/rust-build:1.31-arm64`
  (`make socket-tracer`, log `src/build-socket-tracer.log`). Only the sample driver
  `samples/rust/socket-tracer/src/main.rs` was changed (`src/socket-tracer-driver.diff`): `DF_GO_PIDS`
  selects the Go processes for uprobes (the sample hard-codes pids 101/202/303) and enables Go
  tracing, `DF_QUIET=1` counts events per source instead of printing them, and the hooked socket
  syscalls are the agent's default ten (the sample omits `read`/`write`, which Go's plain HTTP uses).
- Go server: `benchmark/deepflow/go-server{,-http}/main.go` rebuilt for ARM64 with Go 1.22.2
  (`src/servers`). wrk 4.1 (Ubuntu), `-t 2 -c 100 -d 10`, pinned to CPUs 4-5; server pinned to 0-3.
- DeepFlow attaches Go uprobes 60 s after it matches a running process; uprobe legs wait 70 s.

| Leg | What runs |
|---|---|
| no-probe | Go server alone |
| no-uprobe | DeepFlow, kprobes + syscall tracepoints only |
| kernel-uprobe | DeepFlow incl. Go uprobes on the server (TLS read/write, HTTP/2, `runtime.execute`, `runtime.newproc1`), all in the kernel |
| user-uprobe | as kernel-uprobe, but `bpftime_daemon -w 0x41770 -w 0x464f0 -w 0x46998` redirects the goroutine-tracking uprobes (`runtime.execute` entry, `runtime.newproc1` entry and its RET) to userspace and `bpftime attach` injects the agent; TLS/HTTP2 uprobes stay in the kernel (the paper's split) |

## bpftime fixes this needed (dev branch, each with a test that fails without it; official has all of them)

| Commit | Problem |
|---|---|
| `d8470a61` | daemon: syscall tracepoints absent on arm64 (`open`, `dup2`) failed the tracer |
| `4d8b89f9` | daemon: libbpf 1.3+ `dup2`s maps onto reserved fds; untracked, so no program built with current libbpf could be relocated |
| `a43f213d` | daemon: restoring the redirected uprobe path wrote 255 bytes into the loader's heap (DeepFlow aborted: `free(): invalid pointer`) |
| `21182b79` | kernel-user proxy maps: percpu proxy never opened its fd (every lookup failed), hash proxy shared one value buffer across threads (torn reads), wrong percpu layout |
| `1d2944d9` | daemon: kernel LRU hash maps were unsupported ("Unsupported map type: 1009") |
| `e6bfc3d9` | daemon: kernel object ids used as handler indexes; after a few runs ids passed the 6144-entry table and agents ran nothing (run1 below) |

## Results (`bench/run2`, 3 reps rotated, medians of req/s; `bench/summarize.py`)

| Protocol | Size | no-probe | no-uprobe | kernel-uprobe | user-uprobe | user vs kernel |
|---|---|---:|---:|---:|---:|---:|
| https | 1 KB | 75,145 | 55,540 | 32,433 | 24,107 | -25.7% |
| https | 16 KB | 37,979 | 25,736 | 18,406 | 14,835 | -19.4% |
| https | 256 KB | 5,481 | 4,248 | 4,235 | 4,183 | -1.2% |
| http | 1 KB | 93,682 | 64,008 | 52,554 | 37,646 | -28.4% |
| http | 16 KB | 57,307 | 34,755 | 30,668 | 23,960 | -21.9% |
| http | 256 KB | 20,937 | 11,969 | 11,561 | 11,061 | -4.3% |

Audit (per request, during the wrk window): HTTP 4 syscall events in every DeepFlow leg; HTTPS 2 Go TLS
uprobe events; events carrying a goroutine id (filled from `goroutines_map`, which the goroutine
uprobes write) 2.0 in kernel-uprobe and user-uprobe, 0 in no-uprobe. Without injecting the agent
(`checks/bpftime-noattach`) the ids are 0, so in user-uprobe the goroutine tracking runs in bpftime.
Some runs lost events in both uprobe legs (kernel 0.21 and 0.09 per request once each, user 1.33 once).

On ARM64 with upstream DeepFlow, bpftime is slower than the kernel. Per request, against the
no-uprobe leg (1/throughput), the Go uprobes cost the kernel 3.0-3.8 us and bpftime 6.9-13.0 us
on HTTP (only the goroutine uprobes fire there: about 3x), 12.8-15.5 us vs 23.5-28.5 us on HTTPS
(the TLS uprobes, in the kernel in both legs, included).

## Why bpftime is slower (`analysis/`, HTTP 1 KB, `why-slow.sh`)

- Probe hits per request (kernel leg, bpftrace): `runtime.execute` 3.0, `runtime.newproc1` 1.0
  (entry and return probes: 2 hits), about 5 goroutine-uprobe hits.
- In the bpftime leg the Go server made 16 `bpf()` system calls per request (5.28M in the
  window, 86% of all its system calls): about 3 per hit. The goroutine programs share their maps
  with DeepFlow's kernel programs, so each map operation goes through a kernel-user proxy as a
  syscall: the percpu `members_offset` lookup at the start of every program, `proc_info_map`
  lookup, `goroutines_map` / `pid_tgid_callerid_map` / `go_ancerstor_map` updates.
- `perf record -g` on the server (`user-uprobe/perf.data`): the frida enter path is 33.9% of its
  CPU time; inside it the proxied lookups take 21.2% and updates 8.5%, the JIT-compiled programs
  0.43%. Each of those syscalls also runs bpftime_daemon's own `sys_enter_bpf` tracer in the
  kernel (3.25%).

So about nine tenths of bpftime's per-hit cost is the system calls for maps shared with the
kernel; a kernel uprobe pays one trap and accesses the maps in place. bpftime only wins when a
program's maps live in userspace, or are arrays the proxy can mmap (BPF_F_MMAPABLE): eunomia's
DeepFlow fork for the paper replaced `goroutines_map` with an array-backed table, which is
probably what the paper's speed-up relied on.

`bench/run1-invalid-handle-overflow`: the first full run, before `e6bfc3d9`; from some run on the
daemon could not create the programs (kernel ids > 6144) and user-uprobe ran nothing. Not usable.

## Also checked on the way (`src/`)

- `bpftime attach` (frida CLI, built from the dev branch) works on ARM64: libbpf-tools `javagc -p <JVM>`
  with the agent injected into a running root JVM, 50/50 GC events in 6 of 6 runs (`attach-javagc.sh`);
  bpftrace USDT on the JVM through attach likewise (`attach-bt.sh`). An injected agent in a non-root
  process cannot open the root-owned shared memory and stays inactive while the CLI reports success.
- Injecting the agent with all of DeepFlow's Go uprobes in bpftime (no whitelist) crashed the Go
  server (SIGSEGV), and the TLS programs call `bpf_get_current_task` (kernel task struct), which
  userspace cannot provide; this is why only goroutine tracking was moved.
