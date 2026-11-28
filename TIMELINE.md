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

## 11-11: GPU host calls, layer 1

- Calls that need no host removed: ordinary ARRAY maps read and written on the GPU (dev
  1b3d9c99) and `bpf_get_current_pid_tgid` as a per-module constant (aa6eb973): CPU Array map
  update 320 → 51 µs, lookup 371 → 49 µs, CUDA Counter 3,487 → 2,929 µs.
- The watcher's wake-up measured first: a host call after a pause waited for the watcher's sleep,
  60–950 µs. Dev 46f5a7cb made it wait with WFE while a probed kernel may run: 13.5–16.6 µs at any
  gap, at the price of a core held while such kernels run.
- `results/gpu/hostcall-l1-20261111`, `results/gpu/hostwake-20261111`,
  `results/gpu/wfe-20261111`, `results/gpu/hostwake-fix-20261111`; `RESEARCH_DIRECTIONS.md`

## 11-11 – 11-12: how the host-call watcher waits

- WFE measured under full CPU load: the load lost 13% of its throughput and 11% of host calls
  took over 1 ms. Plain short sleeps did better under full load and worse with a core free (WFE
  14 µs). Policies from other systems (KVM's
  halt-poll window, NAPI's deferred interrupts, load-gated spinning) tried as sets A (park on the
  launch note), C (an adaptive poll window plus short deferral sleeps) and B (WFE only with a core
  free), 504 runs; controls (168) showed the single-call gain is the dropped 200 µs busy poll and
  the window is what keeps bursts cheap.
- Two corrections: a window up to 300 µs polled a whole core at request gaps of 55–115 µs (a
  50 µs limit: 0.07 core), and closing the window on one long gap made 32-call bursts under load
  dearer (22.1 against 15.7 µs per call). The rule kept (AC3, 448 runs): the window doubles from 10
  to 50 µs and closes only on two long gaps in a row.
- AC3 in dev (52126f33, WFE now opt-in): against 46f5a7cb 0.02–0.13 instead of 1.0 core while
  probed kernels run with a core free, the load under full load 0.988 instead of 0.863 of its
  speed, graph launches answered in 70 instead of 534 µs; a sparse call takes 69 instead of 14 µs
  with a core free, and a duty-cycled load still sends 30% of calls over 1 ms (WFE 0.1–0.3%).
- `results/gpu/wfe-load-20261111`, `results/gpu/watcher-sleep-20261111`,
  `results/gpu/watcher-policy-20261112` (with `controls/`, `v2/`, `v3/`, `dev/`)

## 11-12: the watcher policies without jetson_clocks

- Eight cells, seven sets, both clock regimes in every round (448 runs): `jetson_clocks` against the
  boot scripts' frequency scaling with CPU idle states on. With a core free, where scaling and idle
  states act, the sleeping sets run at the 730 MHz floor and AC3 stays below the KVM-style window
  rules at 2 calls per launch in every round (5.0–9.1 µs per call with, 7.4–24.6 without); WFE's
  waiting thread keeps its CPU cluster near 1,700 MHz. Under full load the CPU neither scales nor
  idles in either regime, and the burst and load comparisons hold there too.
- Power: WFE 1–90 mW below the sleeping sets under `jetson_clocks`, no difference resolved without
  (tegrastats steps 40 mW). The EMC rate during the runs was not recorded. Reviewed twice
  (refuters per finding), corrections worked in.
- `results/gpu/watcher-policy-20261112/dvfs`

## 11-13 – 11-14: direction 2's design; the two directions on a real application

- **Call-site survey.** It classed none of the 59 host-call sites in bpftime's GPU examples as
  needing a synchronous answer. This is a static classification of examples only
  (`results/gpu/callsite-survey-20261113`).
- **Direction 2's design.** The design chooses, per call site, to eliminate, aggregate, defer or
  synchronize (`results/gpu/dir2-design-20261113`). It is a design only.
  - Reviewed three times: the core held twice, and the last revision of the lifecycle rules is
    unreviewed.
- **The advisor's question.** He asked to measure the two directions on real applications before
  deciding how far to go. A survey of mainstream scenarios found no published work, among the
  sources read, that measures this. On the Orin, in practice only applications' own PTX kernels can
  be probed (`results/gpu/real-scenarios-20261113`).
- **llama.cpp on the Orin** (TinyLlama Q4_K_M, CUDA graphs off; `results/gpu/llama-census-20261113`):
  - Every kernel is the application's own, with sm_87 PTX. Probes on four of them counted every
    thread exactly.
  - Launches span 256 to 4.19M threads.
  - The default per-thread capacity lost 99.6% of the counts.
  - One host call per thread on a kernel with 1.5% of the GPU time made a prefill 307 times slower
    (10.8 µs per call, as projected).
  - Agent start-up added 14.4-14.8 s.
- **Conclusion.** Per-thread host calls and per-thread storage keep CPU-style eBPF tools from being
  usable on this application (partly measured, partly projected), so both directions are needed. We
  judge direction 1 the harder one: it has to give up per-thread semantics, simplicity or
  completeness. Tiers and a per-CPU-like map model are in `RESEARCH_DIRECTIONS.md`.

## 11-14 – 11-15: one direction, a GPU map model; step 0 on the board

- **Direction 2's research scheme dropped (11-14, evening).** Read from the code, the trampoline has
  only six host-call sites, and no CPU eBPF tool runs on the GPU unchanged (attach points, missing
  helpers; an unregistered helper makes `generate_ptx` end the application). "Keep CPU-style
  programs, make fewer host calls" has no target. Layer 1 stays in dev. This replaces the
  "both directions are needed" conclusion above.
- **Direction 1 widened into a GPU map model:** how device-side map storage is allocated, what
  happens when it is full, how the CPU reads it. The goal set on 11-15: little memory, and every
  thread keeps its own copy. Per-thread semantics stay; storage follows the threads resident on the
  hardware (12,288 on the Orin), not the grid (up to 4.19M in llama.cpp).
- **Design space** (`results/gpu/dir1-design-space-20261115`, paper designs only): four approaches
  (a library over existing maps, per-launch storage, a shared chunk pool, storage by hardware
  position), and the facts that rule out host-side per-launch allocation, per-thread rings and
  keeping every record.
- **Step 0: nine on-board checks, run twice with identical verdicts**
  (`results/gpu/step0-checks-20261115`):
  - `%warpid` is an SM-wide hardware slot: (SM, slot) is unique among resident warps, also across
    two kernels, and did not change across 198 rounds of time-slicing; `%gridid` repeats across
    CUDA graph replays. Both are measured, not documented guarantees.
  - The system fence dominates: a record per warp costs 10 ns with `membar.sys`, 1.6 ns without.
    An unfenced 8-B store per thread execution in device memory projects 3-6% of a pp512, against
    35-41% for today's per-thread map operation; atomics in registered memory serialize at about
    80 ns, against about 1 ns in device memory.
  - One leader fence per warp showed no violation in 10^9 rounds while the no-fence control failed,
    but the result is not separable from the fence's duration.
  - Device memory can reach the CPU (two-step copy, flags first; under 1% at 1 MiB per ms in
    llama.cpp). CAS in eBPF is unusable (the JIT drops the cmpxchg result). One `bpf()` reader
    drains about 7M records/s. Segments register up to 2,047 MiB. llama.cpp's exit probes see whole
    warps; the launch hooks see every geometry before graph replays.

## 11-15 – 11-16: step 0's follow-up, held to documented guarantees

Step 0 measured how the Orin behaves; several of its conclusions are behaviour of that one GPU. The
rule for this round: correctness may rest only on documented guarantees, measured behaviour only on
performance (`results/gpu/step0b-checks-20261116`, two passes, the same correctness verdicts).

- **The documents** (verbatim audit of PTX ISA, the CUDA programming guide and the driver API):
  `%warpid` is promised unique only within a CTA, and `%smid`/`%warpid` "may change during
  execution"; a copy that runs while its writer runs is not ordered by any text; publication to a
  reader in another process rests on an undocumented alias assumption (the CPU's mapping and the
  GPU's device pointer differ), which today's bpftime loader shares.
- **What works within them:** state carried from the entry probe to the exit probe in the thread's
  own local memory (0 mismatches, time-slicing included; accepted on a real llama.cpp module;
  0.104 ns per thread execution with both probes); slots claimed per lane with atomics (correct,
  +0.12 ns over a `%warpid` slot). Warp claims with an `__activemask()` mask are wrong.
- **Costs:** today's per-thread map operation is 63% system fence; without the fence 0.135 ns, the
  memory access alone 0.056 ns. Batching 384-1,536 warps behind one system fence brings a record
  from 10.6 to 3.3-3.8 ns, still about 10% of a prefill [projection]. The pieces were not measured
  together.
- The copier's global-capture problem has a documented remedy that held (the copier switches its
  own thread to relaxed capture mode).

## 11-16 – 11-17: the minimal prototype of the GPU map model

The design that rests on documented guarantees, built (`results/gpu/map-model-proto-20261116`;
bpftime branch `proto/map-model`, local, as patches there): a thread's state in its own local
memory, mergeable results added into device memory, delivery ordered by the application's
synchronizations.

- **New maps:** a thread-local array (a `.local` area in every probed kernel, its address passed
  to the probes), a sum array in device memory spread over (SM, lane) shards (any shard gives the
  same sum, so the hardware ids only decide speed), a per-thread array in device memory. A delivery
  thread reads them with a snapshot kernel after counting the application's synchronize calls.
- **Correct:** every probed thread's exit counted on llama.cpp (four kernels, every measured
  item), 0 mismatches for an entry-to-exit carry on a victim with predicated exits and graphs,
  3,000 global-mode captures left valid.
- **Cost of a per-thread latency histogram:** pp512 4.1-4.3%, tg128 33.2-33.3%; the same tool on
  today's maps 98.7% on pp512. tg128's cost is per-thread memory work on mul_mat_vec_q (3.14 x
  10^9 threads doing about 0.4 ns each): empty probes 3.6 points, the carry 18, the merge 10. One
  word per histogram bucket instead of shards would cost about 1.0 ns per thread.
- **Delivery:** complete for the kernels the application's synchronizations waited for; covering
  the last one by construction needs the delivery inside the synchronize call (+0.7 to +4.7
  points); a grid-sized per-thread array costs three 32 MB copies and, delivered at every
  synchronization, 36-87%.
- Two reviews (the code; the results against their data) found 26 code findings and a counting
  bug in the synchronization hooks (the agent's own calls counted); all fixed or stated as limits,
  and the correctness checks run again on the final build.

## 11-17: where the per-thread probe cost goes; the end shapes on the real kernel

Why the latency tool costs 0.28 ns per thread on mul_mat_vec_q, and how far a design held to
documented guarantees can bring it down (`results/gpu/probe-cost-20261117`; a debug-only PTX
override on `proto/map-model`, local).

- **Where it goes** (tg128, mul_mat_vec_q probed alone, four passes): empty probe calls 0.02 ns,
  the two timer helper calls 0.035 (the `%globaltimer` read itself costs nothing measurable), the
  carry 0.07, the log2 chain 0.035, the sum-map lookup through the generic helper with the add
  0.135. Instructions and calls cost on this kernel too; the prototype's "memory work, not calls"
  was wrong, and its merge-cost "carry only" column had lost the exit's timer to ptxas.
- **Design:** documentation and code-path checks, three independent designs, a refutation pass,
  one ladder: timer inline, the probe switch read once per kernel, the sum-map address computed
  inline, the start carried by value, `clz` for the bucket. Warp-level combining stays out (the
  documents do not say lanes agree on `activemask`, and it saved little).
- **End shapes measured** by compiling hand-edited PTX on the real kernel (counts exact in every
  variant): from 0.28 to about 0.11-0.12 ns per thread with calls kept (46 to about 57 t/s), 0.083
  spliced into the kernel; the target for 5% is 0.029. This tool shape cannot reach 5% on this
  kernel, spliced or not. Register count moves the result as much as the instructions do; capping
  registers alone made mul_mat_vec_q itself 3% faster.
- **How much of that is still the map** (the tool unchanged, so its log2 stays; rules fixed before
  the runs and reviewed; variants at equal register counts): the best calls-kept shape costs
  0.134 ns per thread (55.1 t/s, -18%). Of that, carrying the start in a `.local` word costs
  nothing measurable, the add into the sum map 0.065-0.071, and the rest (switch, calls, the
  tool's own timer reads and log2) 0.064-0.068. The map's part falls from about 0.196 in the
  generated code to about 0.068; the add into the sum map is what is left to design.

## 11-18: the sum map's add path

How short the add into the sum map can be on documented guarantees (`results/gpu/probe-cost-20261117`,
section 7; hand-written shapes through the same debug override, the tool unchanged, rules fixed
before the runs, three passes, counts exact in every variant).

- **Design:** the add becomes a `red` on the global address (device memory from `cuMemAlloc` is
  global memory; PTX allows `red` there), with one test (the agent writes `max_entries` 0 when there
  is no device copy) and 32-bit offsets. The routing pass already rewrites constant-fd lookups of
  the thread-local map, and it would do the same for the sum map. The delivery's load moves to the
  same global form, a cautious choice (it is morally strong with the adds either way).
- **Measured:** the add path falls from 0.061-0.069 to 0.010-0.015 ns per thread, and the shape from
  55.2-55.6 to 59.6 t/s (-18% -> -12% on tg128). In three steps: the atomic's form 0.035-0.042,
  the extra tests 0.005-0.009, the arithmetic 0.006-0.008. How much is instruction count and how
  much is waiting is not pinned down (the cost per instruction varies, 0.7-1.1 ps): the generic
  atomic sets a predicate the path then depends on (so, inferred, the thread waits for its
  reply), and `red` sets nothing. A guard for pointers of unknown origin would cost 0.017-0.018
  more.
- **What is left:** about 13 instructions (shard address, the `red`). The map's part of the shape is
  now about 0.014 ns, against about 0.066 for everything outside the map; with the add at zero
  the shape would still run at about 60.5-60.9 t/s (-10%).

## 11-18: the map side built

The map side of the design built into the prototype (`proto/map-model`, six commits on
c27412f4; results `gpu/map-lowering-20261118`); the tool and the framework's parts (probe
calls, the switch, the timer helpers, the log2) are left as they are (user: tool side).

- **Sum map:** constant-fd lookups become the shard address inline, and their 64-bit adds go
  to the global state space (`red`), as section 7 designed. The delivery reads the same way.
  Register definitions are counted fail-closed (an independent review found the first count
  failing open).
- **Lookup keys:** a lookup takes the key the program just stored instead of reading it back
  from the stack (thread-local and sum map). Of the thread-local map's hand-written forms only
  this one saved (0.028-0.030 ns); local and 64-bit accesses did not show and are not built.
- **Measured** (counts exact everywhere, the same before and after): tg128 with
  mul_mat_vec_q probed 46.1 -> 54.5 t/s, 0.283-0.286 -> 0.147-0.149 ns per thread
  (32% -> 20%); the latency tool on four kernels 33% -> 21% on tg128, 3.7-3.9% -> 2.1-2.7%
  on pp512.
- **What is left, split** on hand-edited forms of the built code: the map 0.035 ns per thread,
  everything else (calls, switch, timer helpers, log2) 0.110-0.114; with the map's work gone
  the shape would run at 57.2 t/s (-16%). Carrying the start by value instead of through the
  thread-local area saves 0.015-0.021 (a candidate, not built).

## 11-26: the prototype on the current dev; upstream not followed for now

Official bpftime now moves about 20 commits a day (60+ a week since 11-09), most of them on the
locks of the map handler, shared memory, ringbuf and agent start-up, where the dev stack changes
the most; a trial merge of the 44 commits after 70f7bc1f into dev conflicted in 30 files. Dev
stays on official 70f7bc1f (`dev/uprobe-official-70f7bc1f3363`) until we choose to follow again
(user); a manual daily run against 35015537 was stopped before its migration, nothing kept.

- **Prototype moved** onto that dev (`proto/map-model-70f7bc1f`; results
  `gpu/map-model-move-70f7bc1f-20261126`): one conflict (upstream's early return for modules
  without a trampoline, where the routing pass is called), resolved to the same behavior.
- **Same code, same numbers:** the generated PTX is identical but for `callseq` comment numbers,
  which ptxas ignores; unit tests and seven pt_victim tools exact; tg128 with mul_mat_vec_q probed
  54.5 t/s on both bases, 0.148 ns per thread, difference 0.0000-0.0001 ns.
- **Found on dev itself:** the CUDA test `[ptx-failure]` fails on the new dev without the
  prototype too: since upstream skips modules without probes, its broken module is never
  compiled. The test needs a module with a probe.

## 11-26: the start by value

The thread-local area passed by value, the candidate left on 11-18 (`proto/map-model-70f7bc1f`,
1d95c160, review fixes c4235347 and 7d02d677; results `gpu/map-model-byvalue-20261126`).

- **What:** where a kernel's probes reach the thread-local area only as `lat` does (constant-key
  lookups, 32-bit loads and stores, one 8-byte word), the kernel keeps it in a `.local` slot and
  passes it to its probes as a parameter; a probe that writes it returns it. Inside, the area is
  two registers. Anything else keeps the address form for that kernel, with the reason logged.
- **Measured:** the text the pass emits was timed first as a hand-written form (0.021-0.024 ns per
  thread saved), then built (byte-identical): tg128 with mul_mat_vec_q probed 54.25 -> 56.1 t/s
  (20% -> 17.2-17.5%), four kernels 20.5% -> 18.1-18.2%, pp512 unchanged, counts exact.
- **Checked:** a one-value tool compares every bit at the exit (0 mismatches over 3.9M thread
  executions; with the halves swapped it reports every exit). An independent review found 14
  issues, 13 confirmed: one reachable from ordinary output (a 16-byte area with a writing probe
  would get two return values, which ptxas refuses; now one word only), the rest hypothetical
  inputs and two older fd and register-count bugs; all fixed, and one more line form of the same
  kind found while checking the fixes.

## 11-26: hash maps from GPU probes, the current state

The map model has only array-shaped maps; before designing hash maps, what a GPU probe gets today
was measured (results `gpu/hash-current-20261126`; a read-only survey first, rules before the runs).

- **What it is:** on the Orin every lookup, update and delete of a hash map from a GPU probe is a
  host call through one request slot and one CPU watcher thread; the GPU hash map type (1501) is
  created as an ordinary hash map here (its device implementation is x86-only).
- **Cost:** 10-13 us per operation back to back (any operation: it is the round trip), about
  18-21 us when calls are sparse, with or without CUDA graphs. On llama.cpp: rms_norm with one
  lookup per thread makes a 32-token prefill 341-351 times slower (the 11-13 census measured 307
  times, 10.8 us per call); one call per block on rms_norm costs 44%; one call per launch on the
  four kernels adds about 10 points on tg128 over the same probe without the hash call.
- **Correctness:** keys inserted beforehand or one writer per key count exactly; the examples'
  first-touch counter idiom loses every increment whose lookup missed before the first insert was
  served (thousands per key at one call per thread), and lookup-then-update (cuda-counter's sum)
  loses about half.

## 11-26: a device hash map, stage one designed and checked

A first stage of hash maps in the map model (results `gpu/hash-design-20261126` for the design,
`gpu/hash-round1-20261126` for the checks; nothing built in bpftime yet).

- **Design:** a device key directory (open addressing, slots claimed for good by a 64-bit CAS in
  the trampoline, a bound on reservations, E2BIG when full) and, for add-only u64 values, the sum
  map's (SM, lane) sharded rows. Lookups never wait; an insert that loses to the same key waits a
  bounded time, then returns EEXIST. Other value layouts (stores, deletes) designed, not decided.
- **Round one, rules before the runs:** the llvmbpf output of the hash examples shows their
  counter maps fit stage one (64-bit adds without fetch, NOEXIST inserts). Hand-written forms of
  the hit path on mul_mat_vec_q (tg128): 56.15 t/s with today's sum array, 53.65 with a directory
  hit, i.e. +0.034 ns per thread (a 1,024-slot directory); a called walk costs 0.016 more and a
  row fixed at patch time saves nothing, so the path goes inline. Registers unchanged (56).
- **Protocol stress (standalone CUDA, 10^6 threads x 10 launches):** claim, reservation bound
  and wait held over 3,900 trials; the wait is what prevents NULL after EEXIST. Open: the map
  reports E2BIG when half full, because threads reserve before their CAS (the order has to
  change), and the release publish could not be observed by the test.

## 11-27: the device hash map, round two

The user took the four open decisions as proposed:
- hash tools declare the device hash type;
- first-touch inserts keep bpf's semantics, with losses counted;
- loss counts go in a companion status array;
- R = min(max_entries, 512).

Round two then checked the changed insert path (results `gpu/hash-round2-20261127`; rules before
the runs).

- **Claim, then reserve:** only the thread that wins the CAS takes capacity, and a winner refused
  at capacity keeps its slot as "refused".
  - In 3,848 stress trials (10^6 threads x 10 launches) there were no violations. E2BIG was 0 in
    all 2,805 trials where the keys fit; round one's order gave 7,061-11,695 per trial.
  - Four broken controls each failed as required.
  - Limit: a refused winner keeps its slot. So a map given far more distinct keys than
    max_entries fills its directory, and misses then walk all of it. With one key per thread,
    10 launches took 1,573 ms against 6.4 ms.
- **Publish order:** in a message-passing test of 10^8 instances, the release publish with a
  fenced reader gave 0 weak outcomes, and dropping both halves gave 78,461.
- **Independent reading:** three reviewers had only the code and the PTX memory-model chapter.
  They made 11 findings and none was refuted. The one contract break, E2BIG while the key that
  took the last place is not yet visible, is the same window the kernel's htab has (checked in
  `kernel/bpf/hashtab.c`). It is documented rather than fixed, which sets aside the rules' "fix
  and check again"; the user chose that (11-27). The rest are documented. A code map of the
  bpftime build lists the items to settle first (CPU seal and index, sharded loss counts, one
  layout definition).

## 11-27 – 11-28: the device hash map, stage one built (S0-S6)

The stage-one plan (revision 4, three review rounds) built on the prototype branch
`proto/map-model-70f7bc1f` of bpftime (map type 1533; results `gpu/hash-build-20261127`, the plan
with what the build changed in `gpu/hash-build-plan-20261127`). Built: one layout header (S1), the
CPU side (S2), the GPU functions and the trampoline's branches (S3), the hash snapshot (S4), the
copy and the delivery in the agent (S5), the routing pass (S6). Not yet: tools and docs (S7),
measurement (S8).

- **Writer slots and the owner's claim** are open file description locks on bytes of the
  segment's file, which the kernel releases at exit, kill and exec in any pid namespace. Two
  reviews of the first versions (19 and 38 findings, all but one upheld) led there.
- **The agent (S5):** before init takes its map-binding lock, it claims each 1533 map, seals it
  and copies it to the GPU; after the application's syncs it delivers the GPU's contents to the
  CPU-side mirror and to a companion status array. 18 GPU test configurations, each in a process
  of its own, among them owner deaths at four points, takeovers and a real exit; 32 of 32
  mutations caught.
- **Found while testing:** the kernel merges one description's adjacent locks, so an owner on
  writer slot 255 read its claim (byte 256) as lost and stopped delivering (1 in 10 runs of the
  GPU suite); fixed and reproduced on purpose.
- **Cost:** the hand-routed hit path on llama.cpp's `mul_mat_vec_q` costs +0.036 to +0.038 ns per
  thread over today's sum array (S0.2); the sum array's own path is unchanged by S5 (+0.1267 ns per
  thread against +0.1273, sum exact).
- **Open:** an independent read of the final slots and claim code and of S5 (after 11-30).

## 11-28: the device hash map's tools and measurement (S7, S8)

Stage one of the device hash map (1533) is built and measured (results `gpu/hash-build-20261127`,
`s7/` and `s8/`).

- **Tools and docs (S7).**
  - The probe tools take the map: `pt_hash` with its companion, and `pt_loader` dumps by key.
  - A README for tool authors, and a CLAUDE.md section.
  - The first run of a real tool on a real program found the routing pass placing the hash
    functions after their callers. ptxas refused every routed module, which then ran unprobed.
    Fixed.
- **Measurement (S8),** with rules before the runs and three addenda before theirs:
  - Every count is exact.
  - Against the sum array (1531), on tg128 with every thread of `mul_mat_vec_q` probed: +0.035 to
    +0.040 ns per thread, as expected (53.2 against 55.7-56.0 tokens/s).
  - Against today's host path, a lookup and add per thread on rms_norm: 0.19-0.31 ns against
    10.3 us. The 32-token prefill runs 0.6-0.9% over its base instead of 316 times it.
  - One call per launch: within the noise.
  - Each delivery reads the whole directory, so the tools' default 262,144-entry map cost
    7.2-7.8 ms per delivery and 0.05-0.07 s per tg128 run. The README now advises sizing the map
    near its key count.
- **Measurement notes.**
  - The tool's own side counts (adds into a GPU array in host memory) at first hid the hash map's
    cost.
  - A review caught an addendum that blamed the routing pass for what was the large map's
    delivery.
- **Open:** an independent read of the final writer-slot and claim code and of S5 (after 11-30).

## Where it stands (11-18)

- uprobe map operations: all ten of the official benchmark at or below bpf on one thread
  (10-26 figures in [reports/bpftime-research-overview.md](https://github.com/plsy1/bpftime-benchmark/blob/benchmark-results/jetson/reports/bpftime-research-overview.md),
  section 9); hash maps no longer serialize threads.
- Real tools: 18 checked against bpf, counts exact; ssl-nginx ahead of bpf at every payload.
- System call tracing and CUDA attach work on ARM64 with the dev branch; GPU host calls: layer 1
  done but for several CUDA processes on one segment and lookup address translation, the watcher
  sleeps and parks by measured rules (checked with and without `jetson_clocks`).
- GPU on a real application (llama.cpp on the Orin, graphs off): probes on four kernels counted
  every thread exactly; the default per-thread capacity loses 99.6% of the counts. Start-up is
  14.4-14.8 s longer with the agent.
- GPU map model (the one research direction now): the minimal prototype works within documented
  guarantees (thread-local state, sharded sums, synchronization-ordered delivery): exact counts on
  llama.cpp, about 4% on pp512, 33% on tg128 where every thread of a very fine kernel pays about
  0.3 ns. Hand-written end shapes of the documented design bring that to about 0.11 ns (about 16%,
  with clz in the tool; 0.134 ns, about 18%, with the tool unchanged), 0.08 spliced: 5% on such kernels needs another tool shape or budget. With the tool unchanged the
  map's own part goes from about 0.196 ns to about 0.068, nearly all of it the add into the sum
  map; the add's documented design (a `red` on the global address, one test, 32-bit offsets)
  brings the map's part to about 0.014 and the shape to about 12% (59.6 t/s). The map side is
  now built (sum-map adds global, lookup keys forwarded): 0.147-0.149 ns per thread, about 20%
  on tg128 (54.5 t/s) and 21% with four kernels. Of that the map is 0.035 and the tool's and
  framework's parts 0.110-0.114; carrying the start by value saved 0.015-0.021 more in
  hand-written forms, and built on 11-26 0.025 ns (56.1 t/s, -17.2 to -17.5%). Not built:
  per-record output.
- Open: Gum's per-function usage counter under many threads; the agent's own load time (about
  9 ms); the CUDA late path (`bpftime attach` to a running GPU process; 5–9 µs per launch slower
  than official's); fixes not yet offered upstream.
