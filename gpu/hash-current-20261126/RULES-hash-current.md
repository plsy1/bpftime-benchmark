# Hash maps from GPU probes: the current state (written before the passes, 2026-11-26)

User 11-26: start the hash map, measure the current state first. Survey
(read only): on the Orin every lookup, update and delete of a hash map
(HASH, and GPU_HASH_MAP 1501, which the syscall server creates as HASH on an
integrated GPU) from a GPU probe is a host call: one request slot per
process, one watcher thread, lanes of a warp in turn. The 1501 device
implementation runs only with CUDA IPC (x86). PERCPU_HASH and LRU_HASH from
the GPU are described from code only, not measured here. No fixes this
round, including the example bugs the survey found.

Build: proto/builds/tvf-7d02d677 (dev 70f7bc1f + the map-model prototype;
the hash path is dev's: the routing pass does not run on these tools, as
they hold no 1530/1531/1532 map; asserted from the agent log for every
item). Watcher: AC3 (default), WFE off. Tool: proto/tools/pt_hash.bpf.c
(build_hs.sh). Loader: pt_loader with PT_PREFILL and hash entry counts.

## pt_victim matrix (run_hash.sh, passes h1-h3)

Sizes: D = "pt_victim 1 0" (pt_kdiv once, direct launch, 391 blocks x 256 =
100,096 threads); G = "pt_victim 0 2" (pt_kflat through a CUDA graph, 4
launches x 300 blocks x 48 = 57,600 threads). Every item on both sizes;
items shuffled per pass; clocks checked before each item.

| item | what each selected thread does | host calls |
|---|---|---|
| empty | probes return at once (zero point) | 0 |
| pre | lookup of a key the loader inserted, atomic add 1 | 1 / thread |
| miss | lookup of a key never inserted | 1 / thread |
| upd | update BPF_ANY of an inserted key | 1 / thread |
| del | delete of a key never inserted | 1 / thread |
| tid | entry update(thread key, now); exit lookup, delete | 3 / thread |
| tidnd | tid without the delete | 2 / thread |
| cnt-t | the examples' counter idiom (lookup; add, or insert 1 NOEXIST), one key per kernel | 1-2 / thread |
| cnt-t-sm | the same, key per SM | 1-2 / thread |
| cnt-b-blk | the idiom by thread 0 of each block, key per block | 1-2 / block |
| cnt-b | the idiom by thread 0 of each block, one key per kernel | 1-2 / block |
| cnt-l | the idiom by the leader thread of each launch | 1-2 / launch |
| rmw | lookup, atomic add through the pointer, update BPF_EXIST with that pointer as the value | 2 / thread |

Quantities:
- per call (us) = (client_ms(item) - client_ms(empty, same size and pass)) /
  host calls; per kernel size (D: direct launches, G: graph launches).
  Lookup hit = pre, miss = miss, update = upd, delete of a missing key = del,
  delete of a present key = tid - tidnd.
- correctness, from chk (device adds, exact) and the map:
  - pre: map sum = probe runs (chk 4) and no NULL: expected exact;
  - tid: found = runs, missing 0, delete failures 0, entries left 0;
    tidnd: entries left = threads per launch for the kernel's keys;
  - cnt-*: loss = runs - map sum, insert failures from chk 3: expected > 0
    for cnt-t and cnt-t-sm (first touch: a warp's lookups all miss before
    its inserts), 0 for cnt-b-blk (one thread per key), small or 0 for cnt-b
    and cnt-l; reported as numbers, no pass/fail;
  - rmw: loss = runs - map sum: expected > 0 (adds through a pointer to an
    element the update replaced), reported.
- Third pass is part of the plan (h3); per-call spreads over 20% between
  passes are reported, not re-run.

## llama.cpp (run_hash_llama.sh, passes l1/l2)

llama-bench TinyLlama Q4_K_M, -r 5 with warm-up, CUDA graphs as this
llama.cpp build does by default (GGML_CUDA_GRAPHS=ON; the map-model passes
ran the same way; the 11-13 census ran with graphs off).
| item | probe | workload |
|---|---|---|
| base | none | p32 (-p 32 -n 0), tg128, pp512 |
| link | pre on rms_norm's exit, every thread, key 0 inserted (the 11-13 census shape: 10.8 us per call, 307x on a 32-token prefill) | p32 |
| per block | cnt-b on rms_norm | p32 |
| per launch | cnt-l on the four kernels | tg128, pp512 |
Per call (us) = (s per run - base s per run) / (calls per run), calls from
chk 4 over 6 runs (warm-up + 5). Per-thread rates on tg128 (3.14e9
threads per run on mul_mat_vec_q alone) are computed from the per-call
cost, labelled computed, not measured.

## Addendum: per call by slope; passes s1/s2 (written after h1-h3, before s1/s2)

h1-h3 showed a fixed start-up difference between the hash tools and the
empty probes: cnt-l (2 host calls on D, 5 on G) took 769-791 ms against
417-438 (about 350 ms), so "(ms - empty) / calls" overstates the per-call
cost of the per-thread items by about 350 ms / calls (D: about 3.5 us) and
says nothing for the low-rate items. Per call is now taken by slope: the
same tool on a larger size, D4 = "pt_victim 4 0" (4 direct launches,
400,384 threads) and G8 = "pt_victim 0 8" (16 graph launches, 230,400
threads), against D and G of h1-h3 (median ms): per call = (ms(D4) -
ms(D)) / (calls(D4) - calls(D)), the same for G8 against G. Items: empty,
pre, miss, upd, del, tid, tidnd (empty for reference: its slope is the
kernels' own time). Correctness checks as before (tidnd: entries left =
threads per launch, the keys repeat across launches). Two passes; a slope
that differs by more than 10% between passes is reported, not re-run.

## Addendum: the per-launch zero point and graphs; passes l3/l4 (written after l1/l2, before l3/l4)

l1/l2's per-launch item (four kernels, one thread per launch) was taken
against the run without the agent, but its rate filter (thread and block
index helpers, then the probe-run count) runs on every thread of the four
kernels: 3.1e9 threads per tg128 run on mul_mat_vec_q alone. So l1/l2's
26.5-26.8% and "about 44 us per call" are that tool's whole cost, not the
host calls'. New variant hs_none_leader_kc_llama4 (`ldrf`): the same
filter and count, no hash operation. Per call = (s per run(ldr) - s per
run(ldrf)) / calls per run, both from the same pass. Graphs: the same
three items on tg128 with GGML_CUDA_DISABLE_GRAPHS=1 (`-ng`), to see
whether the sparse-call cost depends on graph launches; one setting each
per pass, no claim from a single run. Items: tg:base tg:ldr tg:ldrf
tg:base-ng tg:ldr-ng tg:ldrf-ng pp:base pp:ldr pp:ldrf. Counts: ldr exact
(hash sum = probe runs); ldrf hash map empty. Two passes.
