DESIGN LADDER: per-thread probe cost, robustness and generality angle (read-only; nothing was built or run)

CONVENTIONS
- M(A), M(B), M(C) mean measured, from measurements.md sections A, B and C.
  - B: the real mul_mat_vec_q, tg128, PT_SET_MMVQ, passes a1/a2.
  - C: the probe_shape benchmark.
  - M(README) means measured, from results/gpu/map-model-proto-20261116/README.md.
- E means estimated; the reasoning is given with each number.
- The a1/a2 spread in B is up to 0.006 ns per item (timer 0.057 vs 0.051). Every speed threshold below is therefore at least 0.004-0.006 ns and must hold in both passes.
- Assumption used only as a scale, not taken from the inputs: about 4 warp instructions per SM per cycle. That gives 8 SMs x 128 lanes x 1.02 GHz ≈ 1.0e12 thread-instructions per second, so about 0.001 ns per thread for each fully exposed SASS instruction, and the 0.031 ns budget ≈ 32 exposed issue slots.
  - This is NOT an upper bound. In B, two helper calls of about three instructions each cost 0.034-0.038 ns (timer minus empty2).
  - mul_mat_vec_q also hides less than the synthetic bodies: real empty2 costs 0.018 M(B), while the synthetic c_empty costs 0.000-0.003 M(C).
- Lines I read myself. None of these is in facts_docs.txt.
  - /home/jetson/src/bpftime-gpu-work/step0b/c5-guarantees/docs/v13.2.2/text/ptx-isa.txt:5476, full paragraph: "A generic address maps to global memory unless it falls within the window for const, local, or shared memory."
  - same file :7033-7045: "Memory operations as defined in Operation types use generic method of memory access, i.e. a generic proxy". Morally strong requires "Both operations are performed via the same proxy."
  - .../v13.2.2/text/cpg-writing-cuda-kernels.txt:246: "Global memory is allocated with CUDA API calls such as cudaMalloc and cudaMallocManaged."
  - .../v13.2.2/text/drv-mem.txt:1513 (cuMemAlloc): "Allocates bytesize bytes of linear memory on the device ... The allocated memory is suitably aligned for any kind of variable."
  - That cuMemAlloc memory lies in the global window is my INFERENCE from these lines; it is not stated in one sentence.
  - /usr/lib/llvm-15/include/llvm/IR/IntrinsicsNVPTX.h:470-531 has the sreg intrinsics ctaid/nctaid/ntid/tid x/y/z, smid, laneid and warpid. It has no globaltimer intrinsic.

CROSS-CUTTING PRINCIPLES (they are what makes the gains carry over to other tools and kernels)
- P1. Every rung is an optimisation that may decline.
  - On any shape it does not recognise, it leaves today's code untouched: the helper callseq, the .local area, the in-function gate.
  - No new matcher may call fail(). A throw in routing loads the whole fatbin unprobed (facts_code section 2).
  - The failure tiers become: new path, then today's path for this module, then unprobed.
- P2. Program-side work goes in llvmbpf at the LLVM IR level. Kernel-side work (call sites, prologue, gate) stays in the PTX routing, because kernel PTX comes from nvcc and has no IR.
  - The lowering runs as the CPU array-inline precedent does (gen-ctx:722-726, 754-758): optimize, lower, optimize again.
  - The order matters. In the fresh module eBPF registers are allocas (compiler.cpp:376-380), so the constant fd and the uses of a returned pointer are visible only after the first optimisation.
- P3. Where 1530 values live (registers or the .local area) is decided once per kernel, before any program for that kernel is compiled, and passed in every request for that kernel.
  - Probe functions are per kernel, so entry and exit cannot disagree.
  - Register mode applies only when every call site of the kernel's probe functions is a kernel entry/exit site.
  - Stub-called functions, __memcapture__ and bpf_main keep today's form (area 0, so a 1530 lookup returns NULL).
- P4. The main path uses no sm_70+ instruction (no activemask, match, redux or elect). It uses only:
  - mov %globaltimer, clz.b64, generic atom.add.u64;
  - .param byte-array return values and predicated calls;
  - all valid at the module's .version 8.1 with LLVM and the trampoline at sm_61 and .target rewritten to the device.
- P5. The late path calls hack_fatbin too (nv_attach_fatbin_record.cpp:219), so every rung applies there.
  - Map constants that can differ per module or process (the base address, whether a device copy exists) are read from map_info at run time, not baked into the PTX.
- P6. For the experiments, put each rung behind an on/off switch in one build. The README notes that build-to-build differences (review fixes, 1.2-1.4 points) were never attributed.

THE LADDER (starting cost: lat 0.282-0.286 ns per thread, M(B))

Rung 0. Plumbing and soundness (no speed by itself)
- Removes: nothing in SASS.
- Where and how:
  - Routing runs for every probed kernel: drop the 1530-only filter and early return (nv_attach_impl.cpp:997, 1007-1008).
  - Descriptors for all GPU map types go to the routing and, as a new request field, to every pass. It joins the patch-cache key by itself (nv_attach_impl.cpp:1073).
    - Fields: fd, type, key_size, value_size, max_entries, shards, area offset, and the per-kernel 1530 mode from P3.
  - An overlay hook in llvmbpf in GPU mode, between generateModule and optimizeModule (gen-ctx:1160-1176), shaped as in P2.
  - Fix constant_fd (routing .cpp:119-157). It must count every instruction whose destination is the register. Today a register defined by mov and then by xor/mad/neg/cvta is taken as a constant fd, which applies the wrong map's layout and can address local memory outside the area.
  - The failure tiers from P1.
  - Size: about 60 lines of plumbing, 10 for constant_fd, 40 for the hook.
- Basis: needs nothing new. The constant_fd fix closes an existing gap against rule 1.
- Saving: 0. Cumulative 0.282-0.286, M(B).
- Fallback: today's path for the module.
- Risks:
  - The routing parser now sees every probed module, so P1 matters more.
  - Threadhist (1532 only) and count-only tools reach rungs 1 and 3 only through this rung.
- Validation:
  - bpftime_nv_attach_tests; kmr tl_route_check (8 sites); pt_victim test set exact; pp512 with kept PTX.
  - New unit test: a register defined by mov and then xor is not treated as constant.

Rung 1. Gate once per thread, in the kernel
- Removes: in each probe, the switch-address MOVs, LDG.E, ISETP and @!P0 RET (entry 0x1e50-0x1ec0, exit 0x2040-0x20b0), so 2 LDG and about 8 instructions per thread.
- Adds: one LDG and ISETP in the kernel prologue, and a predicate on each probe call site.
- Where and how: PTX, in the routing's kernel rewrite (.cpp:403-484).
  - Load __bpftime_probes_enabled once.
  - Guard each call with and.pred on its existing predicate, or with @!on bra.
  - Remove the in-function gate only from functions that only rewritten kernels call.
  - Size: 40-60 lines.
- Basis: needs nothing new. It is the same ld.global under the same contract: kernels launched after the write run unprobed, and running kernels finish with their probes. A thread's entry and exit now always agree; today a thread can see 1 at entry and 0 at exit.
- Saving: E 0 to 0.009.
  - The 0.018 floor, M(B), holds 2 gates and 2 calls and has never been split.
  - In the real kernel one helper call costs about 0.017 (timer item), so the calls may be the whole floor.
  - Cumulative E 0.273-0.286.
- Fallback: the in-function gate, as today.
- Risks:
  - One predicate is live across the kernel. Under register pressure (mul_mat_q at 255 registers), re-reading the switch at each exit site is equally correct, because the switch only goes from 1 to 0.
  - Rung 4 depends on this rung. A register-carry probe must not keep the PTX gate's bare "ret", or its return array would be left unwritten.
  - test_load_substitution's gate test checks the compiled function before routing and still passes. Add a test on the routed module.
- Validation (it also splits the floor): four tg128 PT_SET_MMVQ items.
  - (a) empty2 as today; (b) empty2 with no gate; (c) gate hoisted, switch on; (d) gate hoisted, switch off.
  - Gate = a-b; calls = b-base; hoisted load = d-base.
  - Keep the rung if all of these pass and (c) ≤ (a) + 0.002 in both passes:
    - attach tests;
    - pt_victim exact;
    - detach-switch.sh shows events stopping at the detach.
  - Keeping it is justified even at zero saving, because entry and exit now agree.

Rung 2. Timer and index helpers inline, at IR level
- Removes:
  - CALL 0x5170 twice (0x1fe0, 0x2240), with its moves and the helper's CS2R/RET.
  - Possibly the reload of the start after the timer call (LD.E at 0x2260-0x2270).
  - For pt_carry: the 3 helper CALLs per lin_tid() (503/504/505) and the stack slots they write and the probe reads back.
- Where and how: llvmbpf overlay.
  - 502 becomes inline asm "mov.u64 $0, %globaltimer;", sideeffect, marked inaccessiblememonly and nounwind.
  - 509/510/511 become the nvvm sreg intrinsics.
  - 503/504/505/508 become three u64 stores of the ctaid/ntid/tid/nctaid intrinsics through the given pointers, returning 0.
  - 501, 506, 507 and every host-call helper stay as calls.
  - Size: 80-120 lines.
- Basis: needs nothing new.
  - The same special-register reads, and one 64-bit mov of %globaltimer (facts_docs T2).
  - No documented difference between inline and call (Q5c).
  - The stores keep the trampoline's own width and its 8-byte alignment claim ("*(uint64_t*)x = ...").
- Saving: E 0.030-0.037.
  - The timer item is 0.034-0.038 above empty2, M(B).
  - Inline reads cost about 0 in the synthetic shape (r_carry, M(C)).
  - Cumulative E 0.24-0.26.
- Fallback: the helper call.
- Risks:
  - GUESS: that GVN forwards the start load across the asm read. If it does not, the reload stays (performance only).
  - ptxas may place an inline timer read differently from a CALL; this is not stated. That moves bucket values, not counts. %globaltimer values are target-specific in any case (P92:42073), as with today's helper.
- Validation:
  - PTX/SASS: no call to helper 0502, at most one start load per probe.
  - pt_victim kspin (known spin lengths): the mode bucket per kernel equals the helper build's.
  - tg128: (timer - empty2) ≤ 0.010 in both passes.

Rung 3. Inline the sum-array and device-array lookups at IR level; drop the local guard by provenance
- Removes from the exit probe:
  - the STL of the bucket key (0x24b0);
  - CALL 0x2800 and helper 1's map_type dispatch (LDC plus the ISETP/BRA chain, 0x2800-0x2920);
  - the 1531 tail's LDCs and its generic key load (0x4f90-0x5080);
  - the isspacep.local guard and its local path (QSPC.E.L at 0x2580, 0x2710-0x27f0).
- Leaves:
  - S2UR smid, S2R laneid, about 5 address instructions, and one bounds ISETP (gone when LLVM proves the key range);
  - one generic ATOM.E.ADD.64 with ptxas's cold-path fallback for the shared window.
- For 1532 (threadhist), the thread index is computed inline instead of through the helper's call path.
- Where and how: llvmbpf overlay with the rung-0 descriptors, applied to helper-1 calls whose fd is an LLVM constant of type 1531 or 1532.
  - The key is loaded from the key pointer; LLVM forwards it from the store.
  - The base is read from map_info[fd].extra_buffer with ld.const. This keeps NULL when another process owns the map and keeps the late path's per-module values.
  - The lookup returns NULL unless key < max_entries and base != 0.
  - 1531 index: key*shards + ((smid & (slots-1))<<5 | (laneid & 31)).
  - 1532 index: key*max_thread_count + the thread index in u64. An out-of-range thread takes the original helper-1 call on the cold path, which warns once and returns NULL.
  - The atomic stays a generic atom.add.u64. llvmbpf already gives it align 8 and monotonic order (compiler_utils.cpp:372-388).
  - Lower only when every use of the returned pointer is a 64-bit atomic add (the 1531/1532 contract, unchecked today). Otherwise keep the call and log the contract breach.
  - The filter strips LLVM's ".extern .const map_info" declaration.
  - static_assert the MapBasicInfo offsets 24 and 32, which the IR hard-codes.
  - Size: 200-300 lines plus tests.
- Basis: needs nothing beyond today's premise.
  - The arithmetic is sum_array_offset and array_map_offset. Any shard is correct, because programs only add atomically and the delivery sums the shards.
  - Dropping the guard needs "device memory is not in the local window". This is my inference from the lines above (P92:5476, cpg-writing-cuda-kernels:246, drv-mem:1513). Today's add already requires the address to point to .global or .shared (P92:23363).
  - The alignment of the base is stated (drv-mem:1513), and value_size % 8 == 0 (nv_gpu_model_maps.cpp:83).
  - Keeping generic atom keeps the delivery's morally-strong argument unchanged: same addressing form as the snapshot's generic ld.relaxed.gpu (nv_gpu_model_maps.cpp:139, 179) and the same proxy (ptx-isa.txt:7035, 7045, read by me).
  - "& 31" puts the shard index in range by construction, whatever WARP_SZ is. P92:41248 gives laneid as 0..WARP_SZ-1, and today's code assumes 32. Masked %smid makes non-contiguous or changing values (P92:41436/41462) a speed matter only.
- Saving: E 0.08-0.13.
  - lookup+add is 0.135, M(B).
  - In the synthetic shape the (smid,lane) add costs +0.042-0.055 while the start is in local memory (c_smlane - c_carry, M(C)), but +0.001-0.018 with the start in a register (r_smlane - r_carry, M(C)). Until rung 4, the residual probably sits near the first figure.
  - Cumulative E 0.11-0.18.
- Fallback: the helper-1 call, for a dynamic fd, an escaping pointer, a contract breach or any other map type.
- Risks:
  - A tool that breaks the contract keeps today's (racy) behaviour; the log entry makes the breach visible.
  - Read a ptxas_check after this rung. Today ropek goes from 17 to 32 registers when patched (ptxas_check-final). If it stays at 32 once helper 1 is gone, the floor comes from any CALL. That would be the generality argument for rung 7 on small kernels on other GPUs.
- Optional 3b, OFF the main path, basis to be checked: .global atomics.
  - It removes only ptxas's cold-path predicate and branch after the generic ATOM. Saving E ≤ 0.003.
  - Morally strong needs the same proxy (ptx-isa.txt:7045).
  - Whether a cvta.to.global address and the generic address it came from are "distinct virtual addresses" (aliases, P92:6851/7037) is not stated.
  - So either both the programs' atomics and the snapshot's load move to .global together, or neither does.
  - Never use red in place of atom: P92:7511/7541 say they differ, and the delivery argument was made for atom.
- Validation:
  - PTX/SASS: no call to helper 1 in the exit probe, no QSPC.E.L.
  - The count item's sum is exactly 15,711,207,424.
  - pt_victim lat/hist/carry/tlatomic exact.
  - Unit tests: constant key, dynamic key, out-of-range key gives NULL, base 0 gives NULL, a plain store to a 1531 value is not lowered.
  - Decision on (count - empty2), both passes:
    - ≤ 0.02: done;
    - 0.02-0.06: examine the memory-carry interaction before rung 4;
    - > 0.06: the lowering did not take; stop.

Rung 4. Carry the start by value (registers instead of the 1530 area)
- Removes:
  - the prologue's area zeroing (0x0190-0x01b0) and the area address formed at each site (0x1aa0-0x1ad0);
  - at entry: the key round trip (STL [R1+0x204] / LD.E [R6.64+0x1fc]), the bounds/select sequence and the two 32-bit generic ST of the start;
  - at exit: the same key round trip and the two LD.E of the start;
  - the use of the 592-byte depot when nothing escapes.
- Where and how, IR side (about 150-250 lines):
  - In register mode a probe function takes "present" and W words by value and returns W words.
  - A constant-fd, constant-key 1530 lookup points into a function-local alloca. The alloca is filled from the words and written back at every return. GUESS: SROA promotes it when nothing escapes.
  - Atomics whose underlying object is that alloca become plain load/op/store. The memory is thread-private (P92:3023); today's routing guard does the same at run time.
  - A dynamic key, an escape to a helper, or a non-constant-fd 1530 call (which gets the alloca's address as r5) keeps the copy in function-local memory. That is correct, just slower.
  - present == 0 gives NULL, as for unrouted callers today.
- Where and how, PTX routing (about 100-150 lines):
  - Kernel ".reg .b64" words are set to 0 in the prologue; 0 means "no entry" to the tool.
  - Each call site passes the words and takes them back under the call's own predicate and the gate, so a skipped call leaves them unchanged.
  - Mode per kernel (P3): registers when the area is at most W_MAX words (E: 4) and the kernel's original register count, from the fatbin's resource usage, has at least 2W of headroom. Otherwise the memory area.
- Basis:
  - pass-by-value arguments and return values (P92:22755, 6696);
  - per-thread register state (P92:1545);
  - one return value, as a .param byte array (PTX 7.1.1 per facts_code);
  - predicated call (example in PTX 9.7.14.5).
  - Register versus stack placement is unspecified (P92:43032, 6787): speed only.
  - Preemption: as for any kernel register; not stated (Q9), the same status as today's .local area.
  - For the carried words this also removes today's 8-byte generic accesses into the local window, whose alignment is not stated (T5, Q7c).
- Saving: E 0.07-0.13.
  - carry+log2 is 0.105, M(B), and has never been split.
  - Synthetic, M(C):
    - memory vs register carry: 0.018 vs 0.000 (mem/mmvq) and 0.070 vs -0.001 (empty/mmvq);
    - with the add: c_smlane 0.060-0.073 vs c_param 0.000-0.003 (mem/mmvq).
  - Left after the rung, E 0.02-0.07:
    - floor 0.009-0.018;
    - log2 chain 0.005-0.03;
    - address and atomic 0.001-0.018;
    - parameter passing 0-0.007.
  - Cumulative E 0.02-0.07.
- Fallback: today's .local area, per kernel.
- Risks:
  - Each word keeps 2 registers live across the kernel.
    - mul_mat_vec_q (56 registers, 36 warps/SM) has room.
    - mul_mat_q is at 255 registers with 60 B of spills, so the rule puts it in memory mode.
    - A kernel with .maxnreg or launch bounds gets spills, not lower occupancy.
  - pp512 (lat 4.2%, M(A)) must not regress.
  - GUESS: the LLVM NVPTX aggregate return comes out as a .param array.
  - test_ptxpass "one stack frame" is unaffected, because that program passes a stack pointer to helper 1.
- Validation:
  - PTX: the expected signature and callseq.
  - SASS: no STL/LDL/LD/ST to the area or the depot on mmvq's probe path.
  - ptxas_check:
    - mul_mat_vec_q ≤ 58 registers with 36 warps/SM by registers;
    - mul_mat_q in memory mode with spills unchanged.
  - Correctness:
    - carry and tlatomic each 0 mismatches, with each mode forced on pt_victim;
    - kmr's 7-exit kernel accepted by ptxas;
    - lat and hist exact.
  - Decision, in both passes:
    - tg128 PT_SET_MMVQ: (carryonly - empty2) ≤ 0.03 and lat ≤ 0.07;
    - pp512 PT_SET_LLAMA4: lat under the rule's choice ≤ memory-mode lat + 0.5 points.
  - Force mul_mat_q into register mode once. If spills grow or pp512 gets worse, the rule stands.

Rung 5. clz for the bucket (a tool change, gated)
- Removes: the ISETP/SEL/SHF chain of about 30 instructions (0x2290-0x2490). It adds a 64-bit leading-zero count of a few instructions.
- Where and how:
  - A bpftime helper (for example 512, log2_u64), with a CPU implementation, lowered at IR to 63 - ctlz(v|1), i.e. clz.b64.
  - Tools call it instead of their own chain.
  - Size: about 35 lines plus tool edits; the trampoline must be regenerated.
  - A transparent IR matcher for exactly the pt_common.h and bits.bpf.h chains, proved equal once with a bit-vector solver, is possible but fragile across LLVM versions. It stays off the main path.
- Basis:
  - clz.b64 is documented (PTX 9.7.1.16 per facts_code).
  - Equality with the chain for every v, including log2_u64(0) = 0 = 63 - clz(1), is arithmetic, not measurement.
- Saving: E 0.005-0.025. How much of the chain mul_mat_vec_q hides is not known. Cumulative (endpoint) E 0.015-0.045.
- Fallback: the tool's own chain.
- Risks: a tool change (rule 2 prefers transparent changes) and a new helper ABI to keep.
- Validation first:
  - Add probe_shape shapes r_chain and r_smlane_chain: the rung-4 shape with the branchy chain in registers, the bucket kept alive by the "never" trick. Run 20 rounds x 2 passes.
  - Build the rung only if real lat after rung 4 is > 0.031 and (r_smlane_chain - r_smlane) ≥ 0.004 on empty/mmvq.
  - Keep it if real lat drops by ≥ 0.005 in both passes.

Rung 6. Shard sizing for other GPUs (performance only; no change on this board)
- Removes: nothing here.
- Where and how: nv_gpu_model_maps.cpp:717-731, plus the rung-3 formula. About 30 lines.
  - Slots stay a power of two ≥ 1, capped so that max_entries x slots x 32 x value_size stays under a memory cap (E: tied to the L2 size).
  - The formula works down to 32 shards and down to 1. Today sum_array_offset returns NULL below 32, and a failed allocation makes every lookup NULL, so counts are lost silently.
- Basis: needs nothing new; any shard is correct.
- Why: memory per entry grows with pow2(SM count) x 32. On GPUs with many SMs, or on maps with many entries, the allocation grows by that factor and can fail. Numbers for other GPUs are not derived here.
- Fallback: fewer shards, which means more contention, never a loss.
- Validation: pt_lat on tg128 with slots 1, 2, 4 and 8. Counts must be exact at every setting, including 1 shard in all. Adopt the cap if 2-4 slots cost within 0.003 of 8 slots.

Rung 7 (conditional). Inline the small probe bodies into the kernel
- Removes: both CALL/RET pairs, the BSSY/BSYNC around the call sites (0x0170-0x01d0, 0x1ab0-0x1af0), and the parameter moves.
- Where and how: PTX, in routing, after rungs 1-4 have made the bodies small.
  - Straight-line bodies first (pt_lat's entry); about 100 lines.
  - Then bodies with branches: rename every label and register per copy, and turn ret into a bra to the copy's end. 300-600 lines (facts_code R6).
- Basis: needs nothing new if every name is unique per copy and no .local is declared in an inlined body.
  - Label scope and shadowing are not stated, which is why every name is renamed.
  - Depot overlap is not stated: speed only.
- Saving: E 0.005-0.018; the real floor of 0.018 is the most it can save. Endpoint with this rung E 0.01-0.035.
- Fallback: keep the calls.
- Decision: build only if lat > 0.031 after rungs 1-5, and a hand-inlined copy of the final mmvq probes saves ≥ 0.01 in both passes. The copy is the kept PTX edited by hand and loaded through a debug override of the patched image.

ENDPOINT VS THE 0.031 NS BUDGET
- After rungs 0-5: E 0.015-0.045 ns per thread on tg128 mul_mat_vec_q, from the residual parts listed under rung 4, with clz in place of the chain.
- With rung 7: E 0.01-0.035.
- The budget lies inside both ranges. It is NOT assured. The open items that decide it are the floor's split and how much the real kernel hides.
- The final test is the user's rule: PT_SET_LLAMA4, lat ≤ 5% on tg128 and on pp512 in both passes, count checks exact, pt_victim set exact.
- Threadhist (1532) will not reach 5% on tg128 through these rungs.
  - Its add costs 25.9-26.0 points, M(README).
  - E: about 0.21 ns per thread (1.883 s/0.74 - 1.883 s, over 3.14e9 threads). That matches about 16 B of DRAM read and write per thread on an 8 MB per-kernel array that does not fit the 4 MB L2.
  - That cost is in bytes, not in probe shape.

WHAT CARRIES OVER AND WHAT IS TUNED TO THIS BOARD
- Carries over to other tools, kernels and GPUs: rungs 0-5 and 7.
  - Their correctness rests on documents or arithmetic, with today's code as the fallback, and they use no sm_70+ feature.
  - Dynamic keys: rung 3; rung 4 falls back to memory for them.
  - Helpers taking pointers: rung 2; host-call helpers keep their calls.
  - Atomics on thread-local values: rung 4 by provenance, otherwise the existing guard.
  - Threadhist on 1532: rung 3.
  - pp512 and mul_mat_q: the per-kernel mode in rung 4.
  - The switch contract: rung 1, which also makes it stricter.
  - The late path: P5.
- Tuned, for speed only, to be re-derived per GPU (at run time where possible):
  - the shard slots and the memory cap;
  - W_MAX and the register-headroom rule;
  - whether rungs 5 and 7 pay;
  - the 0.031 budget itself;
  - the effect that a memory-bound body hides probe work.
- On GPUs with more compute per byte of DRAM bandwidth, instruction-count rungs matter relatively less, and byte rungs (the carry, the atomics, threadhist) matter about as much. This is qualitative; no numbers were derived.
- Unchanged: map_info is written in one device's primary context, so several GPUs are still not covered. Reading the base from map_info keeps the per-module structure that a multi-GPU fix would need.

WHAT I WOULD NOT DO
1. Warp combine, i.e. activemask plus match.any or redux plus a leader atomic (R10).
   - C2 measured lanes disagreeing about their activemask results (39,143 and 116,089 of about 4.46M), and agreement is not stated (Q2).
   - The gain is small anyway: r_warp vs r_smlane is -0.003 to -0.011 on empty bodies and 0 on mem, M(C).
2. A global NVPTX DataLayout (R4a). It claims 8-byte alignment for every i64 access, including host-call lookup pointers (alignment not stated) and 1530 entries with value_size % 8 != 0. Misalignment is undefined (P92:5451).
3. PTX-text peepholes for the key round trip or for merging 32-bit pairs (R3-PTX, R4c). They are regexes on LLVM's output; rungs 3 and 4 remove both soundly.
4. %clock64 for durations (T6). It is a per-SM cycle counter, %smid may change, and the reported values would change.
5. A plain read-modify-write on an (smid,lane) shard. Every warp on that SM with that lane shares the word.
6. red instead of atom, or .global on one side only (see 3b).
7. Resting correctness on any of: .param words being in registers, ptxas inlining, %warpid, or the stability of %laneid.
8. CTA-level aggregation with bar.sync at the exits.
   - Exits are predicated and spread over several sites.
   - The text states only that barriers waiting on exited threads are released (P92:22856).
   - Shared memory changes occupancy.
9. Baking the 1531 base address into the PTX. That loses NULL-on-no-copy at run time and the late path's per-module values.

OPEN QUESTIONS
1. How the 0.018 floor splits between the gate load, CALL/RET and BSSY/BSYNC. Rung 1's experiment answers it.
2. Why the synthetic (smid,lane) add costs 0.04-0.055 with a memory carry and about 0 with a register carry (M(C)). The answer sets rung 3's gain before rung 4.
3. How much of the call overhead and the log2 chain mul_mat_vec_q hides. The synthetic bodies hide more.
4. 8-byte generic accesses into the local window. Today's prologue "st.u64 [%bpftime_tl+off]", the guard's ld.b64 and LLVM's own %SP accesses all make them.
   - The alignment of the window base is not stated (T5, Q7c), nor whether generic and .local addresses of the same location are aliases (P92:6851/7037).
   - Register mode removes the exposure for the carried words; memory mode keeps today's.
5. cuMemAlloc memory being in the global window. It is my inference (see conventions), and today's 1531 adds already rely on it. If it must be checked, a run-time isspacep.global (P92:18711) costs one instruction.
6. Where ptxas places inline %globaltimer reads relative to the kernel body. Not stated; it affects values only.
7. Whether rewriting .target to the device before compiling counts as "JIT-compiled to other targets", for which %globaltimer is unspecified (P92:42073). Today's helper has the same exposure.
8. The GUESSes in rungs 2-4 (GVN forwarding across the asm read, SROA promotion, aggregate returns as .param arrays). Each rung's PTX check settles its own.
9. Grid-sized outputs (threadhist) on tg128. That needs its own design, because the cost is bytes per thread.