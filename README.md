# Jetson bpftime results

Results of bpftime on the Jetson Orin Nano 8 GB (aarch64, L4T R39.2.1, BTF kernel
6.8.12-1021-tegra-btf since 2026-10-26): raw data, summaries, the scripts that produced them and the
board state. Results from the x86 NUC are on
[`benchmark-results/x86-nuc`](https://github.com/plsy1/bpftime-benchmark/tree/benchmark-results/x86-nuc);
the earlier analysis reports are on
[`summry/jetson`](https://github.com/plsy1/bpftime-benchmark/tree/summry/jetson). Since late
October each round's conclusions are in its own README here.

## Branches

- Development: `dev/uprobe-official-4708f4092d1e` (official 4708f409) since 2026-11-10
  ([base-4708f409-20261110](uprobe/base-4708f409-20261110/)). Before it
  `dev/uprobe-official-177a1dc465ff`, archived as `archive/uprobe-official-177a1dc465ff`; before
  that `archive/uprobe-official-8da867d541f8`. Both archives hold commits of the daily list: keep
  them.
- Daily: `automation/jetson-daily` replays the list onto each new official commit as
  `dev/uprobe-official-<SHA>` and runs the official uprobe benchmark
  ([official-daily](uprobe/official-daily/)).
- Results directories record the branch names of their day (`codex/` until 2026-10-26, the
  177a1dc465ff dev branch until 2026-11-10); the commit ids are unchanged.

## Removed on 2026-11-10

Results from before 2026-10-19 were removed from this branch: the July ssl-nginx ablations
(`empty-probe`, `full-aligned`, `full-unaligned`, `no-copy_no-output`, `no-output`,
`v4-ablation`), the August and September map path studies and official benchmark reproductions,
and the October 11-18 hash and per-CPU A/B rounds (141 directories and 60 loose logs and notes). They are at the
tag [`jetson-results-before-cleanup-20261110`](https://github.com/plsy1/bpftime-benchmark/tree/jetson-results-before-cleanup-20261110),
as is this README's previous index. Kept from before 10-19, because later results build on them:
the 10-07 official benchmark reproduction and the 10-18 lookup follow-ups.

## Index

The reviews: [reviews-20261031](reviews-20261031/README.md),
[reviews-20261105](reviews-20261105/README.md) (with its reports `report-*.md`).

### GPU (CUDA attach on the Orin Nano)

| results | what |
|---|---|
| [feasibility-20261109](gpu/feasibility-20261109/) | bpftime CUDA attach on the Orin Nano (first look) |
| [examples-20261109](gpu/examples-20261109/) | bpftime CUDA attach on the Orin Nano: example runs, measurements, problems |
| [fixes-20261109](gpu/fixes-20261109/) | CUDA attach on the Orin Nano, the fix round |

### System call tracing (aarch64 rewriting)

| results | what |
|---|---|
| [arm64-trampoline-20261028](syscall/arm64-trampoline-20261028/) | aarch64 system call rewriting: end-to-end check (2026-10-28) |
| [breakdown-20261029](syscall/breakdown-20261029/) | Where bpftime's per-syscall cost goes (ARM64, 2026-10-29) |
| [dispatch-fix-20261029](syscall/dispatch-fix-20261029/) | System call dispatch fix: remeasured (2026-10-29, Jetson) |
| [official-syscall-20261029](syscall/official-syscall-20261029/) | Official benchmark/syscall on ARM64 (2026-10-29) |
| [syscount-nginx-20261029](syscall/syscount-nginx-20261029/) | Official syscount-nginx on the Jetson (2026-10-29) |
| [coverage-20261030](syscall/coverage-20261030/) | aarch64 system call rewriting: what it misses, and the fixes (2026-10-30) |
| [fuse-20261030](syscall/fuse-20261030/) | fuse benchmark on the Jetson (2026-10-30) |
| [realapps-20261102](syscall/realapps-20261102/) | System call rewriting on real runtimes: Go, Node, Java (2026-11-02) |

### Kernel and board

| results | what |
|---|---|
| [crash-20261024](kernel/crash-20261024/) | Kernel panic on 2026-10-24 ~05:50 JST (stock 6.8.12-1021-tegra) |
| [memtest-20261025](kernel/memtest-20261025/) | Memory test after the 2026-10-24 kernel panic (2026-10-25) |
| [l4t-r39.2.1-btf](kernel/l4t-r39.2.1-btf/) | Building and booting the L4T R39.2.1 kernel with BTF |

### Reviews

| results | what |
|---|---|
| [rereview](reviews-20261031/rereview/) | Second round of independent reviews (2026-10-31, dev f2d6ae7d) |
| [rereview2](reviews-20261031/rereview2/) | Third review: the second round's fixes (2026-10-31, dev 9aeb2863) |
| [rereview3](reviews-20261031/rereview3/) | Fourth review: the third round's fixes, with a severity bar (2026-11-01, dev 6ab9cbfe) |
| [fixes](reviews-20261031/fixes/) | Fixes for the review findings (2026-10-31) |
| [refixes](reviews-20261031/refixes/) | Fixes for the second review's findings (2026-10-31) |
| [refixes2](reviews-20261031/refixes2/) | Fixes for the third review's findings (2026-10-31) |
| [experiments](reviews-20261031/experiments/) | Experiments of the reviews |
| [fixes](reviews-20261105/fixes/) | Fixes for the fifth review and problem 13 (2026-11-05, dev 94f9ba8f -> d46ceb8f) |
| [experiments](reviews-20261105/experiments/) | Experiments of the fifth review |

### uprobe: array and hash lookups, array JIT (10-18 to 10-21)

| results | what |
|---|---|
| [uprobe-top-arm64-20261007-strict-repro-official-no-btf](uprobe/uprobe-top-arm64-20261007-strict-repro-official-no-btf/) | Official uprobe benchmark, strict 10-round reproduction (2026-10-07): the baseline of the array JIT rounds |
| [hash-lookup-optimization-20261018.md](uprobe/hash-lookup-optimization-20261018.md) | Accepted ARM64 hash lookup optimizations — 2026-10-18 |
| [ordinary-hash-loop-20261018](uprobe/ordinary-hash-loop-20261018/) | Ordinary hash lookup C++ implementation experiments |
| [percpu-hash-lookup-followup-20261018](uprobe/percpu-hash-lookup-followup-20261018/) | Per-CPU hash lookup follow-up |
| [percpu-hash-flatten-full10-arm64-20261018](uprobe/percpu-hash-flatten-full10-arm64-20261018/) | Per-CPU hash flatten: full ten rounds (2026-10-18) |
| [percpu-array-lookup-followup-20261018](uprobe/percpu-array-lookup-followup-20261018/) | Per-CPU array lookup follow-up |
| [ordinary-array-lookup-cold-20261019](uprobe/ordinary-array-lookup-cold-20261019/) | Ordinary array lookup cold-error-path experiment |
| [percpu-array-lookup-cold-20261019](uprobe/percpu-array-lookup-cold-20261019/) | Per-CPU array lookup: cold-error-path-only experiment |
| [percpu-array-address-dependency-20261019](uprobe/percpu-array-address-dependency-20261019/) | Per-CPU array lookup: address-dependency experiment |
| [array-lookup-load-order-20261019](uprobe/array-lookup-load-order-20261019/) | Ordinary array lookup load-order candidate |
| [array-lookup-reprofile-20261019](uprobe/array-lookup-reprofile-20261019/) | Accepted array lookup reprofile |
| [array-offset-branch-20261019](uprobe/array-offset-branch-20261019/) | Local array implementation offset-pointer decoding experiment |
| [array-jit-inline-poc-20261019](uprobe/array-jit-inline-poc-20261019/) | Ordinary array lookup JIT inline POC |
| [array-jit-inline-integrated-20261020](uprobe/array-jit-inline-integrated-20261020/) | Ordinary-array JIT integration and lifetime validation |
| [percpu-array-jit-inline-20261020](uprobe/percpu-array-jit-inline-20261020/) | Per-CPU array lookup JIT specialization |
| [percpu-array-jit-direct-cpu-20261020](uprobe/percpu-array-jit-direct-cpu-20261020/) | Direct CPU query in the inlined per-CPU array lookup |
| [percpu-array-jit-cpu-abi-20261020](uprobe/percpu-array-jit-cpu-abi-20261020/) | Zero-argument CPU alias: rejected |
| [array-jit-default-20261021](uprobe/array-jit-default-20261021/) | Automatic per-call-site array lookup JIT |
| [array-jit-default-delete-fix-20261021](uprobe/array-jit-default-delete-fix-20261021/) | Default-on array JIT: formal delete regression repair |
| [array-jit-global-data-20261021](uprobe/array-jit-global-data-20261021/) | Array JIT: libbpf global data in bound programs (2026-10-21) |
| [jit-eligibility-survey-20261021](uprobe/jit-eligibility-survey-20261021/) | Array JIT eligibility survey (2026-10-21) |
| [consuming-official-vs-optimized-20261021](uprobe/consuming-official-vs-optimized-20261021/) | Consuming array lookup: unpatched official vs optimized (2026-10-21) |
| [baseline-177a1dc-jit-llvm16-20261021](uprobe/baseline-177a1dc-jit-llvm16-20261021/) | 177a1dc development baseline (array JIT, LLVM 16) |
| [daily-jit-stack-validation-20261021](uprobe/daily-jit-stack-validation-20261021/) | Daily patch stack + array JIT replay validation (2026-10-21) |
| [hash-lookup-helper-gap-20261021](uprobe/hash-lookup-helper-gap-20261021/) | Hash lookup helper gap (2026-10-21) |

### uprobe: hash helpers (10-22, 10-23)

| results | what |
|---|---|
| [hash-gap-history-20261022](uprobe/hash-gap-history-20261022/) | Hash helper gap across the optimization history (2026-10-22) |
| [hash-shape-20261022](uprobe/hash-shape-20261022/) | Hash lookup shape: key order, fill, value use and misses (2026-10-22) |
| [hash-mutate-20261022](uprobe/hash-mutate-20261022/) | Hash update/delete in three program shapes (2026-10-22) |
| [hash-probe-bound-20261022](uprobe/hash-probe-bound-20261022/) | Bounded hash probing (1cb20807, 2026-10-22) |
| [hash-bound-shrink-20261022](uprobe/hash-bound-shrink-20261022/) | Shrinking hash probe bound (af10949b, 2026-10-22) |
| [hash-helper-fixes-20261022](uprobe/hash-helper-fixes-20261022/) | Hash helper fixes (started 2026-10-22; fixes done, formal validation pending) |
| [direct-map-helpers-20261022](uprobe/direct-map-helpers-20261022/) | Direct original map helpers in bound programs (f26cc335, 2026-10-22) |
| [hash-typed-delete-20261023](uprobe/hash-typed-delete-20261023/) | Typed hash delete helper (72aebadd, 2026-10-23) |
| [fork-child-binding-20261023](uprobe/fork-child-binding-20261023/) | a90fa447: fork children execute inherited array JIT programs (2026-10-23) |

### uprobe: ssl-nginx, perf output, credentials (10-23, 10-24)

| results | what |
|---|---|
| [ssl-nginx-2x2-20261023](uprobe/ssl-nginx-2x2-20261023/) | ssl-nginx: does the map optimization show up end to end? (2026-10-23) |
| [ssl-nginx-netns-recheck-20261023](uprobe/ssl-nginx-netns-recheck-20261023/) | Re-check: does the host network namespace (Tailscale/netfilter) cost bpftime extra? (2026-10-23) |
| [perf-lost-wakeup-20261023](uprobe/perf-lost-wakeup-20261023/) | db3d475f: report dropped perf samples and wake the consumer before the ring fills (2026-10-23) |
| [perf-output-no-affinity-20261023](uprobe/perf-output-no-affinity-20261023/) | Software perf output without CPU affinity (2026-10-23) |
| [perf-output-identity-20261024](uprobe/perf-output-identity-20261024/) | bca2ebce: cache the producer pid and tid in software perf output (2026-10-24) |
| [uid-gid-cache-20261024](uprobe/uid-gid-cache-20261024/) | 7ed339ad: cache bpf_get_current_uid_gid, invalidate on credential changes (2026-10-24) |
| [ssl-nginx-ablation-20261024](uprobe/ssl-nginx-ablation-20261024/) | sslsniff path ablation on db3d475f, fair scope (2026-10-24) |
| [ssl-nginx-ablation2-20261024](uprobe/ssl-nginx-ablation2-20261024/) | Path ablation on 7ed339ad, stage 1 split and perf profile (2026-10-24) |
| [ssl-nginx-copysize-20261024](uprobe/ssl-nginx-copysize-20261024/) | Why bpftime's lead over kernel narrows with payload size (2026-10-24) |
| [ssl-nginx-sweep-bca2ebce-20261024](uprobe/ssl-nginx-sweep-bca2ebce-20261024/) | Fair payload sweep with bca2ebce (2026-10-24 20:31-21:01 JST) |

### uprobe: real programs, BTF kernel, DeepFlow (10-25 to 10-27)

| results | what |
|---|---|
| [realprog-audit-20261025](uprobe/realprog-audit-20261025/) | Real-program functional audit (2026-10-25) |
| [realprog-measure-20261025](uprobe/realprog-measure-20261025/) | Real-tool throughput: no tracing / kernel / official / bca2ebce (2026-10-25) |
| [realprog-fixes-20261025](uprobe/realprog-fixes-20261025/) | Fixes for the real-program audit failures (2026-10-25) |
| [realprog-next-20261025](uprobe/realprog-next-20261025/) | Frame-pointer stacks, stack trace map, cgroup arrays (2026-10-25) |
| [realprog-rest-20261026](uprobe/realprog-rest-20261026/) | Remaining bpftime issues from the real-program audit (2026-10-26) |
| [btf-kernel-20261026](uprobe/btf-kernel-20261026/) | BTF kernel: CO-RE real-tool audit and kernel baselines (2026-10-26) |
| [nuc-fixes-20261026](uprobe/nuc-fixes-20261026/) | Fixes for the x86 NUC findings (2026-10-26) |
| [deepflow-btf-20261026](uprobe/deepflow-btf-20261026/) | DeepFlow on ARM64: kernel vs bpftime daemon mode (2026-10-26/27) |
| [map-usage-20261027](uprobe/map-usage-20261027/) | Runtime map usage of real tools on bpftime (2026-10-27/28) |

### uprobe: maps under threads (10-28)

| results | what |
|---|---|
| [dimension-walkthrough-zh.md](uprobe/dimension-walkthrough-zh.md) | 多维度对比：逐项说明（2026-10-28） |
| [dimension-plan.md](uprobe/dimension-plan.md) | Dimension coverage: consolidated problems and improvement plan (draft, 2026-10-28) |
| [dimension-problems.md](uprobe/dimension-problems.md) | Dimension coverage: problems found (running list) |
| [map-threads-20261028](uprobe/map-threads-20261028/) | Entry/exit churn on 1-6 threads: kernel vs bpftime (2026-10-28) |
| [map-table-20261028](uprobe/map-table-20261028/) | Map tables under threads: tracking table and shared counters (2026-10-28) |
| [map-scratch-20261028](uprobe/map-scratch-20261028/) | Large per-CPU scratch value under threads (2026-10-28) |
| [map-torn-20261028](uprobe/map-torn-20261028/) | Torn reads through a hash lookup pointer (2026-10-28) |
| [percpu-unpinned-20261028](uprobe/percpu-unpinned-20261028/) | Per-CPU maps with unpinned threads lose counts (2026-10-28) |
| [percpu-hash-fix-20261028](uprobe/percpu-hash-fix-20261028/) | Per-CPU hash fixes: shared counters before and after (2026-10-28) |
| [dead-holder-20261028](uprobe/dead-holder-20261028/) | Killing a traced process can leave a map lock held forever (2026-10-28) |
| [probe-path-profile-20261028](uprobe/probe-path-profile-20261028/) | Why bpftime's probe path slows down with threads (2026-10-28) |

### uprobe: multi-thread hash maps and their dev runs (10-30 to 11-04)

| results | what |
|---|---|
| [hash-lockfree-lookup-20261030](uprobe/hash-lockfree-lookup-20261030/) | Step 1 of the multi-thread hash map plan: lookups take no lock (2026-10-30) |
| [hash-striped-writers-20261030](uprobe/hash-striped-writers-20261030/) | Step 2 of the multi-thread hash map plan: writers lock stripes (2026-10-30) |
| [dev-6ab9cbfe-20261101](uprobe/dev-6ab9cbfe-20261101/) | uprobe suite and ssl-nginx on dev 6ab9cbfe (2026-11-01 03:58-05:07 JST) |
| [dev-68f19c1f-20261102](uprobe/dev-68f19c1f-20261102/) | uprobe suite and ssl-nginx on dev 68f19c1f (2026-11-02 04:15-05:34 JST) |
| [dev-a4ac0d31-20261102](uprobe/dev-a4ac0d31-20261102/) | uprobe suite and ssl-nginx on dev a4ac0d31 (2026-11-02 18:47-20:38 JST) |
| [perf-lost-audit-20261102](uprobe/perf-lost-audit-20261102/) | Perf event loss re-audit on the Jetson (2026-11-02) |
| [percpu-hash-lockfree-20261102](uprobe/percpu-hash-lockfree-20261102/) | Per-CPU hash without the map lock (2026-11-02) |
| [hash-single-thread-20261102](uprobe/hash-single-thread-20261102/) | Hash maps' single-thread cost, phase 1 (2026-11-02) |
| [hash-redesign-survey-20261102](uprobe/hash-redesign-survey-20261102/) | Hash map redesign: survey (2026-11-02) |
| [hash-keypattern-20261103](uprobe/hash-keypattern-20261103/) | Hash map churn by key pattern, capacity and load, through BPF programs (2026-11-03 00:13-00:47) |
| [dev-e5ba4aa7-20261103](uprobe/dev-e5ba4aa7-20261103/) | Hash map step 2 (dev e5ba4aa7): elements in u32 chains, per-CPU free lists (2026-11-03 04:12-06:07) |
| [dev-b7ded27a-20261103](uprobe/dev-b7ded27a-20261103/) | Review fixes for hash map step 2 (dev 199e23ec, 48767d88, b7ded27a), 2026-11-03 16:20-17:00 |
| [dev-518a6b99-20261103](uprobe/dev-518a6b99-20261103/) | Hash map: updates of existing keys take a spare element (dev 518a6b99), 2026-11-03 17:30-18:35 |
| [dev-7ad5a152-20261104](uprobe/dev-7ad5a152-20261104/) | Hash map: updates in place, lookups re-check their link (dev 7ad5a152), 2026-11-04 00:20-01:25 |

### uprobe: real tools and the problems they found (11-04, 11-05)

| results | what |
|---|---|
| [detach-crash-20261104](uprobe/detach-crash-20261104/) | Problem 7: detach crashes a thread entering a probed function (2026-11-04) |
| [memleak-fix-20261104](uprobe/memleak-fix-20261104/) | memleak deadlock fix, dev ff5515d8 (2026-11-04) |
| [stackmap-lockfree-20261104](uprobe/stackmap-lockfree-20261104/) | Stack trace map found without the lock, dev efe08cbd (2026-11-04) |
| [vs-pre-mt-20261104](uprobe/vs-pre-mt-20261104/) | Real tools before and after the multi-thread map work (2026-11-04) |
| [realtools-20261104](uprobe/realtools-20261104/) | Real tools end to end, dev 7ad5a152 (2026-11-04) |
| [harness-gaps-20261104](uprobe/harness-gaps-20261104/) | Harness gaps closed, four new problems (2026-11-04) |
| [opensnoop-loss-20261105](uprobe/opensnoop-loss-20261105/) | opensnoop delivers half the opens under bpftime: ring buffer, no wakeup |
| [ringbuf-fd-20261105](uprobe/ringbuf-fd-20261105/) | Problem 9 fixed: ringbuf helpers find the map again, dev 6a15a271 (2026-11-05) |
| [rodata-dead-code-20261105](uprobe/rodata-dead-code-20261105/) | Dead code by frozen read-only constants, dev 94f9ba8f (2026-11-05) |
| [exit-log-poison-20261105](uprobe/exit-log-poison-20261105/) | Problems 10 and 11 (2026-11-05) |

### uprobe: dimensions, improvements, start-up (11-06 to 11-10)

| results | what |
|---|---|
| [output-dims-20261106](uprobe/output-dims-20261106/) | Event output by dimension: kernel bpf vs bpftime (2026-11-05/06) |
| [more-dims-20261106](uprobe/more-dims-20261106/) | More dimensions: what the earlier rounds did not measure (2026-11-06) |
| [improve-20261106](uprobe/improve-20261106/) | Improvements from the 11-06 measurements (2026-11-06) |
| [remeasure-20261108](uprobe/remeasure-20261108/) | Where bpftime is still slower than bpf (2026-11-08) |
| [startup-20261108](uprobe/startup-20261108/) | the AOT cache (traced processes link precompiled programs) |
| [frida-detach-crash-20261108](uprobe/frida-detach-crash-20261108/) | Frida detach tests crash intermittently (2026-11-08/09) |
| [base-4708f409-20261110](uprobe/base-4708f409-20261110/) | the development base moves to official 4708f409 |

### Daily

| results | what |
|---|---|
| [official-daily](uprobe/official-daily/) | The daily official-benchmark runs (history.jsonl, latest.json, one directory per official SHA) |
