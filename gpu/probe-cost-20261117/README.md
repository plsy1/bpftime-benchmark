# Per-thread probe cost on fine-grained kernels: attribution and design (2026-11-17)

Follow-up to `gpu/map-model-proto-20261116`. There a latency histogram (`lat`: the entry stores `%globaltimer` in the thread-local map 1530, the exit adds 1 to a log2 bucket in the sum map 1531) cost 33% of llama.cpp's tg128 throughput, about 0.28-0.30 ns per thread on mul_mat_vec_q, whose own work is about 0.41 ns per thread. At most 5% on tg128 allows about 0.031 ns per thread.

This round finds where the 0.28 ns goes, prices probe shapes, checks the documents for each way of cutting it, and writes one design. Nothing in bpftime was changed.

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

**Measure first.** Before any product code, a debug override that compiles a hand-edited patched PTX in place of the generated one, for the real kernel:
- T0: unchanged (must equal `lat`);
- the end shape with the start in today's area;
- the end shape with the start in registers, calls kept;
- the same with `.local` words;
- the same with the log2 chain instead of clz;
- the same with the probe spliced into the kernel (measurement only).

Rules fixed now (both passes, mul_mat_vec_q target 0.029 ns):
- calls kept at or below 0.029: the documented path meets the budget; build it;
- only the spliced form at or below 0.029: build the documented path, and the budget needs the user's decision on splicing;
- spliced above 0.029: build the documented path for its gain and reopen the tool shape or the budget.

## Corrections to gpu/map-model-proto-20261116

- `merge-cost/`'s "carry only" column (0.094-0.101 ns): ptxas removed that kernel's exit timer read and bucket (the bucket was compared with 99, which it cannot reach). The merge columns include the full exit and stand.
- The conclusion "it is the per-thread memory work that costs, not the probe calls" is superseded by section 1: calls and ALU instructions cost on mul_mat_vec_q too.
- step0b C1's carry price (0.104 ns) ran through bpftime's helper calls and is not affected.

## Files

- `analysis.txt`, `meas/run_meas2.sh` (as the prototype's runner, plus one-kernel tool sets), `meas/out/{a1,a2,b1,b2}-*` (driver logs, per-item logs, client and server logs, map sums).
- `tools/`: `pt_timer`, `pt_sreg`, `pt_count`, `pt_carrynolog` (new), with `pt_lat`, `pt_empty`, `pt_common.h` and the Makefile.
- `probe-shape/`: source, SASS, two passes, `analysis.txt`.
- `design/`: inputs (`measurements.md` as given to the panel, before section 1's b-passes; SASS; `facts_docs.txt`, `facts_code.txt`), the three designs, `refutation.md`, `synthesis.md`.
