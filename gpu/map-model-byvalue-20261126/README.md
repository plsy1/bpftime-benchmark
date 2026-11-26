# The start by value: the thread-local area passed by value (2026-11-26)

Follow-up to `gpu/map-lowering-20261118` (the by-value carry, listed there as the next candidate: 0.015-0.021 ns per thread) and `gpu/map-model-move-70f7bc1f-20261126` (the prototype on dev 70f7bc1f). User 11-26: build it. Branch `proto/map-model-70f7bc1f`, two commits on 33d5a3d8 (`patches/`): 1d95c160 (the change) and c4235347 (fixes after an independent review). The tool (`lat`) is unchanged.

Board and settings as before: Orin Nano 8 GB, clocks fixed and checked before every item, llama-bench TinyLlama Q4_K_M `-r 5` with warmup, items shuffled per pass, rules written before each set of passes (`meas/RULES-sum-map-build.md`, the last two addenda).

## What was built

A kernel whose probe functions reach the thread-local area (1530) only in the forms `lat` uses keeps the area in a `.local` slot of its own and passes it to its probes by value, instead of passing the area's address:

- **Kernel:** the slot is zeroed at the start. Before each probe call it is loaded and passed as a parameter; a probe that writes the area returns it, and the kernel stores it back (with the call's guard).
- **Probe function:** the area is two `.b32` registers. A thread-local lookup with a constant key becomes the constant 1 (non-null); a 32-bit load through it becomes `cvt.u64.u32` of a half, a 32-bit store `cvt.u32.u64`. The halves are packed and returned before every `ret` of a function that writes them.
- **When (an allowlist, decided once per probe function):**
  - every map lookup, update or delete (helpers 1-3) names its map with a plain decimal constant;
  - thread-local maps are only looked up, with a constant key the program just stored (key forwarding), in range;
  - each lookup result is used only in compares with 0 and in 32-bit loads and stores with 64-bit registers, at 4-byte offsets inside the value, unguarded, each on a line of its own, never storing the result itself;
  - the area is one 8-byte word (a writing function returns it, and a device function has at most one return value, PTX ISA 7.1.1);
  - no block comments, no directives glued to statements, no guard on a line of its own; each kernel probe call alone on its line.

  Anything else in any probe function of a kernel keeps the address form for the whole kernel, and the agent logs why ("Thread-local area kept in memory: ..."). Map calls with a constant fd of another type get 0 for the area (the trampoline reads it for thread-local maps only). A kernel none of whose probes reaches the area gets no area.
- **Basis:** `.local` is the thread's own (PTX ISA 5.1.5), and the area's address never leaves the rewritten accesses. Device function inputs and returns are `.param`, returns written with `st.param` (5.1.6.4, 7.1). `ld`/`st` with a wider register zero-extend a load and store the low bits (9.4.1), as `cvt.u64.u32` and `cvt.u32.u64` do (6.5.1, zext and chop). The halves are packed and unpacked only by this code; 64-bit accesses keep the address form.

## Hand-written first (passes v1/v2, `meas/analysis-v.txt`)

Three forms of n70's generated `lat_mmvq` PTX (sha 923ea830; `target-shape/make_tvforms.py`), timed through the debug override; 56 registers each:

| form | probe SASS, entry / exit | kernel SASS | t/s (v1 / v2) | per thread over base | saved against tv0 |
|---|---:|---:|---:|---:|---:|
| tv0: the dump unchanged (control) | 21 / 86 | 600 | 54.32 / 54.06 | 0.1494 / 0.1540 ns | - |
| tvpass: the text the pass was to emit | 17 / 70 | 584 | 55.88 / 55.82 | 0.1284 / 0.1302 ns | 0.0210 / 0.0238 |
| tvtight: the 11-18 by-value form | 15 / 70 | 576 | 56.04 / 55.98 | 0.1263 / 0.1281 ns | 0.0231 / 0.0259 |

Base 67.81 / 67.91 t/s. The control read 0.0002 / 0.0026 ns from the build itself. tvpass saved at least 0.006 in both passes, so it was built. It trails the tight form by 0.0021 in both passes (the entry takes the word in as well, and the halves are repacked).

The built pass emits tvpass byte for byte (sha 81ea6349 for both; `target-shape/dumps.txt`).

## Built, on llama.cpp (passes t1/t2, `meas/analysis-t.txt`)

`n70`: the prototype on dev 70f7bc1f without this change (33d5a3d8). `tv`: with it (1d95c160). Base 68.01 / 67.81 t/s on tg128, 2490 / 2509 on pp512.

| tool, workload | without: t/s | with: t/s | s per run (tg) | per thread | overhead |
|---|---:|---:|---:|---:|---:|
| `lat_mmvq` (mul_mat_vec_q only), tg128 | 54.25 / 54.25 | 56.13 / 56.11 | 2.359 -> 2.280-2.281 | 0.150-0.152 -> 0.125-0.127 ns | 20.0-20.2% -> 17.2-17.5% |
| `lat` (four kernels), tg128 | 53.97 / 53.94 | 55.64 / 55.54 | 2.372-2.373 -> 2.300-2.305 | - | 20.4-20.6% -> 18.1-18.2% |
| `lat` (four kernels), pp512 | 2477.7 / 2478.6 | 2477.2 / 2481.7 | - | - | 0.5-1.2% both |

- The change saves 0.0251 / 0.0249 ns per thread on mul_mat_vec_q. That is within 0.006 of the hand-written 0.021-0.024, so the rule's "as measured" holds.
- Counts are exact in every item and equal between the two builds.
- With the four kernels, all three patched modules (one routed kernel each, as before) take the area by value.
- From the start of the map work: 46.1 t/s (32%) -> 54.5 (20%) on 11-18 -> 56.1 t/s (17.2-17.5%) now. Section `gpu/map-lowering-20261118` put the map-free shape at 57.2 t/s.

c4235347 changes neither this PTX nor the four-kernel modules (byte-identical; `target-shape/dumps.txt`), so t1/t2 stand for it.

## Correctness

- **Unit tests** (`check/unit-tests.txt`): `[tl-byvalue]` and `[sum-map-routing]`, 27 cases, 566 assertions.
  - They pin the emitted text: heads, the prologue, give-back before every ret, call sites with labels and guards, the kernel slot.
  - Every fallback form has a row that asserts its reason (35 rows; a lookup call block holding more than the call is refused as a module, as before).
  - The whole binary passes but `[ptx-failure]`, which fails on plain dev 70f7bc1f too (`gpu/map-model-move-70f7bc1f-20261126`).
- **pt_victim** (`check/tvf-pt_victim.txt`, and with 1d95c160 `check/tv-1.txt`): eight tools exact, every count line equal to the previous build's.
  - `lat` takes the area by value.
  - `carry` and `tlatomic` keep the address form, with the reason logged: 16 and 24 bytes. Before the one-word limit, carry's reason was its keys: one vector store writes both, so they are not forwarded.
- **A check of the value itself:** a new tool, `carry1` (`tools/pt_carry1.bpf.c`), keeps one 8-byte value; the entry writes a 64-bit mark of the thread and the exit compares every bit.
  - By value it had 0 mismatches over 3,880,960 thread executions.
  - With the halves swapped in one kernel's exit (override, `check/tv-carry1-mutation.txt`) it reported all 1,000,960 exits of that kernel and none of the other.
- **The address form is unchanged:** the generated PTX of the tools that keep it (carry, tlatomic, hist, latold) is byte-identical to the build without this change, before and after the fixes.

## Independent review (`review/review-1d95c160.json`)

Three lenses (eligibility, the rewrite, tests and claims) went over 1d95c160. Another agent then traced each finding on its input.

- **Results:** 14 findings; 13 confirmed, 1 refuted as a test remark (taken anyway). All are fixed in c4235347.
- **One reachable from ordinary llvmbpf output, not from the measured tools:** a 9-16 byte area with a writing probe got two `.param` return values, which ptxas refuses. The module would then load without probes. Fixed by the one-word limit.
- **The rest are hypothetical inputs:**
  - text the line-based rewrite reads differently from the tokenizer: block comments, `.loc` glued to a statement, a guard on its own line before a `ret`, a probe call sharing its line;
  - a lookup result stored into its own value;
  - two older bugs, which also affect the address form:
    - map fds in octal, or a `mov` inside a comment, misread;
    - `redux.sync`/`stacksave` not counted as register writes;
  - stats and documentation wording.

## Not done

- **Areas over 8 bytes** (16-byte values, several thread-local maps) keep the address form. Two words would need one `.param` byte array as the return value.
- **Keys written with a vector store** (carry) are not forwarded, so they are not constant to the pass.
- **64-bit, 8- and 16-bit and signed accesses** keep the address form: no tool runs them.
- **An independent review of c4235347 itself.**

## Files

- `patches/`: the two commits (format-patch on 33d5a3d8).
- `meas/`:
  - `RULES-sum-map-build.md` (addenda v1/v2 and t1/t2);
  - `run_meas3.sh` (the copy that ran has `tv-` and `n70-` items), `analyze_h.py`, `analyze.py`;
  - `analysis-v.txt`, `analysis-t.txt`, `out/{v1,v2,t1,t2}-*`.
- `target-shape/`:
  - `make_tvforms.py` (with `make_gforms.py`, which it imports);
  - ptxas logs, SASS and probe subroutines of tv0/tvtight/tvpass;
  - `dumps.txt` (every sha compared).
- `check/`: pt_victim runs, the fallback runs (`fb-*`, `fbf-*`; their `hist` FAIL lines are the in-call check that the dump option switches on, not a count), the carry1 runs, the unit tests, the scripts.
- `tools/`: `pt_carry1.bpf.c`, `Makefile`.
- `review/`: the review's findings and verdicts.
