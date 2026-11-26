# Device hash map, round-one checks (2026-11-26)

Checks C1-C4 of the stage-one design in `gpu/hash-design-20261126/DESIGN.md`: a device key directory with permanent slots claimed by a 64-bit CAS, and 1531-style (SM, lane) sharded rows for add-only u64 values. The rules were written before any run (`RULES-round1.md`). Nothing of the design is built in bpftime. C1 and C4 were run by delegated agents; their reports were spot-checked against their raw files.

## C1: what llvmbpf emits for hash accesses (`c1/C1-report.md`)

**Stage-one material.** Every access through a lookup result is a 64-bit atomic add with an unused result, plus null tests. There are no deletes, and inserts are `BPF_NOEXIST` only:
- cuda-counter `call_count`;
- threadscheduling's `sm_histogram` and `warp_histogram`, and the same two maps in threadscheduling_dynamic_hook (1501);
- th-hostcall's counter;
- pt_hash's counter and prefilled builds.

**Not stage-one,** with the reasons:
- cuda-counter `start_ts` and `total_time_ns`, all three cudagraph maps, `thread_mapping` (threadscheduling and dynamic_hook), gpu_shared_map `counter_per_thread`, micro `hash_map`, and pt_hash's rmw, tid, tidnd, upd and del builds;
- the reasons: plain u64 loads or stores through the result, `BPF_ANY`/`BPF_EXIST` updates, deletes, and, in two programs (cuda-counter `total_time_ns`, pt_hash rmw), the lookup pointer itself passed as an update's value;
- nothing stores a lookup result to memory, and every access is at offset 0.

**Bytecode:** the only atomic form in the 23 objects is the add without fetch (`lock *(u64 *)(rX + 0) += rY`). That holds for `__atomic_add_fetch` too, since no program uses the result.

**PTX:** the add is a generic `atom.add.u64` whose destination is dead at all 30 sites.

**Plain u64 accesses:** every one through a lookup result (9 cases) comes out as two 32-bit generic accesses. Stack accesses stay 64-bit. The likely cause, read in the code and not confirmed in the IR, is that llvmbpf builds the loads without an alignment and sets the NVPTX data layout after generating the module.

**Not covered:** gpu_shared_map, dynamic_hook and micro are not built here, so they were checked as bytecode only.

## C2: the hit path by hand on mul_mat_vec_q (`c2/`, passes c2a/c2b)

Forms of the current build's generated PTX (`c2/make_hashforms.py`; dumps in `c2/dumps.txt`), timed through the debug override. The setup is tg128 with only mul_mat_vec_q probed, two passes, base 67.93 / 67.95 t/s.
- The directory is a `.global` array of 1,024 headers in the module, initialized in the PTX; row = key, so the counts stay checkable.
- The header load is `ld.relaxed.gpu.global.b64`, which compiles to `LDG.E.64.STRONG.GPU`. The add stays `RED.E.ADD.64.STRONG.GPU`.
- Every form has 56 registers, as before.

| form | what the exit probe does | t/s (c2a / c2b) | per thread over base | exit SASS |
|---|---|---:|---:|---:|
| lat, unchanged | today's 1531 sum array (the reference) | 56.16 / 56.14 | 0.1259 / 0.1263 ns | 70 |
| lat, directory hit at the home slot | hash, one header load, compare, row from the header, shard, add | 53.64 / 53.67 | 0.1600 / 0.1598 ns | 93 |
| lat, key one slot past home | the same code; the walk's second load runs | 52.77 / 52.74 | 0.1726 / 0.1731 ns | 93 |
| lat, walk in a called function | the walk in a `.func` | 52.53 / 52.57 | 0.1761 / 0.1757 ns | 80 + 32 |
| count, unchanged (constant key) | today's 1531 add | 62.49 / 62.54 | 0.0523 / 0.0519 ns | 27 |
| count, directory hit | the header load at a constant slot, row from the header | 59.22 / 59.16 | 0.0883 / 0.0892 ns | 52 |
| count, row fixed in the PTX | the header load only decides presence | 59.34 / 59.33 | 0.0869 / 0.0872 ns | 50 |

| difference (ns per thread) | c2a | c2b | rule | outcome |
|---|---:|---:|---|---|
| directory hit - sum array (lat) | +0.0341 | +0.0334 | 0.02 < d <= 0.05: build inline; pre-bound constant keys only if they save at least 0.006 | inline |
| row fixed - row from header (count) | -0.0014 | -0.0020 | must be <= -0.006 in both passes | not built |
| called function - inline | +0.0161 | +0.0159 | inline only if at least 0.006 | inline |
| key one slot past home - at home | +0.0126 | +0.0133 | reported | one collision step |
| directory hit - sum array (count) | +0.0361 | +0.0373 | reported | |

- Every item's counts are exact: lat's sum, bucket 194 = 0, count's sum. Every override was logged.
- **A directory hit costs about 0.034-0.037 ns per thread over 1531's sum array,** for a runtime and for a constant key alike. On lat that is 56.1 -> 53.7 t/s (-17.3% -> -21.0%). Today the same histogram as a hash map would be a host call per thread.
- **Scope:** a directory of 1,024 slots with at most 65 live keys the probe touches (260 filled). Larger directories, or keys spread over more slots, were not measured.
- **What remains is the first header load and its test.** This is by elimination: no form removes the load. Fixing the row barely matters, ptxas does not fold the load of a constant slot, and a second slot's load and test add only 0.013.
- **Why it costs what it does is not established.** A strong `.gpu` load is served at the scope where all SMs agree, and whether that is L2 rather than L1 was not measured. A weak load could be cached, but it would race with the CAS. Not pursued.

## C3: SASS and registers (`c3/`)

- **Every C2 form:** 56 registers and the same local frame as today's build (`c3/*.ptxas.log`).
- **Every new form compiles for sm_87** (`c3/forms.ptx`, `forms.sass`):
  - `ld.relaxed.gpu.global.b64` -> `LDG.E.64.STRONG.GPU`;
  - `ld.acquire...` -> the same load plus `CCTL.IVALL`;
  - `atom.relaxed.gpu.global.cas.b64` -> `ATOMG.E.CAS.64.STRONG.GPU`; the acquire form adds `CCTL.IVALL`;
  - `fence.acq_rel.gpu` -> `MEMBAR.ALL.GPU; ERRBAR`;
  - `st.release` -> `MEMBAR.ALL.GPU; ERRBAR; STG.E.64.STRONG.GPU`;
  - `red.global.add.u64` -> `RED.E.ADD.64.STRONG.GPU`;
  - `nanosleep` -> `NANOSLEEP`.
- **No generic forms** appear in the hash paths.
- **sm_61:** CUDA 13.2's ptxas does not accept it at all. The trampoline's `.target sm_61` is rewritten to the device's architecture before compiling, so this does not matter here.

## C4: the claim protocol by hand (`c4/C4-report.md`, `c4/src/c4.cu`, `c4/logs/`)

A standalone CUDA test: 10^6 threads x 10 launches, 64 heavily duplicated keys. Each thread looks its key up; a hit adds 1 to its shard, a miss inserts with `BPF_NOEXIST` (reservation, CAS claim, row, add, release publish). Afterwards every header is scanned.

| run | slots claimed / max | delivered | EEXIST | E2BIG | unstable | result |
|---|---:|---:|---:|---:|---:|---|
| protocol, max_entries 128 | 64 / 128 | 9,986,331 | 5,749 | 7,920 | 0 | clean |
| protocol, max_entries 48 (16 keys never in) | 48 / 48 | 7,487,078 | 4,100 | 2,508,822 | 0 | clean |

- **No violations in any protocol run:** delivered = attempts - EEXIST - E2BIG for all 64 keys and in total, and no key sat in two slots. This includes 3,900 further trials (fresh memory, new seeds; max 128, 64 and 48; 16 keys inserted beforehand; a concurrent reader). Per the rule this reads as "consistent with the model", not as proof.
- **The bounded wait is what prevents NULL after EEXIST.** With the wait bound set to 0, 28,827 of 145,044 EEXIST results were followed by NULL from the thread's own lookup. With the wait, none were.
- **Controls:**
  - **Claim with a plain store instead of the CAS:** fails in 1/1 rule run and 200/200 trials, through wrong sums and several winners per key. It never showed "a key in two slots" (0 of 2,200 trials), so the test cannot see that form of the fault. An added control (plain store, no reservation) shows that the two-slot check itself works (2,071 cases in 200 trials).
  - **No reservation at max_entries 48:** fails as expected (64 slots against 48).
  - **Publish without release:** not observable. The final scan cannot see it by construction, and the concurrent reader was too slow to catch the window.
- **A finding for the design: E2BIG with the map half full.**
  - At max_entries 128 with 64 keys, every trial counted 7,061-11,695 E2BIG, all in the first launch.
  - The cause is that threads reserve capacity before their CAS: many threads inserting the same new key each hold a reservation until they lose. In the rule run, 252 reservations were accepted (64 winners, 188 given back) and 7,920 threads were refused.
  - Every E2BIG is counted and the sums hold, but bpf returns E2BIG only when the map is full. The order (reserve, then claim) has to change before building (below).
- **The wait bound limits sleep, not elapsed time:** waits took up to 84.3 us elapsed. That is a property to state, not a violation.

## What this decides

- **C1: stage one covers the counter maps of the hash examples.** The other maps need the store layout or stay host calls. Plain u64 accesses split into 32-bit halves, as the design assumed.
- **C2: build the hit path inline.**
  - A directory-backed counter costs about 0.034-0.037 ns per thread more than 1531's array: on lat, 53.7 against 56.1 t/s on tg128.
  - Constant keys get no separate pre-bound path.
- **C3: the forms compile as intended,** with no register growth.
- **C4: the claim, the reservation bound and the wait held** (consistent with the model, not proof). **The release publish is not verified:** its control was not observable. Two items are open:
  - **The publish ordering:** it needs a test that can see it (a faster reader, or a check inside the claiming kernel).
  - **The reservation order:** it needs a change, because E2BIG appears below capacity. Candidates for the next round (not decided):
    - walk first, and reserve only when the CAS is about to be tried on a slot that is still empty;
    - or claim first and reserve after winning, with a counted overshoot bounded by the threads in flight (the directory has twice max_entries slots).

    Either needs C4 again.
- **The user's decisions from the design still stand:** whether hash tools may declare the device hash type, the first-touch semantics, the loss channel, and R.

## Files

- `RULES-round1.md`: the rules.
- `c1/`: report, bytecode, analysis, dumped PTX and probe functions, tools.
- `c2/`:
  - `make_hashforms.py`, `sassfn.py`, `dumps.txt`;
  - the runner and analysis as they ran (`run_meas3.sh`, `analyze_h.py`, `analyze.py`);
  - `analysis-c2.txt` and `out/c2{a,b}-*`.
- `c3/`: ptxas logs, kernel SASS and probe subroutines of every C2 form, `forms.ptx`/`forms.sass`.
- `c4/`: report, source, build scripts, PTX and SASS (binaries and nvcc intermediates left on the board), logs.
