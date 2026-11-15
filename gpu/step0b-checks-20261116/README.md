# Step 0, follow-up: what the per-thread map model may rely on (2026-11-15/16)

Follow-up to `gpu/step0-checks-20261115`. Step 0 measured how the Orin behaves; several of its conclusions are behaviour of this one GPU, not documented guarantees. The rule for this round (user, 11-15): **correctness may rest only on documented guarantees** (PTX ISA, its memory consistency model, the CUDA programming guide, the driver API); measured-only behaviour may only set performance parameters, or be checked at run time with a counted fallback.

Five tasks:
- **C1** carry: can the entry probe hand state to the exit probe through the thread's own local memory, in code the dev passes produce?
- **C2** self-claim: can a thread or warp claim a storage slot by itself, so that exclusivity does not depend on what `%warpid` means?
- **C3** cost parts: where does today's per-thread map operation spend its time, what does slot addressing cost, and what do thread-end merge and write-out cost?
- **C4** batched fence: what does publishing many warps' records behind one system fence cost?
- **C5** guarantee audit: for every step-0 conclusion and every mechanism above, what do the documents actually say (verbatim quotes)? Plus the capture-mode remedy for the copier of step 0's check 3.

- **Board:** Jetson Orin, sm_87, CUDA 13.2, driver 595.78. It rebooted at 16:41 on 11-15; clocks were locked again (`jetson_clocks`, by the user) before the runs, and cpu5 read 1,728,000 kHz and the GPU 1,020 MHz around every step.
- **Runs:** two serial passes, 23:52-00:13 and 00:13-00:34 JST, every step rc 0, apt idle, nothing else on the GPU (`logs/`, driver `run_step0b_all.sh`). Correctness verdicts are the same in both passes. Two cost classifications sit on their thresholds and differ: C3's entry tag plus exit check (0.1067 ns, ">= 10%", then 0.1049 ns, "single digit"), and C4's smallest batch for two of seven protocols (384, then 1,536 warps per fence). Timed values agree within a few percent (C3: 306 items, pass 2 / pass 1 median 0.995; C4: 798 items, median 1.000, the 36 items that moved by more than 10% are all at full occupancy). Numbers below are pass 2's unless both are given. C1, C3 and C4 gate their timing against step 0's (ratios 0.98-1.18).
- **Programs:** written and adversarially reviewed by separate agents before the runs, with decision rules fixed in each README before any real run; smoke runs are not used below. Dev 52126f33 (read-only) supplied the PTX passes for C1 and C3.
- **[projection]** as in step 0: a cost measured on empty kernels, applied to every kernel of one 512-token prefill (192.56M thread executions or 6.02M warp records over 204.6 ms of GPU time). Real kernels overlap probe work with their own, so it may err either way.

## Verdicts

| task | question | verdict |
|---|---|---|
| C1 | entry-to-exit state in the thread's own local memory | **works**: 0 mismatches in every case, time-slicing included (no warp moved, so migration itself was not exercised); ptxas accepts it on a real llama.cpp module with no register change; 0.104 ns per thread execution (9.8% [projection]) with both probes, about the price of a `%warpid` slot with a tag (0.10 ns in C2 and C3), so its advantage is the guarantee, not speed |
| C2 | slots claimed by the threads themselves | **per-lane claims and warp claims with a constructed mask are correct; warp claims with an `__activemask()` mask are not** (millions of violations); by its pre-registered rule every correct claim is "large" (> 10%): the cheapest adds 0.12 ns (11%) over a `%warpid`-addressed slot, 0.214 ns (20%) in all |
| C3 | where today's op spends its time | the system fence is 63% of it (0.230 of 0.366 ns in device memory); without the fence 0.135 ns, inlined 0.091 ns; special-register addressing costs nothing measurable; a gpu-scope fence is 7x cheaper than a system fence |
| C4 | many warps behind one system fence | from 10.6 ns per warp record (one fence per warp, as step 0) to 3.3-3.8 ns at 384-1,536 warps per fence, about 3x less, but still 10-11% [projection] at anchor, not single digit; converged warps only; the chunk counter must be in device memory (98-656 ns in registered memory); no violation observed |
| C5 | what is documented | `%warpid` uniqueness across CTAs and id stability are **not** documented (the stability is explicitly denied); exclusivity must come from an atomic claim, and the slot must be carried from entry to exit; copy-out while the writer runs is **not** documented (correcting step 0); in-kernel publication to another process rests on an undocumented alias assumption; the capture-mode remedy for the copier **holds** |

## C1: carry state through the thread's own local memory (`c1-carry`)

- **Mechanism (deterministic):** a kernel-scope `.local` buffer whose generic address (`cvta.local`) is passed to every probe call as an extra argument: accepted by ptxas and sound. A module-scope `.local`: rejected by ptxas ("Module-scoped variables in .local state space are not allowed with ABI"). A helper returning the address of a `.local` in its own frame: accepted but unsound (it failed once a 2-KiB frame ran between entry and exit). The dev JIT ends the process on an unknown helper id, so in this check eBPF reaches the buffer through a map lookup on a new map type (1540) that a PTX post-pass redirects; a real design would change the passes or register a helper.
- **Correctness, both passes:** the carry and its direct form had 0 mismatches against independent ground truth in every trial: two kernels (several rets, divergent early returns) at three shapes (up to 262,144 threads x 3 launches), 1,000 graph replays, two concurrent streams sharing all SMs, and warps held between entry and exit while a second process forced time-slicing (66 gap episodes per launch, 16,896 warp-gaps, gaps of 2.5 ms; no warp moved). Controls failed as they must: a shared buffer and a lane-indexed one mismatched in every trial, the helper-frame form in thousands. Local memory is not cleared between kernels, so a mark must show the entry ran (the no-mark control read the previous kernel's values as valid).
- **Cost** at anchor, ns per thread execution including both probe calls (pass 1 / pass 2): empty probes 0.026 / 0.026 (2.5%); a value only (timer helper) 0.044 (4.1%); **carry, direct form 0.104 / 0.104 (9.8%)**; carry through the map-lookup wrapper 0.162 (15.2%); a `%warpid`-addressed device slot with an owner tag through the same wrapper 0.285 (26.8%); today's 1512 path, entry write and exit read in registered memory, 0.783 (73.7%). Without the wrapper, C2 and C3 price a `%warpid` slot with a tag at 0.097-0.105 ns, the same as the direct carry: the carry wins on guarantees, not on speed.
- **On a real module:** injected into the patched llama.cpp rms_norm module, ptxas accepts every variant. Like for like (the same probes with and without the carry), registers stay at 38 and the local frame grows 632 -> 656 B (896 B with a 256-B buffer); the step from 32 to 38 registers comes from adding the entry probe itself. Blocks per SM at 512 threads unchanged (3), with 2 registers of headroom.

## C2: slots claimed by the threads themselves (`c2-claim`)

- **Correct, both passes:** per-lane claims from four pool kinds (bitmaps and lock-free free lists, per SM or global), and warp claims whose `__shfl_sync` mask is known by construction: 0 double ownership in every case (divergent exits, two streams, graph replays, time-slicing, churn). Controls: releasing at a warp's first exit (instead of its last lane's) double-owned slots in every run; a free-list stack without a version tag hit ABA 17.7-148.5M times, the versioned stacks 0.
- **Incorrect: warp claims with `mask = __activemask()`**, 3.5-41.7M violations per pool variant. Reproducer: with a loop in the leader-only region (a retry), lanes run `__shfl_sync` with a mask that leaves them out (253,474 and 773,735 of 29.7M calls, not inlined and inlined); with a single atomic, 0. Step 0's check 7 used a single `atomicAdd` and found 0; that verdict does not extend to a claim with retries. The guide requires every named lane to execute the intrinsic with the same mask (C5, claim 9), which `__activemask()` does not ensure.
- **Cost** at anchor, the whole mechanism (claim, tag, release, per-lane exit accounting), ns per thread execution [projection]: `%warpid`-addressed slot with a tag, no claim, 0.097 (9.1%); **the cheapest correct claim, a lock-free per-SM per-lane pool, 0.214 (20.2%)**; a correct warp claim with a constructed mask 0.682 (64.2%); stack pools 4.4-35 ns. The claim's increment over the unclaimed slot is 0.117 ns (11%). The rule fixed before the run (small <= 5%, moderate <= 10% of increment) classes every correct claim as **large**.

## C3: cost parts (`c3-costparts`)

Built from the dev kretprobe pass's real output for threadhist (as step 0's reference), with variants edited from a copy. Anchor, device memory, ns per thread execution [projection]:

| level | ns | % |
|---|---:|---:|
| today's 1512 lookup-and-increment | 0.366 | 34.4 |
| without its `membar.sys` | 0.135 | 12.7 |
| also inlined (no helper call) | 0.091 | 8.6 |
| also without the probe call | 0.084 | 7.9 |
| the memory operation alone (read-modify-write) | 0.056 | 5.2 |
| an 8-B store | 0.034 | 3.2 |

- Terms: the fence 0.230 ns (21.7 points), the helper call and dispatch 0.044, the inlined lookup work 0.029, the probe call 0.007. In registered memory the fence is the same (0.236) but the read-modify-write costs 0.16 ns (uncached read).
- **Slot addressing:** reading `%smid`/`%warpid`/`%laneid` and computing the address: 0.0013-0.0020 ns (negligible); the entry clear/tag store alone 0.060 ns; an entry tag plus an exit check 0.105-0.107 ns (on the 10% line: ">= 10%" in pass 1, 9.9% in pass 2); an atomic claim and release adds 0.31 ns, 0.42 ns in all.
- **Fence scope:** per warp at anchor, `membar.gl` / `fence.acq_rel.gpu` 1.0 ns, `membar.sys` / `fence.acq_rel.sys` 6.9 ns.
- **Thread-end merge and write-out** (step 0's check-2 data, both passes, no new run), [projection] for one per warp on every kernel, device memory: a per-warp reduction then one `red` 2.2-2.7% at llama shapes (14.9% at full occupancy); a record into a per-SM ring 7.0-7.7% without a fence, 25.8-27.1% with `membar.sys`; both need converged warps (warp collectives), which exit probes cannot assume (C2), so the per-lane figure applies there: a per-lane atomic on its own word 5.4-14.4%. In registered memory every one of them costs 50-860%.

## C4: one system fence for many warps (`c4-batch-fence`)

Contributors write records, order them for the GPU (gpu-scope release), and increment the chunk's counter; the warp that completes the chunk acquires, issues one system fence and stores the chunk's flag. Anchor, 32-B record per warp, ns per warp record (pass 2):

| warps per system fence | 1 | 8 | 48 | 96 | 384 | 1,536 |
|---|---:|---:|---:|---:|---:|---:|
| `rel_ar` (literal, shared fence) | 15.10 | 11.69 | 6.54 | 4.99 | 3.68 | 3.34 |
| `rmw_ar` (the only literal acquire) | 15.13 | 11.67 | 6.67 | 5.11 | 3.82 | 3.46 |
| no fence at all (unsafe, lower bound) | 3.76 | 2.70 | 2.56 | 2.52 | 2.49 | 2.48 |

- Reference: a record plus one system fence per warp, no counter (step 0's protocol) 10.64 ns (step 0: 10.06). At one warp per fence the batched protocol costs more (15 ns: the counter and the acquire); its gain needs large batches.
- Decided output: the smallest batch whose cost stays under 2x the unfenced record (3.81 ns) is 384 warps per fence for five protocols (3.68-3.80 ns, 10.8-11.2% [projection]); for `ar_aa` and `rmw_ar` it was 384 in pass 1 and 1,536 in pass 2 (3.46-3.50 ns, 10.2-10.3%), marginal at 384. About 3x less than one fence per warp, still about 10% at anchor: step 0's "single digit only with batched publication" is not met by this batching.
- Warp collectives (a leader per warp) are used, so these figures hold for converged warps only.
- **The chunk counter must be in device memory:** in registered memory the same protocol costs 98 ns per record at one warp per fence and 656 ns at 8,192.
- Correctness: 0 violations in up to 2 x 10^8 chunk-rounds per protocol and batch size; the controls fail (no fence: 1,220 of 7.8 x 10^7 at one warp per fence, 35% of chunk-rounds at 48; flag before the records: fails at every size). As in step 0, a system fence lasts microseconds, so the zeros are "consistent with the model", not attributable to the fences.
- **Virtual aliases.** The GPU writes through the segment's device pointer and the CPU reads through its own mapping; on this board the addresses differ. The PTX model treats distinct virtual addresses of one location as aliases and documents synchronization across them only through API synchronization (kernel end, then a stream or event synchronization). Publication while the kernel runs, here and in step 0, rests on an undocumented assumption that the CPU's mapping counts as the GPU's address.

## C5: guarantee audit and capture remedy (`c5-guarantees`)

62 source pages plus the CUDA 13.2.2 driver-API archive, 78 saved pages in all (PTX ISA 9.5, CUDA C++ Programming Guide v13.5 and the v12.9 archive, CUDA for Tegra, driver API); every quote is verbatim and machine-checked against the fetched text (`run_all.sh`; the page copies are not kept here, `docs/SOURCES.tsv` and `docs/MANIFEST.tsv` list URLs and hashes). Classes: D documented, P partly, NF not found, NF-c not found and contradicted by weaker text.

| # | claim | class | what the design may rely on |
|---|---|---|---|
| 1 | `%warpid` is an SM-wide slot, unique among resident warps | NF-c (unique only within a CTA) | nothing; exclusivity from an atomic claim, the hardware slot as a hint |
| 2 | `%smid`/`%warpid` do not change while a warp runs | NF-c ("may change during execution"; "unsafe to rely upon") | nothing; compute the slot once at entry and carry it to exit |
| 3 | `%gridid` unique per launch except graph replays | P ("temporal grid launch number for this context") | a diagnostic field only; a launch id, if needed, from the design's own counter |
| 4 | one leader fence after `__syncwarp` publishes the other lanes' stores | P (D for CUDA C++ with a libcu++ system fence or release store and a valid mask, read in the same process) | the CUDA C++ form, with a constructed mask |
| 5 | flags first, payload second, two copies: payload at least as new | NF for copy against a running kernel (step 0 said otherwise; corrected), D for copy after copy | a copy ordered after the writer's completion by a stream or event edge |
| 6 | local memory survives preemption | P (thread-local storage for the kernel's execution; preemption not named) | thread state, as every CUDA program does; C1 recomputes the address at each call |
| 7 | atomics on registered memory are correct GPU-GPU and GPU-CPU | D with conditions | GPU-GPU at `.gpu` scope; GPU-CPU only at `.sys` scope with `hostNativeAtomicSupported` = 1 (1 on the Orin); CPU reads during a run must be atomic loads |
| 8 | kernel completion makes its writes visible to the host after synchronization | D for the synchronizing thread, P for a reader in another process | hand over through an in-process thread that synchronizes |
| 9 | warp intrinsics are correct with partial masks after divergent returns | NF for an `__activemask()` mask, D for a mask known by construction | per-lane paths in exit probes, or a constructed mask |
| 10 | another thread's async copy invalidates global-mode captures, not relaxed ones | P | the copier thread switches itself to relaxed mode (below); never synchronize the context |

**Capture remedy, both passes:** with the application capturing in global mode, an asynchronous copier thread invalidated 1,732-8,482 captures in its default mode; after `cuThreadExchangeStreamCaptureMode` to relaxed on the copier's own thread, 0 (verdict "REMEDY HOLDS" for the async and event-wait copiers). A context synchronize from the copier crashed the process (rc 139) in both passes, as in step 0.

## What this means for a per-thread map model

1. **Exclusivity comes from an atomic claim, not from `%warpid`.** The cheapest correct claim (per-lane, per-SM pool) costs 0.12 ns per thread execution more than a `%warpid`-addressed slot; a tag checked at exit alone is a detector, not a guarantee.
2. **The slot (or the state itself) is carried from entry to exit in the thread's own local memory**, which works in pass output, survived time-slicing (no warp moved; the documents describe local memory as the thread's storage for the kernel's execution and do not name preemption), fits a real module without lowering occupancy, and costs 0.104 ns with both probes. State that only lives from entry to exit needs no slot at all. Local memory is not cleared between kernels: the entry must mark it (a store, about 0.06 ns).
3. **Warp-level cooperation needs a constructed mask**; with `__activemask()` a claim with retries is wrong. Exit probes and `__memcapture` take a per-lane path.
4. **Device memory for slots, counters and chunk counters**; registered-memory atomics serialize (about 80 ns) and RMWs read uncached.
5. **The fence is the cost of today's op**, not the helper path; fence-free per-thread state costs 0.09-0.14 ns (8.6-12.7%), the memory access alone 0.056 ns (5.2%).
6. **What the CPU reads:** the documented route is to read after the kernel completed and an in-process thread synchronized with it; publishing while the kernel runs needs one system fence per many warps (3.3-3.8 ns per record at 384-1,536 warps per fence, about 10% [projection]) and rests on an undocumented alias assumption. The same assumption underlies today's bpftime, whose loader reads GPU maps through its own mapping while kernels run. Copy-out from device memory is sound only after the writer completed.
7. **Start-up and run-time checks:** `hostNativeAtomicSupported`, compute capability, and counted misses where a claim fails; the step-0 programs can serve as a hardware-assumption check on other GPUs, but their results only choose fast paths, never correctness.
8. **Not measured: the combination.** The pieces have prices (carry 9.8%, a claimed slot 20%, a fence-free op 8.6-12.7%, batched publication about 10%), but no check combined them, and they share costs (probe calls, address arithmetic), so they cannot be added. Step 0's 3-6% for an 8-B store is a floor, not the price of a thread's own copy under the documented-only rule.

## Files

- `c1-carry/`, `c2-claim/`, `c3-costparts/`, `c4-batch-fence/`, `c5-guarantees/`: each check's README (questions, method, decision rules, limits, review changes), sources and scripts; `real-run/` holds both passes' outputs (`compare-passes.md` in C3 and C4; `capture-remedy/real-run/` in C5).
- `run_step0b_all.sh`, `logs/`: the driver and both passes' board-state logs.
