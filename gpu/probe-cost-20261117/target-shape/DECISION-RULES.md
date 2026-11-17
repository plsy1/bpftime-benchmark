# Target-shape test on the real kernel: rules fixed before the runs (2026-11-17)

Items (tg128, probes on the largest mul_mat_vec_q only, -r 5 with warmup,
two shuffled passes, clocks checked before every item): base, empty2, lat
(generated), and lat with its patched PTX replaced through
BPFTIME_CUDA_PTX_OVERRIDE_DIR by the variants of make_variants.py:
t0 (unchanged), area, reg, loc, chain, inline, regcap.

Cost of an item = (time per run - base's in the same pass) / 3,137,339,392
thread executions, in ns per thread. Target for mul_mat_vec_q alone: 0.029 ns
(0.031 ns is 5% on tg128; the other three kernels of the full tool take the
rest, from the README of gpu/probe-cost-20261117).

Validity (an item that fails one is reported and excluded):
- the client log shows "PTX override" for every variant item;
- the sum map holds exactly 15,711,207,424 and bucket 194 (no start) is 0;
- t0 is within 0.006 ns of lat in both passes (the hook alone changes
  nothing); otherwise the whole run is void.

Rules (each must hold in both passes):
- D1: min(reg, loc, regcap) <= 0.029: the documented path with calls kept
  can meet the budget. (regcap adds .maxnreg, a documented performance
  directive; added before the runs because reg and chain compiled to 64
  registers, 32 warps per SM, against the generated kernel's 56, 36 warps.)
- D2: min(reg, loc, regcap) > 0.029 >= inline: the budget needs the probe
  spliced into the kernel, which is the user's decision.
- D3: inline > 0.029: the shape misses the budget even spliced.
- D4: the clz step is worth building only if chain - reg >= 0.01.
- D5: carrying the start in a register (instead of a .local word) only if
  reg < loc - 0.006 (or regcap < loc - 0.006).
- D6: if area - reg >= 0.03, carrying the start by value comes before the
  lookup inlining in the build order.

## Addendum, written after passes c1/c2 and before passes d1/d2

c1/c2 decided D1-D6 (see the README). regcap compiled to 48 registers, fewer
than the kernel without probes (56), so part of its result may be the cap
speeding up mul_mat_vec_q itself. Passes d1/d2 (base, none, nonecap, regcap,
loc) only explain that; they change no decision:
- none: the kernel without probe calls, compiled in this module (56 regs);
- nonecap: the same with .maxnreg 56 (48 regs).
If nonecap is faster than none by 0.006 ns or more in both passes, regcap is
also quoted against nonecap (probe cost at the same register cap).

## Addendum 2, written after d1/d2 and before e1/e2

The spliced form (inline, 64 registers) was compared with a capped call form
(regcap, 48 registers) and judged without a cap; at equal registers (reg vs
inline, both 64) removing the calls saved about 0.036 ns. Passes e1/e2 (base,
nonecap, regcap, inlinecap = inline with .maxnreg 56) rerun D2/D3 at equal
caps: cost of each capped variant is also quoted against nonecap. D2 holds
if regcap - nonecap > 0.029 >= inlinecap - nonecap in both passes; D3 if
inlinecap - nonecap > 0.029.

## Addendum 3, map residual: written before passes f1/f2 (2026-11-17 afternoon)

Question: in the best documented shapes, how much of the per-thread cost is
still the map model's own work? The tool stays unchanged, so every variant
here keeps the tool's 6-step log2 chain (no clz; clz is a tool change and
appears only in the link item `loc`). New variants (make_variants.py, same
dump; definitions in its docstring): empty, nomap, locc, locc-noadd,
regc-noadd and the capped emptycap, nomapcap, loccap, loccap-noadd, regcapc,
regcapc-noadd.

Registers (ptxas -O3 sm_87, cuobjdump -res-usage, before any run):
- no cap, 56 registers: none, empty, nomap, locc-noadd, locc (STACK 0 for
  none/empty/nomap; 8 for the two loc forms = their own .local word, no
  spills: STL/LDL count equals the written st.local/ld.local);
- .maxnreg 56, 48 registers: nonecap, emptycap, nomapcap, loccap-noadd,
  loccap, regcapc-noadd, regcapc (STACK as above);
- 64 registers: chain, regc-noadd (only their difference is used).

Items, two passes shuffled (f1, f2): base, t0, loc, none, empty, nomap,
locc-noadd, locc, nonecap, emptycap, nomapcap, loccap-noadd, loccap,
regcapc-noadd, regcapc, regc-noadd, chain.

Quantities, per pass, ns per thread (time difference / 3,137,339,392):
- A = full - noadd: the sum map's work after the bound compare (constant
  loads of the buffer and shard count, NULL tests, %smid and %laneid, the
  shard address, isspacep, the atomic add);
- C = noadd - nomap: carrying the start (the .local word or the register,
  the entry's return value, the exit's parameter, the no-start test on the
  carried value), against a stand-in start loaded from constant memory.
  C is biased LOW: nomap pays a keep-alive the noadd form does not (one
  constant compare and a never-taken trap in the entry);
- M = full - nomap = A + C: the map model's residual in that shape;
- T = nomap - empty: the tool's own work (two timer reads, subtract, chain,
  key, bound compare) plus the keep-alive;
- F = empty - none: the switch and the two calls.
Families: U = no cap (none, empty, nomap, locc-noadd, locc); K = cap
(nonecap, emptycap, nomapcap, loccap-noadd, loccap) and the register-carry
pair inside K (regcapc-noadd, regcapc: A only, plus C against nomapcap);
R64 = chain, regc-noadd (A only).

Validity (each in both passes; a failing quantity is reported, not used):
- V1 links: t0 - base within 0.006 of 0.282-0.284; loc - base within 0.006
  of 0.116-0.118. Otherwise the passes are void.
- V2 counts: full forms (locc, loccap, regcapc, chain, loc, t0) hold exactly
  15,711,207,424 with bucket 194 = 0; none, empty, nomap and every -noadd
  form hold 0; every override item logs "PTX override".
- V3 registers: a difference is used only between members with the same
  REG count and no spills (as listed above).
- V4 order: within U and within K, none <= empty <= nomap <= noadd <= full
  to within 0.006 (a nomap at the level of empty means its body did not run).
- V5 noise: a quantity whose two passes differ by more than 0.006 gets a
  third pass and the median of three is reported.

Interpretation (fixed now; U's loc form is the one the design builds by
default, K is the second reading):
- R1: A >= 0.03 in both passes: the sum map's add path is the main map cost
  left; it is designed next.
- R2: C >= 0.03 in both passes: carrying the start still costs; its form is
  designed next (R1 and R2 can both hold).
- R3: M <= 0.01 in both U and K: the map model is at its floor in this
  shape; what remains is outside the map.
- R4: anything else (0.01 < M, A < 0.03 and C < 0.03): reported, no decision.
Also reported, without a rule: the non-map floor in two readings, (a) the
generated code's call floor + timer helper calls + log2 (0.018 + 0.035 +
0.035, passes a1/a2) and (b) nomap - none in this shape (timer read inline,
switch read once); which bpftime-side changes are in scope is the user's
decision.

### Addendum 3a: after an independent review of the variants and these rules, still before f1

The review (two read-only reviewers) confirmed the variants: module byte-identical outside the probed kernel's call sites, the inserted
functions and the cap line; each -noadd form differs from its full form in
one PTX line plus the 23 add-path lines; the traps cannot fire (max_entries
260; keys 130..194; the stand-in start 260 is not 0); ptxas kept both timer
reads and the whole chain in nomap and every -noadd exit (42 SASS
instructions each); the empty bodies are real calls. Changes and additions:
- A also contains the add path's control structure: BSSY/BSYNC around the
  exit call, YIELD, extra predicated returns, and in the loc form a BSSY/BSYNC
  pair around the no-start branch (about 17 executed instructions in the loc
  form, one BSSY pair fewer in the reg forms).
- C: the no-start test and subtract are in both members and cancel. C's low
  bias is the nomap entry's keep-alive: 2 ISETP and a move per thread (the
  trap never runs).
- Kreg's C (regcapc-noadd - nomapcap) is the start held in uniform
  registers under the cap, and it adds 2 instructions per main-loop iteration.
  It is reported as "carry plus a loop cost from register pressure", not as a
  per-probe constant. A within Kreg is clean (equal loops).
- F and floor (b) include more than switch + calls: with an exit probe every
  thread runs to the exit, and the reduction gets divergence checks.
- A and C are marginal costs in this order (the add with the carry present;
  the carry without the add). M is judged from its own values.
- V3 assumes the run-time compiler (libnvptxcompiler, same CUDA 13.2) gives
  the same register counts as the CLI ptxas used here.
- The chain in the variants has LLVM's structure, with one setp fewer than
  the generated code. That changes T only, not A, C or M.
New or sharpened rules:
- V6 bodies ran: T = nomap - empty >= 0.012 in both passes, per family (U, K).
  Otherwise that family's T, C and M are void.
- Families: R1-R3 are decided on U. K and Kreg are reported; a disagreement
  with U is reported, with no decision. R64 is report-only. V4 applies to U,
  K and Kreg; for R64, only regc-noadd <= chain.
- V5 with a third pass: f3 reruns all 17 items. With three passes, a rule
  holds if it holds for the median and in at least 2 of 3. A pass voided by
  V1 is rerun once with the same items.
- R3, sharpened: "at its floor" needs M + 0.006 <= 0.01 in both passes (on U).
  An M at or below 0.01 that fails this is reported as "below resolution".
- A quantity excluded by V4 or V6 leaves its rule unjudged. R4 is simply
  "otherwise".
- Analysis: a zero-sum item with no 1531 sum is MISSING. A full item with
  bucket 194 != 0 is WRONG.

## Addendum 4, the sum map's add path: written before passes g1/g2 (2026-11-18)

Question: how short can the 1531 add path (everything after the bound
compare) be made on documented guarantees, and what is left. Same hook, same
dump, locc's carry (kernel .local word), the tool unchanged (6-step log2).
New variants (make_variants.py ADDPATH; registers 56 for all, ptxas -O3
sm_87, cuobjdump -res-usage, checked before any run):
- addg: the add as `red.global.add.u64` on the `cvta.to.global` address;
  the NULL-copy and shard-count checks and the 64-bit arithmetic as locc.
- addone: addg with the bound compare as the only check (stands for a bound
  the agent writes: max_entries, or 0 without a device copy).
- addmin: addone with 32-bit offsets, S = 256 and V = 8 as immediates.
- addminnored: addmin without the red; the address kept alive by a
  never-true compare against max_entries guarding a trap (zero-sum item).
- addq: addmin's check and arithmetic, the add guarded by isspacep.global
  (red.global if it holds, else the generic atom.add): the lowering for a
  pointer whose origin is unknown.
- addmingen: addmin with a generic `red.add.u64` (no cvta, no .global).
Compiled, not timed: addminc (S, V from map_info's existing fields: 2
executed SASS more than addmin), addmina (atom.global.add.u64 with an unused
result: ptxas emits the same RED as addmin, plus one YIELD).

Executed SASS of the exit subroutine on the normal path (start present, key
in range), counted from the listings before the runs: locc-noadd 41,
locc 90, addg 73, addone 64, addmin 54, addminnored 53 (its keep-alive: 2
ISETP, 2 moves and a predicated return in place of the red's 3 moves, red and
return), addq 68, addmingen 58 (generic ATOM.E with a returned predicate;
the shared and local fallbacks are not executed).

Items per pass (11, shuffled by the pass label): base none locc-noadd locc
addg addone addmin addminnored addq addmingen t0.

Quantities (ns per thread, 3,137,339,392 threads per run):
- end to end: A_cur = locc - locc-noadd; A_min = addmin - locc-noadd;
  saved = locc - addmin.
- timed splits (reported; each is a few SASS, near the noise): space =
  locc - addg, checks = addg - addone, arith = addone - addmin, guard =
  addq - addmin, atomic = addmin - addminnored, addr = addminnored -
  locc-noadd (with the keep-alive bias), gen = addmingen - addmin.
  A split is "resolved" only at >= 0.012 in every pass; below that it is
  "not resolved by timing" and the executed SASS count above is the
  attribution.
Checks:
- V1': A_cur within 0.059..0.077 (f1/f2: 0.065-0.071, +-0.006) and t0 - base
  within 0.283 +-0.007; otherwise the pass is void and rerun once.
- Counts: full items (locc addg addone addmin addq addmingen t0) sum exactly
  15,711,207,424 with bucket 194 = 0; zero-sum items (none, locc-noadd,
  addminnored) sum 0; every override logged.
- V4': locc >= addg >= addone >= addmin >= addminnored >= locc-noadd in
  s per run, each within 0.006 ns; a violation is reported and the
  quantities spanning it are not judged.
- Third pass (all 11 items) if A_cur, A_min or saved differ by more than
  0.006 between g1 and g2; then median of three, a rule holding in at least
  2 of 3 and for the median.
Interpretation (fixed now):
- R5: A_min <= A_cur / 2 in both passes: the documented shortening (red on
  the global address, one check, 32-bit arithmetic) at least halves the add
  path; it is the design. Otherwise: the shortening's gain is not shown.
- R6, what is left: atomic >= addr + 0.012 in both passes: the remainder is
  the atomic itself (the floor for one atomic add per thread); addr >=
  atomic + 0.012: the shard and address computation is the next design
  question; neither: not resolved by timing (SASS: the address is ~8
  instructions, the red 1 plus its operands).
- R7: guard >= 0.012 in both passes: telling the lowering where the pointer
  came from (constant-fd 1531 lookups) is worth building; otherwise the
  guarded lowering's cost is not resolved by timing and the SASS count
  (14 instructions) stands.
- gen (generic red vs red.global) has no rule: it is reported to choose the
  documented form (generic red keeps the delivery's generic load at the same
  address; red.global needs the delivery to load through .global as well).

### Addendum 4a, after g1-g3 (a counting correction; the rules above are unchanged)

An independent check of the SASS found a BSSY/BSYNC pair around the exit
call in the kernel bodies of locc, addminnored, addq and addmingen (not in
locc-noadd, addg, addone, addmin). The rest of every body has the same
instruction count block for block (479; main loop 138) but differs in
scheduling and instruction choice (1-5 main-loop instructions per variant). Per
thread: locc 92, addminnored 55, addq 70, addmingen 60. So guard = 16 SASS,
locc - addg = 19, gen = 6, and addminnored runs one instruction more than
addmin. R7's statement "the SASS count (14 instructions) stands" would read
16; R7 held by timing, so this changes no verdict.
