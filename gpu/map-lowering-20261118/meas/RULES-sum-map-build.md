# Sum-map change, built: rules fixed before passes h1/h2 (2026-11-18)

What is measured: the prototype's agent and server before (c27412f4) and
after (00f12354: 026f490f plus the review fixes) the change that makes constant-fd
sum-map lookups inline and their adds global (proto/builds/old-*, new-*;
run_meas3.sh items old-<tool> / new-<tool>). The tools are unchanged.

Items per pass (shuffled by the pass label):
  tg:base  tg:old-lat_mmvq  tg:new-lat_mmvq  tg:old-lat  tg:new-lat
  pp:base  pp:old-lat  pp:new-lat
  tg:new-lat_mmvq@tl0 @tlK @tlKL @tlKLW  (thread-local map forms, below)
(lat_mmvq: only mul_mat_vec_q probed; lat: the four kernels, as passes p1/p2).

Correctness per item (analyze_h.py; an item failing is invalid and not used):
- lat_mmvq: the 1531 sum exactly 15,711,207,424, bucket 194 = 0.
- lat: every kernel's timed exits against step0/e7-llama's counts (pp512
  exactly 6c; tg128 between 5c and 5c + c/64), no exit without a start,
  final delivery done (as README rule 1).
- old and new give the same counts on the same workload.

Quantities: ns per thread for lat_mmvq (s per run over base / 3,137,339,392);
overhead = 1 - t/s / base's t/s for every tool item; the saving = old - new.
Link check: old-lat_mmvq - base within 0.283 +- 0.007 ns (earlier passes);
otherwise the pass is void and rerun once.
Third pass (all items) if old - new for lat_mmvq differs by more than 0.006 ns
between h1 and h2, or a lat overhead by more than 1 point.
Expected (not a rule): the generated add path cost 0.124-0.125 ns with the
helper lookup (passes a1/a2); hand-written shapes put the inline lookup with
a global red at about 0.01-0.03, so new-lat_mmvq near 0.18-0.19 ns.

Thread-local map (1530) forms, hand-written on the new build's generated
lat_mmvq PTX (target-shape/make_tlvariants.py; dump sha 646fb20b..., the same
in two dumps; all 56 registers): tl0 the dump unchanged (control), tlK keys
forwarded from the stored register (both 1530 lookups and the 1531 lookup),
tlKL + the 1530 value through ld.local/st.local and the area zeroed with
st.local, tlKLW + 64-bit accesses. Executed SASS of the probes: entry 31 /
21 / 22 / 23, exit 97 / 86 / 82 / 80 (tl0 / tlK / tlKL / tlKLW).
Steps (ns per thread saved): K = tl0 - tlK, L = tlK - tlKL, W = tlKL - tlKLW;
control = tl0 - new-lat_mmvq (the override itself; expected within 0.006).
A step "helps" when it saves at least 0.006 in every pass (two passes; with
three, in at least 2 of 3 and for the median); otherwise "not shown", and the
SASS count is reported. Only steps that help are built into the routing.
Every override item must log the override and keep the exact counts.

## Addendum: the key forwarding built (newk-0890dbdd), passes k1/k2

Written after h1/h2 (K helped, 0.028-0.030; L and W not shown) and before
k1/k2. Items: tg:base tg:new-lat_mmvq tg:newk-lat_mmvq tg:new-lat
tg:newk-lat pp:base pp:new-lat pp:newk-lat. Same correctness checks.
Expected: newk-lat_mmvq about tlK's 0.146-0.149 ns (h1/h2); the built
form is "as measured" if new - newk is within 0.006 of h1/h2's K
(0.028-0.030) in both passes, otherwise the difference is reported.
Third pass if new - newk differs by more than 0.006 between passes.
