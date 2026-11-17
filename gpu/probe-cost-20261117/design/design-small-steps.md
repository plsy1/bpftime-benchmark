LADDER: smallest PTX, trampoline and llvmbpf changes toward 0.031 ns per thread (tg128, largest mul_mat_vec_q). Read-only analysis. Nothing was built or run.

## 0. Accounting rules for every number below

**Baseline.**
- All real-kernel numbers come from measurements.md table B.
- lat costs 0.284 ns per thread (a1 0.286, a2 0.282).
- Its bundles: call floor 0.018, timer 0.035, carry+log2 0.105, lookup+add 0.135. Together they come to 0.293.
- Each bundle is a measured upper bound for any rung that lies inside it. Every split inside a bundle is an estimate.

**Noise.** Two timing passes in B differ by up to 0.006 ns (timer 0.057 / 0.051). A rung expected to save less than about 0.01 is gated on a SASS diff, not on two timing passes.

**Table C** is used only to rank shapes: a start held in a register is far cheaper than one held in local memory, and inline costs about the same as passing it as a parameter. C under-predicts the real kernel: c_empty at the mmvq shape costs +0.003, the real empty2 costs 0.018. No C amount is carried over as a real saving.

**Instruction model (estimated).**
- Today the probe path runs about 208 dynamic SASS instructions per thread. This is my count from probe-sass-excerpt.txt plus the kernel call sites at 0170-01d0 and 1aa0-1af0, for a thread with a start and non-NULL lookups:
  - kernel side 13
  - entry probe 31, timer helper 3
  - exit probe 78, timer helper 3
  - helper 1 path 56
  - after the helper 24
- Implied cost per instruction:
  - lookup+add: 0.135 / ~75 instructions = about 0.0018
  - carry+log2: 0.105 / ~80 = about 0.0013
  - call floor: 0.018 / ~25 = about 0.0007
  - The GPU's issue limit is about 0.00096: 8 SMs x 4 schedulers x 1 warp instruction per clock x 1.02 GHz, over 32 threads.
- I use 0.0013-0.0018 per instruction for rungs that remove memory, call or indirect-branch work, and 0.0007-0.0013 for ALU-only rungs. Each estimate is capped by its bundle.
- Side estimate: mmvq's own 0.41 ns per thread is about 430 issue slots, which suggests it is near issue-bound. That would explain why probe instructions are not hidden. Not measured.

**Converting to tg128.** Loss = 1 - 1.883/(1.883 + 3.137e9·x). This reproduces B's lat (0.284 gives -32%). Reference points:

| x (ns per thread) | tg128 loss |
|---:|---:|
| 0.20 | -25% |
| 0.16 | -21% |
| 0.12 | -17% |
| 0.10 | -14% |
| 0.08 | -12% |
| 0.06 | -9% |
| 0.04 | -6% |
| 0.031 | -5% |

**The budget versus the call floor.** The call floor (0.018, measured) is 58% of the budget. Any end state that keeps the two probe CALLs has about 0.013 left for timer, carry, bucket and add together, which is roughly 10-18 instructions by the model. The ideal shape alone (two timer reads, clz, shard address, one red) is already about 20 instructions.

## Rungs, ordered by saving per line of code

Ratios use the central estimates. Rungs 1b and 3a are tiny or uncertain and are SASS-gated. Rung 7 has the second-best ratio but needs a tool change, so it is listed last.

---

### Rung 0. Routing for every probed module (enabler, F2)

- **Removes:** nothing for lat, which already has a 1530 map and so already goes through routing.
- **Where:** nv_attach_impl.cpp.
  - route_thread_local_maps_of_module drops the 1530-only filter (:997) and the early return (:1007-1008).
  - It passes 1531 descriptors with type, base, shards, value_size and max_entries.
  - The kernel area rewrite runs only when a 1530 map exists.
  - About 20-30 lines.
- **Basis:** needs nothing new.
- **Saving:** 0 on lat. It is needed before rungs 2, 4 and 5 help tools without a 1530 map, such as count.
- **Risks:** modules that load probed today without routing could hit routing's fail() and load unprobed. Rule for every new rewrite: on an unknown shape, keep the original callseq; never throw.
- **Validate:** count and pt_hist load probed (route-stats present), totals are exact, nv_attach tests pass.

---

### Rung 1. Inline the timer (R2a, PTX)

- **Removes:**
  - 1fd0 MOV R8 and 1fe0 CALL.REL.NOINC 0x5170 (entry)
  - 2230 MOV R8 and 2240 CALL (exit)
  - 5170 IMAD.MOV R9 and 5190 RET, executed twice
  - In total 8 dynamic instructions and two call/return pairs. The CS2R SR_GLOBALTIMERLO stays, now inline.
- **Where:** route_function_body (nv_thread_local_routing.cpp:293-382).
  - New branch before the `id<1||id>3` filter (:325).
  - A `_bpf_helper_ext_0502` callseq becomes `mov.u64 <retval>, %globaltimer;`, using the existing retval regex (:301-302).
  - A callseq whose result is unused is deleted.
  - About 15 lines. Ratio about 0.001 ns per line.
- **Basis:** needs nothing new.
  - It is the same register the helper reads (default_trampoline.cu:588-592; P92:42056/42073), with one 64-bit read (facts_docs T2).
  - No documented difference between a call and inline code (Q5(c)).
  - Where the read lands relative to other instructions is not specified today either (Q4(c)).
- **Saving (estimated):** 0.006-0.025.
  - Low end: 8 instructions at the model rates.
  - High end: the timer bundle, 0.035 measured, which also holds two CS2R reads and compares that stay.
  - Cumulative: 0.259-0.278.
- **Risks:**
  - An unknown callseq shape keeps the call.
  - The helper stays defined for unrouted code and for test_cuda_host_call.
  - ptxas may move the CS2R by a few instructions. That changes the duration slightly, with no correctness effect.
- **Validate:**
  - Patched mmvq SASS has no CALL to the CS2R stub and has two inline CS2R.
  - The lat sum map holds exactly 15,711,207,424.
  - check/run_test_set.sh passes.
  - tg128 PT_SET_MMVQ, 2 passes, interleaved with the previous build.
  - Keep if the saving is >= 0.005 in both passes. Otherwise keep on SASS grounds only if neither pass is worse than -0.005.

---

### Rung 1b. Mark the timer helper memory-free (llvmbpf overlay; only if the reload survives rung 1)

- **Removes:** the reload of *s after the timer: 2250 ULDC.64, 2260 LD.E R9, 2270 LD.E R11. LLVM re-reads *s because helper declarations carry no memory attributes (compiler.cpp:263-277).
- **Where:**
  - A new vm/compat/llvm-vm patch at the helper declarations (compiler.cpp:270-276, where `is_gpu` is in scope).
  - On the GPU, `_bpf_helper_ext_0502` gets inaccessiblememonly, nounwind and willreturn.
  - About 5-10 lines, plus registering the patch in vm/compat/llvm-vm/CMakeLists.txt:16-73.
- **Basis:** a fact about bpftime's own helper body, true by construction. It must NOT be readnone, because readnone would let two reads in one program be merged.
- **Saving (estimated):** 0-0.006, which is 3 instructions including 2 generic local loads. SASS gate only. Cumulative: about 0.253-0.278.
- **Risks:** overlay maintenance. The CPU path is untouched.
- **Validate:** the exit-probe PTX has one load pair of *s and the SASS diff shows the deletion. No timing decision.

---

### Rung 2. Inline the sum-map (1531) lookup for a constant fd (R1a)

- **Removes:**
  - 2510 CALL.REL.NOINC 0x2800, its setup (24d0, 24e0, 24f0) and the result moves (2530, 2540).
  - The whole helper-1 path for type 1531, 56 dynamic instructions:
    - 2800-28a0: YIELD, BSSY B9, the map_info index (HFMA2.MMA, IMAD), LDC map_type
    - the type tests: ISETP/BRA at 2890-28a0 and 3a20-3a30
    - the jump table 40f0-4140, ending in BRX
    - 4f20-4f70: LDC.64 extra_buffer and its NULL test
    - 4f80-5130: the four 64-bit compares, the IMAD/IMAD.WIDE/SHF chain, a second LDC.64 extra_buffer and LDC value_size
    - 5140-5160: BSYNC, RET
  - In total 7 LDC, 1 BRX and 1 CALL/RET.
  - Added: about 11 instructions (ISETP bound, S2UR SR_VIRTUALSMID, S2R SR_LANEID, LOP3 x2, IMAD.WIDE/SHF, IADD3 x2, SEL x2). Net about -51.
  - Stays: the STL key, the key's LD.E (moved from 4f90 into the inline code), the NULL test at 2520-2570, and the atomic.
- **Where:** route_function_body, a branch beside the 1530 one (:333-361), for id==1 and a constant fd of type 1531, in routed or unrouted functions.
  - Emits: `ld.u32 k,[p1]; setp.lt.u32 p,k,ME; mov %smid; and MASK; shl 5; or %laneid; mad.wide k*SHARDS+shard; scale by value_size; add base; selp rv,a,0,p`.
  - Constants: immediates from nv_attach_impl::map_basic_info (create_map_basic_info runs at attach, before any module is patched).
  - extra_buffer==0 or shards<32 emits `mov rv,0`, which is today's NULL.
  - Alternative: ld.const from map_info, which keeps the PTX process-independent and costs 1-3 more instructions.
  - Plumbing: rung 0's descriptors and thread_local_map_desc (nv_thread_local_routing.hpp:30-35).
  - About 80-120 + 20 lines. Ratio about 0.0005 ns per line.
- **Basis:** needs nothing new.
  - It is the same arithmetic as sum_array_offset (default_trampoline.cu:264-275).
  - %smid may change and is not dense (P92:41436, 41462). It is masked, and any shard is correct: only atomics write and the delivery sums all shards (trampoline :259-263).
  - %laneid is 0..31 (P92:41248).
  - The key bound check is kept.
- **Saving (estimated):** 0.05-0.08 (51 instructions at 0.001-0.0016; LDC and ALU-heavy). Capped by lookup+add, 0.135 measured. Cumulative: about 0.17-0.23.
- **Risks:**
  - Late path: hack_fatbin also runs from nv_attach_fatbin_record.cpp:219. The immediates equal what that path writes into map_info (:263-291), and ld.const reads the module's own map_info, so both are consistent.
  - The base is baked into the PTX. That is fine: ptx_pool is per process, and several GPUs are not covered today anyway.
  - A map freed while a kernel still runs behaves as today.
  - If the fd is not constant per constant_fd() (:119-157), the helper call is kept.
- **Validate:**
  - SASS has no CALL to helper 1 on the exit path, and a new route-stats counter shows 1 direct sum lookup for mmvq.
  - lat total is exact. The count tool (with rung 0) total is exact. check_test_set passes.
  - tg128, 2 passes. Keep if the saving is >= 0.02 in both. If it is below that, stop and re-derive the model before rung 3.

---

### Rung 3. The key without the generic round trip (R3, PTX)

**3a, form only.**
- When p1 resolves to %SP+imm (through `add.s64 X,Y,imm`, `add.u64 Y,%SP,0` and `cvta.local.u64 %SP,%SPL`), the key load becomes `ld.local.u32 k,[%SPL+imm]`.
- **SASS:** three generic key LD.E become LDL (1f10, 2110, and rung 2's load). The generic-address pairs 1ef0/1f00, 20f0/2100 and 24c0/2500 go.
- **Size:** about 40 lines.
- **Basis:** this rung improves the basis.
  - Today LLVM writes the key with st.local, and routing or helper 1 reads it through a generic address. That mixed form's alias status is not stated (facts_docs Q7(c), T5 condition 2).
  - 3a uses one form for both the store and the load. Address identity follows from the window rule (P92:5476).
- **Saving (estimated):** 0-0.008. SASS gate.

**3b, forwarding.**
- If the slot's defining `st.local.u32 [%SPL+imm],V` is in the same basic block, before the callseq, with no st, atom, red, call or label in between, use `mov.u32 k,V`.
- For a constant V below max_entries, emit the address with no selp.
- Then, if %SP and %SPL have no uses left except st.local addresses, delete those stores.
- **Removes:** STL 1ee0, 20e0 and 24b0; the 3 key loads; the %SP address math (1ed0, 20c0, 24c0/2500); and for the two constant 1530 keys IMAD.WIDE, ISETP and SEL x2 (1f60-1f90, 2150-2190). About -22, including 6 local-memory operations.
- **Size:** about 40 more lines. Ratio about 0.0004 ns per line for 3a and 3b together.
- **Basis:** needs nothing new. It rests on single-thread program order within one basic block, and .local is private to the thread (P92:3023), so a store nobody reads can be dropped.
- **Saving (estimated):** 0.02-0.04, capped by the carry+log2 and lookup+add bundles. Cumulative: about 0.13-0.21.

**Risks.**
- A missed write gives a wrong key and a wrong bucket, and exact totals would NOT detect it.
- pt_carry passes stack pointers to helpers 503-505, so its stack stores must not be deleted. Forwarding is still allowed there.

**Validate.**
- SASS: no STL in lat's probes.
- route-stats: 3 forwarded keys, 3 deleted stores.
- A new deterministic victim test: a computed key (for example tid % 7) into a 1531 map with exactly known per-key totals. pt_carry and pt_tlatomic pass.
- tg128, 2 passes. Keep 3b if the saving is >= 0.01 in both. Keep 3a on basis grounds alone.

---

### Rung 4. Probe switch read once per kernel (R9)

- **Removes:** per probe, 1e50/1e60 and 2040/2050 (switch address), 1e80 and 2080 LDG.E, 1eb0 and 20a0 ISETP, 1ec0 and 20b0 @!P0 RET: 10 instructions including 2 dependent LDGs. Adds about 4 instructions (one LDG) in the kernel prologue, plus predicates on the call sites. Net -6 and one LDG.
- **Where:** the routing's kernel rewrite (.cpp:403-484, prologue :464-471, label and predicate parsing :438-446).
  - The prologue does `ld.global.u32` of the switch and `setp.ne`.
  - Calls become `@pon call`, or `and.pred q,p,pon; @q call` for a predicated site.
  - The gate block (exact text from core.cpp:200-209) is deleted only from functions called solely by rewritten kernels. Stub-called functions, memcapture and bpf_main keep theirs.
  - About 40-60 lines.
- **Basis:** needs nothing new. It is the same plain load of the same switch. The CLAUDE.md contract still holds: kernels launched after the write run unprobed, and running kernels finish with their probes. It holds more strictly, since a thread's entry and exit now see one value.
- **Saving (estimated):** 0.003-0.012. The gate's share of the 0.018 floor is unmeasured. Cumulative: about 0.12-0.21.
- **Risks:**
  - test_load_substitution.cpp:66-91 checks the compile-time gate, which is still inserted and only removed in routing, so it still passes.
  - The CLAUDE.md text must change.
  - One more live predicate in mul_mat_q (255 regs). Predicates are a separate register file; the P2R spill risk is small.
- **Validate:**
  - First, empty2 against empty2 built without gate_probe_function, PT_SET_MMVQ, 2 passes, to measure the gate's share of the floor.
  - Then a SASS diff, detach-switch.sh (events stop at the detach: 5 then 6), and the [load-substitution] and [late-path] tests.
  - If the gate share is under 0.005, accept on SASS and contract grounds without timing.

---

### Rung 5. Sum-map add as red.global, without the isspacep guard (R8)

- **Removes:** 2580 QSPC.E.L, 2590 BSSY, 25a0 @P1 BRA, the generic ATOM's P0 result and 25f0 @P0 BRA, 2700-2750 (BSYNC, BSSY, @!P1 BRA, BSYNC), and the result path 2760-27b0. ATOM.E.ADD.64 becomes RED.E.ADD.64. About -14.
- **Where:** guard_generic_atomics (.cpp:159-291).
  - Applies only when the address register is the selp output of a rung-2 lookup: a single definition emitted by routing itself, optionally plus an immediate below value_size.
  - Emits `cvta.to.global.u64; red.global.add.u64` when the destination is never read, otherwise `atom.global`.
  - About 30-50 lines.
- **Basis:** the same premise as today's unguarded `@!l atom` branch.
  - The 1531 buffer is cuMemAlloc'd device memory (nv_gpu_model_maps.cpp:741) in the .global window.
  - atom and red are allowed on .global (P92:23363/23623), and cvta.to.global is defined inside the window (P92:18780).
  - The default .relaxed/.gpu is unchanged (P92:23369/23371, 23629/23631).
  - A quote that driver-allocated memory lies in PTX's .global window is NOT in facts_docs.txt. Today's path needs it too.
- **Saving (estimated):** 0-0.01. Measured hint: count has no 1530 map (tools/pt_count.bpf.c), so routing and its guard did not run for it. The parts, with an unguarded count, sum to 0.293 against the guarded lat at 0.284 (table B), so the guard costs no more than about the noise. That is why this rung is ranked last of the small set. Cumulative: about 0.11-0.21.
- **Risks:** a wrong provenance claim would put red.global on a .local address, which is undefined (P92:23363, 18780). Hence routing-emitted provenance only. pt_tlatomic's atomics on 1530 values keep the guard.
- **Validate:** SASS shows RED with no QSPC on the exit path, totals are exact, pt_tlatomic passes. SASS gate only. Drop the rung if the SASS gain is only the guard and a 4-pass timing shows nothing.

**Endpoint of the small transparent set (rungs 1-5, about 300-400 lines):** estimated 0.12-0.20, central about 0.16, which is about -17% to -25% on tg128 (central -21%). Bottom-up check: about 107 dynamic instructions remain, including the start's 5 local-memory accesses and the 33-instruction log2. At 0.0011-0.0016 each that is 0.12-0.17.

---

### Rung 6. Carry the start in registers (R7b at PTX level; transparent but large)

- **Removes:**
  - Kernel: 0190, 01a0, 01b0 (the generic ST.E.64 that zeroes the area) and 1aa0/1ad0 (the cvta at each call site).
  - Entry: 1fa0-1fc0 (NULL test), 2000 and 2020 (the two ST.E of the start), and the area-pointer moves at 1f20-1f50.
  - Exit: 21a0-21c0 (NULL test), 21d0-21f0 (ULDC and the two LD.E of the start), and 2120/2140.
  - About -20 with +5 moves, net about -15. These are all the remaining local-memory accesses. With rung 3b, lat's probes touch no local memory at all.
- **Where:** routing.
  - The kernel gets `.reg .b64 %bptw0`, set to 0 in the prologue instead of the area.
  - The entry returns the words; with more than one word, a .param byte array (single return value, facts_code R7b / ISA 7.1.1). The exit receives them as arguments.
  - Inside the probe: `ld.param` into registers. Every use of a constant-key 1530 lookup result is rewritten: ld/st `[p+c]` becomes mov or `mov.b64 {lo,hi}`, and `setp.eq p,0` becomes false. The words are written back before every ret, including the gate's ret if rung 4 is absent.
  - Per map, fall back to the area if the pointer is copied, moved by a non-constant offset, passed to a callseq, stored, or used in an atom or red (pt_tlatomic). Also fall back for areas larger than a few words and in unrouted functions.
  - About 200-300 lines. Ratio about 0.0001 ns per line.
- **Basis:** needs nothing new, and it is better documented than today.
  - Arguments and return values are pass-by-value (P92:22755, 6696), and register state is per thread (P92:1545).
  - Registers versus stack is unspecified (P92:43032, 6787), which only affects performance.
  - It also removes today's generic 8-byte store to the area (01b0), whose alignment as a generic address is not stated (T5 condition 1).
- **Saving (estimated):** 0.015-0.03, capped by what rung 3 leaves of carry+log2 (0.105 measured). C ranks a register-held start far below a local-memory start on every shape, but C's amounts are not carried over. Cumulative: about 0.09-0.18 (central about 0.14).
- **Risks:**
  - An incomplete use analysis gives silent wrong values. This is the ladder's largest correctness risk.
  - mul_mat_q (255 regs, 60 B of spills): two more registers live across the kernel, so ptxas may spill them. Performance only.
  - Late path: the rung needs kprobe and kretprobe on one patched kernel, which is a known late-path problem (#708) that lat has today. Unchanged.
- **Validate:**
  - SASS: no ST.E, LD.E, STL or LDL in lat's probes, and no ST.E.64 in the prologue.
  - pt_carry: 0 mismatches.
  - pt_tlatomic falls back (route-stats shows 0 carried words) and passes.
  - A new escape test (the value pointer passed to a helper, or stored) falls back.
  - ptxas_check: mul_mat_q's registers and spills against rung 5. pp512 with all four kernels must not be worse by more than 1 point.
  - tg128, 2 passes. Keep if the saving is >= 0.01 in both.

---

### Rung 7 (tool change, optional). clz instead of the log2 chain (R5)

- **Removes:** 2290-2490 except the subtraction (the 32-instruction ISETP/SEL/SHF/LOP3 chain plus IADD3 0x82). It becomes about 5 instructions (LOP3 |1, FLO.U32 x2, ISETP/SEL, IADD3). Net about -27 ALU.
- **Where:**
  - A trampoline helper 512 computing 63-clz(v|1), registered in core.cpp:234-253, with trampoline_ptx.h regenerated.
  - It is inlined at its callseq like rung 1 (`or.b64; clz.b64; sub; cvt`).
  - The tools replace log2_u64 (pt_common.h:21-31) with the helper.
  - About 35 lines plus the tool edit. Ratio about 0.0008 ns per line.
- **Basis:** clz.b64 exists (facts_code R5, PTX ISA 9.7.1.16; not quoted in facts_docs.txt). Because v|1 is never 0, clz(0) is never used, and 63-clz(v|1) equals log2_u64(v) for every v including 0 and 1. Needs nothing new.
- **Saving (estimated):** 0.019-0.035. How the 0.105 bundle splits between carry and log2 is unmeasured. Cumulative: about 0.06-0.15 by summing the rungs. Bottom-up gives 0.05-0.08 (next section).
- **Validate:**
  - First, measure the log2 share: carryonly against a variant whose exit compares the raw difference with a never-reached value instead of computing log2, 2 passes. Do the rung only if the share is >= 0.015.
  - Then check helper output against log2_u64 in a GPU test kernel on the edge inputs (0, 1, 2^k-1, 2^k, 2^k+1 for k 0..63) plus random inputs.
  - lat totals exact; tg128, 2 passes.

## Checkpoint, fixed in advance

After rungs 1-3:
1. Compute the measured saving per removed dynamic instruction.
2. Re-project the endpoint of "all transparent rungs + rung 6 (+7)".
3. Decide by the projection:
   - <= 0.031: finish the ladder.
   - 0.031-0.06: add rung 7.
   - > 0.06: stop adding PTX-text rungs after rung 5. Plan the IR-level route (F1) together with an inlining study instead, because rungs 3b and 6 are what IR lowering does more robustly.

Gates that apply to every rung:
- The SASS diff shows exactly the predicted deletions before anything is timed.
- A golden-PTX unit test per rewrite, including "unknown shape keeps the callseq". There are no in-tree routing tests today (facts_code section 4).
- nv_attach tests pass. lat on llama gives exactly 15,711,207,424.
- ptxas_check on the four kernels. If mul_mat_q's spills rise, pp512 must not lose more than 1 point.

## Expected endpoint against the 0.031 budget

| After | Estimated ns per thread | tg128 loss |
|---|---|---|
| Rungs 1-5 (small, transparent) | 0.12-0.20 | -17% to -25% |
| + rung 6 | 0.09-0.18 | about -13% to -24% |
| + rung 7 (tool change) | about 0.06-0.12, most likely 0.07-0.09 | about -10% to -13% |

Bottom-up for the full ladder: about 65 dynamic instructions remain:
- calls and their moves: about 16
- kernel gate: 4
- entry: about 5
- exit: about 40 (start test 4, CS2R, 64-bit subtract 2, clz 5, key 1, bound 1, smid/lane 2, shard 3, address 4, selp 2, NULL test 3, operand 2, RED, RET)

At 0.0007-0.0013 each that is 0.05-0.08.

**The budget is not reached.**
- With the two CALLs kept, the floor (0.018 measured, minus the unmeasured gate share) leaves 0.013-0.016 for about 45 non-call instructions. That needs <= 0.0003 ns per instruction, a third of the issue limit. B shows the real mmvq exposing 0.0007-0.0018 per instruction.
- Reaching the budget needs two more things:
  - Inlining the probes into the kernel (R6), which removes the call floor.
  - Removing the eBPF-shape overhead (two NULL tests, selp, key bound, start test and moves, about 15 instructions). One way is lowering constant-fd lookups in LLVM IR (F1), so LLVM folds them using known non-NULL bases and the clz range.
- The remaining 20-25 instructions would cost about 0.015-0.03 by the model, which is borderline at best. That is a different design round from this ladder.

## What I would NOT do, and why

- **R4(a), the NVPTX DataLayout one-liner.** It is the smallest change, but it makes every i64 access claim 8-byte alignment. Misalignment is undefined (P92:5451), and eBPF programs can access a u64 at a 4-aligned address: packed data, 1530 entries with value_size%8 != 0, and host-call lookup pointers whose alignment is not stated.
- **R4(c), merging 32-bit pairs into 64-bit generic accesses on the area.** The window-base alignment of the generic address is not stated (T5). Rung 6 subsumes it.
- **R10, warp combine through activemask/match.** Not documented: agreement among the lanes named in one activemask result is not stated (facts_docs Q2), and step0b C2 measured disagreement (39,143 and 116,089 of about 4.46M calls). The measured gain over (smid,lane) shards is also small (C: r_warp against r_smlane).
- **%clock64 instead of %globaltimer.** It counts cycles on a per-SM counter, and %smid may change after preemption (T6). Not valid as nanoseconds.
- **call.uni, to drop the BSSY/BSYNC around probe calls.** facts_docs Q5(c) marks the basis as a derivation, not a quote. The gain is about 2 instructions per site.
- **A transparent log2-idiom recognizer at IR level.** Fragile, and each idiom needs its own equivalence proof. Rung 7 is explicit instead.
- **Trampoline-only tweaks to helper 1's dispatch** (for example reordering the type tests to avoid the BRX). The CALL and the LDCs stay, and rung 2 subsumes it.
- **R6 inside this ladder.** It is 300-600 lines. Label scope across nested blocks and register shadowing are not stated (renaming per copy avoids relying on them), and it adds register pressure on mul_mat_q. It is the next step if the checkpoint says so, not a small rung.
- **.abi_preserve.** It needs a module .version of 9.0 (the trampoline sets 8.1), and today's SASS shows no save or restore around the probe calls, so no gain is expected.

## Open questions

1. What share of the 0.018 call floor is the gate? Experiment: empty2 against empty2 without the gate.
2. How does the 0.105 carry+log2 bundle split between carry and log2? Experiment: carryonly against carryonly without log2.
3. After rung 1, does ptxas already drop the *s reload, which would make rung 1b unnecessary? Does it forward STL to LDL after rung 3a, or drop dead depot stores?
4. How much of each probe instruction does mmvq expose? Use ncu instruction counts and issue utilization to test the near-issue-bound estimate (0.41 ns is about 430 issue slots per thread). This decides the endpoint more than any rung.
5. A documentation quote that cuMemAlloc'd device memory lies in PTX's .global window. Today's atom path and rung 5 both assume it; it is not in facts_docs.txt.
6. Today's kernel prologue zeroes the area with a generic 8-byte `st.u64` (SASS 01b0), relying on an 8-aligned generic address, which is not stated (T5). The routing's local-path fallback for 64-bit atomics on 1530 values does the same.
   - Cheap fix within rule 1: zero the area with `st.local.u64 [__bpftime_tl+off]`. The .align 16 of a .local variable is documented (P92:4074). This is 2 lines.
   - Alternatively, rung 6 removes the area for carried maps.
7. Does the generic ATOM's P0 output make the thread wait for the atomic's round trip? This decides whether rung 5 is worth anything.
8. After the ladder, is the IR-level route (F1) plus probe inlining acceptable as the next round? By this analysis it is the only path toward 0.031.