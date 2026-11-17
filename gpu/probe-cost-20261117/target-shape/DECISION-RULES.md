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
