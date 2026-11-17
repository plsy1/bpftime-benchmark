FINAL LADDER: per-thread cost of pt_lat on tg128 mul_mat_vec_q (mmvq). Read-only synthesis of three ladders and the refuter review. Nothing was built, run or modified.

## 0. Labels and accounting

**Labels**
- [M-A], [M-B], [M-C]: measured, from measurements.md tables A, B and C.
- [M-ptxas]: measured, from results/gpu/map-model-proto-20261116/ptx/ptxas_check-final.txt.
- [SASS]: instruction counts I made from probe-sass-excerpt.txt and mmvq-patched-lat.ins.txt.
- [E]: estimate; the reasoning is given beside it.
- [G]: a guess about LLVM or ptxas behaviour. A PTX or SASS check settles each one.
- "fd" means map descriptor number. "Words" means the 8-byte values of the thread-local map 1530.

**Document references**
- "P92:n (Qx)" or "(Tx)" means the quote is in facts_docs.txt.
- "P92:n [read]" means I read the line myself in step0b/c5-guarantees/docs/v13.2.2/text/. It is not in facts_docs.txt. This applies to:
  - P92:2299, the full sentence at 5476, and 6793, 6847, 7045, 7199, 7309, 10582, 16686, 22614 and 22825;
  - drv-mem.txt:1513;
  - cpg-writing-cuda-kernels.txt:242 and 246.

**Baseline**
- lat costs 0.284 ns per thread (a1 0.286, a2 0.282) [M-B].
- Its bundles are:
  - call floor 0.018;
  - timer 0.035;
  - carry + log2 0.105;
  - lookup + add 0.135, which was measured on count: unrouted, with no guard.
- Each bundle caps any saving that lies inside it. Every split inside a bundle is [E].
- The two passes differ by up to 0.006 [M-B]. So no timing threshold below is under 0.006. A rung expected to save less than about 0.01 is decided by its SASS diff plus a rule that it must not regress.

**Cost per instruction [E from M-B]**
- Today about 212 SASS instructions run per thread [SASS].
- Average costs:
  - empty2: 0.018 / ~28 = 0.64 ps;
  - lat: 0.284 / 212 = 1.34 ps;
  - lookup+add: 0.135 / ~75 = 1.8 ps;
  - timer: 0.035 / ~16 = 2.2 ps.
- No amount from table C is carried over. C is used only to rank shapes: c_empty at the mmvq shape costs +0.003 [M-C], while the real empty2 costs 0.018 [M-B].

**Conversion to tg128 (mmvq only)**
- loss = 1 - 1.883 / (1.883 + 3.137e9·x), where x is the added ns per thread.

| x (ns per thread) | tg128 loss |
|---:|---:|
| 0.284 | -32% |
| 0.20 | -25% |
| 0.12 | -17% |
| 0.09 | -13% |
| 0.06 | -9% |
| 0.045 | -7% |
| 0.031 | -5% |

- The budget is "in all", so the final test is PT_SET_LLAMA4. The other kernels add about 0.015 ns-equivalent today [E: A 0.299 against B 0.284, from different passes]. If they shrink in proportion, the mmvq target is about 0.029 [E].

**Common protocol for every rung**
- One build, with each rung behind its own on/off switch. Each rung is timed against the same build with that rung off, in interleaved runs.
- Before measuring: clocks checked and no leftover processes.
- Timing: tg128 PT_SET_MMVQ, `-r 5` plus warmup, 2 shuffled passes.
- Count check: the sum map must hold exactly 15,711,207,424 for count and lat.
- Correctness set: pt_victim exact (lat, hist, carry, tlatomic), bpftime_nv_attach_tests, and the runtime [cuda-*] tests.
- ptxas_check on the four kernels.
- Every rewrite gets a golden-PTX unit test that includes "an unknown shape is left as today". There are no in-tree routing tests today (facts_code §4).
- Fallback rule: a new matcher never throws. When a shape is unknown, today's code stays.

## 1. The ladder (main path: documented, ordered cheapest first, each measurable alone)

Sources: SS = small-steps, TS = target-shape, RG = robust-general.

**Where each kind of change lives**
- Kernel-side work (the switch, call sites, the prologue) goes in PTX routing, because kernel PTX comes from nvcc and has no IR.
- Work that needs data flow (the constant fd, key forwarding, the carry) goes in LLVM IR, in llvmbpf.
- SS's PTX key-forwarding rungs (3a/3b) are therefore not separate rungs. The IR lookup (rung 3) and the IR carry (rung 5) remove the key round trip through store-to-load forwarding [G].

---

### Rung 0. constant_fd soundness fix [RG 0; refuter item 1]

- **Removes:** nothing.
- **Where:** nv_thread_local_routing.cpp:119-157. Anchor the definition regex at the opcode, and count every instruction whose destination is the register.
  - Today mad, neg, rem, min/max, shf, not and similar are missed.
  - xor and cvta are already counted, because the current regex is unanchored.
  - About 10 lines.
- **Why first:** a misidentified fd applies another map's layout to a pointer. That is a correctness gap in today's code.
- **Basis:** nothing new.
- **Saving:** 0. Cumulative 0.284 [M-B].
- **Validate:**
  - Unit test: a register defined by mov and then by mad (and then by neg) is not constant.
  - Patched PTX is byte-identical for all current tools.

---

### Rung 1. IR hook + inline timer (helper 502) [SS 1+1b; TS R1 hook + R3; RG 2]

**Removes** [SASS]:
- entry 1fd0 MOV and 1fe0 CALL;
- exit 2230 MOV and 2240 CALL;
- the helper's 5170 IMAD.MOV and 5190 RET, twice;
- the reload of the start after the timer, 2250 ULDC plus 2260/2270 LD.E, if GVN forwards [G].

That is about 11 executed instructions, including 2 generic local loads. The CS2R stays, now inline.

**Where:**
- New overlay patch vm/compat/llvm-vm/gpu-lowering.patch.
  - In generate_ptx (gen-ctx:1160-1176): optimize, then lower, then optimize again. This follows the CPU array-inline precedent (gen-ctx:722-726, 754-758).
  - The order matters: eBPF registers are allocas until the first optimization (compiler.cpp:376-380).
- `_bpf_helper_ext_0502` becomes `call i64 asm sideeffect "mov.u64 $0, %globaltimer;"` with inaccessiblememonly and nounwind.
  - It must not be readnone, which would let two reads merge.
- Optional in the same rung: 509/510/511 become nvvm sreg intrinsics. This does not affect lat.
- No request field is needed yet.
- About 70-110 lines.

**Basis:** nothing new.
- It is the same register the helper reads (default_trampoline.cu:588-592; P92:42056/42073, T2), with one 64-bit read.
- The read stays inside the called probe function.
- Placement relative to other instructions is not stated in either form (Q4c), and neither is inline versus call (Q5c).

**Expected:** 0.007-0.024 [E: 11 instructions × 0.64-2.2 ps, capped by the 0.035 timer bundle].
- Cumulative 0.260-0.277 (-30% to -32%).
- RG's 0.030-0.037 is not used, because it carries C amounts over (refuter #11).

**Risk:**
- Low. The helper stays defined for unlowered code and test_cuda_host_call.
- If GVN does not forward, the reload stays; that affects performance only.

**Validate:**
- PTX: no call to 0502; at most one load pair of *s per exit.
- SASS: no CALL 0x5170.
- lat exact.
- pt_victim kspin buckets equal the helper build's.

**Decision:**
- Keep if lat falls by at least 0.007 in both passes.
- Otherwise keep on SASS grounds if neither pass is worse by more than 0.006, because the hook is needed by rungs 3 and 5.

---

### Rung 2. Read the probe switch once per kernel [SS 4; TS R4; RG 1]

**Scope:** modules routing already handles, which includes lat because it has a 1530 map. Rung 6 widens it to all modules.

**Removes** [SASS]:
- entry 1e50-1e80 and 1eb0-1ec0;
- exit 2040-20b0 (MOV/ULDC of the switch address, LDG.E, ISETP, @!P0 RET).
- That is about 12 instructions, including 2 dependent LDGs. About 4 are added in the prologue, plus predicates on the call sites. Net about -8, and one LDG fewer.

**Where:**
- Routing's kernel rewrite (nv_thread_local_routing.cpp:403-484): the prologue at :464-471, and the label and predicate parsing at :438-446.
- The prologue does `ld.global.u32` of the switch and `setp`.
- Each call becomes `@on call`, or `and.pred q,p,on; @q call` at a predicated site. Never `.uni` (P92:22753, Q5b).
- The gate block (exact text, core.cpp:200-209) is removed only from functions that only rewritten kernel sites call. Stub-called functions, memcapture and bpf_main keep it.
- About 40-60 lines.

**Basis:** nothing new.
- It is the same plain load.
- "Threads with a False guard predicate do nothing" (P92:22614 [read]).
- "Launched after the write runs unprobed" keeps the same API ordering, and a thread's entry and exit now always agree.

**Expected:** 0.003-0.009 [E]. The gate's share of the 0.018 floor is unmeasured; M1 measures it. Cumulative 0.251-0.274.

**Risks:**
- mmvq has 56 registers, which is exactly the 36 warps/SM boundary (ptxas_check.py:37-39) [M-ptxas]. A predicate kept live across the body should sit in a predicate register (P0-P6 are in use [SASS]). If it lands in a general register, mmvq drops to 32 warps/SM.
  - Fallback: reload the switch at each exit site. That is equally correct, because within a substituted module the switch only goes from 1 to 0 (CLAUDE.md).
- The CLAUDE.md contract text must change.
- Add a test on the routed module. test_load_substitution still passes, because the gate is still inserted at compile time.

**Validate:**
- SASS: one switch LDG per kernel and none in the probes.
- ptxas: mmvq ≤ 56 registers; mmq spills unchanged.
- detach-switch.sh: events stop at the detach.

**Decision:**
- If M1's gate share is at least 0.007: keep if empty2 falls by at least 0.006 in both passes.
- Otherwise accept on SASS and contract grounds if neither pass is worse by more than 0.006. It is also a precondition of rung 5.

---

### Rung 3. Inline the 1531 lookup for a constant fd, in IR, guard kept [SS 2; TS R2 lookup; RG 3 lookup]

**Removes** [SASS]:
- 24c0-2500 call setup, 2510 CALL, and the result moves 2530/2540;
- helper 1's path for type 1531, about 56 instructions:
  - 2800-28a0 (index, LDC map_type, type tests);
  - 3a20-3a30;
  - the BRX jump table at 40f0-4140;
  - 4f20-5160 (LDCs of extra_buffer, value_size and max_entries; the generic LD.E of the key; compares; shard math; BSYNC/RET);
- the key's generic reload.

About 12-14 are added: map_info loads, S2UR smid, S2R laneid, LOP/SHF, IMAD.WIDE, a bound ISETP, a base≠0 test, SEL. Net about -50. The key's STL at 24b0 may stay [G].

**Where:**
- New request field `gpu_maps`: fd, type, key_size, value_size, max_entries, shards. It carries no addresses.
  - Filled in hack_fatbin (nv_attach_impl.cpp:1056-1125) from map_basic_info.
  - Passed through compile_ebpf_to_ptx_from_words (core.cpp:226-274) to the vm.
  - It joins the patch-cache key automatically (:1073).
- In the lowering, a helper-1 call whose fd is an LLVM constant of type 1531 becomes:
  - a load of the key (GVN forwards the stored key [G]);
  - `key < max_entries`;
  - base and shards read from `map_info[fd]` with ld.const at offsets 24 and 32 (static_assert those offsets, F3);
  - NULL if base == 0, shards < 32 or the key is out of range;
  - shard = `((smid & ((shards>>5)-1))<<5) | (laneid & 31)`, which is exactly sum_array_offset (default_trampoline.cu:264-275).
- Lower only when every use of the returned pointer is a 64-bit atomic add or a NULL compare. Otherwise keep the call and log the breach (refuter 4c/4d). Never refuse to load the module.
- The atomic stays LLVM's generic atomicrmw (monotonic, align 8). In this rung routing still guards it.
- Strip `.extern .const map_info` in the filter (as at core.cpp:280-283).
- About 150-220 lines.

**Basis:** nothing new.
- Same arithmetic and NULL conditions as the helper.
- Any shard is correct: only atomics write, and the delivery sums all shards (default_trampoline.cu:259-263).
- Masked %smid is in range for any value (P92:41436, 41462; Q8).
- %laneid is 0..WARP_SZ-1 (P92:41248), and WARP_SZ is 32 on all targets to date (P92:2299 [read]); `& 31` makes the range hold by construction.
- Reading map_info at run time keeps NULL-on-no-copy and the late path's per-module values (nv_attach_fatbin_record.cpp:263-291).

**Expected:** 0.04-0.09 [E: about 50 net instructions × 0.8-1.8 ps, capped by the 0.135 bundle]. Cumulative 0.16-0.23 (-21% to -28%).
- TS's 0.105-0.125 is not used. At this rung the start is still in local memory, and C ranks that shape high (c_smlane against c_carry) (refuter #8).

**Risks:**
- Tools with dynamic fds keep helper 1.
- There is one extra O3 pass, only when a lowering fires.
- [G] whether ptxas folds the map_info loads into constant operands.

**Validate:**
- With the lowering switched off, the patched PTX is byte-identical for every tool.
- SASS: no CALL to helper 1 on the exit path.
- count and lat exact; pt_victim exact.
- Unit tests: constant key, dynamic key, key out of range gives NULL, base 0 gives NULL, a plain store to a 1531 value keeps the call.

**Decision:**
- Keep if count falls by at least 0.02 in both passes.
- If it falls by less, stop and attribute before rung 4.

**Extension 3b (not needed for lat): 1532.**
- The index must use the trampoline's exact int arithmetic (getGlobalThreadId, default_trampoline.cu:186-194). RG's "u64" would differ for grids past 2^31.
- An out-of-range thread takes the cold helper-1 call.

---

### Rung 4. Skip the isspacep guard on rung-3 atomics (SASS-gated) [refuter 5a; SS 5 and RG 3 guard part, without .global]

- **Removes** [SASS]: 2580 QSPC.E.L, 2590 BSSY, 25a0 @P1 BRA, 2700-2750, 2760-27b0 and the 27c0-27f0 tail. About 14 executed instructions.
- **Where:**
  - The lowering emits the 1531 add as an inline-asm block carrying a fixed tag, for example `atom.add.u64 ...; // bpftime:sum1531`. The instruction text is the same as today's generic atom (default .relaxed and .gpu).
  - guard_generic_atomics (nv_thread_local_routing.cpp:159-291) skips exactly the tagged lines.
  - eBPF programs cannot emit inline asm, so the tag can only come from the lowering. Provenance therefore holds by construction.
  - Every untagged atomic keeps the guard, including pt_tlatomic's.
  - About 30-40 lines.
- **Basis:** the same premise as today, marked as an inference.
  - Today's not-local branch already runs this generic atom on this address, which requires that "the address points to .global or .shared" (P92:23363, Q6). Today's exact counts already rest on that premise.
  - The premise is an inference from three lines:
    - a generic address "maps to global memory unless it falls within the window for const, local, or shared memory" (P92:5476 [read]);
    - cuMemAlloc returns "linear memory on the device" (drv-mem.txt:1513 [read]);
    - "Global memory (also called device memory)" (cpg-writing-cuda-kernels.txt:242 [read]).
  - No single sentence states that a cuMemAlloc pointer lies outside the local window. The rung adds no premise; it stops re-checking one at run time.
- **Expected:** 0.005-0.02 [E: about 14 instructions × 0.4-1.34 ps]. SS's "measured hint" is not used (refuter #2). Cumulative 0.14-0.23.
- **Validate:**
  - SASS: no QSPC on the 1531 add.
  - Exact counts; pt_tlatomic passes.
- **Decision:**
  - Keep if lat falls by at least 0.006 in both passes.
  - Otherwise keep on SASS grounds if neither pass is worse by more than 0.006.

---

### Rung 5. Carry the 1530 words by value [SS 6; TS R5/R8; RG 4]

**Precondition:** rung 2. Return values must be moved into the return parameters before every ret (P92:22825 [read]), and a hoisted gate leaves no `@off ret` inside the probe.

**Removes** [SASS]:
- kernel: 0190-01b0 (area address and the generic ST.E.64 zeroing) and 1aa0/1ad0;
- entry: 1ed0-1fc0 (key STL, generic key LD.E, IMAD.WIDE, ISETP, SEL ×2, NULL test, RET guard, area moves) and 2000/2020 (two generic ST.E of the start);
- exit: 20c0-21c0 and 21d0-21f0 (two generic LD.E of the start).

That is about 40 instructions, including 8-10 generic accesses into the local window. About 6-10 are added: parameter moves, plus one STL and one LDL in local mode. Net about -30.

**Where, IR side** (about 150-250 lines):
- In by-value mode, the probe function takes W i64 words, with W capped at 4 [E]. It returns them as one `.param .align 8 .b8 retval[8W]` (P92:6793 [read]).
- A constant-fd, constant-key 1530 lookup becomes a GEP into a local mirror, which is filled from the parameters and written back at every return.
- Atomics on the mirror become plain load/op/store. The mirror is private (.local, P92:3023).
- A dynamic key or an escaping pointer keeps the mirror in memory: correct, but slower.
- [G] SROA promotes the mirror when nothing escapes.

**Where, routing side** (about 100-150 lines): the mode is chosen once per kernel, before that kernel's programs are compiled, so entry and exit always agree.
- Register mode: `.reg .b64` words, set to 0 in the prologue, passed in and taken back under the call's own predicate.
- Local mode: `.local .align 16 .b8 __bpftime_tl[8W]`, accessed only with ld.local/st.local at constant 8-aligned offsets, zeroed with st.local, and never handed out as a generic address.
- A kernel where any program needs today's pointer form stays entirely on today's form. Such programs are those with an area over the cap, bpf-to-bpf calls, stub-called functions or a non-constant fd. This rule means one area is never accessed through both generic and .local addresses.

**Mode rule (tuning, fixed now):**
- Default to local mode.
- Use register mode on a kernel only if both hold:
  - ptxas (--verbose) reports no fall in warps/SM by registers and no new spills;
  - for mmvq, step M3 measures it at least 0.006 faster in both passes.
- mmvq at 56 registers is exactly on the boundary: two more registers give 32 warps/SM [E from the allocation rule].
- mmq (255 registers, 60 B of spills [M-ptxas]) goes to local mode.

**Basis:**
- Arguments and return values are pass-by-value (P92:22755, 6696; Q5).
- Single return value as a .param byte array (P92:6793 [read]).
- Per-thread register state (P92:1545; Q9).
- .local is private (P92:3023), `.align` is honoured (P92:4074), and natural alignment holds (P92:5451; Q7).
- Physical placement is unspecified (P92:43032, 6787), which affects performance only.
- Survival of registers across preemption is not stated (Q9). That is the same status as every kernel register and today's area.
- The rung improves the basis: it removes today's generic accesses into the local window, whose alignment and alias status are not stated (T5, Q7c).

**Expected:** 0.025-0.06 [E: about 30 instructions × 0.8-1.8 ps, inside the carry part of the 0.105 bundle; C ranks a register-held start far below a local-memory one, but no C amount is used]. Cumulative 0.08-0.20 (-12% to -25%).

**Risks:**
- An incomplete escape analysis gives silent wrong values. The IR analysis must fall back on any use it does not recognise.
- pt_carry keeps its stack, because helpers 503-505 take stack pointers. That is correct.
- [G] LLVM emits the aggregate return as a .param array.
- Late path #708 is unchanged.

**Validate:**
- SASS: no generic LD/ST into the local window and no depot STL/LDL on mmvq's probe path. Local mode shows exactly one STL at the entry site and one LDL at the exit site.
- ptxas: mmvq warps/SM per the mode rule; mmq spills unchanged.
- pt_carry and pt_tlatomic have 0 mismatches with each mode forced.
- kmr's 7-exit forms are accepted.
- lat and hist exact.
- pp512 PT_SET_LLAMA4 is not worse by more than 0.5 points.

**Decision:** keep if carryonly and lat each fall by at least 0.012 in both passes.

---

### Rung 6. Routing for every probed module (generality; no change for lat) [SS 0; RG 0; refuter item 1]

- **Where:** drop the 1530-only filter and the early return (nv_attach_impl.cpp:997, 1007-1008). About 30-50 lines.
- **Conditions:**
  - In a module without a 1530 map, the existing throws (:486-501, :190-191, :211-226, :362-366) leave the module as it is today: probed and unrouted, not unprobed.
  - Untagged atomics now get the guard. This closes today's unguarded stack-atomic case.
  - count does not pay for the guard after rung 4.
  - Update the kmr route-stats expectations.
- **Basis:** nothing new.
- **Saving:** 0 for lat. It brings rungs 2 and 4 to count-like tools.
- **Validate:**
  - Every tool loads probed as today.
  - count exact.
  - detach-switch.sh and the nv_attach tests pass.
  - count is not worse by more than 0.006.

---

### Rung 7. clz helper (the only tool change; gated by M3) [SS 7; TS R6; RG 5]

- **Removes** [SASS]: the log2 chain at 2290-2490, about 32 instructions of ISETP/SEL/SHF/LOP3. About 5-6 replace it: LOP3 |1, FLO, ISETP/SEL, IADD3. Net about -26 ALU instructions.
- **Where:**
  - Helper 512, `63 - clz(v|1)`.
  - Defined in default_trampoline.cu; trampoline_ptx.h regenerated.
  - Registered in core.cpp:234-253 and wherever the CPU side checks helper ids.
  - Lowered in IR to `63 - llvm.ctlz.i64(v|1)` [G: NVPTX gives clz.b64].
  - pt_common.h's log2_u64 calls the helper: a one-line tool change.
  - About 40 lines.
- **Basis:**
  - clz.b64 is defined for every input, with a result of 0..64 (P92:10582 [read]; facts_code R5).
  - v|1 is never 0, so 63 - clz(v|1) equals log2_u64(v) for every v, including 0 and 1. That is arithmetic. Buckets are identical.
- **Expected:** 0.01-0.035 [E: 26 ALU instructions × 0.4-1.34 ps; mmvq may hide ALU work]. Cumulative 0.05-0.19 top-down.
- **Gate (fixed now):** build only if M3 gives T-chain minus T-reg ≥ 0.01 in both passes, or if D1 holds only with clz.
- **Validate:**
  - GPU unit test against log2_u64 on 0, 1, 2^k-1, 2^k and 2^k+1 (k = 0..63) plus random inputs.
  - SASS shows FLO and no chain.
  - lat exact.
  - Keep if lat falls by at least 0.008 in both passes.
- **Transparent alternative:** an IR matcher for the known log2 chains. It is not undocumented, but it is fragile, so it is not on the main path.

**Sanity checks**
- Per-rung high ends sum to 0.238 and low ends to 0.09. Both are below 0.284.
- About 650-950 lines in total [E].

## 2. Not on the main path (undocumented basis, or refuted for the main path)

- **N1. Splice probe bodies into the kernel** [TS R7; RG 7]: a derivation, not documented.
  - Why it is a derivation:
    - Equivalence of a call and its inlined body is not stated (Q5c).
    - Timer placement relative to kernel code is not stated (Q4c).
    - On sm_6x the kernel's later .sync instructions require convergence (P92:16861, Q1), which today's divergent-ret reconvergence provides (P92:22821, Q5). mmvq uses warp-synchronous code (BRA.DIV at 1480 [SASS]).
  - Conditions if the user accepts the derivation:
    - leaf body, no .local, no warp-synchronous instruction;
    - every register and label renamed per copy;
    - an sm_70+ target or a branch-free body;
    - a per-kernel SASS check that the entry CS2R comes before the body's first global access and the exit CS2R after its last.
  - Saving: at most the call part of the 0.018 floor [M-B]; [E] 0.008-0.015.
- **N2. .global red/atom through cvta.to.global** [SS 5; TS R2]: refuted for the main path.
  - Window membership is not stated (P92:18780, Q7b).
  - The snapshot reads with generic ld.relaxed.gpu (nv_gpu_model_maps.cpp:139-179). Whether a cvta.to.global address and the generic address it came from are aliases is not stated (P92:6851, 7037 Q7c; 6847, 7045, 7309 [read]). Atomicity needs morally strong operations.
  - Revive only if both the programs' adds and the snapshot loads move to .global.
  - Saving ≤ 0.003 [E].
- **N3. Warp combine via activemask/match/redux** [R10]: lane agreement is not stated (Q2), and step0b C2 measured disagreement (39,143 and 116,089 of about 4.46M calls). The gain is small (r_warp against r_smlane [M-C]).
- **N4. call.uni** to drop BSSY/BSYNC: a derivation (Q5c). About 2 instructions per site.
- **N5. Switch read through ld.global.nc or the constant bank:** whether host writes are visible on those paths is not in the collected docs.

**Not done, for other reasons**
- The R4a DataLayout one-liner: misaligned i64 accesses are undefined (P92:5451).
- PTX-text key forwarding and pair merging (SS 3a/3b, R4c): rungs 3 and 5 subsume them. The refuter also found missing conditions: V redefined before the call, and the `%rd1 = %SPL+0` alias.
- %clock64: it changes the reported values (T6).
- Refusing to load a module on a 1531 contract breach (TS R2).
- Baking device addresses into the PTX.
- Lane-only or SM-only shards.
- CTA aggregation with barriers at exits.
- Shard sizing for other GPUs (RG 6): performance only, no change on this board. Its formula needs at least 32 shards today.

## 3. Expected endpoint against the 0.031 budget

| after | ns per thread, mmvq | tg128, mmvq only |
|---|---:|---:|
| today | 0.284 [M-B] | -32% |
| rungs 0-2 | 0.25-0.274 [E] | -29% to -32% |
| + rung 3 | 0.16-0.23 [E] | -21% to -28% |
| + rung 4 | 0.14-0.23 [E] | -19% to -28% |
| + rung 5 (transparent end) | 0.08-0.20 top-down; 0.055-0.115 bottom-up [E] | -8% to -25% |
| + rung 7 (one tool line) | 0.05-0.19 top-down; 0.04-0.10 bottom-up [E] | -6% to -24% |

- **Bottom-up method:** about 60-75 instructions remain [SASS: 212 minus the removals listed per rung]. At 0.64-1.34 ps each [E from M-B] that is 0.04-0.10.
  - The kernel side keeps the switch, BSSY/CALL/BSYNC ×2 and the moves.
  - The entry keeps about 4.
  - The exit keeps about 35-42: start test, CS2R, subtract, clz, key, bound, map_info, smid/lane, shard address, NULL tests, ATOM, RET.
- **Honest range for the full main path:** 0.045-0.12, most likely 0.05-0.09. That is about -7% to -13% on tg128 for mmvq alone, plus about 5% relative for the other kernels [E].
- **The documented main path with calls kept is not shown to reach 0.031.**
  - Even at the lowest B-derived rate (0.64 ps), about 60 instructions cost about 0.038.
  - Adding N1 (the splice) gives about 0.035-0.08 [E]. That still reaches the budget only at its low end, and it is a derivation: whether to accept it is the user's decision, not a rung.
- pp512 lat is 4.2% today [M-A] and must not regress.

## 4. What to measure first (before any product code)

**Biggest assumption:** what the real mmvq charges per remaining probe instruction, and whether removing local-memory traffic gives the large drop table C shows. B-derived rates span 0.64-2.2 ps, which alone moves the endpoint by a factor of 3.

Experiments, cheapest first:

**M2. log2 share** (tool variant only, no bpftime code)
- carrynolog: pt_lat with the bucket replaced by a compare of the raw delta with a 64-bit constant that ptxas cannot prove unreachable.
- First check in SASS that both CS2R reads and the subtract survive. The earlier m_carry lost them (measurements.md C correction).
- Measure against carryonly, 2 passes.

**M1. gate share of the floor** (about 5 lines, debug only)
- An env switch that skips gate_probe_function (core.cpp:196-224).
- Measure empty2 against empty2-nogate, 2 passes.

**M3. the end shape on the real kernel** (about 30-50 lines, debug only, never on the main path)
- Needs an env-gated override at compile_ptxs (nv_attach_fatbin_record.cpp:103-170): if `<dir>/<sha256 of the patched PTX>.ptx` exists, compile that file instead. The sha256 is already the ptx_pool key (:139).
- What exists today does not do this:
  - BPFTIME_CUDA_KEEP_PTX only keeps the work directories (nv_attach_impl.cpp:2044);
  - BPFTIME_CUDA_LATE_PTX_DIR supplies original PTX for the late path (:747).
- Hand-edit the kept patched PTX of lat (PT_SET_MMVQ) into the variants below. Every variant keeps the switch load, the no-start test, the bounds and NULL checks, and the 1531 add, so the count check (15,711,207,424) holds.
  - T0: unchanged, passed through the hook. It must equal lat within 0.006.
  - T-mem: the end shape, with the start still in today's generic area.
  - T-reg: the end shape with calls kept and words in kernel .reg.
  - T-loc: the same with words in kernel .local, using ld.local/st.local only.
  - T-chain: T-reg with the log2 chain instead of clz.
  - T-inline: T-reg spliced. This is a measurement only; N1 stays off the main path.
- Run base and empty2 in the same 2 shuffled passes.

**Decision rules (fixed now; mmvq target 0.029; must hold in both passes)**
- D1: min(T-reg, T-loc) ≤ 0.029 → the documented main path can meet the budget. Build rungs 0-6, and rung 7 per D4.
- D2: min(T-reg, T-loc) > 0.029 ≥ T-inline → the budget needs N1. Build rungs 0-6 anyway, and put N1 to the user with its conditions.
- D3: T-inline > 0.029 → the shape misses the budget even when spliced. Build rungs 0-5 for their gain, and reopen the design (tool shape or budget) before any splice work.
- D4: rung 7 only if T-chain minus T-reg ≥ 0.01, or if D1 holds only with clz.
- D5: mmvq carry mode = register only if T-reg < T-loc - 0.006. The measurement already includes any occupancy loss.
- D6: if T-mem minus T-reg ≥ 0.03, build rung 5 before rungs 3-4. It depends only on rungs 1 and 2.

## 5. Open questions

1. The real per-instruction cost (M3), the gate share (M1) and the log2 share (M2).
2. [G] items to confirm in PTX/SASS:
   - GVN forwarding across the inaccessiblememonly asm;
   - SROA promoting the mirror under LLVM 15's legacy pass manager;
   - the aggregate return as a .param array;
   - llvm.ctlz giving clz.b64;
   - map_info loads as constant operands;
   - whether the key STL survives rung 3.
3. Whether cuMemAlloc memory lies outside the local window is an inference shared with today's path. To make it a run-time check: isspacep.global (P92:18711, Q6), with the atom predicated on it and a fallback to the guarded path otherwise. Cost about 1-3 instructions [E]. The .param window lies inside the .global window (P92:5476 [read]; 18715), which would need a word of justification.
4. Today's fallback forms still make generic 4/8-byte accesses into the local window. Their alignment and alias status are not stated (T5, Q7c). Rung 5 removes them only in by-value kernels.
5. Rung 5's mode rule adds one ptxas compile per probed kernel at attach. Is that cost acceptable, or should a static rule be used?
6. If D2 occurs, does the user accept N1 (a derivation from documented call semantics, with a per-kernel SASS placement check as the run-time check)?
7. 1532 (threadhist): its cost is bytes, about 0.21 ns per thread [E per RG: DRAM traffic on an 8 MB array]. Rungs 3b-5 will not bring it near 5%; it needs its own design.
8. The late path (#708) for entry plus exit on one kernel is unchanged by every rung.

## Files read

- /home/jetson/src/bpftime-gpu-work/proto/design-20261117/: measurements.md, probe-sass-excerpt.txt, facts_docs.txt, facts_code.txt, mmvq-patched-lat.ins.txt (kernel call sites).
- /home/jetson/src/results/gpu/map-model-proto-20261116/tools/: pt_lat.bpf.c, pt_common.h.
- /home/jetson/src/bpftime-gpu-work/proto/probe-shape/probe_shape.cu.
- /home/jetson/src/results/gpu/map-model-proto-20261116/ptx/ptxas_check-final.txt and ptxas_check.py.
- In /home/jetson/src/bpftime-proto:
  - nv_attach_impl.cpp:745-760 and :2040-2046 (existing env hooks);
  - nv_gpu_model_maps.cpp:130-182;
  - default_trampoline.cu:183-200 and :255-295.
- In /home/jetson/src/bpftime-gpu-work/step0b/c5-guarantees/docs/v13.2.2/text/: the [read] lines listed in section 0.