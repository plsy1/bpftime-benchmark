# S8: measurement and correctness against today (2026-11-28)

Rules: `../S8-RULES.md` (written before the runs; one addendum after A and C, before its runs).
Build: the worktree's build-cuda at a3db63689 (proto/map-model-70f7bc1f: S1-S7 and the delivery's
per-hash-map exit line), agent and server for every item. Clocks checked by the harnesses before
every item (cpu5 1728000, GPU 1020 MHz); GPU lock; bounded runs.

## Summary

- **Correct.** Every count checked is exact, on pt_victim (228 runs) and on llama.cpp:
  - a 1533 map's values equal the operations that succeeded;
  - every lost first touch is one EEXIST in the companion.
- **Against the sum array (1531),** on llama.cpp tg128 with every thread of `mul_mat_vec_q` probed:
  - +0.035 to +0.040 ns per thread, S0.2's expected cost: 53.2 tokens/s against 55.7-56.0
    (67.6 without the agent);
  - the build's routing emits what S0.2 timed (difference 0.0003 ns or less).
- **Against today's host path,** one lookup and add per thread on rms_norm:
  - 0.8-0.9 ns per call against 10.3 µs;
  - the 32-token prefill runs 2.4-2.7% over its base instead of 316 times its base.
- **Sparse use** (one call per launch): the hash operations are within the noise. The per-launch
  counter's cost is the routing pass's change to every thread's path (0.05-0.07 s per tg128 run).
  That holds for every device-model map.
- **Delivery:**
  - 0.3-0.5 ms per delivery at 512-1,024 rows with the GPU free;
  - in-process deliveries wait for the application's kernels;
  - a 262,144-entry map takes 7.8 ms per delivery (the CPU passes over all 524,288 slots), above
    the 2 ms rule: a cost of the schedule for large maps;
  - the first delivery sets up its module once (0.35-0.62 s).
- **Measurement notes:** pt_hash's side counts (adds to a GPU array in registered host memory)
  dominated the first 1533 numbers on pt_victim and in dlink. The two addenda measured without
  them.

## A. pt_victim: 1533 against the host path (passes v1-v3; 180 runs, all valid)

**Counts.** Every 1533 check holds in every pass and size:
- pre: each value = the probe runs;
- miss: the map stays empty;
- the counter idiom: sum = runs - EEXIST - E2BIG, and the companion's EEXIST + E2BIG = the tool's
  own failed inserts.

The counter idiom's first-touch losses (D: pt_kdiv once; G: pt_kflat through a graph):

| item | size | runs | host path lost (v1 / v2 / v3) | 1533 lost (= EEXIST, every pass) |
|---|---|---:|---|---:|
| cnt-t (every thread, one key per kernel) | D | 100,096 | 4,836 / 3,179 / 4,109 | 8,191 |
| cnt-t | G | 57,600 | 2,578 / 1,952 / 2,271 | 6,143 |
| cnt-t-sm (key per SM) | D | 100,096 | 5,690 / 5,492 / 5,285 | 8,184 |
| cnt-b-blk (thread 0 of each block, key per block) | D | 391 | 0 / 0 / 0 | 0 |
| cnt-b (thread 0 of each block, one key) | D | 391 | 3 / 10 / 4 | 31 |
| cnt-b | G | 1,200 | 16 / 13 / 19 | 127 / 127 / 101 |
| cnt-l (leader of each launch) | D, G | 1, 4 | 0 | 0 |

The device path loses more first touches than the host path: all threads of the first waves
look up before any insert is published, while host calls are served one at a time. The losses
are exact and counted: every lost increment is one EEXIST in the companion.

**Time per call by slope, as A measured it** (D4 against D, µs per call; three passes):

| item | host path | 1533 |
|---|---:|---:|
| pre (lookup + add) | 12.70-12.91 | 1.03-1.12 |
| miss | 11.93-15.82 | 0.97-1.06 |
| cnt-t | 12.53-13.03 | 0.86-1.20 |

Both columns include the tools' side counts: every thread adds twice to `chk`, a GPU array in
registered host memory, on a few shared words. For 1533 that is nearly all of it: even a lookup
of a missing key, with no atomic in the hash path, cost about 1 µs, and each extra launch of
pt_kdiv about 112 ms. Hence the addendum.

## Addendum: without the side counts (passes n1, n2; 44 runs, all valid; `-DHS_NOCHK`)

Counts from the map alone, all exact:
- pre: value = threads x launches;
- miss: empty;
- cnt-t: sum + EEXIST + E2BIG = threads x launches.

| item | D ms | D4 ms | G ms | G8 ms | per call, D4 against D | per call, G8 against G |
|---|---:|---:|---:|---:|---|---|
| empty (probes return at once) | 429 | 428.5 | 440 | 431 | +6 / -7 ms | -18 / 0 ms |
| 1533 pre | 546 | 546 | 553.5 | 552 | +20 / -20 ns | +29 / -46 ns |
| 1533 miss | 541 | 533 | 533.5 | 538.5 | -3 / -50 ns | +23 / +35 ns |
| 1533 cnt-t | 668.5 | 653 | 655.5 | 655.5 | 0 / -103 ns | -12 / +12 ns |
| host pre | 1,988.5 | 5,194 | 1,127.5 | 2,888 | 11,626 / 9,724 ns | 10,365 / 10,012 ns |

- A 1533 operation costs less than this harness can resolve: 43 ns per call for D4-D, 127 ns for
  G8-G (the empty item's spread over the extra threads). B1 measures it on llama.cpp.
- The host path costs 9.7 to 11.6 µs per call.
- A run with a 1533 map starts about 117 ms later than the empty probes (D: 546 against 429 ms).
  That covers the copy and the delivery's set-up; the host path's D run is 1,988 ms.

## C. Delivery cost

**With the GPU free** (the addendum's `kmod` items: key = thread index mod N, all N rows live,
`pt_victim 4 0`, no side counts):

| rows | per in-process delivery (per hash map, passes n1 / n2) | longest | exit delivery |
|---:|---|---:|---:|
| 512 | 318 / 325 µs | 407 / 528 µs | 228 / 223 µs |
| 1,024 | 349 / 325 µs | 466 / 480 µs | 232 / 251 µs |

All are below the 2 ms rule. The first delivery of a process also sets up the hash snapshot
module (load and warm-up), about 0.35 s, once.

**Reported from A and C as they ran (with the side counts):**
- In-process deliveries took 40-110 ms. The snapshot waited behind the probed kernel, which the
  side counts had slowed to about 112 ms per launch, and the exit delivery after it took 0.25 ms.
  On a busy GPU a delivery's latency is the wait for the application's kernels; its own work is
  below 0.5 ms at these sizes.
- The set-up measured 0.62 s there (against 0.35 s without the side counts): the module load
  competes with the probed kernel.
- **A 262,144-entry map** (the tools' default size): the exit delivery took 7.8 ms (three
  passes: 7.80 / 7.80 / 7.87 ms). That is above the 2 ms rule, so it is reported as a cost of the
  schedule for large maps: the delivery's CPU passes run over all 524,288 directory slots
  whatever the live count, against the 20 ms busy period.

## B1. The cost on top of 1531 (llama.cpp tg128, mul_mat_vec_q, every thread; passes b1, b2)

TinyLlama Q4_K_M, llama-bench -r 5. The probed kernel runs 3,137,339,392 threads per run, and
the lat tool times each thread. The histogram is kept in a 1531 sum array or a 1533 hash map.
Every item is valid:
- lat and the override control: sum = WANT, bucket 194 = 0;
- latd: sum + EEXIST = WANT (EEXIST 31,355 and 17,629 first touches, unstable 0, E2BIG 0),
  1,560-1,562 deliveries, syncs covered = syncs at exit;
- every override item logged "PTX override".

| item | tokens/s (b1 / b2) | per thread over base, ns (b1 / b2) |
|---|---|---|
| base (no agent) | 67.64 / 67.55 | |
| lat, 1531 sum array | 56.03 / 55.74 | 0.1249 / 0.1280 |
| latd, 1533 hash map (the build's routing) | 53.15 / 53.17 | 0.1644 / 0.1634 |
| S0.2's hand-routed form, regenerated on this build | 53.14 / 53.14 | 0.1646 / 0.1638 |
| the override's control (this build's dump unchanged) | 55.73 / 55.78 | 0.1289 / 0.1274 |

- **1533 over 1531: +0.0395 and +0.0354 ns per thread.** S0.2 expected +0.0358 to +0.0381, and
  this is far below the rule's 0.058, so no SASS comparison was due.
- The build's routing and S0.2's hand-written form differ by -0.0002 and -0.0003 ns: the pass emits
  what S0.2 timed.
- The S0.2 re-check on this build, hand-routed minus control: +0.0357 and +0.0364 ns.
- Per tokens/s: 53.2 against 55.7-56.0 with the sum array, 67.6 without the agent.

S0.2's override files are keyed by the patched PTX's sha256, which S3 changed, so they were
regenerated from this build's dump (`variants-gen/`, `dump-lat/`; `s0/make_s02.py` applied
unchanged).

## B2, B3. Against today's host path on llama.cpp (passes l1, l2; addendum 2: l3, l4)

Every item is valid and its counts hold:
- link, dlink, linkn and dlinkn: value = probe runs = 4,233,216;
- blk: 1-2 lost first touches;
- dblk: sum = runs - EEXIST (15);
- ldr and dldr: exact, EEXIST 0;
- dnone: the map stays empty.

The analysis marks dnone "INVALID" only because it checks for an inline lookup, which dnone by
design has none of.

**rms_norm, p32 (prefill of 32 tokens), every thread: a lookup of key 0 and an add:**

| item | tokens/s | time against base | per call |
|---|---|---:|---:|
| base | 1,380-1,391 | | |
| host path, with side counts (link) | 3.88 / 4.01 | x346-356 | 11.27-11.65 µs |
| 1533, with side counts (dlink) | 34.00 / 34.76 | x40-41 | 1.27-1.30 µs |
| host path, no side counts (linkn) | 4.38 / 4.39 | x315-317 | 10.30-10.33 µs |
| **1533, no side counts (dlinkn)** | **1,347.7 / 1,354.1** | **2.4-2.7% slower** | **0.79-0.89 ns** |

On a real kernel, one lookup and add on the device hash map costs under a nanosecond per thread,
about 12,000 times less than the host call it replaces. The 32-token prefill goes from 316 times
the base time to 2.4-2.7% over it.

**rms_norm, p32, thread 0 of each block (one key):** host path 757 / 774 tokens/s (44-45% fewer,
13.2-13.9 µs per call); 1533 1,246 / 1,253 (9-10% fewer, 1.71-1.88 µs per call with the side
counts, which this tool keeps).

**tg128, the leader thread of each launch of the four kernels:**

| item | tokens/s (l1, l2 / l3, l4) | per run against the filter alone |
|---|---|---|
| base | 67.56-67.79 | |
| the rate filter and the run count only (ldrf) | 56.83-56.87 / 56.63-56.85 | |
| host path (ldr) | 49.59 / 49.69 | +0.324-0.331 s (20.7-21.2 µs per call) |
| 1533 (dldr) | 54.61 / 54.75 / 55.01 / 55.09 | +0.085 / +0.093 / +0.067 / +0.072 s |
| 1533 declared, unused (dnone, addendum 2) | 55.41 / 55.19 | +0.050 / +0.068 s |

- **Most of the per-launch counter's cost is the routing pass, not the hash map.** With a 1533 map
  present, the routing pass runs on the probed modules and changes every thread's path through
  the tool's rate filter (its helper calls). It does so for any device-model map, 1531 included.
- That alone costs 0.050-0.068 s per run.
- The 15,616 hash operations per run add 0.017 / 0.005 s, within the passes' spread.
- The host path's counter costs 0.32-0.33 s per run.

## D. Re-checks on the final build (a3db63689)

| check | result |
|---|---|
| S0.2 | B1's hand-routed form regenerated and timed on this build: +0.0357 / +0.0364 ns over its control |
| S2.12, `check_devhash_orders.sh` | OK (writer `ldaddal` then `ldar`, sealer `stlr` then `ldar`, poll `ldar`, reader `ldapr`) |
| `[gpu-devhash-cpu]` | 24 cases, 6,975 assertions, all pass |
| `[gpu-devhash-device]` (S3, S4, S5, S6) | 29 cases, 211 assertions, all pass |
| `bpftime_nv_attach_tests` (S1 `[devhash-layout]`, S3.11 host text checks, S6 `[hash-map-routing]`) | 85 of 86; `[ptx-failure]` fails, as on plain dev |

(`recheck/`; the order check's first run lacked its build-directory argument and was run again.)
