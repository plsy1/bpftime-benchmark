# bpftime on the Jetson Orin: timeline

What was done to and measured on bpftime, a userspace eBPF runtime, on a Jetson Orin Nano 8 GB
(aarch64), with an x86 NUC as the second platform. The question from the start: how does bpftime
do against kernel eBPF ("bpf" below) on ARM64, where is it slower, and can it be made not to lose.
Each entry says what was done, the numbers that mattered and where the details are. Newest last.

Links: `results/…` is the [`benchmark-results/jetson`](https://github.com/plsy1/bpftime-benchmark/tree/benchmark-results/jetson)
branch; `tag/…` is the tag
[`jetson-results-before-cleanup-20261110`](https://github.com/plsy1/bpftime-benchmark/tree/jetson-results-before-cleanup-20261110),
which keeps the results from before 2026-10-19; `reports/…` is `results/reports/`, the analysis
documents up to 2026-10-30. Commits are on the dev branch
(`dev/uprobe-official-4708f4092d1e`; before 2026-11-10 `archive/uprobe-official-177a1dc465ff`).

## 2026-06-26 – 07-11: test infrastructure

- Runners and GitHub Actions workflows for ARM64 and x64 (uprobe, syscall, ssl-nginx with
  sslsniff, syscount-nginx, MPK), comparing official v0.2.0 and master.
- The Jetson's stock kernel had no BTF: a no-BTF image and build (`archive/official-no-btf`).
- The workflows and that branch were deleted on 2026-11-10; all measuring is done on the boards.

## 07-12 – 07-27: ssl-nginx end to end

- bpftime was 3.8–12.6% behind bpf on the Jetson, with large variance; on x64 it led.
- Three causes, peeled off one by one: software perf records not 8-byte aligned (hidden event loss,
  bimodal results); about 72% from `bpf_perf_event_output` pinning the CPU for every event (four
  extra system calls that protected nothing); `bpf_probe_read_user` reinstalling a SIGSEGV
  handler on every call.
- After the fixes bpftime was 7.6% (16 B) / 9.8% (256 KB) ahead of bpf, variance from 8.7–16.6%
  down to 1–3%.
- [reports/ssl-nginx/bpftime-arm-performance-root-cause-20260727.md](https://github.com/plsy1/bpftime-benchmark/blob/benchmark-results/jetson/reports/ssl-nginx/bpftime-arm-performance-root-cause-20260727.md)

## 07-30 – 08-16: where uprobe map operations spend their time

- Fixed the benchmark's semantics (hash delete mostly deleted missing keys).
- ARM64 and x64 side by side; the bpftime − bpf gap taken apart layer by layer (L0–L3), then
  production-path A/B in one binary, down to the source lines: generic dispatch, `std::function`,
  Boost shared-memory containers, value copies, node teardown.
- [reports/uprobe/bpftime-uprobe-comprehensive-investigation-report-20260813.md](https://github.com/plsy1/bpftime-benchmark/blob/benchmark-results/jetson/reports/uprobe/bpftime-uprobe-comprehensive-investigation-report-20260813.md);
  data under `tag/uprobe/*-202608*`.

## 10-11 – 10-19: C++ map paths

- Per-CPU array and hash paths without `std::function`, fewer copies, integer hashing for 4-byte
  keys, node and value reuse; map-type-specific helpers chosen at load time; hash lookup
  hashing, probing and key compare.
- 8 of the official benchmark's 10 map operations faster than bpf; only the two array lookups left.
- The daily automation starts (10-14): it follows official bpftime, replays our patch list onto
  every new official commit, builds, tests and runs the official uprobe benchmark
  (`automation/jetson-daily`).
- [reports/archive/bpftime-uprobe-optimization-progress-20261011.md](https://github.com/plsy1/bpftime-benchmark/blob/benchmark-results/jetson/reports/archive/bpftime-uprobe-optimization-progress-20261011.md);
  data under `tag/uprobe/*-202610{11..18}*`.

## 10-19 – 10-21: array lookups inlined by the JIT

- Like the kernel's `map_gen_lookup`: array lookups become direct loads in LLVM IR (llvmbpf had
  the code, never enabled). Per-CPU arrays too, on by default, mixed with hash maps, other
  helpers and global data.
- A consuming benchmark of our own (the official one never uses the lookup's result).
- Array lookup 2.5–4.4× bpf → 0.53–0.83×. A survey of libbpf-tools: 93% of lookups are hash
  lookups, so hash maps came next.
- `results/uprobe/array-jit-*-20261019..21`

## 10-21 – 10-23: hash maps (first pass)

- Fixes for programs mixing JIT-specialized and other maps; buckets doubled, tombstones reclaimed,
  bounded probing; a typed delete helper.
- Hash lookup 0.77–1.01× bpf in every shape measured (misses, after churn), update/delete
  0.26–0.34×.
- `results/uprobe/hash-*-20261022`, `hash-typed-delete-20261023`

## 10-23 – 10-24: back to ssl-nginx

- The map work did not show end to end (+0.2%): sslsniff does a few map operations per event.
- Two correctness fixes: inherited JIT programs did not run in fork children (nginx workers lost
  every event, `a90fa447`); perf buffers silently dropped about half the events at 256 KB
  (`db3d475f`, official too). After them all 7 payload sizes beat bpf (+3.4% to +7.6%).
- Path ablation: half of the remaining loss was `bpf_get_current_uid_gid`'s two system calls per
  call (cached, `7ed339ad`), then `getpid`/`gettid` in perf output (cached, `bca2ebce`): 16 B now
  17.4% ahead of bpf.
- `results/uprobe/ssl-nginx-*-202610{23,24}`, `perf-*`, `uid-gid-cache-20261024`

## 10-24 – 10-26: a BTF kernel

- Built L4T R39.2.1's kernel with BTF, syscall tracepoints and kprobes
  (`6.8.12-1021-tegra-btf`); a kernel panic on the stock kernel that night, memtest clean; booted
  and made default on 10-26. bpf baselines remeasured: unchanged within ±3%.
- `results/kernel/`, `results/uprobe/btf-kernel-20261026`

## 10-25 – 10-26: real tools

- Seven real uprobe tools (libbpf-tools funclatency, gethostlatency, memleak, bashreadline;
  bpftime's examples) against bpf, official and dev: memleak and funclatency did not work on
  official either. 18 fixes across the uprobe hooks, syscall server, runtime, maps, helpers and
  the llvmbpf JIT.
- All seven count exactly what bpf counts; gethostlatency 1.45×, memleak 1.28×, funclatency
  3.1–3.6× bpf's throughput. With the BTF kernel the audit grew to 18 tools (sslsniff, USDT in a
  JVM, bpftrace), all exact.
- [reports/realprog/bpftime-realprog-compatibility-20261026.md](https://github.com/plsy1/bpftime-benchmark/blob/benchmark-results/jetson/reports/realprog/bpftime-realprog-compatibility-20261026.md),
  `results/uprobe/realprog-*`

## From 10-26: the x86 NUC

- A second session on an x86 NUC (i5-1240P, Debian 13) builds and measures the same dev commits:
  all 10 map operations faster than bpf there too (0.25–0.81×), ssl-nginx +48% (16 B); it reports
  what breaks on x86 (the x86 hook's registers, an LTO link error, a hash map miss rate).
- [`benchmark-results/x86-nuc`](https://github.com/plsy1/bpftime-benchmark/tree/benchmark-results/x86-nuc)
  (its `reports/` holds the former `summry/x86-nuc`).

## 10-27 – 10-28: map dimensions

- Usage counters in bpftime (`09421773`, off by default) over 16 real tools: insert at entry,
  lookup and delete at exit, 4–8 byte keys, all single-threaded. Then controlled tests along those
  shapes at 1/2/4/6 threads.
- Single thread: equal to or faster than bpf. Multi-thread is the weak spot: 6 threads, 15.5 µs per
  call against 6.7 (one spin lock for the whole hash map, lookups included; Gum's per-function
  usage counter). Per-CPU hash crashed the traced process on concurrent inserts (official too):
  fixed.
- `results/uprobe/map-*-20261028`, `uprobe/dimension-walkthrough-zh.md`

## 10-28 – 11-02: system call tracing on aarch64

- The aarch64 side of bpftime's system call rewriting had been a TODO upstream since 2024
  (`bpftime start -s` could not load). Each `svc` now branches to its own gate in a nearby
  trampoline, all registers kept (`7a7d29b8`). This unblocked the official syscall,
  syscount-nginx and fuse benchmarks and syscall tracepoint tools on ARM64.
- Dispatch without heap allocations (`2e850c45`): +282 → +146 ns per write over no tracing with
  one empty program (bpf +86 ns).
- Coverage: libraries loaded later, the vDSO's fallback paths, only real code sections, an opt-in
  seccomp lazy mode for code never seen. Real runtimes: every dynamically linked Go program
  crashed with the agent loaded (the hook used 18 KB of goroutine stack); the hook now runs on a
  stack of its own (`7d9690c3`); Go, Node and Java run traced.
- `results/syscall/`

## 10-30 – 11-04: maps under threads

- Hash lookups take no lock (per-bucket generation), writers lock stripes (`9c3e0345`,
  `cb3f29d2`, `f4fdcbcd`); per-CPU hash without the map lock (`a73f6753`).
- A survey of the table under realistic keys (clustering, tombstones), then a redesign as the
  kernel's preallocated htab: u32 chains, per-CPU free lists, a seeded hash, updates through a
  spare element so lookups never miss a present key (the kernel itself misses up to 0.63% on x86).
- Stack trace map without the lock: memleak 1.99/1.80/1.22× bpf at 1/2/4 threads; at 6 threads
  still 0.79×, bound by Gum's per-function usage counter.
- `results/uprobe/hash-*-2026{1030,1102,1103}`, `dev-*`, `stackmap-lockfree-20261104`,
  `vs-pre-mt-20261104`

## 10-31 – 11-05: independent reviews

- Five rounds of reviews of the dev branch by agents given only the code and the requirements,
  each finding checked and, where possible, reproduced: fork handlers that never ran under
  `dlmopen`, misaligned hash values (SIGBUS on atomics), use-after-free in the per-CPU hash, the
  hook's unwind information, races in the Frida detach and the hook's stack, and more. Each fix
  came with a test that fails without it.
- `results/reviews-20261031`, `results/reviews-20261105`

## 11-04 – 11-05: real tools again, and what they found

- ssl-nginx with the new maps: 16 B +18.5%, 256 KiB +9.9% against bpf.
- Found and fixed: a detach crashing threads inside a probed function; memleak deadlocking on
  threads; opensnoop delivering half its events (ring buffer without wakeups); ringbuf helpers
  losing their map; libbpf's dead code for read-only options disabling whole programs; logging and
  libbpf poison calls at exit. The harness now also covers opensnoop, javagc through
  `bpftime attach` and get_stack_id.
- `results/uprobe/realtools-20261104` and the 11-04/11-05 directories

## 11-06 – 11-08: the dimensions left, and an improvement round

- Event output by burst, size and consumer speed; then start-up, many probes, probe reads, long
  runs. Findings: every traced process started 22–27 ms late; 0.42 ms per probe per start;
  consumers either woke for every sample (costly for the traced process) or 0.5 ms late.
- The improvement round (16 commits): consumers nap after records and producers wake them only
  past the watermark (ssl-nginx 12,673 req/s against bpf's 12,150; gethostlatency p50 90 µs),
  checked probe reads without scratch copies, module lookups once per process, and more.
- `results/uprobe/output-dims-20261106`, `more-dims-20261106`, `improve-20261106`,
  `remeasure-20261108`

## 11-08 – 11-09: start-up (AOT cache)

- 93% of a traced process's start under memleak was compiling its 22 programs. The syscall server
  now compiles each program once and agents link the object, with per-process symbols for the
  addresses of maps and global data.
- A process starting under memleak 308 → 38 ms (bpf 0.7), with one program 22.5 → 13.6 ms;
  16 processes starting at once, the last 220 → 80 ms. Generated code as fast as the JIT's.
- The intermittent Frida detach test crash: Gum writing a function's first redirect while other
  threads run it (not a detach); the tests hook first.
- `results/uprobe/startup-20261108`, `frida-detach-crash-20261108`

## 11-09 – 11-10: CUDA attach on the Orin's GPU

- bpftime's CUDA attach run on the Orin (CUDA 13.2, sm_87) for the first time: of 18 example
  cases 3 worked; 17 problems found (default nvcc 13 builds and static cudart never instrumented,
  two probes on one kernel breaking the application's results, host calls of all GPU threads
  sharing one thread's arguments, a GPU ring buffer losing events, 1.06 ms per host call,
  1.6 s start-up, every loader crashing at exit, …).
- Fixed: patched images loaded where cudart loads its fatbins, so every launch kind runs the probes;
  host-call and ring buffer protocol (about 10 µs per call; a probe making map calls 21.3 →
  0.35 ms per launch); one GPU stack frame (16 KB → 592 B per thread); start-up 1.7 s → 0.35 s;
  the application's environment and locale left alone; an agent without a usable GPU keeps its CPU
  probes. 19 of 19 example cases pass.
- `results/gpu/feasibility-20261109`, `examples-20261109`, `fixes-20261109`

## 11-10: a new base, and a clean-up

- Development moved to official 4708f409 (`dev/uprobe-official-4708f4092d1e`): the daily list,
  now 236 commits, replays there; the one commit that conflicted with upstream (#709) is kept in
  adapted form.
- Results from before 10-19 moved to the tag; `summry/*` merged into the results branches'
  `reports/`; CI and stale branches deleted. `results/uprobe/base-4708f409-20261110`

## 11-10: the upstream GPU benchmark, official against dev

- Upstream's `benchmark/gpu` run on the Orin with official 4708f409 and dev c115b33d, 3 rounds
  each, next to upstream's RTX 5090 and P40 tables. Official only instruments a victim built with
  the legacy launch call and probes on the host-memory map types; as shipped, every micro case
  fails to load on the Orin and the CUDA Counter runs uninstrumented.
- Host-call cases: official 33.7 ms per launch (the same on the 5090 and the P40), dev 0.32–0.37
  ms; CUDA Counter 337 → 3.5 ms. Device-only probes in the same band on both builds (1.1–1.5× of
  baseline); dev slower on GPU ring buffer small/medium (+12–19%), not explained yet.
- `results/gpu/compare-20261110`

## 11-10: which dev change causes each GPU difference

- Every official-vs-dev difference traced to a dev commit (`attribution.md`), then the open ones
  measured: each candidate change undone on dev, 10 rounds interleaved with official and dev.
- Host calls 100× faster: the watcher's 1 ms sleep removed. Ring buffer small cases slower: dev's
  drop path on a full ring (undone, dev goes below official). Light probes and small map cases
  1–15% faster: patched images loaded at load time instead of redirecting each launch (forcing
  the old path makes dev slower than official everywhere). The 3-round entry+exit and memtrace
  gaps were noise. Open: dev's fallback launch path 5–9 µs per launch slower than official's.
- GPU array update at 1M threads (dev 10% faster) is not a speed-up: 1M threads read and write
  one uncached host value, and how fast that drains depends on when warps arrive. Dev's device-code
  changes together set that timing (undoing all three closes 55% of the gap); with the adds
  removed on the same grid dev is 13% slower instead. Segment size, the value's position, CUDA
  registration and the launch path ruled out; about 90 µs left unattributed.
- `results/gpu/compare-20261110/attribution.md`, `results/gpu/ablate-20261110`,
  `results/gpu/xlarge-20261110`

## Where it stands (11-10)

- uprobe map operations: all ten of the official benchmark at or below bpf on one thread
  (10-26 figures in [reports/bpftime-research-overview.md](https://github.com/plsy1/bpftime-benchmark/blob/benchmark-results/jetson/reports/bpftime-research-overview.md),
  section 9); hash maps no longer serialize threads.
- Real tools: 18 checked against bpf, counts exact; ssl-nginx ahead of bpf at every payload.
- System call tracing and CUDA attach work on ARM64 with the dev branch.
- Open: Gum's per-function usage counter under many threads; the agent's own load time (about
  9 ms); the CUDA late path (`bpftime attach` to a running GPU process; 5–9 µs per launch slower
  than official's); fixes not yet offered upstream.
