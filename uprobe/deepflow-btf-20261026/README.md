# DeepFlow on ARM64: kernel vs bpftime daemon mode (2026-10-26/27)

The official `bpftime/benchmark/deepflow` scenario on the Jetson with **upstream DeepFlow v7.2.3**
(native ARM64 support, BPF programs unmodified) instead of eunomia-bpf's x86-only v6.1.1 fork
(which also replaced two of DeepFlow's hash maps with hand-written tables on array maps, see the end). Kernel
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
kernel; a kernel uprobe pays one trap and accesses the maps in place. bpftime avoids them only
for maps that live in userspace, or arrays created with `BPF_F_MMAPABLE`, which the array proxy
maps directly.

**The paper's fork.** eunomia-bpf/deepflow (officeyutong, 2023-11-13..17, messages "Mock?",
"Update with mocked hashmap?", "Update uprobe-go impl with custom hashmap", "Update with implemented
hashmap") replaced `goroutines_map` and `uprobe_offsets_map` (both `BPF_MAP_TYPE_HASH` in v6.1.1)
with linear-probing tables of 229 slots on `BPF_MAP_TYPE_ARRAY`, one `bpf_map_lookup_elem` per
probed slot. The arrays are not `BPF_F_MMAPABLE`, so the kernel refuses to mmap them and today's
array proxy falls back to a syscall per lookup (`array_map_kernel_user.cpp`, re-mmap failure path):
the change saves no syscalls here. The reason for it is not recorded; it is not a missing proxy
(the hash proxy was added on 2023-10-26). With the fallback a lookup returns a copy, so the
tables' writes through the returned pointer (`ent->used = 1`, key and value copies) do not reach
the kernel map, and the kernel TLS programs do not see the goroutine ids written in userspace:
reproduced with today's bpftime on the same change ported to v7.2.3 (variant A below). The fork
itself is x86-only and bpftime of that time differs, so whether the paper's runs had this is not
known. Our run2 legs do not depend on it: the audit shows the goroutine ids written by bpftime
reach the kernel programs.

## Array-table variants (eunomia-style, `bench/run-arraytbl-A`, `-B`; requested experiment)

**This changes DeepFlow's BPF programs**, unlike everything above; both legs of a variant run the
same changed programs. `src/arraytbl-variant.diff` (on v7.2.3, copy `deepflow-v7.2.3-arraytbl`):
`goroutines_map` and `proc_info_map` (the two maps the goroutine programs look up and update) become
229-slot linear-probing tables on `BPF_MAP_TYPE_ARRAY` (slot = state + key + value; states free,
used, deleted; multiplicative hash), as in eunomia-bpf/deepflow; `go_tracer.c` fills the
`proc_info` table from userspace slot by slot. Differences from the fork: deleted slots are marked
and a lookup stops at the first never-used slot, and at most 16 slots are probed. Scanning all 229
slots made `df_T_exit_sendmsg` too complex for the verifier (E2BIG), DeepFlow fell back to its
`common` variant, whose fully unrolled goroutine program (25,948 instructions) exceeds the
daemon's 4096-instruction capture; with 16 both legs load the usual `5.2_plus` programs.

- **A**: arrays as in the fork (not mmapable).
- **B**: the same with `BPF_F_MMAPABLE` on both arrays (`DF_TABLES_MMAPABLE=1`, set in `user/load.c`),
  so the bpftime array proxy maps the kernel arrays and programs read and write them in place.

Functional check first (`checks/arraytbl`, 5 HTTPS requests, TLS events carrying a goroutine id):
kernel 10/10 in both variants; bpftime **A 0/10** (the agent logs the failed re-mmap, err 22, and
falls back to copies), **B 10/10**.

Throughput as above (3 reps, rotated, legs no-uprobe/kernel-uprobe/user-uprobe; no-probe is
DeepFlow-independent and taken from run2), `bench/summarize-arraytbl.py` → `summary-arraytbl.txt`:

| Protocol | Size | run2 kernel | run2 bpftime | run2 | A kernel | A bpftime | A | B kernel | B bpftime | B |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| https | 1 KB | 32,433 | 24,107 | -25.7% | 32,532 | 24,465 | -24.8% | 32,912 | 25,468 | -22.6% |
| https | 16 KB | 18,406 | 14,835 | -19.4% | 18,503 | 15,163 | -18.0% | 18,517 | 15,501 | -16.3% |
| https | 256 KB | 4,235 | 4,183 | -1.2% | 4,199 | 4,216 | +0.4% | 4,167 | 4,198 | +0.7% |
| http | 1 KB | 52,554 | 37,646 | -28.4% | 52,360 | 36,469 | -30.3% | 52,483 | 40,647 | -22.6% |
| http | 16 KB | 30,668 | 23,960 | -21.9% | 30,473 | 24,361 | -20.1% | 30,407 | 26,116 | -14.1% |
| http | 256 KB | 11,561 | 11,061 | -4.3% | 11,756 | 11,221 | -4.6% | 11,699 | 11,219 | -4.1% |

Go uprobe cost per request against each run's no-uprobe leg, kernel / bpftime (us): HTTP 1 KB
run2 3.4 / 10.9, A 3.6 / 11.9, B 3.5 / 9.1; HTTP 16 KB 3.8 / 13.0, 4.1 / 12.4, 4.4 / 9.8;
HTTPS 1 KB 12.8 / 23.5, 12.6 / 22.8, 12.4 / 21.2.

- The tables cost the kernel nothing measurable (kernel legs within 2% of run2).
- A in bpftime: goroutine ids 0.0 per request in all 18 runs (kernel leg about 2.0): the
  throughput is that of a broken configuration.
- B in bpftime: ids about 2.0 per request (one HTTPS 256 KB run lost all TLS events, as single runs
  of both legs do in every variant). bpftime gains 3-8 points against the kernel at 1-16 KB
  (HTTP 1 KB 10.9 → 9.1 us per request) and is still 14-23% slower.
- Why still slower (`analysis/arraytbl-B-user-uprobe`, HTTP 1 KB, same script with
  `T=… TRACER_ENV=DF_TABLES_MMAPABLE=1`): 8.8 `bpf()` calls per request (run2: 16). `perf`: the
  frida enter path 26.0% of the server's CPU (run2 33.9%); proxied lookups 16.7% (21.2%), of
  which 14.5% is the percpu `members_offset` lookup (`read_all_cpus`: one syscall reading every
  CPU's value at the start of each program run); updates 4.8% (8.5%: `pid_tgid_callerid_map`,
  `go_ancerstor_map`, still hash maps). A kernel percpu array cannot be mmapped, so this lookup
  stays a syscall unless the proxy caches it (DeepFlow writes the offsets once at start).

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
