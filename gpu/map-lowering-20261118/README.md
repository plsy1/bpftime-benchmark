# The map model's lowering, built: sum-map adds and lookup keys (2026-11-18)

Follow-up to `gpu/probe-cost-20261117`, section 7. There, hand-written shapes showed that the sum map's (1531) add path could be made much cheaper on documented guarantees. Here the map side of that design is built into the prototype (branch `proto/map-model`, five commits on c27412f4, `patches/`), and the thread-local map's (1530) side is measured and built where it pays. The tool (`lat`: entry stores `%globaltimer` in 1530, exit adds 1 to a log2 bucket in 1531) is unchanged. What is left outside the map (probe calls, the switch read in every probe, the timer helper calls, the log2) belongs to the tool and the framework, and is not touched (user, 11-18).

Board and settings as before: Orin Nano 8 GB (sm_87, 8 SMs), clocks fixed and checked before every item, llama-bench TinyLlama Q4_K_M `-r 5` with warmup, items shuffled per pass. Rules written before each set of passes: `meas/RULES-sum-map-build.md`.

## What was built (`patches/`)

1. **Sum-map lookups inline, their adds on the global state space** (026f490f):
   - A lookup of a sum map with a constant map fd, in any probe function, becomes the calling thread's shard address. This is the trampoline's `sum_array_offset`, with the map's geometry and device copy written in as constants.
   - A 64-bit add whose address register only such lookups write becomes `cvta.to.global` + `atom/red.global`. ptxas then emits `RED` where the generic atomic was an `ATOM.E` with a predicate that a return depends on.
   - Basis: `cuMemAlloc` "Allocates device memory"; global memory is device memory (programming guide); a generic address outside the const, local and shared windows maps to global memory (PTX ISA 6.4.1.1).
   - Other generic atomics keep the `isspacep.local` wrapper, now with an `isspacep.global` branch.
   - The delivery's snapshot kernel reads through `ld.relaxed.gpu.global` on the same address, so it stays morally strong with the adds (PTX ISA 8.7).
2. **Fixes after an independent review** (00f12354):
   - Register definitions are counted per statement and fail closed: vector destinations, call returns, labels and several statements on a line now count.
   - A constant fd must be a single `mov` of an integer.
   - A map without a device copy keeps the trampoline's lookup, so a copy made later is still found.
   - Only 64-bit adds take the fast path.
3. **Lookup keys forwarded** (0890dbdd): an inline lookup takes its key from the register the program just stored into the key's stack slot, instead of reading it back through the slot's generic address.
   - Conditions: the store is in the same basic block, and nothing in between could write the slot or the register.
   - This applies to the thread-local and the sum-map lookups.
   - It also closes a remaining fail-open in the definition count: a `.loc` glued to the next statement, and `/* */` comments.
4. **No forwarding across a guarded store, call, atomic or reduction** (1fee25d1). Found in self-review. llvmbpf emits none today, and `lat_mmvq`'s generated PTX is byte-identical before and after (sha256 41b5bf76…), so passes k1-k3 stand.
5. **Fixes after the final review** (dd99d1ac). An independent review of c27412f4..1fee25d1 ran from three lenses, and each finding was then reproduced or refuted by another agent. None of its findings is reachable from llvmbpf's output today. Changes:
   - **Key forwarding fails closed** where it failed open. It now gives up at:
     - a label or brace between the store and the call (another path or scope);
     - any directive (a `.loc` glues to the next instruction);
     - a literal that is not plain decimal (PTX reads `0770` as octal);
     - a store whose state space does not match its base.
   - **Processes with only device per-thread maps (1532)** now run the pass too. Their adds take the `.global` form the delivery reads with; before, they stayed generic against a `.global` load.
   - **Tests** for the 1530 lookup block, helpers 2 and 3, more barriers and the whole 64-bit lookup.
   - `lat_mmvq`'s generated PTX is still 41b5bf76….

Correctness:
- **Unit tests:** 17 cases, 215 assertions (`[sum-map-routing]`, `check/unit-tests.txt`). They pin down the whole emitted lookups in order (thread-local and sum map, 32- and 64-bit index), the global add, and every form that must fall back or must not be forwarded.
- **End to end:** after every commit, pt_victim's five tools counted exactly (`check/addpath1-6.txt`: carry, lat, latold, hist, histold).
- **llama.cpp:** every measured item kept its exact counts, the same before and after.
  - `lat_mmvq`: 1531 sum 15,711,207,424, bucket 194 = 0.
  - `lat` on four kernels: every kernel's exits against step 0's counts, none without a start.

## Measured, built (tg128 and pp512)

`lat_mmvq` probes only mul_mat_vec_q (3,137,339,392 threads per run); `lat` probes the four kernels, as in `gpu/map-model-proto-20261116`. Per thread = s per run over base / 3,137,339,392.

Before against after the sum-map change (passes h1/h2, `meas/analysis-h.txt`; base 68.05 / 67.79 t/s on tg128, 2499 / 2505 on pp512):

| tool, workload | before: t/s | after: t/s | s per run (tg) | per thread | overhead |
|---|---:|---:|---:|---:|---:|
| `lat_mmvq`, tg128 | 46.07 / 46.10 | 52.48 / 52.50 | 2.777-2.779 -> 2.438-2.439 | 0.283-0.286 -> 0.175-0.178 ns | 32.0-32.3% -> 22.6-22.9% |
| `lat`, tg128 | 45.39 / 45.38 | 51.98 / 51.95 | 2.820-2.821 -> 2.462-2.464 | - | 33.1-33.3% -> 23.4-23.6% |
| `lat`, pp512 | 2407.8 / 2406.7 | 2442.8 / 2447.6 | - | - | 3.7-3.9% -> 2.3% |

The sum-map change saves 0.108 ns per thread (0.1082 / 0.1079). Section 7's hand-written shapes put the add path at 0.010-0.015 ns, against the generated code's 0.124-0.125 with the helper lookup. The built change takes out 0.108 of that.

Key forwarding built, against the build before it (passes k1-k3, a third pass because pp512 `lat` before it read 2.8 / 5.0%; `meas/analysis-k.txt`; base 67.92 / 67.92 / 68.12 and 2516 / 2517 / 2504):

| tool, workload | without: t/s | with: t/s | per thread | overhead |
|---|---:|---:|---:|---:|
| `lat_mmvq`, tg128 | 52.49 / 52.52 / 52.50 | 54.54 / 54.51 / 54.59 | 0.176-0.178 -> 0.147-0.149 ns | 22.7-22.9% -> 19.7-19.9% |
| `lat`, tg128 | 52.01 / 51.99 / 51.95 | 53.96 / 53.98 / 54.05 | - | 23.4-23.7% -> 20.5-20.7% |
| `lat`, pp512 | 2444.9 / 2390.6 / 2442.7 | 2449.4 / 2449.9 / 2452.8 | - | 2.5-5.0% (median 2.8) -> 2.1-2.7% (median 2.7) |

Forwarding saves 0.028-0.030 ns per thread (0.0292 / 0.0284 / 0.0298), as its hand-written form did (below). The rule's "as measured" holds.

All together on tg128 with mul_mat_vec_q probed: 46.1 -> 54.5 t/s, 0.283-0.286 -> 0.147-0.149 ns per thread (about half), 32% -> 20%. With the four kernels the drop goes from 33% to 21%; on pp512 from 3.7-3.9% to 2.1-2.7%.

## The thread-local map's forms, hand-written first (passes h1/h2)

Before building anything for 1530, its accesses in the new build's generated `lat_mmvq` PTX were edited by hand (`target-shape/make_tlvariants.py`; dump sha in `target-shape/dump-sha256.txt`, identical in two dumps; all 56 registers).
- **The key goes through the stack.** Each lookup's key reaches the lookup through the stack: the program stores it with `st.local`, and the lookup loads it back through the slot's generic address. The same holds for the sum-map lookup.
- **Generic 32-bit accesses.** The 8-byte value is read and written through generic addresses, as two 32-bit accesses.

| form | executed SASS, entry / exit probe | per thread (h1 / h2) | step saved (h1 / h2) |
|---|---:|---:|---:|
| the dump unchanged (control) | 31 / 97 | 0.1770 / 0.1756 ns | against the build: -0.0010 / +0.0003 |
| keys forwarded from the stored register | 21 / 86 | 0.1488 / 0.1460 ns | 0.0282 / 0.0297 |
| + value through `ld.local`/`st.local`, area zeroed with `st.local` | 22 / 82 | 0.1445 / 0.1425 ns | 0.0042 / 0.0035 |
| + 64-bit accesses | 23 / 80 | 0.1461 / 0.1438 ns | -0.0016 / -0.0013 |

Only key forwarding saved at least 0.006 in both passes (the rule), so only it was built. Local accesses and 64-bit accesses are not built.

## What is left

On tg128 with mul_mat_vec_q probed, the built shape costs 0.147-0.149 ns per thread (54.5 t/s, -20%).
- **Section 6's breakdown:** the hand-written best shape left 0.064-0.068 outside the map with the timer read inline. The generated code's tool and framework parts measured about 0.088 (calls 0.018, timer helper calls 0.035, log2 0.035; section 1).
- **Map part not measured:** the map's remaining part in the built code was not measured separately.
- **Not ours to change:** what the user counts as tool side is not touched (11-18): probe calls, the switch each probe reads, the timer helper calls and the log2.

Not done:
- The 1530 value through local accesses: it showed 0.0035-0.0042.
- Splitting the remaining cost of the built code into map and non-map parts.
- The pass's load time: it rescans a probe function's body per query. The final review measured lat_mmvq's module at about 15 ms against about 5 ms before, and it grows quadratically with very large probes. This is paid at module load only, not on the GPU. Caching the definitions per function would remove it.

## Files

- `patches/`: the four commits (format-patch on c27412f4).
- `meas/`:
  - `RULES-sum-map-build.md`, with its addendum for k1-k3.
  - `run_meas3.sh`: `old-`, `new-` and `newk-` items choose a copy of the agent and server.
  - `analyze_h.py`, `analyze.py`.
  - `analysis-h.txt`, `analysis-k.txt`, `out/{h1,h2,k1,k2,k3}-*`.
- `target-shape/`: `make_tlvariants.py`, the dump's sha, registers, ptxas logs and the probe subroutines of each form.
- `check/`: `run_test_set.sh`, `check_test_set.py`, the five-tool checks after each commit, and the unit-test output.
