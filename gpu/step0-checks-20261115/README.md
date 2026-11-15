# Step 0: on-board checks for the GPU map model (2026-11-15)

The nine checks of section 5 of `gpu/dir1-design-space-20261115/README.md`, run on the Orin before any design work.
Each check asks a question about the hardware or the runtime that the storage design depends on; none of them builds the design.

**Revised by `gpu/step0b-checks-20261116`** (a follow-up that checked every conclusion against the documents and tested the mechanisms that avoid undocumented behaviour):
- Checks 1's slot exclusivity and id stability are measured behaviour of this board, not documented: the PTX ISA promises `%warpid` unique only within a CTA and says ids may change during execution. A design takes exclusivity from an atomic claim and carries the slot from entry to exit.
- Check 3's "two-step copy, flags first" is **not** documented while the writer runs; only a copy ordered after the writer's completion is. Its global-capture problem has a documented remedy that held (the copier switches its own thread to relaxed mode).
- Check 7's "correct under partial masks" holds for a single atomic in the leader's region; a warp claim with retries and an `__activemask()` mask is wrong. Use a mask known by construction, or per-lane paths.
- Check 4 and the publication protocols rest on an undocumented assumption when the reader is another process (the CPU's mapping and the GPU's device pointer are aliases).
- Check 2's "single digit only with batched publication": batching 384-1,536 warps behind one system fence was measured at 3.3-3.8 ns per warp record, about 10% [projection] at anchor, so batching alone does not reach single digit.
- Check 5's fetch-add is `.gpu` scope; a CPU read of the same word while the kernel runs is outside the memory model (GPU-CPU atomicity needs `.sys` scope).

- **Board:** Jetson Orin, sm_87, 8 SMs, CUDA 13.2, driver 595.78, Linux 6.8.12-1021-tegra-btf. cpu5 at 1,728,000 kHz and the GPU at 1,020 MHz before and after every step (`logs/main-20261115-050439.log`).
- **Runs:** all 13 steps in one serial pass, 05:04-05:53 JST, every step rc 0, nothing else using the GPU (the busiest processes and the clocks are logged around each step). The daily timer was disabled; apt's timers fire at 06:24, after the pass. A second pass, 06:04-06:54 JST, repeated everything (section "Second pass"); the numbers below are pass 1's. Driver: `run_step0_all.sh` (the pass-2 version, which also waits for apt before timed steps; pass 1's version is in this directory's first commit).
- **bpftime:** dev 52126f33 (`build-cuda` of the dev worktree) for checks 5, 6 and 8. Checks 7 (llama part) and 9 used an instrumentation-only build of the same commit: four read-only helpers (520 activemask, 521 `%gridid`, 522 `%nsmid`, 523 `%nwarpid`) and a launch-geometry log in every launch hook. It is commit 7e78a5d2 on the local branch `exp/step0-instrumentation` (never pushed); the patch is in `instrumentation-build/`.
- **Programs:** written and then adversarially reviewed by separate agents before the runs. Each check's README holds its question, method, the decision rules written before the real run, the commands and the known limits; the reviews' rule changes are listed there with their reasons. Smoke runs are not used anywhere below.
- **Layout:** `eN-*/` = sources and scripts of one check, `eN-*/real-run/` = the output of the pass above (big text files gzipped, nsys reports left out).

## Verdicts

| check | question | verdict |
|---|---|---|
| 1 | Are (SM, warp slot) ids exclusive and stable; is `%gridid` one per launch? | exclusive among co-resident warps, also across two grids; stable across time-slicing; `%gridid` unique per launch **except graph replays** |
| 2 | What do records, fences and atomics cost in registered vs device memory? | a record per warp costs about one warp memory instruction (reading 2); **the system fence dominates**; an unfenced 8-B store per thread in device memory projects 3-6% at llama shapes (today's 1512 op 35-41%); contended atomics are 20-200x dearer in registered memory |
| 3 | Can storage the CPU reads live in device memory? | yes: agent copy-out with a flags-first two-step copy, or import of a POSIX-FD export in the reader |
| 4 | Is one leader fence per warp enough to publish to the CPU? | no violation in 10^9 rounds while the no-fence control fails; safe in practice, not attributable to the fence's ordering |
| 5 | Can tool authors use eBPF atomics? | 64-bit fetch-add yes; CAS no (the CMPXCHG lowering bug, confirmed on the GPU) |
| 6 | How fast can the CPU drain, and how large can the segment be? | one reader about 7M 40-B records/s through `bpf()`; segments up to 2,047 MiB register, 2,048 MiB fails |
| 7 | Do warp instructions in a helper work under partial masks; how many records per call? | correct; at llama.cpp exit probes every call had 32 lanes |
| 8 | Can a `.global` be appended to a patched module; how much constant memory is left? | yes up to 1 GiB; 24,568 B of constant memory left |
| 9 | Do the launch hooks see every geometry before graph replays run it? | yes in llama.cpp pp512 and tg128 |

## 1. Hardware ids (`e1-ids`)

- **Sizes:** max `%smid`+1 = 8 = `%nsmid`; max `%warpid`+1 = 48 = `%nwarpid` (a ptxas immediate, not a hardware read). 8 x 48 x 32 = 12,288 lane slots.
- **Exclusive:** 0 (SM, warp slot) collisions in 33 launches whose co-residency was proven by a capped all-warp barrier: block sizes 32-1,024 including a partial warp (48), and 9 pairs of grids on two streams that shared all 8 SMs. Controls: the CTA-relative key (SM, tid/32) collides in the same launches; an oversubscribed launch (2x capacity) hits the cap, as it must.
- **`%warpid` is a hardware slot, not CTA-relative:** it equals tid/32 in 466 of 10,752 warps. Slots are dense: 1-warp CTAs at 16 per SM use 0-15.
- **Stable:** 0 changes between entry and exit; with a second process forcing time-slicing, 198 gap episodes and 76,032 warp-gaps of about 2.5 ms, 0 id changes (control without the second process: 0 gaps). Same-context stream priority never preempted (30 high-priority launches all started after the long kernel ended).
- **`%gridid`:** unique and strictly increasing over 1,000 launches, two streams and two modules (PTX JIT, `cuLibraryLoadData`). **A graph node keeps one value across all 1,000 replays and across `cudaGraphExecUpdate`** (2,997 duplicates of 3,000 in each graph case); nodes and graph execs differ from each other. The high 32 bits were always 0.
- Not tested: preemption other than time-slicing (debugger, checkpoint), co-residency across processes.

## 2. Placement and atomic cost (`e2-cost`)

10 interleaved rounds, medians with [min-max]; registered memory made as bpftime makes it (shm, prefault, `cuMemHostRegister`). Shapes: anchor = 512 x 512 threads (rms_norm's pp512 launch), large = 4,194,304 threads, full = 12,288 resident threads.

- **Today's 1512 lookup-and-increment** (built by the dev kretprobe pass from threadhist's eBPF): 0.433 ns per thread execution [0.432-0.434], 13.85 ns per warp. The llama.cpp figure was 0.621 (ratio 0.70, inside the [0.5, 2] gate); loop and exit forms agree (1.01).
- **One 32-B record per warp (lane 0, then `membar.sys`): 10.06 ns per warp** = 0.73x today's per-warp op. **Reading 2:** a record per warp costs about one warp memory instruction, not 1/32 of it.
- **The fence is the cost.** The same record without the fence: 1.61 ns (registered), 1.50 ns (device). With it: 10.06 (registered), 7.98 (device). Plain stores cost the same in both memories (ratio 1.0-1.3).
- **A fence per lane costs about what one fence per warp costs:** a converged warp executes `membar.sys` as one instruction. At anchor, one lane storing and fencing per warp took 57.4 us per launch, all 32 lanes storing and fencing 67.6 us; check 4's protocols A (one fence) and B (every lane) ran at the same rate. Today's per-lane fence in 1512 is therefore not a 32x cost.
- **[projection] one record per warp on every pp512 kernel** (6.02M records over 204.6 ms; empty kernels, so the cost is fully exposed):

| shape | fenced, registered | fenced, device | unfenced, registered | unfenced, device |
|---|---:|---:|---:|---:|
| anchor | 29.6% (lane 0) / 21.8% (4 lanes) | 23.5% / 22.4% | 4.7% / 4.1% | 4.4% / 4.1% |
| large | 28.6% / 21.2% | 23.0% / 21.2% | 3.9% / 3.6% | 3.9% / 3.6% |
| full | 33.9% / 25.9% | 28.3% / 27.2% | 19.2% / 17.3% | 16.4% / 16.4% |

  Rule 2: single-digit percent is **reachable only with batched publication** (one fence per many records) at the anchor and large shapes, and not at full occupancy. Rule 3: copy-out from device memory is **not required** for records (placement barely matters for plain stores).
- **[projection] one operation per thread execution on every pp512 kernel** (192.56M thread executions over 204.6 ms), ns per thread execution at R = 1 and the projected overhead; the full shape's deltas are 2-4 us, near the launch floor, so its row is the least precise:

| per thread | memory | fence | anchor | large | full |
|---|---|---|---:|---:|---:|
| 8-B store (coalesced) | device | none | 0.032 ns, 3.0% | 0.060 ns, 5.7% | 0.174 ns, 16.4% |
| 8-B store (coalesced) | registered | none | 0.065 ns, 6.2% | 0.060 ns, 5.7% | 0.215 ns, 20.3% |
| 8-B store (coalesced) | device | `membar.sys` | 0.259 ns, 24.3% | 0.249 ns, 23.5% | 0.282 ns, 26.5% |
| 8-B store (coalesced) | registered | `membar.sys` | 0.258 ns, 24.3% | 0.246 ns, 23.2% | 0.337 ns, 31.7% |
| atomic add on its own 8-B word | device | none | 0.057 ns, 5.4% | 0.153 ns, 14.4% | 0.195 ns, 18.4% |
| atomic add on its own 8-B word | registered | none | 0.291 ns, 27.4% | 0.275 ns, 25.9% | 0.489 ns, 46.1% |
| today's 1512 lookup-and-increment | registered | `membar.sys` | 0.433 ns, 40.7% | 0.420 ns, 39.5% | 0.615 ns, 57.9% |
| today's op on device memory | device | `membar.sys` | 0.369 ns, 34.7% | 0.506 ns, 47.6% | 0.533 ns, 50.2% |
| 32-B record per thread | device | none | 0.748 ns, 70.4% | 0.739 ns, 69.6% | 0.867 ns, 81.6% |
| 32-B record per thread | registered | none | 2.99 ns, 282% | 2.93 ns, 276% | 3.01 ns, 283% |

  The stores are a lower bound for writing a thread's own slot: no helper call, no address computation. Today's op on device memory costs nearly as much as on registered memory, so its cost is the helper path and the fence, not the placement. A 32-B record from every thread on every kernel is out of reach in either memory.
- **Atomics** (registered / device, full occupancy unless noted): a per-lane add on its own word 2.5x; every lane on one word per warp 183x (anchor 218x); one word per SM hit by 48 warps 19x, by CAS loop 45x. Per counted op on one word per SM at R = 16: device memory falls from 50 ns (1 warp per SM) to 1.3 ns (48 warps), registered memory stays at 82-106 ns from 1 to 48 warps: atomics on registered memory serialize at about 80 ns each, and more warps do not raise their throughput.
- A 32-B record per thread: registered 3.5-4x device.
- Limits: empty kernels (no overlap with real work), one record size, offline-compiled reference module.

## 3. Device memory to the reader (`e3-copyout`)

- **Part A, a DtoH copy concurrent with the writer:** a single copy does not guarantee that a chunk whose flag reads complete carries its payload: layouts with the flag far after the payload, and a copy of the payload before its flags, violate at every size (up to 2.3M violating chunks). Copying the flag area first and the payload second (`sepff`) showed 0 violations at every size; the reversed-writer controls detect stale payloads. Verdict: **fixable with a two-step copy, flags first**. Copy speed: 20.6 / 26.0 / 28.3 GB/s for 64 KiB / 1 MiB / 16 MiB idle, 3.3 / 6.3 / 6.9 GB/s while the writer saturates DRAM (latency p50 28 / 65 / 616 us idle, 83 / 234 / 2,515 us loaded).
- **Part B0, an agent thread copying on its own non-blocking stream next to stream capture:** 0 capture errors in relaxed and thread-local capture modes; in **global** capture mode the async copier invalidated captures (1,767 of 4,991 iterations; 8,573 of 8,573 with event waits). Controls: a copy on the legacy stream next to a blocking capture stream invalidated relaxed-mode captures (1,260 of 4,520), `cuMemAlloc` and a context synchronize invalidated global-mode captures; `cuMemAlloc` in relaxed mode showed 0 errors (it fires only in global mode). **The context-synchronize control in relaxed mode crashed the process (rc 139)**; in the smokes it once crashed and once invalidated every capture. A copier must not synchronize the context.
- **Part B, the copier inside llama-bench with graphs on** (3 rounds; loss vs no shim; noise 0.6%): idle 0.2% / 0.0% (pp512 / tg128); 1 MiB every 1 ms 0.3% / 0.6%; 16 MiB every 1 ms 3.5% / 7.9%; continuous 5.4% / 12.2%. Graph launches unchanged (nsys `cudaGraphLaunch` 256 = the shim's count; 99.2% of kernels in graphs).
- **Part C, POSIX-FD export of `cuMemCreate` memory:** a second process imports it and reads it with a kernel and by DtoH (27.3 GB/s, same as local memory) and sees the writer's updates. A direct CPU mapping of the exported dmabuf is not usable (88-117k stale words persist after the writer stops); `cuMemSetAccess` for a host location: NOT_SUPPORTED; the RDMA-capable variant: `cuMemCreate` INVALID_DEVICE. The reader needs its own CUDA context, which time-slices with the application (cost not measured).

## 4. CPU/GPU publication in registered memory (`e4-publish`)

- **Protocol A (lanes write, `__syncwarp`, one leader `membar.sys`, flag):** 0 violations in 1.0 x 10^9 rounds (normal) and 1.0 x 10^9 (stress: payload over several lines, readers backwards); 95% upper bound 3 x 10^-9 per round. B (every lane fences): 0 in 1-2 x 10^8. D1/D2 (release-store variants): 0. **Control C (no fence) fails:** 1,370 violating rounds in 2 x 10^8 (normal), 299,982 in 2 x 10^8 (stress).
- **Not attributable to the fence's ordering:** under load a `membar.sys` lasted 28 us at the median (p99 41 us; normal 5-6 us), and a fenceless wait of the same length (Cd) also showed 0 violations in 2 x 10^8 stress rounds. In normal mode pass 1 was undecided (Cd failed twice); in pass 2 Cd showed 0 in 2 x 10^8 against 1,776 expected, so both modes point to the fence's duration. A0 (the publishing lane wrote no payload) 0 in 2 x 10^8 against C0's 17,210 in 10^7, with the same caveat.
- **Cost:** A and B ran at the same rate (16.3M rounds/s normal, 3.44M stress): fencing in every lane cost no more than one fence per warp here.
- **CPU release-store, then GPU CAS:** 0 stale reads and 0 double winners in 10^7 rounds (3 x 10^6 with acquire and under stress); the wrong-order controls detect stale data (253,301 and 174 rounds).
- Verdict: **one leader fence per warp is safe in practice on this GPU**; that the fence orders the other lanes' stores is not shown by the data (the PTX memory model's `bar.warp.sync` guarantee is the formal basis, see `e4-publish/README.md`).

## 5. eBPF atomics as tools write them (`e5-atomics`)

- **Compile** (clang-15 and 16, default / v1 / v2 / v3): 64-bit `__sync_fetch_and_add` with a used result compiles at every level (BPF_ATOMIC | FETCH); 32-bit needs v3; 64-bit CAS compiles everywhere, 32-bit needs v3. bpftime's examples set no `-mcpu`; libbpf-tools set v3 for one tool only.
- **PTX/SASS:** `atom.add.u64` (generic, no scope given) = `ATOM.E.ADD.64.STRONG.GPU`.
- **Run, 122,880 claims (10 launches x 12,288 threads, kretprobe, 1513 maps):** fetch-add: counter = calls, 0 gaps, 0 duplicate indices. Control (plain `+=`): counter 90, 122,790 duplicates. **CAS claim loop: counter 102, 122,778 duplicate indices: unusable.** The `atom.cas` result register is never read (SASS destination RZ): the CMPXCHG lowering bug (`vm/llvm-jit/src/compiler.cpp:1697-1718`) is confirmed on the GPU. Not fixed here.

## 6. Drain throughput and segment size (`e6-drain`)

- **One reader through the intercepted `bpf()`:** ARRAY 40-B values 7.18M records/s (139 ns per lookup), 1513 6.72M/s (149 ns); 4 KiB values about 10 GB/s; 1 MiB values 10.4 GB/s. `BPF_MAP_LOOKUP_BATCH`: unsupported (EINVAL). QUEUE pop 7.5M/s. Against one record per warp: 8.7% of decode's 77.3M/s, 22.8% of prefill's 29.4M/s.
- **Effect on the GPU:** draining 6.7M 40-B records/s slowed a device-memory bandwidth kernel by 1.5% and llama pp512 by 0.1% (6 rounds, median; the rounds without a drain spread 0.19%). An ssh copy and a short parse of a 2.5 MB file ran in the last one to two minutes of the llama variant (05:22-05:23); the 0.1% is inside the spread either way. 1-MiB reads at 7.9 GB/s slowed the bandwidth kernel by 11.5%; a CPU memory hog at 12.6 GB/s slowed it by 39.9% and pp512 by 7.9%.
- **Segment size:** 128, 1,024, 1,536, 1,792 and 2,047 MiB were registered whole and fully resident (the server commits the memory when it creates the segment; agent start to `main` 0.27 s at 128 MiB, 1.64 s at 2,047 MiB). **2,048 MiB: `cuMemHostRegister` failed with 2 (OUT_OF_MEMORY)** with about 2 GiB still available, so the limit looks like a registration size cap just below 2 GiB [inferred].

## 7. Warp instructions under partial masks (`e7-warp`, `e7-llama`)

- **Standalone:** a `__noinline__` reservation helper (`__activemask`, lowest-lane leader, `atomicAdd`, `__shfl_sync`) was correct under early returns, absent low lanes, nested call sites, lane-dependent loops and calls before every load and store: 0 violations in about 4.0-4.3M calls per case, native and driver-JIT paths; the wrong-rank and lane-0-leader controls gave 0.2-1.0M violations. In these synthetic kernels a call held 5.7-17.4 lanes on average, and often lanes from different call sites.
- **llama.cpp exit probes** (rms_norm, the largest mul_mat_q, mul_mat_vec_q, RoPE K; pp512 and tg128): **every call had 32 active lanes**; thread counts match the census exactly (e.g. mul_mat_vec_q in tg128: 3,137,339,392 threads, 98,041,856 calls). Controls: two exit groups 200 us apart make two calls of 16; groups leaving close together merge inside the probe into one call of 32 (the probe function starts with a global load, where the warp reconverges). Per-warp chunks of 32 waste nothing on these kernels.

## 8. Packaging limits (`e8-package`)

- A `.global` appended to the patched rms_norm module (trampoline and map_info included) loads with `cuLibraryLoadData`, is found by `cuLibraryGetGlobal` with its size, and reads back what was written, at 288 KiB, 2.8 MiB, 32 MiB, 256 MiB and 1 GiB (32 MiB also through the driver JIT). Free memory dropped by 0-2 MiB up to 256 MiB and by 687-844 MiB at 1 GiB.
- **Constant memory:** map_info occupies 40,968 B of bank 3; 24,568 B are left of 64 KiB (a 24,568-B `.const` compiles, 24,576 B fails).

## 9. Launch-hook coverage (`e9-hooks`)

- **pp512** (no graphs): 444 launches, all through hooked `cudaLaunchKernel`; every geometry seen (444 hook lines = nsys).
- **tg128** (graphs on): 54,272 launches, 53,848 of them in 2 graphs (314 and 110 nodes, 127 replays each). Every node's geometry was seen at a hook during capture (424 capture lines) before any replay, the largest launch (4,096,000 threads) included; kernel sets equal nsys's. Instrumentation controls (direct, node add, node set, exec set) pass.
- Not covered: geometries changed through `cudaGraphExecUpdate`, `cuLaunchKernelEx` and cooperative launches (unhooked, unused by llama.cpp).

## Second pass (06:04-06:54 JST)

The same 13 steps with the same scripts and builds, in a different order (timed steps first, before apt's 06:24 timers; checks 1, 7, 5, 8 and 4 next; the llama steps last). Every step rc 0, clocks fixed throughout. apt's daily services ran only during check 4 (06:24:18-06:24:49 and 06:28:21-06:29:07), which counts violations, not time. Full comparison: `compare-passes.md` (made by `compare_passes.py` in the work directory, from the `real-run/` directories kept here).

- **Every verdict is the same.** Checks 1, 8, 9 and the llama part of 7 printed identical verdict lines; checks 5 and 7 (standalone) differ only in the counts of their deliberately racy controls.
- **Check 2:** 508 timing rows, pass 2 / pass 1 median 0.998 (0.71-1.17); 14 rows moved by more than 10%, all contended atomics at the large shape or deltas of a few microseconds at full occupancy. Headline values moved by 0.0-1.9% (the unfenced per-warp record by 5.8%): today's op 0.433 -> 0.431 ns, record with fence 10.06 -> 9.91 ns, 8-B per-thread store in device memory 0.032 -> 0.032 ns, registered atomics on one word per SM 81.7 -> 81.9 ns.
- **Check 6:** drain rates within 1.4%; GPU slowdowns 1.48 / 11.5 / 18.0 / 39.0-39.9% in both; 2,047 MiB registers and 2,048 MiB fails in both; llama pp512 under the 40-B drain 0.03% (pass 1: 0.09%).
- **Check 3:** the same layouts violate (farafter, seppf) and pass; llama losses within 0.4 points (16 MiB per 1 ms: 3.6% / 8.3% against 3.5% / 7.9%); the relaxed-mode context-synchronize control crashed again (rc 139).
- **Check 4:** A, B, D, Am and A0 0 violations again (A 10^9 rounds in each mode); C failed 1,776 (normal) and 465,548 (stress) times in 2 x 10^8; the attribution to the fence's duration now holds in both modes.

## What this means for a per-thread map model

The model the user set on 11-15 keeps per-thread semantics and allocates storage by hardware concurrency (12,288 resident threads here), not by grid size. The checks bear on it as follows.

1. **Thread-to-slot mapping is sound within a context:** (SM, warp slot, lane) is exclusive among resident warps, also across concurrent grids, and was stable across time-slicing. The PTX ISA still allows ids to change, so a mismatch must be detected and counted, not assumed away.
2. **`%gridid` cannot tell graph replays apart**, so it cannot be a slot owner's launch tag under graphs.
3. **Per-slot state the CPU never reads belongs in device memory:** contended atomics there cost about 1 ns per op instead of about 80 ns, and a per-module `.global` works (constant memory is tight: 24 KiB left).
4. **A thread's own slot is cheap; the fence is not.** An 8-B store per thread execution on every pp512 kernel projects 3-6% at llama's shapes (device memory, no fence), against 35-41% for today's 1512 op; with a `membar.sys` it is 23-24%, and a fence costs the same whether one lane or the whole warp issues it. State the CPU never reads needs no system fence. What the CPU reads does: a record per warp with one fence per warp projects 21-34%, the same records with no fence 4-5%, but unfenced data can be read stale (check 4's control). Covering many warps' records with one fence (per chunk, per SM, at kernel end) is the open mechanism; step 0 did not test it. One leader fence per warp was safe in 10^9 rounds.
5. **Per-thread records to the CPU do not fit:** a 32-B record from every thread on every kernel projects 70-97% in device memory and 270-300% in registered memory.
6. **Delivery is bounded by the reader:** one reader drains about 7M records/s, a tenth of one record per warp in decode. Merged results must be the default; per-thread records to the CPU are an opt-in stream with counted drops (or need more readers or a batch API, which bpftime lacks).
7. **Device memory for CPU-read data is possible** (two-step copy-out, under 1% at 1 MiB per ms; or import in the reader), but an asynchronous copier breaks applications that capture in global mode.
8. **Warp-level reservation is correct**, and llama.cpp's exit probes see whole warps.
9. **Tool-written CAS is broken** in today's JIT; claims must live in helpers.

Not covered by step 0 and needed by that model: entry-to-exit state in a thread's own registers or local memory, the cost of merging or writing out at thread end, and clearing a slot before reuse.

## Files

- `run_step0_all.sh`, `logs/main-20261115-050439.log` (pass 1), `logs/main-20261115-060428.log` (pass 2): the driver and the board-state logs.
- `compare-passes.md`, `compare_passes.py`: pass 1 against pass 2.
- `instrumentation-build/0001-*.patch`: the helper and geometry-log patch (7e78a5d2).
- `e1-ids/` ... `e9-hooks/`, `common/`: one directory per check (README, sources, scripts, `real-run/`).
