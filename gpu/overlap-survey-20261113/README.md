# How much gpu_ext and upstream bpftime overlap with directions 1 and 2 (2026-11-13)

Question: before designing direction 2 (per-call-site execution of GPU helper calls) and direction 1
(GPU maps sized by thread index) of `RESEARCH_DIRECTIONS.md` (main), what do gpu_ext (paper arXiv
2512.12615, repo eunomia-bpf/gpu_ext) and upstream bpftime (eunomia-bpf/bpftime) already cover?

## Method (`scripts/workflow.js`, output `workflow-output.json`)

Three readers, one per source (the paper, read in full from the arXiv HTML and PDF, v1 and v2; the
gpu_ext repository, blobless, `git fetch` on 2026-11-13; upstream bpftime, `git fetch official` on
2026-11-13, master and all `official/revision/*` branches, PRs and issues through `gh`), each
followed by a checker that re-opened every cited source. 56 findings checked: 38 confirmed, 18
corrected, none struck; the checkers added items. Where a checker corrected a finding, the
correction holds. Approaches compared, from direction 1: size at launch, a shared buffer reserved
per warp, slots by hardware position, cheap fixes; from direction 2: automatic per-warp or
per-call-site execution.

## gpu_ext's status

- Paper "Safe and Programmable GPU Resource Management with eBPF" (arXiv title "gpu_ext:
  Extensible OS Policies for GPUs via eBPF", v2 2025-12-20), rejected at OSDI '26 (spring cycle),
  reframed around a unified execution model ("asynchronous resource state machines with
  BPF-directed lifecycle transitions") and accepted at ASPLOS '27 (paper repository
  yunwei37/gbpf-paper 364290f, 2026-10-11: ASPLOS '27 camera-ready, DOI 10.1145/3845814.3850045;
  `resubmission-changes.tex`). Main contribution: eBPF struct_ops hooks in the NVIDIA open kernel
  modules (eviction, prefetch, scheduling). The device side runs on bpftime.
- gpu_ext master has no commits after 50a51ad (2026-09-24).

## Direction 1: nobody sizes per-thread storage by the grid

| approach | paper | gpu_ext repository | upstream bpftime |
|---|---|---|---|
| size at launch | no (the word "grid" does not occur) | no: runners compute thread counts offline and set `BPFTIME_MAP_GPU_THREAD_COUNT` by hand; still lost data (a pp32 preflight dropped 450,560 events out of range) | no, on master and every branch |
| shared buffer reserved per warp | no (warp-leader execution, not a reserved buffer) | hand-written in one tool: one 1503 map value with 16,384 warps x 44 events (about 22 MiB), constants fitted to the pp512 run; the notes say no runtime shared reservation exists | no |
| slots by hardware position | per-warp / per-SM shards in prose only, no sizes, not evaluated | two hand-written uses | helpers 508-511 (merged #523, 2025-12-08) and two hand-written examples |
| cheap fixes | no | - | only on the unmerged revision chain (thread-count validation for 1502/1527, cap 1,048,576; drop counters by reason for 1527); 1512 never covered; no launch-time warning anywhere |

The paper shows the per-thread array and the GPU ringbuf only as two bars of Fig. 12(a) (per-op
latency, -78% and -69% against eGPU-style injection); nothing on capacity, overflow or memory.
Master today: the per-thread array prints and then writes past the map; the ring silently returns
for threads past the count and halves its thread count on allocation failure (a911ee68,
2026-01-02).

## Direction 2: upstream's automatic warp execution is per program, opt-in, unmerged

- `official/revision/automatic-warp-execution` (2026-09, LinuxDev9002, no PR; needs the SIMT
  verifier of open PR #624; env `BPFTIME_GPU_AUTO_WARP_EXECUTION`): a yes/no decision per
  program. A program is admitted only if every side effect is idempotent across the warp (stores
  of warp-uniform values to shared GPU maps 1501/1503/1504/1513); any atomic, `perf_event_output`,
  `trace_printk`, `puts`, `cuda_exit` or write to per-thread maps rejects it. Admitted programs run
  the handler in one leader lane (activemask + vote.ballot). No aggregation, no deferral, no
  analysis of result use or commutativity, no per-call-site choice. kernelretsnoop is not eligible;
  on a microbenchmark it was 57% slower or neutral although handler calls fell 32x. The checker
  read, without running it, that a non-atomic `*val = *val + 1` on a shared map would pass
  eligibility, so leader-only execution would undercount.
- The paper describes warp-leader execution (ballot/shuffle, broadcast) and hierarchical shards
  merged at kernel completion with "relaxed, eventual consistency" as implemented. gpu_ext's own
  audit (2026-09-04) found no such implementation; a future-work rewording (54e15e2) was reverted
  (11c744d, 2026-09-05), so the camera-ready text keeps the claim. The low-overhead tool results
  come from hand-written tools (warp-leader `if`, GPU map types, workload-fitted sizes).
- Fig. 12(b)'s "6000x" CPU-map cost times the serialized host-RPC path with official's 1 ms
  watcher sleep (about 34 ms for 32 calls), not a GPU-side operation.

## Also found

- Upstream master (42cf71c4, 2026-11-12) still copies a host call's key/value into the shared
  request before taking the lock (lanes overwrite each other's arguments; fixed in dev 518f05b5,
  and on the unmerged 39d09919).
- After 2026-11-11 the only related upstream change is #740 (an example's shared counter made
  atomic after lockstep lanes lost 9 of 10 updates).

## Limits

The arXiv-to-camera-ready section mapping was not checked; gpu_ext raw logs and git-lfs payloads
were not read; line numbers that gpu_ext notes quote for the authors' local bpftime trees were
not verified.
