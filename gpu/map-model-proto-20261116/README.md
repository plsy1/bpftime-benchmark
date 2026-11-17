# Per-thread map model: minimal prototype (2026-11-16/17)

Follow-up to `gpu/step0b-checks-20261116`. The minimal prototype of the design built on the rule of that round: correctness on documented guarantees, measured behaviour only for speed. It covers a thread's state carried in its own local memory (type 1), mergeable results added into device memory (type 2), and delivery to the CPU ordered by documented synchronization. Per-record output (type 3) is not in it.

**Corrections (2026-11-17, `gpu/probe-cost-20261117`).** `merge-cost/`'s "carry only" column lost its exit timer read and bucket to ptxas (the bucket was compared with 99, which it cannot reach); the merge columns stand. "It is the per-thread memory work that costs, not the probe calls" is superseded: on mul_mat_vec_q the timer helper calls cost about 0.035 ns per thread, the log2 chain (ALU only) about 0.035 ns, the 1531 lookup through helper 1 with the add about 0.135 ns.

## Summary

- **Counts on a real application.** llama.cpp (TinyLlama Q4_K_M, llama-bench), probes on rms_norm, the largest mul_mat_q, the largest mul_mat_vec_q and rope at once. Every probed thread's exit was counted in every measured item (each item is the warmup plus five runs):
  - pp512: exactly 6x step0/e7-llama's per-run counts;
  - tg128: exactly 5 runs plus the warmup's one token (5c + c/128) for the three kernels decode launches (mul_mat_q does not run in tg128), 3.14 x 10^9 exits per run.

  On a victim with predicated exits, partial warps and graph replays, an entry-to-exit carry of the thread index had 0 mismatches.
- **Cost of a per-thread latency histogram (`lat`)**, as overhead on llama-bench throughput, default delivery, two passes on 606e74a5:
  - pp512: **4.1-4.3%**;
  - tg128: **33.2-33.3%**.

  Passes p1/p2 (builds e3f4e18f + c2826dbc, where `lat` was 3.6-4.0% and 31.8-31.9%):
  - the same tool on today's maps: pp512 98.7%, 78-79x below base throughput and 75-76x below `lat`;
  - with only the start moved to today's per-thread array: pp512 38-39%, tg128 70%.
- **Decision rules** (fixed before the runs, `meas/DECISION-RULES.md`):
  - Rule 3, `lat` at most 5% on both workloads: **met on pp512** (3.6-4.3% in all six passes), **not met on tg128** (31.8-33.3%). With in-call delivery, the mode whose end-of-run completeness rests on documents, pp512 is 4.8-5.2%, above 5% in one of two passes.
  - Rule 4, cheaper than today: **met** in p1/p2. With in-call delivery, `hist` costs more than today's `histold` on both workloads.
- **Where tg128's cost goes** (passes p3/p4, pre-review builds, `lat` 31.9-32.0%):
  - empty entry and exit probes: 3.4-3.8 points;
  - the carry (start stored, read back, bucket computed): 18.2-18.3 more points (`carryonly` 21.8-22.0% in all);
  - the merge (one sharded atomic add): 10.0-10.1 points.
  - The review fixes added 1.2-1.4 points (606e74a5: 33.2-33.3%), not attributed.

  tg128 is mostly mul_mat_vec_q: 3.14 x 10^9 threads per run doing about 0.39-0.40 ns of work each. About 0.30 ns of probe work per thread (606e74a5; 0.28 ns before) is therefore +74-77% on that kernel.
- **The merge needs shards** (empty kernels, nearly every thread in one bucket, with the carry): one device word per bucket costs 1.09-1.10 ns per thread, the add alone about 1.0 ns; with (SM, lane) shards it is 0.15-0.16 ns, the add alone about 0.06 ns.
- **Per-thread-index output gets cheaper in time, not in memory.** threadhist in device memory (`hist`), against today's `histold`:
  - pp512: 5.1-5.3% against 27-28%;
  - tg128: 28.6% against 52.6-52.9%.

  But `hist` holds three grid-sized copies (device, pinned staging, shared: 3 x 32 MB at 1M thread indices x 4 kernels) against today's one (32 MB). Delivered inside every synchronize call, it costs 35.8-36.0% (pp512) and 87.4% (tg128).
- **Delivery.** Each delivery reads the count of the application's synchronize calls before it enqueues its snapshot. The values it copies into the agent then hold every add of the kernels those calls waited for. That rests on PTX ISA 8.9.4 for the agent's thread. For the tool's process, which reads the shared segment, it rests on the CPU's memory model (step0b claim 8: partly documented).
  - The last synchronization is covered in-process by construction only when the delivery runs inside the synchronize call (`BPFTIME_GPU_DELIVERY_AT_SYNC=1`). For `lat` that costs +0.7 to +1.0 points (pp512) and +4.6 to +4.7 (tg128).
  - In the default mode it depends on timing. On llama.cpp it was covered for `lat` and not for `hist`, whose 32 MB copy is delivered once a second.
  - A program that exits without synchronizing after its last launch can lose those launches: `pt_exitfast` lost 49,152 of 73,728 exits, and the status shows that it never synchronized.

## What was built

bpftime branch `proto/map-model` (local, from dev 52126f33): `patches/0001-0004` (e3f4e18f, c2826dbc, 606e74a5, c940870b).

| type | where a thread's value lives | what the CPU sees | correctness relies on |
|---|---|---|---|
| 1530 thread-local array | a kernel-scope `.local` area the agent adds to every probed kernel, zeroed when the kernel starts; its `cvta.local` address is passed to the probe functions | nothing | local memory is the thread's own storage for the kernel's run (PTX ISA 5.1.5; programming guide 2.3.3); across preemption the documents do not say (step0b claim 6, partly documented; time-slicing measured in C1) |
| 1531 sum array | u64 counters in device memory, spread over (SM, lane) shards; programs must change them with 64-bit atomic adds only (a contract, not checked) | each word summed over the shards | atomic adds are atomic; any shard gives the same sum, so `%smid`/`%laneid` only decide speed |
| 1532 device per-thread array | 1502's layout (a value per key and thread index) in device memory; 64-bit atomic adds only (contract) | the agent's copy | as 1531 |

- **Routing** (`nv_thread_local_routing.cpp`, after the PTX passes, before the trampoline):
  - The area and its zeroing go at the kernel's start. Its address is formed at every probe call site, after a label on the call's line and before its predicate. Probe functions take it as their one parameter.
  - A thread-local lookup with a constant map fd becomes the address computation itself (key bounds checked, NULL outside). Other map helper calls get the address as their fifth argument (eBPF r5); probe functions no routed kernel calls get 0.
  - In a process with thread-local maps, every generic `atom`/`red` in a probe function gets an `isspacep.local` test (add, and, or, xor, exch, cas; 32/64-bit; unpredicated). A local address takes a plain read-modify-write, since a scalar `atom` is defined only on `.global`/`.shared` or generic addresses pointing there (PTX ISA, atom: `ptx-isa.txt` line 26571 of step0b's copy).
  - The area is at most 1 KiB. Any other shape leaves the whole fatbin unpatched (load hook) or fails the direct run.
- **Delivery** (agent thread, relaxed capture mode, its own non-blocking stream):
  - A snapshot kernel reads every word with `ld.relaxed.gpu`. The programs' adds lower to `atom.add.u64 [addr], 1`: generic, no scope, so `.gpu` and relaxed. Reads and adds are therefore morally strong: no data race (PTX ISA 8.7.1), single-copy atomicity (8.10.3).
  - The kernel sums the shards into pinned staging memory. After a successful query of its stream, the thread copies the staging memory into the shared values.
  - Hooks on `cuStreamSynchronize(_ptsz)`, `cuCtxSynchronize(_v2)` and `cuEventSynchronize` count the application's successful calls. The agent's own synchronizations are excluded (c940870b). Synchronous copies and other implicitly synchronizing calls are not counted.
  - A delivery reads the count before it enqueues its snapshot (PTX ISA 8.9.4, API synchronization):
    - completion of a stream's last task synchronizes with the return of its synchronize;
    - a device or context synchronize acts on all streams;
    - the start of an enqueue call synchronizes with the start of the task.
  - Schedule:
    - sum maps: within 2 ms of an uncovered synchronization, every 20 ms while their values change, else every 200 ms;
    - per-thread maps: 1 s, 1 s and 2 s;
    - `BPFTIME_GPU_DELIVERY_AT_SYNC=1`: inside the synchronize call.
  - The exit delivery (atexit) is best effort and bounded to about 1.7 s in all (500 ms for the delivery thread, 200 ms for its lock, a 1 s deadline on the snapshot): CUDA calls after `main` are undefined by the programming guide (`cpg-legacy-12.9.txt` line 2368 of step0b's copy).
- **Status words** of 1531/1532 (key `max_entries + i`):
  0. deliveries;
  1. the exit delivery ran;
  2. the owner pid;
  3. synchronizations covered by the latest in-process delivery;
  4. synchronizations at exit;
  5. probed launches noted after the last synchronization.

  Graph launches and the launches today's hooks cannot resolve are not noted. So word 5 can read 0 with launches after the last synchronization (it did in `pt_exitfast`), and "covered" means the last synchronization was covered, not that every launch preceded it.
- **Lifetime and processes:**
  - the first device copy in a process takes a reference on the map;
  - one process per map delivers: a second live agent gets no device copy, and its lookups return NULL;
  - the device copy is zeroed on a stream of its own before its address is published;
  - fork children do nothing at exit.

## Correctness

Tools (`tools/`). Each pair has one eBPF source, and the map types differ. Today's variants also clear the start slot at exit (`lat`), or count with a plain `*cnt += 1` (`hist`):

| tool | the model | today |
|---|---|---|
| `lat`: the entry stores `%globaltimer`; the exit adds 1 to bucket log2(now - start), per kernel | start in 1530, histogram in 1531 | `latold`: start in 1502 (1512 on the Orin, registered host memory), histogram in 1503 (1513), atomic adds. `latmix`: start in 1512, histogram in 1531 |
| `hist` (threadhist): exits per thread index | 1532, atomic add | `histold`: 1502 (1512), `*cnt += 1` |
| `carry`: the entry stores (thread index ^ salt) + 1 and the time; the exit checks both | 1530, counts in 1531 | |
| `tlatomic`: 64-bit fetch-add, 32-bit add and exchange on thread-local values | 1530 | |

Victim `pt_victim`:
- lanes leave through predicated `exit` instructions (four probe call sites), with data-dependent spins and a partial last block: 100,096 threads x 10 launches;
- a graph of two launches with 48-thread blocks, replayed 100 times.

The checks below ran on the final build (c940870b), except where noted. Outputs are in `check/out/`, `runs/`, `kmr/`, `ptx/` and `tests/`.

| check | result |
|---|---|
| carry, default and in-call delivery | entries = exits: 1,000,960 (pt_kdiv) and 2,880,000 (pt_kflat); 0 mismatches, 0 without entry, 0 NULL |
| tlatomic, both modes | 0 mismatches over 3.88M thread executions; every fetch-add returned 0 |
| lat, latmix, latold | per-kernel histogram totals exact, 0 exits without a start |
| hist, histold | every thread index called exactly L times (100,096 and 14,400 indices), nothing beyond the grid |
| last synchronization covered (the victim exits right after it; 2 synchronizations) | in-call: yes for carry, lat, tlatomic and hist. Default: carry, latmix and tlatomic yes; lat no (1 of 2); hist no (0 of 2); there the counts came from the exit delivery |
| capture stress: 3,000 global-mode captures back to back, deliveries every 1 ms (2,246) or inside each synchronize (3,004) | 0 captures invalidated, counts exact, last synchronization covered in both |
| exit without synchronizing (`pt_exitfast`: 3 launches of 24,576 threads spinning 200 ms, then `return`) | 24,576 of 73,728 exits delivered, in both modes; status: 0 synchronizations, 0 launches noted after the last one |
| C1's kmr (seven exit sites in five forms: `@p ret`, `@p exit`, `exit`, `ret`, `$L: ret`) through the C++ routing (`kmr/`) | 8 call sites routed (1 entry, 7 exits); the address is formed after the label at `$L_kmr_c:`; ptxas accepts the module |
| ptxas on the four llama kernels (`ptx/ptxas_check-final.txt`; today's PTX from the pre-review build, which the fixes do not touch) | registers: 32/255/56/32 with the model, 38/255/56/34 today, 32/255/56/17 unpatched. Warps per SM by registers unchanged. Local frame 640 B (mul_mat_q 704 B), today 632 B (696 B): +8 B, the area |
| llama.cpp pp512 with kept PTX (final build) | the guard in each of the four modules (1 atomic each); counts exactly 2x (warmup + 1 run) |
| `bpftime_nv_attach_tests`; runtime `[cuda-host-call]`, `[cuda-ringbuf]`, `[cuda-watcher-policy]`, `[cuda-no-device]`, `[gpu-map-host]`, `[map-create]` (`tests/`) | all pass |

Two reviews, listed in `REVIEW.md` with their outcomes:
- **Code review:** 26 findings (about 17 distinct issues), all confirmed by a second reviewer, fixed or changed in 606e74a5:
  - a generic atomic on a thread-local value (undefined: now guarded);
  - device maps outliving a tool's close (now a reference);
  - the exit delivery after `main` (now best effort, completeness from in-process deliveries);
  - unbounded waits at exit;
  - fork children;
  - unordered zeroing.
- **Check of this README against the data:** found the synchronize hooks counting the agent's own calls (fixed in c940870b). It also led to the reruns above and to corrections throughout.

Not fixed:
- "64-bit atomic adds only" on 1531/1532 (a contract);
- `cudaDeviceReset` (frees the device copies);
- the snapshot's morally-strong argument assumes one memory synchronization domain. Before Hopper CUDA reports one (`cpg-legacy-12.9.txt` lines 3116 and 3126); the Orin is an Ampere GPU. Not checked at run time.

## Measurement on llama.cpp

Method:
- llama-bench `-r 5` with warmup (`meas/run_meas.sh`): pp512 (`-p 512 -n 0`) and tg128 (`-p 0 -n 128`, CUDA graphs on); probes on all four kernels at once.
- Overhead = 1 - median samples_ts / base's in the same pass.
- Six passes, serial, items shuffled per pass, clocks fixed and checked before every item.
- Decision rules fixed before the runs (`meas/DECISION-RULES.md`, with two addenda written before p3/p4 and p5/p6).
- Items with count checks: every `lat`, `latmix`, `latold`, `hist` and `histold` item, with or without in-call delivery. All passed (`meas/analysis-*.txt`). `base`, `agent`, `empty1`, `empty2` and `carryonly` have no count check.
- The 32 MB `calls.bin` files of `hist` are not kept; their check results are in the analysis files.

**Final build, passes p5 and p6.** Built from 606e74a5. c940870b changes only which synchronizations are counted, not the probes or the delivery work.

| tool | pp512 | tg128 | last synchronization covered (status) |
|---|---:|---:|---|
| base (t/s) | 2,516 / 2,512 | 67.97 / 67.95 | |
| lat | 4.27% / 4.07% | 33.24% / 33.28% | yes (reported) |
| lat, in-call delivery | 5.23% / 4.79% | 37.91% / 37.86% | yes (judged) |
| hist | 5.26% / 5.07% | 28.59% / 28.58% | no: 829-857 of 859, 9,271-9,341 of 9,749 (reported) |
| hist, in-call delivery | 35.95% / 35.82% | 87.44% / 87.41% | yes (judged) |

**Against today (passes p1 and p2, builds e3f4e18f + c2826dbc, before the review fixes):**

| tool | pp512 | tg128 |
|---|---:|---:|
| agent loaded, probes on kernels llama.cpp lacks | 0.35% / -0.39% | 0.41% / 0.65% |
| lat | 3.60% / 3.95% | 31.91% / 31.84% |
| latmix (start in 1512) | 39.13% / 38.07% | 70.39% / 70.39% |
| latold (today) | 98.72% / 98.74% | not run (over 30 min per run) |
| latold on dev 52126f33's own build | 98.71% / 98.74% | |
| hist | 4.66% / 5.02% | 28.40% / 28.44% |
| histold (today) | 26.96% / 27.53% | 52.58% / 52.65% |
| histold on dev 52126f33's own build | 27.84% / 27.99% | 52.87% / 52.89% |

- The dev-build controls agree with the prototype build within 0.9 points: the prototype's trampoline does not slow today's maps.
- With the review fixes, lat and hist moved by 0.2-1.4 points (pass means), within the 2-point rule. The cause was not isolated.

**Attribution (rule 5 and addendum 1).**
- p1/p2: moving the start from 1512 to the thread-local map saves 34.1-35.5 points on pp512 and 38.5-38.6 on tg128 (latmix - lat). The sum map instead of 1513 saves 59.6-60.7 points on pp512 (latold - latmix); this share was not measured on tg128.
- p3/p4 (builds e3f4e18f + c2826dbc):

| item | pp512 | tg128 |
|---|---:|---:|
| empty entry and exit probes (lat's floor) | 0.89% / 0.49% | 3.42% / 3.79% |
| carry only (start stored and read back, bucket computed, no add) | 2.61% / 2.24% | 21.75% / 21.98% |
| lat | 4.30% / 3.69% | 31.85% / 32.00% |
| empty exit probe (hist's floor) | 1.03% / 0.52% | 2.23% / 2.54% |
| hist | 5.09% / 4.83% | 28.21% / 28.48% |

So:

| part | pp512 (points) | tg128 (points) |
|---|---:|---:|
| floor | 0.5-0.9 | 3.4-3.8 |
| carry | 1.7 | 18.2-18.3 |
| merge | 1.5-1.7 | 10.0-10.1 |
| hist's add (hist - empty1) | 4.1-4.3 | 25.9-26.0 |

Per thread execution on tg128, from the time per run (128 tokens) and 3.14 x 10^9 executions:
- empty probes: about 0.02 ns;
- carry: 0.145 ns;
- merge: 0.11 ns;
- lat: 0.28 ns in all (0.30 ns on the final build).

step0b C1 priced the carry with both probe calls at 0.104 ns on empty kernels, 0.078 ns above its empty probes. Like for like here: 0.146 ns above the empty probes. On a kernel whose threads do about 0.4 ns of work, it is the per-thread memory work that costs, not the probe calls.

**Merge placement** (`merge-cost/`): empty kernels with the carry and nearly every thread in one bucket; two passes with clocks fixed (the board state was not logged); ns per thread execution over an empty kernel:

| shape | carry only (exit timer and bucket removed by ptxas, see Corrections) | + add to one word per bucket | per SM | per lane | per (SM, lane) |
|---|---:|---:|---:|---:|---:|
| 512 x 512 | 0.098 / 0.101 | 1.090 / 1.103 | 0.363 / 0.366 | 0.297 / 0.287 | 0.163 / 0.159 |
| 8,192 x 512 | 0.094 / 0.097 | 1.086 / 1.099 | 0.353 / 0.356 | 0.292 / 0.281 | 0.159 / 0.154 |

The benchmark adds with `red.global.add`, not the prototype's guarded generic `atom.add`. The 24 x 512 shape is dropped: every reference batch hit the launch gate's cap (the host could not queue it in time).

**Memory** (four kernels; per-thread maps sized for 1,048,576 thread indices; the largest launch has 720,896 threads, mul_mat_vec_q in tg128, `gpu/llama-census-20261113`):

| tool | device | pinned staging | shared segment | local memory per thread |
|---|---:|---:|---:|---:|
| lat | 532,480 B (260 words x 256 shards) | 2,080 B | 2,080 B of values + 8 status slots | 8 B area (frame 640 B; mul_mat_q 704 B) |
| latold | | | 8,390,688 B (1512 start 8 MB + 1513) | frame 632 B (mul_mat_q 696 B) |
| hist | 33,554,432 B | 33,554,432 B | 33,554,432 B of values + 8 status slots | |
| histold | | | 33,554,432 B | |

**Delivery** (p5/p6, per map):

| item | deliveries | average | longest |
|---|---:|---:|---:|
| `lat`, default, pp512 | 882-1,072 | 0.61-0.65 ms | 3.9-4.1 ms |
| `lat`, default, tg128 | 1,842-1,845 | 0.30-0.31 ms | 15.9-16.3 ms |
| `lat`, in-call, pp512 | 1,618-1,624 | 0.37-0.43 ms | 2.1-4.2 ms |
| `lat`, in-call, tg128 | 10,244-10,245 | 0.14-0.15 ms | 0.8 ms |
| `hist` (32 MB), default | 47-52 | 7.9-8.0 ms | 21 ms |
| `hist`, in-call | 908 (pp512), 9,781 (tg128) | 6.9-7.2 ms | 21 ms |

`hist` in-call copied for 6.4-6.5 s per pp512 item and 67.2-67.4 s per tg128 item. The delivery thread's set-up (module load and first launch) took about 1 ms in the llama runs, 172-371 ms in the victim and stress runs (cause not logged), and 134 ms in `pt_exitfast` (the GPU full).

**Start-up** (reported, not judged): one llama-bench invocation (warmup and five runs), most of it the agent patching and compiling the probed modules.

| item | pp512 | tg128 |
|---|---:|---:|
| without the agent | 1.8 s | 10 s |
| agent that patches nothing | 27.6 s | 27.8 s |
| `lat` | 152-155 s | 113-114 s |
| `latold` | 240-242 s | |
| `hist` | 98-99 s | 76.5-76.8 s |

## What this means

1. **Types 1 and 2 work on a real application under the rule, with three parts the documents cover only partly:** local memory across preemption, relaxed capture mode (backed by the capture stress), and the hand-over to the reader process.
   - The counts were exact.
   - The probes use no slots and no system fences.
   - Memory is bounded by the hardware (8 B of local memory per thread) and by the histogram (532 KB), where today's per-thread start needs a slot per thread index (8 MB at 1M).
2. **The model removes most of today's cost.**
   - On pp512 the same tool goes from 98.7% to about 4%: the sum map saves about 60 points and the thread-local start about 35.
   - On tg128 it is still 33% (rule 3 not met). Every thread of a very fine-grained kernel pays about 0.30 ns of probe work; the probe calls alone cost 3.6%.
3. **Shards are part of the design, not an optimization.** With nearly every thread in one bucket, one word per bucket costs more (about 1.0 ns for the add) than today's whole per-thread map operation (0.37 ns in device memory, step0b C3).
4. **Grid-sized results (threadhist) get cheaper in time but not in memory.**
   - Delivering them takes three copies.
   - In-call delivery takes 36% of pp512's throughput and 87% of tg128's.
   - The model's CPU-facing results should stay summaries. Per-thread values belong in type 3 (records with counted drops), not built here.
5. **Completeness at the end has a price and a precondition.**
   - The price is in-call delivery: +0.7 to +4.7 points for `lat`.
   - The precondition is an application that synchronizes after its last launch.
   - In the default mode the counts were exact in every item, but where no in-process delivery covered the last synchronization (`hist`, and every item of p1-p4, which had no such status), the last values came from the exit delivery, which rests on CUDA calls after `main`.

## Not done, limits

- Not done:
  - type 3 (per-record output);
  - several GPU processes per segment (refused);
  - `cudaDeviceReset`;
  - noting graph launches and unresolved launches.
  - "Atomic adds only" on 1531/1532 is a contract, not checked.
- Cheaper carry and merge were not attempted:
  - constant-key lookups still go through the eBPF stack in local memory (a store and a load per lookup);
  - the sum map's lookup is a helper call;
  - the timer read is a helper call.
- One application and one board, with four kernels probed at once; per-thread maps sized for 1M thread indices.
- Files:
  - `patches/`: the branch;
  - `tools/`, `check/` (with `out/`), `meas/` (driver, analysis, decision rules, per-item logs), `merge-cost/`, `ptx/`, `kmr/`;
  - `runs/`: the final-build victim, stress and exit runs, plus the two functional llama runs of 11-16 whose PTX the first register check used;
  - `tests/`, `review/` (raw review outputs), `REVIEW.md`.
