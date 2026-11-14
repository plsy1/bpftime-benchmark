# e2-cost: placement and atomic cost (step 0, check 2)

Design note: `/home/jetson/src/results/gpu/dir1-design-space-20261115/README.md`, section 5, check 2:

> **Placement and atomic cost** (A, B, C, D).
> - Registered host memory against cuMemAlloc memory, at 8 x 1,536 threads and at 64 threads.
> - Cases: plain store against `atom.add.u64` per lane; one word hit by 1 to 48 warps per SM with atomicAdd and with CAS; with and without `membar.sys`; today's 1512 lookup-and-increment as the reference.
> - Decides: which reading of the 0.6 ns figure is right; whether any design the CPU can read can reach single-digit percent on the Orin; whether copy-out from device memory is required.

Status: built and smoke-tested only. **No measurement has been taken.** The smoke runs had one or two rounds, and other agents shared the GPU during some of them. The real run is `run_all.sh` on a quiet board.

## Questions

1. **The 0.6 ns figure.** threadhist on rms_norm in llama.cpp added about 7 ms per pp512 for 11,273,216 per-thread lookups and increments in registered memory. That is 0.621 ns per thread execution, or 19.9 ns per warp (census README:93). Two readings:
   - Reading 1: one record per warp costs about 0.6 ns, because writes share transactions.
   - Reading 2: one record per warp costs about 19.2 ns, because cost goes per warp memory instruction.
   Which is right on this board?
2. **Single-digit percent.** Can any storage the CPU can read reach single-digit percent overhead [projection: one record per warp on every kernel of a pp512, 6.02M records over 204.6 ms of GPU time]?
3. **Copy-out.** Is copy-out from device memory required?

## Method

Everything runs in one program, `build/bench2`, which uses only the driver API. All kernels are launched with `cuLaunchKernel`, so the cases and the reference share one launch path.

**Memory.**
- Registered: 128 MiB, made the way bpftime makes its segment (`../common/step0_regmem.h`): `shm_open("step0_e2_<pid>")`, `ftruncate`, `mmap(MAP_SHARED)`, a read-prefault of every 4 KiB page as bpftime's `prefault_range` does, `cuMemHostRegister(DEVICEMAP|PORTABLE)`, `cuMemHostGetDevicePointer`. The name is unlinked as soon as the mapping exists, so a killed run leaves nothing in `/dev/shm`.
- Device: 128 MiB from `cuMemAlloc`.
- At R = 1, no case wraps. 4,194,304 threads x 32 B is exactly 128 MiB.

**Shapes** (`--shapes`). Blocks have 512 threads, except tiny.

| name | grid | threads | why |
|---|---|---:|---|
| tiny | 1 x 64 | 64 | the 64-thread case |
| full | 24 x 512 | 12,288 | full occupancy: 8 SMs x 1,536, 3 blocks per SM, %warpid 0-47 |
| anchor | 512 x 512 | 262,144 | rms_norm's launch behind the 0.6 ns figure: 43 of its 45 pp512 launches are 512 x 512 and 2 are 1 x 512 (confirmed in `llama/census/pp512-nograph.sqlite`, CUPTI_ACTIVITY_KIND_KERNEL); also the median pp512 launch |
| large | 8,192 x 512 | 4,194,304 | the largest llama.cpp launch; warps are replaced |

**R.** Each thread does R operations, R = 1 and Rbig (tiny 64, full 16, anchor 8, large 2). Stores and records go to a fresh address in every iteration. Counters stay on their word.

**Cases** (`src/kernels.cu`). Each case comes without (`f0`) and with (`f1`) `membar.sys` after the operation, in registered (`reg`) and device (`dev`) memory.

| case | operation |
|---|---|
| st_lane32 | `st.global.u64` per lane to its own word (coalesced) |
| st_lane1 | the same store, lane 0 of each warp only |
| rec_lane0 | one 32-byte record per warp, written by lane 0 as 4 x u64 |
| rec_lane4 | one 32-byte record per warp, lanes 0-3 writing 8 B each |
| rec_thread | one 32-byte record per thread (4 x u64 per lane) |
| atom_lane | `atom.global.add.u64` per lane on its own word; the result is used |
| atom_warp_all | all 32 lanes add to one word per warp (no compiler aggregation, checked in SASS) |
| atom_warp_leader | lane 0 adds to one word per warp |
| atom_warp_leader_sys | the same with `atom.sys` |
| red_warp_leader | lane 0, `red.global.add.u64` (result unused) |
| atom_sm k | lane 0 of each warp with `%warpid < k` adds to one word per SM (`%smid % 64`, 128 B apart); k = 1, 2, 4, 8, 16, 32, 48 at full occupancy; at large only k = 48 (every warp) |
| cas_sm k | the same with a CAS increment loop (`ld.volatile` then `atom.cas`, capped at 65,536 tries, cap hits counted) |
| comp_rec | design D's record path, kept out of the decision rules: lane 0 fetch-adds the SM's cursor, lanes 0-3 write the record into a per-SM ring, `__syncwarp`, lane 0 (`f1`: `membar.sys`) then `st.relaxed.sys` of the slot's lap word |
| ref1512 | **today's 1512 lookup-and-increment, exactly as bpftime builds it** (below); `ref1512_loop` is the R-loop form at R = 1 |
| empty, ref_empty | empty kernels of the same shape, one from each module |

**The reference.** `ref/build_ref.py` calls the dev build's own kretprobe pass, `build-cuda/.../libptxpass_kretprobe.so`, through `process_input()`, as nv_attach_impl does:
- Inputs: the real eBPF of threadhist-llama (`threadhist.bpf.o`, with the map relocation resolved as libbpf does: LDDW src = 1, imm = fd) and reference kernels of the same signature.
- The pass output is wrapped with the dev tree's checked-in `trampoline_ptx.h` and the probe switch, as `hack_fatbin` does: `rewrite_ptx_target(filter_out_version_headers_ptx(TRAMPOLINE + SWITCH + passes), "sm_87")`.
- It is compiled with `ptxas -arch=sm_87`, the same compiler as bpftime's nvPTXCompiler with `--gpu-name=sm_87`.
- `map_info[5]` (registered) and `map_info[6]` (device) are set with `cuModuleGetGlobal`: type 1512, key 4, value 8, max_entries 1, max_thread_count 4,194,304, so no thread reaches the printf path.
- The per-thread path, from the SASS: exit probe -> `CALL` probe function -> probe-switch `LDG` -> `CALL _bpf_helper_ext_0001` -> map-type branches -> `getGlobalThreadId` -> `MEMBAR.SC.SYS; ERRBAR; CCTL.IVALL` -> return -> `LD.E` x2, `ST.E` x2 (the JIT splits the u64 into 32-bit halves) -> `RET`.
- 32 registers and a 632-byte stack, the JIT's 512-byte eBPF stack.

**Validation** (once per item, before timing, `items.json`):
- Atomics: the sum of the touched words equals the operation count.
- Stores: the last iteration's values are sampled (about 4,096 units).
- Reference: every slot equals R.
- atom_sm and cas_sm: every participating warp logs `(%smid, %warpid, t0, t1)`. This gives the participant count, the maximum number of participants resident on one SM at once (`k_max_concurrent`), and the maximum `%warpid`.
- Also recorded: occupancy, registers and local memory per kernel.

**Timing.**
- Per item, one batch of N back-to-back launches between two CUDA events (driver API `cuEventRecord` / `cuEventElapsedTime`, the same mechanism as cudaEvents), after one untimed launch (skipped for items over 20 ms).
- **Launch gate.** In front of the first event, a one-thread kernel `k_hold` polls a pinned, device-mapped host word until the host has queued all N launches and the second event, then exits (cap: 200 ms by globaltimer and 2^24 polls; a cap hit marks the batch `hold_capped` in `raw.csv`, and `analyze.py` drops such batches and counts them). Without the gate, a kernel shorter than the host's cost per `cuLaunchKernel` (about 3.5 us here) is timed at the host's launch rate: in a gated two-round smoke the 64-thread and full-occupancy empty kernels took 2.2 us per launch against 3.3 us ungated, and the 64-thread `st_lane32 reg f0` delta went from 0.07 us (unresolved) to 1.06 us. The anchor shape (empty 7.5 us) was the same either way. `--no-hold` restores the ungated timing.
- N is calibrated in the same run to about 4 ms per batch (1-200).
- 10 rounds; each round runs every item once in an order shuffled by `mt19937_64(seed + round)`.
- For atom_sm, cas_sm and comp_rec, the counter words are read before and after each batch, which gives the exact number of operations the batch did (`ops_batch`), whatever the scheduler did.

**Derived values** (`analyze.py`):
- delta = item time minus the empty kernel of the same shape **and module**, paired per round: `ref_empty` for `ref1512` and `ref1512_loop` (the reference module), `empty` for every other case. `ref_empty` launched about 0.5 us slower than `empty` at the full-occupancy shape in a gated smoke (0.07 us at anchor); `sanity.ref_empty_minus_empty_us` reports it.
- Per thread-op = delta / (threads x R); per warp-op = delta / (warps x R); per counted op = delta / ops_batch.
- Slope = (t(Rbig) - t(1)) / ((Rbig - 1) x warps).
- Each value has its median, min and max over rounds.

## Decision rules (written before any real run)

The thresholds are the ones in the task. They were written before the real run and were not tuned to the smoke results. One honest note: the smoke showed `membar.sys` dominating the record cost. The split of rule 2 into fenced and unfenced variants was added after seeing that, and it does not move any threshold.

All values are medians over rounds at R = 1, registered memory, shape **anchor** (the shape behind the 0.6 ns figure). large and full are reported as secondary readings and do not decide.

**Gate.**
- g = today's 1512 cost per thread execution (`ref1512 reg f1`). L = 0.621 ns.
- If g/L is outside [0.5, 2], the microbenchmark does not reproduce the llama figure. The reading verdict is then "inconclusive", and the data are reported alongside.
- If g's round min or max leaves that range, the verdict is marked "gate unstable".
- The ratio of the reference's loop form to its exit form at R = 1 is printed next to the verdict. Outside [0.8, 1.25], the reference's cost depends on how it is called, and the reading is reported for both forms. In the one-round smoke it was 0.70; an earlier two-round smoke gave 1.01.

**Rule 1: reading.**
- P = 32 g, today's per-warp cost. Q = per-warp cost of `rec_lane0 reg f1` (one 32-byte record per warp by lane 0, with its `membar.sys`).
- **Reading 1** if Q <= 2 g. **Reading 2** if Q >= 0.5 P. **In between** otherwise.
- If the round spread of Q crosses a threshold, the verdict is marked unstable.
- `rec_lane0 reg f0` (no fence) is reported as secondary.

**Rule 2: single-digit percent** [projection].
- For each record variant (rec_lane0, rec_lane4, comp_rec; f0/f1; reg/dev): overhead % = 6.02M x Q_v / 204.6 ms. 10% corresponds to Q_v = 3.40 ns per record.
- **Reachable with per-record publication** if any fenced (f1) registered variant projects < 10%.
- **Reachable only with batched publication** if only unfenced (f0) registered variants do. "Batched" means one fence per many records, or a read after the kernel.
- **Not reachable in registered memory** if none does.

**Rule 3: copy-out.**
- **Required** if every registered record variant projects >= 10% while a device-memory variant projects < 10%.
- **Required for per-record publication only** if every fenced registered variant is >= 10% while a fenced device variant is < 10%.
- Otherwise not required by this rule. Device memory still needs the copy step of check 3, which is not costed here.

**Controls that show the test can fail.**
- Every timed kernel is checked in SASS (`build/sass_check.json`): the store, atomic or fence must be inside the R loop, `f0` kernels must contain no MEMBAR, and red must not become an atom.
- Validation counts every operation.
- The empty kernels of both modules bound the launch overhead (`sanity.ref_empty_minus_empty_us`).
- `ref1512_loop / ref1512` at R = 1 checks that the loop form costs the same as the exit form.
- A delta is "resolved" only if it is positive in every round. Ratios with a device delta under 0.5 us are NA.

**Ratio (3).** Registered/device = median delta (reg) / median delta (dev), per case.

## Commands (main session, quiet board, clocks checked)

```
cd /home/jetson/src/bpftime-gpu-work/step0/e2-cost
./build.sh                 # CPU only: kernels, reference module, SASS check, bench2 (about 20 s)
./run_all.sh 10            # 10 rounds; timeout -k 5 900; results/run-<time>/
```

- `run_all.sh` records the clocks, GPU load and `/dev/shm` before and after. It writes `raw.csv`, `items.json`, `summary.csv`, `derived.json` and `summary.md`.
- `bench2 --help`-style options are in `src/bench2.cpp`: `--shapes`, `--rounds`, `--only REGEX`, `--rbig`, `--ks`, `--max-wall-s`.

**Expected wall time:** about 2.5 minutes for 10 rounds. In the one-round smoke on an idle GPU (load 0), validation plus calibration took 49 s and one round took 9.7 s. 512 items per round. The largest items are cas_sm k = 48 and atom_warp_all in registered memory at the large shape, at 0.25-1.3 s per launch.

## Known limits

- **Empty kernels.** The cost is fully exposed. In llama.cpp kernels, record writes overlap with real work, so the projection could overstate the cost, or understate it if real kernels already saturate memory. The anchor gate checks this only for today's op.
- **The projection** applies one shape's per-record cost to all 444 pp512 launches. It is labeled [projection].
- **%warpid** selects k warps per SM only without warp replacement: at full occupancy, smoke validation found k_max_concurrent = k exactly, with %warpid 0-47. At the large shape a subset depends on the scheduler (72 to 28,506 of 131,072 warps in a smoke), so only k = 48 is run there. ops_batch makes every per-op value exact anyway.
- **Atomic scope.** Only `atom_warp_leader_sys` uses `.sys` scope. The other atomics are `.gpu`, the CUDA default.
- **ptxas aggregates the per-SM atomics.** In `atom_sm` (and the cursor of `comp_rec`, and the validation counters) the address is warp-uniform (`%smid`, a parameter), and ptxas 13.2 turned the inline `atom.global.add.u64` into a warp-aggregated form: `VOTEU.ANY`, `FLO`, `UPOPC`, one `ATOMG` by the elected lane, then `SHFL.IDX` x2 and `POPC` to hand out the returned values. Only lane 0 is active there, so the number of atomics per warp is unchanged (validation counts them), but each operation carries those extra instructions on its dependency chain. `atom_warp_leader` and `atom_warp_all` are not aggregated (their address is not provably uniform; checked in `build/kernels.sass`).
- **Reading "ns per counted op".** It is the batch delta divided by every counted operation on all 8 SMs. With k warps per SM on 8 independent words, the cost per operation on one word is about 8 times that number, and at small k and R = 1 it mostly reflects one atomic's latency, not throughput (use the slope from R = 1 to Rbig).
- **Rbig.** At large Rbig is 2 (run time), so its slope is the least precise.
- **comp_rec** uses one ring per SM with 4,096 slots, and lap words are never read. It is kept out of the decision rules.
- **The reference module** is compiled by offline ptxas, not by the agent's nvPTXCompiler at run time. Both are CUDA 13.2 with the same target and default -O3. Not done: a diff against a `BPFTIME_CUDA_KEEP_PTX=1` dump of a live agent run. The old `/tmp/bpftime-fatbin-work.*` dumps predate the trampoline's last change and were not used.
- **Concurrency.** Timings are only valid with no other GPU process. GPU time-slicing changes everything.

## Changes made in review (2026-11-15, before any real run)

- Review smoke outputs (not measurements): `results/review-smokes-20261115/` (`hold`/`nohold`: the gate comparison; `g1`-`g3`, `anchor1`, `full1`, `large1`).
- Added the timing gate (`k_hold`, above) and the `hold_capped` column; `--no-hold` keeps the old timing. No threshold or rule changed.
- The reference's delta is now taken against its own module's empty kernel (`ref_empty`), not the kernels module's (`empty`).
- Documented the warp aggregation of per-SM atomics and how to read "ns per counted op".
- Review smokes, not measurements (quiet board, gated; three processes of 4 rounds, anchor): the exit form of `ref1512 reg` cost 0.430-0.438 ns per thread, the loop form 0.434-0.441 (ratio 1.01), `rec_lane0 reg f1` 10.0-10.5 ns per warp. The 0.632 of `results/smoke-full1` (loop/exit 0.70) did not recur; if the real run shows a loop/exit ratio outside [0.8, 1.25] again, run it a second time before reading the verdict.

## Files

- `src/kernels.cu`: the cases.
- `src/bench2.cpp`: the host program.
- `../common/step0_regmem.h`: bpftime-style registered memory.
- `ref/build_ref.py`: the reference module.
- `sass_check.py`: the SASS checks.
- `analyze.py`: the analysis.
- `build.sh`, `run_all.sh`: build and run.
- `build/`: cubins, the kept PTX (`build/kernels.ptx`, `build/keep/`, `build/ref/ref1512.ptx`), SASS (`*.sass`), `sass_check.json`, `ref1512.meta.json` (sha256 of the inputs).
- `results/smoke-full1/`: the one-round smoke. **Not a measurement.**
