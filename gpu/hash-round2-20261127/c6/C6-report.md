# C6: the publish order (message-passing test), RULES-round2.md

**Outcome:**
- A: 0 weak in 10^8.
- B: 0 weak in 10^8.
- C: 0 weak in 10^8.
- D: 78,461 weak in 10^8, all with writer and reader on different SMs.

Device: Jetson Orin Nano, sm_87, 8 SMs, L2 4 MiB, CUDA 13.2, driver 595.78, MAXN_SUPER, GPU clock 1020 MHz (`logs/env.txt`).

## Reading first

Written 2026-11-26 23:50 JST, after the shakedown runs and before any rule run.

Four passages were corrected after the runs (2026-11-26 23:57 JST). Each is marked **[corrected]**, and the original wording is kept in "Corrections" at the end of this section.

**Kernel paths.**
- The rules say lookups take the row from the same 64-bit word as the state.
- Checked in `r2/src/c5.cu` (read only): the header is one word `(K << 32) | (row << 3) | state` (lines 384, 469), published with `st.release.gpu.global.b64` (line 179).
- So a lookup that sees a live header has the row in the same load.
- **[corrected]** `lookup` (c5.cu lines 241-262) reads only header words with `ld.relaxed`. It returns the address of the row's shard, computed from the header, and reads no row value.
- The ops kernel (lines 505-530) only does `red_add` into that address.
- The one place in c5.cu that reads row values is the concurrent-reader check (lines 546-580). It uses `ld.relaxed` and `fence.acq_rel.gpu` on the header before summing (line 569).

**The delivery snapshot.**
- `bpftime-proto-70f7/runtime/src/bpf_map/gpu/nv_gpu_model_maps.cpp` (branch proto/map-model-70f7bc1f, 7d02d677), lines 139-189: `bpftime_model_snapshot` sums shards with `ld.relaxed.gpu.global.u64` and has no fence.
- It sums array-map shards today; it reads no hash headers. For the hash map it would need, per header, an acquire pattern before summing that header's row (the rules' requirement).
- A missing acquire would show as a key with sum 0 in a mid-run snapshot.

**The guarantee.** PTX ISA 9.5, local text copy `bpftime-gpu-work/step0b/c5-guarantees/docs/text/ptx-isa.txt`.

**[corrected]** The quotes below are verbatim, one line of the text each. Line numbers are in the local copy.

- 8.8, release patterns (lines 7395, 7397, 7399): "A release pattern on a location M consists of one of the following:", then "A release operation on M", then "E.g.: st.release [M]; or atom.release [M]; or mbarrier.arrive.release [M];".
- 8.8 (line 7417): "Any memory synchronization established by a release pattern only affects operations occurring in program order before the first instruction in that pattern." The writer's `red` is before the `st.release` in program order.
- 8.8, acquire patterns (lines 7419, 7429, 7431): "An acquire pattern on a location M consists of one of the following:", then "Or a strong read on M followed by an acquire memory fence in program order", then "E.g.: ld.relaxed [M]; fence.acquire; or atom.relaxed [M]; fence.acquire;".
- 8.8 (lines 7443, 7445): "Note that while atomic reductions conceptually perform a strong read as part of its read-modify-write sequence, this strong read does not form an acquire pattern." and "E.g.: red.add [M], 1; fence.acquire; is not an acquire pattern." This is about the reader side; the C6 reader's strong read is an `ld`.
- 8.9.4 (line 7487): "A release pattern X synchronizes with an acquire pattern Y, if a write operation in X precedes a read operation in Y in observation order, and the first operation in X and the last operation in Y are morally strong."
- **[corrected] The fence used, `fence.acq_rel.gpu`.**
  - The fence instruction's description (line 26369): "fence.acq_rel is a light-weight fence that is sufficient for memory synchronization in most programs. Instances of fence.acq_rel synchronize when combined with additional memory operations as described in acquire and release patterns in the Memory Consistency Model."
  - The operation table (line 7229) defines a memory fence operation as "A membar, fence.sc or fence.acq_rel instruction."
  - The 8.8 acquire example uses `fence.acquire`. The text quoted here does not say in so many words that `fence.acq_rel` is an "acquire memory fence". Treating `ld.relaxed` + `fence.acq_rel.gpu` as the acquire pattern is the rules' reading, and the same form as c5.cu's reader.

**[corrected]** Under that reading, variant A (st.release publish, reader `ld.relaxed` then `fence.acq_rel.gpu`) is the release/acquire pair of 8.8 and 8.9.4. B and C each drop one half of it; D drops both.

**Corrections (2026-11-26, after the runs).** Original wording, replaced above:
1. "No GPU code in c5.cu reads a row's values except the concurrent-reader check (lines 546-569)": it rested on a grep of helper definitions, not call sites. Replaced by what the lookup and ops code were read to do.
2. The two 8.8 quotes were stitched, with inserted periods and trimmed examples: "A release operation on M. E.g.: st.release [M]" and "a strong read on M followed by an acquire memory fence in program order. E.g.: ld.relaxed [M]; fence.acquire". Replaced by verbatim lines.
3. "So variant A ... is the pattern the document guarantees": it went beyond the quoted text, which names `fence.acquire`, not `fence.acq_rel`. Now attributed to the rules' reading, with the fence text quoted.
4. "B, C and D each drop one half of it": D drops both halves.

## The test (what was built)

`src/c6.cu`, one source. `-DVARIANT=A|B|C|D` selects the rules' variants. `S` and `SD` are harness checks outside the rules table.

| build | writer (after a seeded delay) | reader after seeing flag = 1 |
|---|---|---|
| A | `red.global.add.u64 [data], 1; st.release.gpu.global.b64 [flag], 1;` | `fence.acq_rel.gpu; ld.relaxed.gpu.global.b64 data` |
| B | `red.global.add.u64 [data], 1; st.relaxed.gpu.global.b64 [flag], 1;` | `fence.acq_rel.gpu; ld.relaxed.gpu.global.b64 data` |
| C | `red.global.add.u64 [data], 1; st.release.gpu.global.b64 [flag], 1;` | `ld.relaxed.gpu.global.b64 data` |
| D | `red.global.add.u64 [data], 1; st.relaxed.gpu.global.b64 [flag], 1;` | `ld.relaxed.gpu.global.b64 data` |
| S (check) | `st.relaxed.gpu.global.b64 [flag], 1; red.global.add.u64 [data], 1;` (order swapped) | `ld.relaxed.gpu.global.b64 data` |
| SD (check) | as S, with a 4096-cycle busy wait between the two writes | `ld.relaxed.gpu.global.b64 data` |

**Accesses.**
- In the test kernel (`c6_kernel`), every access to data, flag and the pacing words is inline PTX on a `cvta.to.global` address.
- The tally/reset kernel (`c6_tally`) reads and zeroes data and flag with plain C++ loads and stores, and the host zeroes the regions once with `cudaMemset`. Both are separated from the test kernel by kernel boundaries on the same stream.

**Reader.**
- It spins on `ld.relaxed.gpu.global.b64 flag` until the value is not 0, for at most 2^20 polls. A run out of polls is counted as a timeout.
- Value 1: the variant's acquire form, then `ld.relaxed.gpu.global.b64 data`.
- Weak outcome: flag 1, data 0.

**Threads.**
- 96 blocks of 128 threads: 12 per SM, which is the occupancy limit, so all blocks are resident at once.
- 64 test blocks; lane 0 of each warp is a writer or a reader, so 256 warps form 128 pairs per launch. The other 31 lanes exit.
- Each launch pairs warps at random (seeded), never with writer and reader in the same block.
- 32 stress blocks (4,096 threads).

**Rounds.**
- A pair runs up to 256 instances per launch, each on its own cells.
- Before round k, the writer waits until the reader has stored "spinning on round k" (bounded by 2^20 polls). It then busy-waits a seeded 0-1023 cycles and does its two writes.

**Layout.**
- data and flag are two separate `cudaMalloc` regions of 4 MiB. The run checks at start that each is 4 KiB-aligned and that they are disjoint, so an instance's data word and flag word are never in the same 4 KiB page.
- The two regions are adjacent: the logs show `data=0x19f032000 flag=0x19f432000`, so flag starts where data ends.
- One cell per 128-byte line. Each launch assigns cells with a fresh random permutation, separately for data and flag, so neighbouring instances get unrelated lines.
- The L2 slice hash is not known and was not measured, so no slice separation is claimed.

**Stress.**
- Stress threads hammer a separate 16 MiB region: random lines, half `red.global.add.u64`, a quarter `st.relaxed.gpu`, a quarter `ld.relaxed.gpu`.
- They start before the test lanes (the test lanes wait, bounded, for all stress threads to arrive) and stop when every reader is done (bounded at 2^22 operations).

**SM.** `%smid` (SASS `S2R SR_VIRTUALSMID`) is read per instance by the writer before its writes and by the reader before its first poll.

**Tally and reset.**
- A second kernel on the same stream classifies every instance and checks that the final data and flag words are both 1. It zeroes cells and records for the next launch.
- The host checks every launch exactly: strong + weak + timeout + bad flag + missing = instances launched.
- At the end, a scan checks that every data and flag word is 0.

**Stop.**
- 10^8 instances (781,250 rounds per pair x 128 pairs) or 600 s from the first launch, whichever first.
- Each run is one fresh process: `flock <gpu.lock> timeout 900 ./build/c6_<v>` (`run.sh`).
- Exit 2 on a weak outcome in A, 3 on a harness error.

## Forms confirmed (frozen build, `logs/sass-check.txt`, `logs/env.txt`)

**PTX diffs.** Code only, ignoring `.loc` and the `-lineinfo` name strings:
- A to B: exactly `st.release.gpu.global.b64` to `st.relaxed.gpu.global.b64`.
- A to C: exactly `fence.acq_rel.gpu` removed.
- A to D: both.

**SASS of `c6_kernel`.** Instruction text copied from `logs/sass-check.txt`:
- Writer, A and C: `RED.E.ADD.64.STRONG.GPU`, `MEMBAR.ALL.GPU`, an address `LEA` (for the smid record), `ERRBAR`, `STG.E.64.STRONG.GPU`.
- Writer, B and D: `RED.E.ADD.64.STRONG.GPU`, one `IADD3`, `STG.E.64.STRONG.GPU`.
- Reader, A and B: the poll loop `LDG.E.64.STRONG.GPU` / `ISETP` / `@!P0 BRA P1` back to the load, then `BSYNC`, compares and predicated moves, then `MEMBAR.ALL.GPU; ERRBAR; CCTL.IVALL; LDG.E.64.STRONG.GPU` (data).
- Reader, C and D: the same loop, then compares and two predicated moves, then `@!P1 LDG.E.64.STRONG.GPU` (data).
- No memory instruction lies between the flag load that ends the loop and the data load.
- MEMBAR count in `c6_kernel`: A 2, B 1, C 1, D 0, S 0, SD 0.
- No generic `LD`/`ST`/`ATOM` in any variant.
- S: `STG.E.64.STRONG.GPU`, one `IADD3`, `RED.E.ADD.64.STRONG.GPU`. SD: `STG.E.64.STRONG.GPU`, the `CS2R SR_CLOCKLO` loop to 0x1000 cycles, a `LEA`, `RED.E.ADD.64.STRONG.GPU`.
- ptxas: `c6_kernel` uses 28 registers, no stack, no spills, in every variant.

## Shakedown (not rule runs)

Labelled `logs/shakedown-*.log`, 10^6 instances each (1,000,064 with the per-pair rounding).
- `shakedown-1..4` used an earlier build: smid recorded once per warp, no stress-op counter.
- `shakedown-5-*-build2` used the frozen build (same sha256 as `logs/env.txt`).
- Results are reported in the results section below for completeness. They do not count toward the stop rule.

## Results

**Runs.** Each run was one process under the GPU lock with `timeout 900`; `run_all.sh` ran them in order between 23:51:35 and 23:52:41 JST.
- Every log's binary sha256 matches `logs/env.txt`.
- No `c4_|c5_|c6_|llama|pt_victim` process was listed before any run (log headers).
- The tables come from `summarize.py`, which reads only `logs/<run>.log`.

### Outcome per the stop rule

Each rule run stopped at the instance target, 10^8 instances, well inside 600 s.

| variant | publish / reader | outcome | log |
|---|---|---|---|
| A | release / fence | **0 weak in 100,000,000** (flag seen in all 100,000,000) | `logs/rule-A.log` |
| B | relaxed / fence | 0 weak in 100,000,000 | `logs/rule-B.log` |
| C | release / no fence | 0 weak in 100,000,000 | `logs/rule-C.log` |
| D | relaxed / no fence | **78,461 weak in 100,000,000**: same-SM 0 of 11,151,702, different-SM 78,461 of 88,848,298 | `logs/rule-D.log` |

- A had no weak outcome, so the rule "a weak outcome in A stops the build" did not trigger (exit 0).
- The rules define "not observable on this device by this test" only for the case where B, C and D all show none. That case did not occur, because D showed weak outcomes.
- The rules have no branch for "D weak, B and C none".

### Counts

| run | instances | flag seen | weak same-SM | weak diff-SM | timeouts | same-SM inst | diff-SM inst | saw 0 then 1 | seen on first poll | stress ops | wall s | exit |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| rule-A | 100,000,000 | 100,000,000 | 0 | 0 | 0 | 11,151,702 | 88,848,298 | 99,557,731 | 442,269 | 9,614,274,560 | 17.42 | 0 |
| rule-B | 100,000,000 | 100,000,000 | 0 | 0 | 0 | 11,151,702 | 88,848,298 | 76,653,219 | 23,346,781 | 9,599,995,904 | 17.73 | 0 |
| rule-C | 100,000,000 | 100,000,000 | 0 | 0 | 0 | 11,151,702 | 88,848,298 | 99,952,636 | 47,364 | 7,188,492,288 | 14.01 | 0 |
| rule-D | 100,000,000 | 100,000,000 | 0 | 78,461 | 0 | 11,151,702 | 88,848,298 | 97,118,082 | 2,881,918 | 6,399,991,808 | 12.80 | 0 |

Column notes:
- "wall s" is the binary's own clock, from the first launch to the last.
- Same-SM and diff-SM instance counts are equal across the four runs because all runs use the same seed (0xc6c6), so they get the same pairings and cell permutations.
- "saw 0 then 1": the reader's poll read 0 at least once before it read 1.
- "seen on first poll": the first poll already returned 1. These are still valid instances: flag 1 was read and then data was read.

Harness-error counters (every column after "mean polls" is 0 in every run):

| run | launches | mean polls | bad flag | bad data | missing record | final word not 1 | smid bad | same-block pairs | writer wait timeouts | barrier timeouts | stress op cap | nonzero words after the final scan |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| rule-A | 3,052 | 5.32 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| rule-B | 3,052 | 2.04 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| rule-C | 3,052 | 6.75 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| rule-D | 3,052 | 2.65 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

**SM pairs, rule-D.** From the matrix in `logs/rule-D.log`:
- All 8 diagonal cells (writer and reader on the same SM, 1,371,586 to 1,408,644 instances each) have 0 weak.
- Each of the 56 off-diagonal cells has between 1,268 and 1,493 weak.
- In rule-A the matrix has 0 weak in all 64 cells.

**Weak entries, rule-D.**
- The log prints the first 40 of 4,096 kept entries.
- All 40 are on different SMs.
- All 40 have polls of 2 to 7, so in each the reader read flag 0 before it read 1. None was seen on the first poll.

### Harness checks (outside the rules table; frozen build, 10^7 instances each)

| run | writer order | weak (same-SM / diff-SM) | timeouts | log |
|---|---|---|---|---|
| check-SD | flag, 4096-cycle wait, add | 5,991,449 (726,250 / 5,265,199) | 0 | `logs/check-SD-1e7.log` |
| check-S | flag, add (no wait) | 10,935 (7 / 10,928) | 0 | `logs/check-S-1e7.log` |

- The counting path reports weak outcomes when the writer's order makes them possible.
- With the two writes swapped back to back (S), the reader caught the gap in 10,935 of 10^7 instances.

### Shakedown (not rule runs)

10^6 instances each (1,000,064 with rounding). All had 0 timeouts and 0 harness errors.

| log | build | weak (same-SM / diff-SM) |
|---|---|---|
| shakedown-1-A-1e6 | earlier | 0 |
| shakedown-2-SD-1e6 | earlier | 605,348 (77,162 / 528,186) |
| shakedown-3-S-1e6 | earlier | 1,570 (0 / 1,570) |
| shakedown-4-D-1e6 | earlier | 1,173 (0 / 1,173) |
| shakedown-5-A-1e6-build2 | frozen | 0 |
| shakedown-5-B-1e6-build2 | frozen | 0 |
| shakedown-5-C-1e6-build2 | frozen | 0 |
| shakedown-5-D-1e6-build2 | frozen | 730 (0 / 730) |
| shakedown-5-S-1e6-build2 | frozen | 1,219 (0 / 1,219) |
| shakedown-5-SD-1e6-build2 | frozen | 606,232 (77,066 / 529,166) |

The D weak outcomes were first seen in shakedown, before the "Reading first" section was written. The build was not changed because of them. The only change between the two builds is per-instance smid recording plus two counters (stress ops, same-block pairs).

## Limits of this test

- One device (Orin Nano, 8 SMs), one stress pattern, one layout (128-byte cells, two 4 MiB regions), one seed. 10^8 instances per variant.
- The L2 slice of each word is not known.
- Zero weak outcomes in A, B and C are counts from this test, not proofs.

## Files

- `src/c6.cu`: the test (`-DVARIANT=A|B|C|D|S|SD`).
- `build.sh`: nvcc `-arch=sm_87 -O3 -lineinfo -std=c++17 -Xptxas -v --keep`. It writes `build/c6_<v>`, `.ptx`, `.sass`, `ptxas-<v>.txt`.
- `run.sh <log> <variant> [args]`: one run under `flock <gpu.lock> timeout 900`. It never overwrites a log.
- `run_all.sh`: the frozen runs (check-SD, check-S, rule-A..D).
- `sass_check.py`: forms and PTX diffs, output in `logs/sass-check.txt`.
- `summarize.py`: the tables above, from the logs.
- `logs/env.txt`: versions, clocks, and sha256 of the source, scripts, binaries, PTX and SASS at the freeze.
- `logs/rule-{A,B,C,D}.log`, `logs/check-{S,SD}-1e7.log`, `logs/shakedown-*.log`, `logs/run_all.out`.
