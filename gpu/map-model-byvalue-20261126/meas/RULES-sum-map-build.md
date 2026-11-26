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

## Addendum: what is left, split; passes m1/m2 (written before them)

Built code cur-7e5861b9 (= 1fee25d1 plus the final review's fixes); its
generated lat_mmvq PTX is 41b5bf76..., the same as newk-0890dbdd's (and the
four-kernel lat modules are identical too), so k1-k3 stand for it. Forms of
that PTX (target-shape/make_gforms.py; all 56 registers; executed SASS of
the probes entry / exit: g0 21 / 86, gnomap 17 / 54, gbyval 15 / 70):
- g0 the dump unchanged (control, expected within 0.006 of cur-lat_mmvq);
- gnomap the map work removed (no thread-local lookup or access, no sum-map
  lookup or add), the tool's work kept (timer helper calls, no-start test,
  subtract, log2, key, bound compare guarding a trap; a stand-in start from
  constant memory);
- gbyval the start carried by value (entry returns it, the kernel keeps it
  in a .local word, the exit takes it as a parameter; no thread-local
  area), the sum map as built.
Items: tg:base tg:cur-lat_mmvq tg:cur-lat_mmvq@g0 tg:cur-lat_mmvq@gnomap
tg:cur-lat_mmvq@gbyval tg:newk-hist tg:cur-hist.
Quantities (ns per thread): map part = g0 - gnomap; by-value saving = g0 -
gbyval; rest = gnomap over base (the tool's and the framework's part with
the calls as generated). Rule: by value "helps" if it saves at least 0.006
in every pass (then it is a candidate to build; not built in this round);
the split is reported, with the keep-alive biases (a compare and a trap per
probe in gnomap). hist (1532 only): before (newk) and after (cur) the pass
runs for it; counts must match; overhead reported. Third pass if any of
map part, by-value saving or hist overhead differs by more than 0.006 ns /
1 point between m1 and m2.

## Addendum: the prototype moved onto dev 70f7bc1f; passes r1/r2 (written before them, 2026-11-26)

User 11-26: do not follow upstream now (it moves about 20 commits a day);
continue on dev `dev/uprobe-official-70f7bc1f3363` (c71a9987). The 11
prototype commits (52126f33..662bd83e) were rebased onto c71a9987 as
`proto/map-model-70f7bc1f` (33d5a3d8): one conflict, in `hack_fatbin`
(upstream returns early when no trampoline is added; the routing call now
sits after that return), the rest applied as they were.

Builds, same cmake options (gcc-13, LLVM 15, RelWithDebInfo, no LTO, CUDA
13.2):
- `o52-662bd83e`: the prototype on the old dev base (agent and server of
  bpftime-proto/build-cuda, built 11-18 at 662bd83e);
- `n70-33d5a3d8`: the same commits on the new dev base.

Before the passes: lat_mmvq's and the four-kernel lat's generated PTX under
n70 dumped (BPFTIME_CUDA_PTX_DUMP_DIR) and compared with o52's (41b5bf76...;
813a383c / b0cb7b73 / e5e3d405). Unit tests ([sum-map-routing] and the
nv_attach tests) and the seven pt_victim tools exact under n70 first.

Items: tg:base tg:o52-lat_mmvq tg:n70-lat_mmvq tg:o52-lat tg:n70-lat
pp:base pp:o52-lat pp:n70-lat. Same correctness checks (exact counts).
Rule: the move "keeps the numbers" if n70 and o52 differ by at most 0.006 ns
per thread on lat_mmvq (tg128) in both passes and the lat overheads (tg128,
pp512) by at most 1 point; otherwise the difference is reported (with the PTX
comparison). Third pass if o52 - n70 differs by more than 0.006 ns between
passes or an overhead by more than 1 point.

## Addendum: the start by value, hand forms on the new base; passes v1/v2 (written before them, 2026-11-26)

User 11-26: build the by-value carry. Design (before building it): a kernel
whose probe functions use the thread-local area only through constant-key
lookups and plain 32/64-bit loads and stores keeps the area in a `.local`
slot of its own and passes it by value; a probe function that accesses the
area takes it in, one that writes it also returns it; inside, the area is
.b32 halves, the lookup a non-null constant, each access a register move;
anything else keeps today's area. Forms of n70's generated lat_mmvq PTX
(target-shape/make_tvforms.py on 923ea830; 56 registers each; probe SASS
entry / exit and kernel total: tv0 21 / 86, 600; tvtight 15 / 70, 576;
tvpass 17 / 70, 584):
- tv0: the dump unchanged (control);
- tvtight: the 11-18 by-value form regenerated (entry returns the start and
  takes nothing, exit takes it and returns nothing, lookups and null tests
  gone);
- tvpass: the exact text the pass is to emit (the built pass must reproduce
  it byte for byte, so this measurement stands for the built code).
Items: tg:base tg:n70-lat_mmvq tg:n70-lat_mmvq@tv0 tg:n70-lat_mmvq@tvtight
tg:n70-lat_mmvq@tvpass. Counts exact (sum, bucket 194 = 0), override logged.
Rule: build as designed if tvpass saves at least 0.006 ns per thread against
tv0 in both passes. Report tvpass - tvtight; if tvpass trails tvtight by
0.006 or more in both passes, report that gap (no path analysis for now).
Control: tv0 within 0.006 of n70-lat_mmvq. Third pass if a saving differs by
more than 0.006 between passes.

## Addendum: the by-value carry built (tv-1d95c160); passes t1/t2 (written before them, 2026-11-26)

v1/v2: tvpass saved 0.0210 / 0.0238 ns per thread against tv0 (rule met:
build), tvtight 0.0231 / 0.0259, gap 0.0021 / 0.0021. Built as 1d95c160 on
proto/map-model-70f7bc1f; its generated lat_mmvq PTX equals tvpass byte for
byte. Fallback tools' PTX (carry, tlatomic, hist, latold on pt_victim) is
byte-identical to n70's.
Items: tg:base tg:n70-lat_mmvq tg:tv-lat_mmvq tg:n70-lat tg:tv-lat pp:base
pp:n70-lat pp:tv-lat. Counts exact and equal between n70 and tv.
Rule: the built form is "as measured" if n70 - tv on lat_mmvq is within
0.006 ns of 0.021-0.024 (v1/v2) in both passes; otherwise the difference is
reported. The four-kernel lat overheads (tg128, pp512) are reported. Third
pass if n70 - tv differs by more than 0.006 between passes or an overhead by
more than 1 point.
