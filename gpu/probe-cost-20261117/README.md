# Per-thread probe cost on fine-grained kernels: attribution and design (2026-11-17)

Follow-up to `gpu/map-model-proto-20261116`. There a latency histogram (`lat`: the entry stores `%globaltimer` in the thread-local map 1530, the exit adds 1 to a log2 bucket in the sum map 1531) cost 33% of llama.cpp's tg128 throughput, about 0.28-0.30 ns per thread on mul_mat_vec_q, whose own work is about 0.41 ns per thread. At most 5% on tg128 allows about 0.031 ns per thread.

This round finds where the 0.28 ns goes, prices probe shapes, checks the documents for each way of cutting it, writes one design, and measures its end shapes on the real kernel. The only bpftime change is a debug-only hook for that measurement.

Board and settings as before: Orin Nano 8 GB (sm_87, 8 SMs), clocks fixed and checked before every item, prototype build c940870b, llama-bench TinyLlama Q4_K_M `-r 5` with warmup.

## 1. Where the 0.28 ns goes (real kernel)

tg128 with probes on the largest mul_mat_vec_q only (3,137,339,392 thread executions per run). Four passes, two items in common (`analysis.txt`, `meas/out/`). ns per thread = (time per run - base) / thread executions.

| tool variant | what the probes do | t/s | added per thread |
|---|---|---:|---:|
| base | no agent | 67.6-68.0 | - |
| empty2 | entry and exit return at once | 65.1-65.8 | 0.017-0.026 ns |
| sreg | each probe calls helper 511 (`%laneid`) | 62.4 | 0.054 ns |
| timer | each probe calls helper 502 (`%globaltimer`) | 62.0-62.4 | 0.051-0.057 ns |
| carrynolog | timer + start stored in 1530 and read back, raw delta compared, no bucket | 56.2 | 0.126 ns |
| carryonly | + the log2 bucket | 53.5-53.6 | 0.157-0.162 ns |
| count | empty entry; exit: 1531 lookup with a constant key through helper 1, atomic add | 53.9-54.0 | 0.153-0.154 ns |
| lat | full tool | 46.0-46.1 | 0.282-0.286 ns |

Counts: the sum map held exactly 15,711,207,424 (5 runs + the warmup token) for `count` and `lat` in both passes.

Parts:
- two empty probe calls (with the switch load): 0.017-0.026 ns;
- two timer helper calls: about 0.035 ns. Calling helper 511 instead costs the same, so this is the call itself; the `%globaltimer` read costs nothing measurable;
- carrying the start (1530 lookup, the key's stack round trip, two 32-bit generic stores, four 32-bit generic loads): about 0.072 ns;
- the log2 chain (about 30 ALU instructions): about 0.035 ns;
- the 1531 lookup through helper 1 and the atomic add: about 0.135 ns.

The probe's instructions are not hidden behind mul_mat_vec_q's memory traffic: the log2 chain is ALU work only and costs about 1 ps per instruction per thread; calls cost more. This replaces the prototype README's "it is the per-thread memory work that costs, not the probe calls".

What runs today per thread (SASS of the patched kernel, `design/probe-sass-excerpt.txt`): about 210 instructions. Entry: switch load, key stored to the eBPF stack and read back, call to the timer helper, two 32-bit stores of the start. Exit: switch load, key round trip, four 32-bit loads of the start, call to the timer helper, 30-instruction log2, bucket key to the stack, call to helper 1 (a compare chain on the map type, several constant loads, the shard address), `isspacep` guard, generic atomic add.

## 2. Probe shapes, hand-written (`probe-shape/`)

ns per thread added over the same kernel without probes, two passes of 20 rounds. `mem` reads 16 bytes per thread (a memory-bound stream); `mmvq` is mul_mat_vec_q's largest launch shape (5,632 x 128).

| shape | empty kernel, mmvq launch | mem kernel, mmvq launch |
|---|---:|---:|
| base | 0.047 | 0.165 |
| two noinline empty calls | +0.003 | +0.000 |
| noinline, start in local memory, no add | +0.070-0.071 | +0.018-0.019 |
| the same + add to an (SM, lane) shard | +0.123-0.125 | +0.060-0.073 |
| noinline, start returned by the entry and passed to the exit, + shard add | +0.010 | +0.000 |
| inline, start in a register, + shard add | +0.009-0.017 | +0.000 |
| the same + warp combine (activemask, match.any) | +0.006 | +0.000 |

The order of the shapes is useful; the amounts do not carry over: on the real kernel two empty calls cost 0.017-0.026 ns, here +0.003 and +0.000. Warp combine gains little over (SM, lane) shards.

## 3. What the documents allow (`design/facts_docs.txt`)

- Warp combine needs every lane named in an `activemask` result to hold the same mask. No document states that, and step0b C2 measured disagreement on this board. Not used.
- Passing the start as a call argument or return value: arguments are pass-by-value (PTX ISA 9.2, `call`); whether they travel in registers is up to the compiler, which affects speed only.
- `%globaltimer`: documented only as a 64-bit nanosecond timer intended for NVIDIA tools. Reading it inline rests on the same text as the helper does.
- An 8-byte access needs a naturally aligned address, or the behaviour is undefined. The alignment of the local window's base is not stated, so 8-byte generic accesses into local memory are not on the main path.
- No document says how to make ptxas inline a function, or that a call and its spliced body behave the same.

## 4. Design (`design/synthesis.md`, adjusted for section 1)

Three independent designs, one review that tried to refute every step, and one synthesis. In order:

| step | removes | where | basis | expected ns per thread after it |
|---|---|---|---|---:|
| fix the constant-fd test | nothing (a misread fd would apply another map's layout) | routing | none new | 0.284 |
| timer inline | two helper calls; a reload of the start | llvmbpf GPU lowering | same register as the helper | about 0.25-0.26 (measured share 0.035) |
| switch read once per kernel | one global load per probe call | routing (kernel prologue) | same plain load; the switch contract holds | about 0.25 |
| 1531 lookup inline (constant fd) | the call to helper 1 and its type dispatch | llvmbpf GPU lowering + map facts in the request | same arithmetic as the helper | about 0.16-0.21 |
| no `isspacep` on those adds | the guard | lowering tags the add, routing skips it | today's premise (device memory is not local) checked once instead of per add; or keep a one-instruction `isspacep.global` check | about 0.15-0.20 |
| start carried by value | the area, the key round trip, the 32-bit generic accesses | lowering (arguments, return value) + routing | pass-by-value; private `.local` with `ld.local`/`st.local` as fallback | about 0.09-0.17 |
| clz bucket (one line in the tool) | the 30-instruction log2 (measured 0.035) | new helper 512, lowered to `clz` | `clz.b64` is defined for every input; same buckets | about 0.06-0.15 |

- About 650-950 lines in all, estimated.
- Expected end with the probe calls kept, tg128 with mul_mat_vec_q probed alone (64.6 t/s at 5%): 0.06-0.15 ns per thread from the steps above (54-62 t/s); 0.05-0.09 counting the roughly 60 instructions left at today's cost per instruction (59-63 t/s). The estimate rests on a per-instruction cost that is not measured on this kernel.
- Not on the main path: splicing the probe body into the kernel (no document covers the equivalence), warp combine (above), `.global` conversions of the shard address (window membership not stated), `call.uni`.
- Open for the user: whether to accept splicing as a derivation if the budget needs it; the one-line tool change for clz; threadhist (1532) needs its own design, since its cost is bytes per thread.

The expected ends above were estimates; section 5 measured the end shapes on the real kernel, and they replace them.

## 5. The end shapes on the real kernel (`target-shape/`)

A debug-only hook (`patches/0001`, local commit c27412f4 on proto/map-model) compiles a hand-edited patched PTX in place of the generated one (`BPFTIME_CUDA_PTX_OVERRIDE_DIR`, keyed by the generated PTX's sha256). `make_variants.py` edits only mul_mat_vec_q's call sites and adds new probe functions. Every variant keeps the probe switch (read once per kernel), the no-start bucket, the sum map's NULL conditions and the add; the end shape reads `%globaltimer` inline, takes `63 - clz(delta | 1)`, computes the (SM, lane) shard address from `map_info` and checks `isspacep.global` before the generic `atom.add`. Rules were fixed before the runs (`target-shape/DECISION-RULES.md`, with an addendum before d1/d2).

tg128, mul_mat_vec_q only, passes c1/c2 (`target-shape/analysis.txt`):

| variant | registers | t/s | s per run | added per thread |
|---|---:|---:|---:|---:|
| base (no agent) | 56 | 67.7-67.8 | 1.89 | - |
| empty probes | | 65.9 | 1.94 | 0.016-0.018 ns |
| lat, generated | 56 | 46.1 | 2.78 | 0.282-0.284 ns |
| t0: lat through the hook, unchanged | 56 | 46.1 | 2.78 | 0.282-0.284 ns |
| area: end shape, start in today's area | 64 | 50.6 | 2.53 | 0.204-0.205 ns |
| chain: start by value, 30-instruction log2 | 64 | 53.5-53.6 | 2.39 | 0.158-0.161 ns |
| reg: start by value (register) | 64 | 54.3-54.4 | 2.35 | 0.148 ns |
| loc: start in a kernel `.local` word | 56 | 56.7 | 2.26 | 0.116-0.118 ns |
| inline: probe bodies spliced, no calls | 64 | 57.1 | 2.24 | 0.111-0.113 ns |
| regcap: reg with `.maxnreg 56` | 48 | 58.7 | 2.18 | 0.092-0.093 ns |
| inlinecap: inline with `.maxnreg 56` (passes e1/e2) | 48 | 61.4 | 2.08-2.09 | 0.062-0.064 ns |

Every variant counted every exit (15,711,207,424) with 0 in the no-start bucket, and logged the override.

Passes d1/d2 explain regcap: the kernel without probe calls, compiled in the same module, runs at 67.4-67.9 t/s with 56 registers (`none`) and at 70.1 t/s with the cap (`nonecap`, 48 registers): the cap alone makes mul_mat_vec_q 0.019-0.023 ns per thread (about 3%) faster. That is a side finding about the kernel, not a probe saving. Against `nonecap` regcap's probes cost 0.113-0.114 ns per thread (d1/d2: regcap 0.093-0.095, loc 0.117-0.118).

Passes e1/e2 (addendum 2) cap the spliced form too: against `nonecap` in the same passes, inlinecap costs 0.083 ns per thread and regcap 0.114. At equal registers (reg and inline, both 64; regcap and inlinecap, both 48) removing the calls saves about 0.031-0.036 ns per thread.

Probe cost at equal registers, best forms:

| form | against the same kernel without probes | against the application as shipped |
|---|---:|---:|
| calls kept, start in a `.local` word (56 registers) | 0.116-0.118 ns | 56.7 t/s, 2.26 s per run |
| calls kept, start in a register, cap 56 (48 registers) | 0.113-0.114 ns | 58.6-58.7 t/s, 2.18 s per run |
| spliced, cap 56 (48 registers) | 0.083 ns | 61.4 t/s, 2.08-2.09 s per run |

Verdicts (rules fixed before the runs, both passes):
- D1, calls kept at or below 0.029 ns: **not met** (best 0.092; 0.113 at equal registers).
- D2/D3: spliced 0.111-0.113 uncapped, 0.083 against `nonecap` capped (addendum 2), both > 0.029: **D3**. This tool shape (two timer reads, a bucket and one add per thread) cannot reach 5% on mul_mat_vec_q on this board, spliced or not. These are hand-written end shapes, so they bound what the documented path can reach: at best 0.284 -> about 0.11-0.12 ns per thread, 46 -> about 57 t/s (-32% -> about -16%); the real pipeline (LLVM lowering, the eBPF stack, the map facts) may not reach them. The 5% target needs another tool shape or another budget. What the remaining 0.08-0.12 ns consists of was not measured.
- D4, clz: chain - reg = 0.011-0.013 >= 0.01: build it.
- D5, register vs `.local`: reg (64 registers) is slower than loc; regcap beats loc only through the register cap, which speeds up the kernel itself. Carry the start in a `.local` word unless the registers stay at the kernel's own count.
- D6: area - reg = 0.056 >= 0.03: carrying the start by value comes before inlining the lookup.
- Splicing the probe into the kernel saves about 0.031-0.036 ns per thread at equal registers (it is not documented to behave like the call; section 3). It does not change the verdict.
- Register count moves the result as much as the probe's instructions do (reg 0.148 against loc 0.117 and regcap 0.093). A product must check registers per kernel (as the design's mode rule said); mul_mat_vec_q sits at the 36-warp boundary.

## Corrections to gpu/map-model-proto-20261116

- `merge-cost/`'s "carry only" column (0.094-0.101 ns): ptxas removed that kernel's exit timer read and bucket (the bucket was compared with 99, which it cannot reach). The merge columns include the full exit and stand.
- The conclusion "it is the per-thread memory work that costs, not the probe calls" is superseded by section 1: calls and ALU instructions cost on mul_mat_vec_q too.
- step0b C1's carry price (0.104 ns) ran through bpftime's helper calls and is not affected.

## Files

- `analysis.txt`, `meas/run_meas2.sh` (as the prototype's runner, plus one-kernel tool sets), `meas/out/{a1,a2,b1,b2}-*` (driver logs, per-item logs, client and server logs, map sums).
- `tools/`: `pt_timer`, `pt_sreg`, `pt_count`, `pt_carrynolog` (new), with `pt_lat`, `pt_empty`, `pt_common.h` and the Makefile.
- `probe-shape/`: source, SASS, two passes, `analysis.txt`.
- `design/`: inputs (`measurements.md` as given to the panel, before section 1's b-passes; SASS; `facts_docs.txt`, `facts_code.txt`), the three designs, `refutation.md`, `synthesis.md`.
- `target-shape/`: `make_variants.py` (the variants, from the dumped PTX whose sha256 is in `dump-sha256.txt`; the 11.6 MB PTX files are not kept), `DECISION-RULES.md`, `analysis.txt`, `variants-info/` (registers, ptxas logs, SASS of mul_mat_vec_q per variant); `meas/run_meas3.sh` (items `tool@variant`), `meas/out/{c1,c2,d1,d2,e1,e2}-*`; `patches/0001` (the hook).
