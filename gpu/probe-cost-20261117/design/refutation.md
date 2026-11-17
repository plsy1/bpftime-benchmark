REFUTATION REVIEW OF THE THREE LADDERS (read-only; nothing was built, run or modified)

How numbers are labelled in this review:
- [M-B] and [M-C] are measured values from measurements.md tables B and C.
- [M-ptxas] is measured, from results/gpu/map-model-proto-20261116/ptx/ptxas_check-final.txt.
- [SASS] is counted by me from probe-sass-excerpt.txt, mmvq-patched-lat.ins.txt or probe_shape.sass.
- [E] is an estimate; the reasoning is given next to it.

Bottom line:
- No ladder shows the 0.031 ns budget being met on a documented main path by its own numbers.
  - small-steps estimates 0.05-0.08 and says the budget is not reached.
  - target-shape reaches it only at the low end of its endpoint C. Endpoint C includes a tool change (clz) and the splice, which rests on a derivation.
  - robust-general's 0.015-0.045 depends on carrying table C amounts over to the real kernel. The measurements file warns against that.

======================================================================
PER RUNG (duplicates merged; each ladder's own names in brackets)
======================================================================

1. Routing for every probed module, map descriptors and the IR hook [small-steps 0; robust-general 0; target-shape R1]
Verdict: STANDS WITH CONDITIONS.
- Basis: plumbing only, so no new documentation premise.
- Breakage the ladders miss or understate:
  (a) Newly applied guard.
    - route_function_body always ends with guard_generic_atomics (nv_thread_local_routing.cpp:381).
    - Once routing runs for modules without a 1530 map, tools such as count and hist get the isspacep guard on every generic atomic. That is about 14 executed instructions per atomic [SASS: 2580-25a0, 2700-2750, 2760-27b0 in lat].
    - So the saving is not 0 for those tools; it is a cost until rung 5 below lands.
    - It also closes an existing undefined case: today, without a 1530 map, a generic atomic on an eBPF-stack address is unguarded (P92:23363).
  (b) Existing throws become reachable for every probed module, not only those with a 1530 map. Any of them loads the module unprobed (nv_thread_local_routing.hpp:48-49; nv_attach_impl.cpp:1189-1204) where today it loads probed:
    - a routed function called from an unrewritten site (:486-501), e.g. a stub-mode entry call in both the kernel and a device function (kprobe_entry main.cpp:58-81);
    - a predicated generic atomic (:190-191) or an unsupported one (:211-226);
    - a helper call without a fifth argument (:362-366).
    The ladders' "no new matcher throws" rule does not cover these existing throws.
  (c) The kmr route-stats expectations change.
- constant_fd gap: I tested the any_def regex in Python.
  - xor.b64 and cvta.local.u64 ARE already counted, because the regex is unanchored ("or" matches inside "xor", "cvt" is a prefix of "cvta").
  - mad, neg, rem, min/max, shf and not are NOT counted.
  - The gap is real, but robust-general's examples (xor, cvta) and its unit test ("mov then xor is not constant") test something that already passes.
- Conditions:
  - count every instruction whose destination is the register, anchored at the opcode;
  - run the guard only when a 1530 map exists, or land this rung together with rung 5, or accept the cost knowingly;
  - when no 1530 map exists, turn the existing throws into "leave this module as today";
  - with the lowering off, the patched PTX must be byte-identical (target-shape R1).

2. Inline the timer, helper 502 [small-steps 1 (PTX); target-shape R3 (IR); robust-general 2 (IR, also 503-511)]
Verdict: STANDS WITH CONDITIONS.
- Basis:
  - It is the same instruction as the helper body, `mov.u64 %globaltimer` (default_trampoline.cu:99-104, 588-592). One 64-bit read (facts_docs T2; P92:42056/42073).
  - Inline versus call is not stated (Q5c), and the read's placement is not stated in either form (Q4c). In the non-spliced form the read stays inside the called probe function, so the documented basis is unchanged.
  - The IR form needs sideeffect plus inaccessiblememonly, not readnone, so two reads are not merged. All ladders get this right.
- robust-general extensions:
  - 503/504/505/508 lowered to 8-byte stores: same assumption as the trampoline's `*(uint64_t*)x` (default_trampoline.cu:594-645).
  - 509-511 lowered to no-memory sreg intrinsics: LLVM may merge two %smid or %warpid reads. The merged result is one of the original possible executions (a refinement), so it is acceptable.
- Cost:
  - The timer bundle (0.035, derived from [M-B]) holds two CALL/RET pairs plus two CS2R reads and the compares. The CS2R reads stay.
  - small-steps (0.006-0.025 [E]) and target-shape (0.01-0.03 [E]) are consistent with that.
  - robust-general's 0.030-0.037 [E] claims nearly the whole bundle on the strength of r_carry being about 0 [M-C]. C amounts do not carry over: c_empty at the mmvq shape is +0.003 [M-C], while the real empty2 is 0.017-0.019 [M-B].
  - C's call shape is also structurally lighter: probe_shape.sass has no BSSY/BSYNC around the CALLs, no switch load and a 0x10 frame. The patched mmvq has BSSY/BSYNC at 0170/01d0 and 1ab0/1af0, a switch LDG and a 0x280 frame [SASS].
- Conditions:
  - an unknown callseq shape keeps the call;
  - the helper stays defined (test_cuda_host_call, unrouted code);
  - the JIT clause (P92:42073) is unchanged.

3. Timer helper declared memory-free [small-steps 1b]
Verdict: STANDS WITH CONDITIONS.
- Basis: true by construction (the helper body reads only a special register).
- Conditions: GPU only; inaccessiblememonly, not readnone; decided by a SASS gate only (saving 0-0.006 [E]).
- Note: the attribute also lets LLVM move the read across the probe's ordinary loads and stores. That changes values only, and is not documented either way.
- target-shape R3 and robust-general rung 2 subsume this rung.

4. Inline the 1531/1532 lookup for a constant fd [small-steps 2 (PTX); target-shape R2 lookup part (IR); robust-general 3 lookup part (IR)]
Verdict: STANDS WITH CONDITIONS.
- Basis:
  - It is the arithmetic of sum_array_offset (default_trampoline.cu:264-275), with the same NULL conditions: extra_buffer 0, shards < 32, key >= max_entries.
  - Any shard is correct, because programs add atomically and the delivery sums all S shards.
  - Masked %smid (P92:41436/41462).
  - %laneid is 0..WARP_SZ-1 (P92:41248), and WARP_SZ is 32 on all targets to date (P92:2299). So 0..31 is documented today; `& 31` is cheap future-proofing.
- Baked immediates (small-steps): verified consistent.
  - One device copy per map per process, never freed; a failed allocation is not cached (nv_gpu_model_maps.cpp:678-770).
  - The late path copies impl.map_basic_info into map_info (nv_attach_fatbin_record.cpp:285-288), the same values routing would use.
  - ld.const from map_info remains the more robust choice (robust-general P5).
- Conditions:
  (a) PTX form: the constant_fd fix from rung 1 is a precondition. A misidentified fd applies another map's base and layout to an atomic.
  (b) 1532: the lowered thread index must use the trampoline's exact arithmetic (getGlobalThreadId uses int, default_trampoline.cu:186-194).
    - robust-general says "u64". For grids past 2^31, the cold-path helper 1 would compute a different (wrapped) index.
    - It could then return a valid pointer for another thread instead of NULL, giving wrong per-thread counts.
  (c) Wording of robust-general's lowering condition: "every use is a 64-bit atomic add" literally excludes the tools' own `if (c)` NULL test (pt_lat.bpf.c:50-52). It must allow "atomic add or NULL compare".
  (d) target-shape's "any other access is refused at compile time, so the module loads unprobed" is a breakage. A tool that reads a 1531 value (racy, but it loads today) would lose every probe in that module. robust-general's fallback (keep the call, log the breach) is the transparent choice.
- Cost:
  - The lookup+add bundle (0.135, derived from [M-B]) was measured on pt_count, which has no 1530 map: unrouted, no guard, constant key.
  - small-steps' 0.05-0.08 [E: about 51 net instructions at 1.0-1.6 ps] is consistent with it.
  - target-shape's 0.105-0.125 [E] assumes a residual equal to r_smlane minus r_carry (0.010-0.018 [M-C]). At target-shape's R2 the start still lives in local memory. The matching C difference there is c_smlane minus c_carry: 0.053-0.054 (empty/mmvq) and 0.042-0.054 (mem/mmvq) [computed from M-C]. So that rung's estimate is optimistic by about 0.03-0.04 [E]; robust-general states this correctly.

5. Drop the isspacep guard on sum-map atomics, and/or move them to .global red/atom [small-steps 5; target-shape R2 atomic part; robust-general 3 and 3b]
5a. Drop the guard, keep the generic atom, by routing- or IR-emitted provenance (robust-general 3; small-steps 5 without the .global part)
Verdict: STANDS WITH CONDITIONS.
- Basis:
  - Today's not-local branch already runs a generic atom that requires the address to point to .global/.shared (P92:23363).
  - Generic addresses map to global memory unless in the const/local/shared windows (P92:5476). cuMemAlloc returns "linear memory on the device ... suitably aligned for any kind of variable" (drv-mem.txt:1513). "Global memory (also called device memory)" (cpg-writing-cuda-kernels.txt:242), "allocated with ... cudaMalloc" (:246).
  - That a cuMemAlloc'd generic address is outside the local window is an INFERENCE shared with today's path. It is not in facts_docs.txt.
- Conditions:
  - provenance only from an address register with a single definition emitted by routing or the lowering, plus an immediate below value_size;
  - a marker that guard_generic_atomics recognises (robust-general does not say how the PTX-level guard will skip a generic atom);
  - pt_tlatomic's 1530 atomics keep the guard.
- Cost: unknown.
  - small-steps' "measured hint" (parts 0.293 against lat 0.284, so the guard costs no more than noise) is not valid. It only shows the bundles are non-additive, and count is a different, unrouted shape.
  - The executed guard path is about 14 instructions [SASS].

5b. Switch to cvta.to.global plus red.global/atom.global while the snapshot stays generic (small-steps 5; target-shape R2)
Verdict: REFUTED for the main path. Allowed only as a marked off-main-path step under the condition below.
- Unstated step 1, the window: cvta.to.global is undefined outside the .global window (P92:18780). The text mentions "the .global window" (P92:5476, 18715) but does not state which generic addresses it contains.
- Unstated step 2, the address form against the delivery snapshot, which reads with generic ld.relaxed.gpu (nv_gpu_model_maps.cpp:139-179):
  - Overlap requires that two operations "specify the same virtual address" (P92:6847).
  - Distinct virtual addresses of one location are aliases (P92:6851) and "behave as if using different proxies" (P92:7037).
  - Morally strong requires the same proxy (P92:7045), and "Atomicity is not guaranteed if the operations are not morally strong" (P92:7309).
  - Proxy-preserved causality across aliases needs "an alias proxy fence along the base causality path" (P92:7199).
  - Whether a cvta.to.global address and the generic address are distinct virtual addresses is NOT STATED EITHER WAY.
- Counter-evidence:
  - P92:7167 (CUDA API synchronization "participate[s] in proxy-preserved base causality order") may cover the final, API-synchronized count, but it is not explicit for aliases.
  - The ISA's own red litmus (P92 section 8.11.1) mixes a generic st.release and red.global on one variable. That is an example, not a stated rule.
  - The concurrent drain-thread snapshots have no cover at all: torn values in what the CPU reads mid-run are not excluded.
- Condition to revive: move both sides, the programs' adds and the snapshot's loads, to .global. Otherwise keep the generic form (5a).
- red versus atom alone (P92:7511/7541) does not matter here, since nothing acquires through the counters.

6. The key without the generic round trip [small-steps 3a, 3b]
Verdict: STANDS WITH CONDITIONS (both parts).
- 3a basis: the store and the load use one .local address form, and generic equals local plus the window base (P92:5476). This improves today's basis by removing the mixed-form alias gap (Q7c, T5 condition 2).
- 3b basis: per-thread program order on private .local memory (P92:3023).
- Missing conditions:
  (a) V must not be redefined between its st.local and the callseq. The rule excludes only st, atom, red, call and labels, and LLVM PTX registers can have several definitions after PHI lowering.
  (b) Stores go through `%rd1 = add.u64 %SPL, 0` (kmr/routed.ptx:30, and `ld.local.u64 [%rd1+344]` near :5621), not `[%SPL+imm]` directly. The matcher must track that alias.
  (c) Single-definition checks on the whole %SP chain. Dead-store deletion needs a transitive use analysis.
- Breakage: none in the listed tests. pt_carry keeps its stores because its stack pointers go to 503-505. The test_ptxpass one-frame test is unaffected.
- Cost: 0.02-0.04 [E].
  - C ranks local-memory carry far above register carry: c_smlane +0.060/+0.073 against c_param +0.000 on mem/mmvq [M-C].
  - So removing local-memory traffic may matter more than the per-instruction model credits. This cannot be decided without a real-kernel measurement.

7. Load the probe switch once per kernel [small-steps 4; target-shape R4; robust-general 1]
Verdict: STANDS WITH CONDITIONS.
- Basis:
  - It is the same plain ld.global.
  - A false guard means the thread does nothing (P92:22614), so a skipped call leaves words and registers unchanged.
  - No .uni is used, which is correct (P92:22753; Q5b).
  - "Launched after the write" rests on the same API ordering as today.
  - Within a substituted module the switch only goes from 1 to 0 (CLAUDE.md: a later session's probes are not patched into those modules). So robust-general's "re-read at exits is equally correct" holds there.
- Conditions:
  (a) Occupancy check on mmvq, not only mul_mat_q.
    - mmvq is at 56 registers and 36 warps/SM [M-ptxas], which is exactly the rounding boundary of ptxas_check.py:37-39 (256 registers per warp).
    - The patched body already uses P0-P6 [SASS]. A kernel-long predicate landing in a general register is a risk, not a fact; but at 57+ registers it would mean 32 warps/SM.
    - Fall back to reloading the switch at exit sites.
  (b) All of rung 1's breakage applies.
  (c) Add a test on the routed module (test_load_substitution still passes).
  (d) Update the CLAUDE.md contract text.
- Cost: the gate's share of the 0.018 floor [M-B] is unmeasured.
  - target-shape's keep threshold ("empty2 falls by >= 0.002") is below the measured pass spread of up to 0.006 [M-B], so two passes cannot decide it.
  - small-steps' rule (a SASS gate below 0.01) is the sound one.

8. Carry the start by value [small-steps 6 (PTX, kernel .reg); target-shape R5 (IR mirror, kernel .local words) and R8 (.reg words); robust-general 4 (IR, kernel .reg, per-kernel mode)]
Verdict: STANDS WITH CONDITIONS (every form).
- Basis:
  - Arguments and return values are pass-by-value (P92:22755, 6696); one return value, so a .param byte array (P92:6793).
  - Return values must be moved before ret (P92:22825). So every ret, including the gate's `@off ret`, must write the words back, or the gate must be hoisted first. small-steps and target-shape cover this; robust-general states it as a rung-1 dependency.
  - Per-thread register state (P92:1545). Placement is unspecified, which affects performance only (P92:43032).
  - The pack/unpack form `mov.b64 {lo,hi}` is documented (P92:16686, PTX 1.0, all targets).
  - target-shape's kernel area with .align 16 and ld.local/st.local at 8-aligned offsets is documented (P92:4074, 5451).
  - Survival across preemption: not stated (Q9). This is the same basis as every kernel register.
- Conditions and breakage:
  (a) mmvq occupancy.
    - robust-general's "mul_mat_vec_q has room" and "≤ 58 registers with 36 warps/SM" contradict ptxas_check.py:37-39: 58 registers round to 2048 per warp, which gives 32 warps/SM. target-shape R8 states this correctly.
    - small-steps 6 validates only mul_mat_q.
    - robust-general's mode rule uses the original register count, but patching alone changes it (ropek 17 to 32 [M-ptxas]).
  (b) small-steps' PTX-text use analysis must fall back on any multiply-defined or unrecognised use, since an incomplete analysis gives silent wrong values. The IR form is more robust.
  (c) pt_tlatomic must fall back (small-steps) or use the mirror read-modify-write (target-shape, robust-general). pt_carry fits. bpf_main, memcapture and stub-called functions keep today's form. Late path #708 is unchanged.
- Cost:
  - target-shape R5's kernel-side STL/LDL form is c_smlane-like. C ranks that shape well above c_param (+0.060/+0.073 against +0.000 on mem/mmvq [M-C]); c_smlane also carries a flag LDL/STL and an STL just before the RED, so I give no amount.
  - So R8's "-0.003 to +0.003" [E] is not supported by C's ranking, and §1's SASS model (r_smlane, register start) is not the shape R5 produces.
  - robust-general rung 4's high end (0.13 [E]) exceeds its own carry+log2 bundle (0.105, derived from [M-B]).
  - robust-general's high ends for rungs 1-4 sum to 0.306 [E], more than lat's whole 0.284 [M-B]. That double counts the add/carry interaction.

9. clz helper [small-steps 7; target-shape R6; robust-general 5]
Verdict: STANDS WITH CONDITIONS.
- Basis: clz.b64 is defined for every input, with a result of 0..64 (P92:10568ff). 63 - clz(v|1) equals log2_u64(v) for every v, including 0 and 1, by arithmetic. Buckets are identical.
- Conditions:
  - it is a tool change (rule 2 allows it but prefers transparent changes);
  - the new helper id must be accepted on the CPU side wherever helper ids are checked;
  - build it only after the log2 share of the 0.105 bundle is measured.
- Breakage: none for unchanged tools.

10. Splice probe bodies into the kernel [target-shape R7; robust-general 7]
Verdict: REFUTED for the main path (same standard as 5b). Allowed as a marked, off-main-path derivation under these conditions:
- leaf body; no .local; no warp-synchronous instruction; every register and label renamed per copy;
- a target of sm_70 or later, or a branch-free (predicated) body;
- a per-kernel check of the SASS timer placement, as target-shape proposes.
Reasons:
- Call/inline equivalence is not stated (Q5c).
- The divergent-ret reconvergence (P92:22821) is lost. On sm_6x targets, the kernel's later shfl.sync, vote.sync or bar.warp.sync require convergence, or the behaviour is undefined (P92:16861, 23075, 24072). So robust-general's "needs nothing new" and "carries over to other GPUs" are wrong as stated.
- Timer placement relative to kernel code is not stated (Q4c). target-shape adds a check; robust-general has none.
- target-shape's "mandatory under D3" puts a derivation on the main path, which conflicts with rule 3 unless the user accepts it.
Cost: at most the 0.018 floor [M-B]. c_empty (+0.003 [M-C]) is not a model of the real call floor (see rung 2).

11. Real-kernel target measurement and PTX override hook [target-shape R0; the floor-split and log2-split experiments in all three]
Verdict: STANDS (measurement only, off the main path).
Conditions:
- The hand-edited variants must keep the checks the rungs would generate (switch, no-start test, bounds, NULL). Otherwise D1-D5 compare unlike shapes.
- Thresholds must exceed the 0.006 pass spread [M-B].
- The env-gated, sha256-keyed design is right.

12. Shard sizing [robust-general 6]
Verdict: STANDS WITH CONDITIONS (performance only).
- Basis: any shard is correct, by atomicity plus summation.
- Conditions:
  - the index must be below shards by construction. The current formula `((smid & (slots-1)) << 5) | lane` needs at least 32 shards, so "down to 1" needs a different formula;
  - the delivery's S must equal the device copy's.

======================================================================
THE NINE "LOOK ESPECIALLY FOR" ITEMS
======================================================================
1. Alignment of generic addresses into .local.
   - The unstated window-base alignment (Q7c, T5) applies to every generic local access today, 4-byte ones included: the start's ST.E/LD.E at 2000/2020/21e0/21f0/2260/2270 and the key LD.E at 1f10/2110 [SASS].
   - It is not limited to the 8-byte zeroing store. Only rung 6 (3a) and rung 8 (by-value carry) remove such accesses; every memory-mode fallback keeps them.
2. Atomics on generic addresses.
   - 5a has the same premise as today's path.
   - 5b adds the unstated alias step.
   - The routing guard stays the backstop for 1530 values.
3. %globaltimer.
   - Inline and IR forms read the same register; the JIT clause (P92:42073) is unchanged.
   - Placement is not stated either way. The splice needs a placement check.
   - Monotonicity is not stated (Q4c). pt_carry's "start > now" mismatch check, and lat's subtraction, already depend on it today.
4. Warp .sync masks at exit sites.
   - No rung adds a .sync, and all three reject R10 correctly.
   - Fallbacks to helper 1 for host-call map types keep the trampoline's `__activemask()` plus `__syncwarp(active)` unchanged.
   - The only new exposure is the splice on sm_6x (rung 10).
5. Parameter passing: documented for rung 8 (P92:22755, 6696, 6793, 22614, 22825, 16686). Register versus stack placement is unspecified, which affects performance only.
6. Preemption: register survival is not stated (Q9), the same as for any kernel register. A %smid change is covered by masking plus atomics.
7. The switch contract: rung 7 keeps "launched after the write runs unprobed" and makes a thread's entry and exit agree. The CLAUDE.md text must change.
8. Thread-local area zeroing:
   - by-value forms zero the words in the prologue;
   - target-shape's st.local zeroing is documented because its probes no longer read the area through generic addresses;
   - small-steps' open question 6 (st.local zeroing alone) creates a mixed form (see the wrong-claims list).
9. What the CPU sees:
   - shard choice, clz and the timer inlining keep exact counts and buckets;
   - 5b leaves concurrent deliveries without an atomicity guarantee;
   - a mismatched 1532 index (rung 4b) changes per-thread counts;
   - target-shape R2's refuse-to-load removes every probe of a module.

======================================================================
CLAIMS IN THE LADDERS THAT ARE WRONG
======================================================================
1. small-steps 5 ("the same premise as today's unguarded @!l atom branch") and target-shape R2 ("the same as today's atom, with no new assumption"), for the .global forms. The .global switch adds the window premise (P92:18780) and the alias question against the generic snapshot (P92:6847, 6851, 7037, 7045, 7199, 7309).
2. small-steps 5's "measured hint" that the guard costs no more than noise. Parts 0.293 against lat 0.284 [M-B] only shows non-additivity, and count is unrouted.
3. small-steps open question 6: st.local zeroing is not "within rule 1" while the probes read the area through generic addresses. It trades the alignment gap for the generic/.local alias gap (T5 condition 2, Q7c).
4. robust-general rung 4: "mul_mat_vec_q ... has room" and "≤ 58 registers with 36 warps/SM by registers". By ptxas_check.py:37-39, 58 registers give 32 warps/SM; 56 is exactly the 36-warp boundary [M-ptxas].
5. robust-general rung 0: the constant_fd examples xor and cvta, and the "mov then xor" unit test. Both are already counted by the unanchored regex; the real gap is mad, neg, rem, min/max, shf, not and similar. The same xor error is in facts_code R1/section 2.
6. robust-general rung 3: "lower only when every use ... is a 64-bit atomic add" excludes the tools' own NULL test.
7. robust-general rung 7: "needs nothing new" and "carries over to other GPUs" (sm_6x convergence, P92:16861; Q5c). target-shape: "R7 is mandatory under D3" puts a derivation on the main path, against rule 3.
8. target-shape R2: the residual is "consistent with r_smlane minus r_carry". At that rung the start is in local memory, where C gives c_smlane minus c_carry, 0.042-0.054 [computed from M-C].
9. target-shape R2/§0: "removes ... ptxas's generic-atom expansion (2600-26f0)". From the branch structure (ATOM.E P0, then @P0 BRA 0x2700 at 25e0/25f0), that block does not run on the global path; the meaning of P0 is inferred, so this is [E]. The removed guard is also not part of count's 0.135 bundle.
10. target-shape §1 and R8: the SASS model is r_smlane for a target that keeps the start in .local, and R8 is put at ±0.003. C's ranking (c_smlane against c_param [M-C]) does not support that.
11. robust-general rung 2's saving of 0.030-0.037 depends on carrying C amounts over. The CS2R reads and compares stay inside the 0.035 bundle.
12. robust-general's per-rung high ends sum to 0.306 against lat 0.284 [M-B], and rung 4's high end (0.13) exceeds its 0.105 bundle.
13. robust-general's endpoint 0.015-0.045 (0.01-0.035 with rung 7).
    - Its low end needs about 0.00015-0.0003 ns per remaining non-call instruction, below every B-derived rate (0.64-2.2 ps per instruction; target-shape §0 [E from M-B]).
    - About 35-45 non-call instructions remain [E]. At 0.64 ps that is already 0.022 or more, before any call floor.
14. Keep thresholds of 0.002-0.005 ns in two passes (target-shape R3, R4, R7; robust-general rung 1's +0.002 test) are below the measured pass spread of up to 0.006 [M-B], so two passes cannot decide them.
15. small-steps' side remark that mmvq is "near issue-bound" (an issue limit of 0.96 ps). The "4 schedulers per SM" premise is not in the inputs, and empty2's derived 0.64 ps per instruction [E from M-B] is below that limit. The "budget not reached" conclusion stands on B-derived rates, not on this remark.
16. small-steps (Endpoint) and robust-general compare an mmvq-only figure with a budget stated "in all". With PT_SET_LLAMA4 the other kernels add about 0.015 today [E: A's 0.299 against B's 0.284, from different passes]. The full-tool test (robust-general's final rule) is the right one.
17. small-steps rung 3b omits that V must not be redefined before the callseq, and that stores go through the %rd1 = %SPL+0 alias. These are a missing condition, not a false number.
