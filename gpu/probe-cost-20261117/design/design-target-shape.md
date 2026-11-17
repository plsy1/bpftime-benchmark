TARGET-SHAPE LADDER FOR pt_lat ON tg128 mul_mat_vec_q (read-only design, 2026-11-17)

Labels used below:
- [M] measured, with the table it comes from (measurements.md A/B/C, or merge-cost result-q1q2.txt).
- [E] estimated, with the reasoning given.
- [G] my guess about LLVM or ptxas behaviour; it needs a check.

I counted the instruction numbers myself from probe-sass-excerpt.txt and mmvq-patched-lat.ins.txt. They follow the path with the switch on, a start present and the key in range. Nothing was modified, built or run.

======================================================================
0. CALIBRATION: WHAT CAN BE CLAIMED
======================================================================
- **Only table B is real-kernel data. Table C does not carry over.**
  - The one shape measured both ways: c_empty at the mmvq launch shape costs +0.003 ns (empty body) and +0.000 (mem body) [M, C]. The real empty2 costs 0.017-0.019 [M, B].
  - So the "~0 ns" of c_param and r_smlane on mem/mmvq is not used as an expected cost anywhere below.
- **Today's lat executes about 212 SASS instructions per thread [E, counted].**
  - Entry, 41: kernel site 7 (0170-01d0), probe 31 (1e50-2030), timer helper 3.
  - Exit, 171:
    - site 6 (1aa0-1af0);
    - probe 78 (2040-2510);
    - timer helper 3;
    - helper 1 for type 1531, about 60: 2800-28a0, 3a20-3a30, the BRX jump table at 40f0-4140, and 4f20-5160;
    - after the return, about 24: the routing guard (QSPC.E.L at 2580, plus 2710-27e0) and ptxas's own expansion of the generic atom (ATOM.E at 25e0; QSPC.E.S, the ATOMS.CAST.SPIN loop and LD/ST at 2600-26f0).
- **Cost per executed instruction on the real mmvq [E from M, B]:**
  - lat: 0.284/212 = 1.34 ps.
  - empty2: 0.018/~28 = 0.64 ps.
  - timer item: 0.035/~16 = 2.2 ps.
  - These differ by a factor of 3.4, so every conversion below is a range.
  - For scale [E]: the board issues at most 8 SMs x 4 schedulers x 32 lanes x 1.02 GHz = 1.04e12 thread-instructions/s, i.e. 0.96 ps each.
  - The 0.031 ns budget is therefore about 23-48 executed instructions per thread [E].
- **Conversion to throughput:** x ns added per mmvq thread is about 1.67·x of tg128 run time (3.137e9 threads / 1.883 s). 0.031 gives -5%, 0.05 gives -7.7%, 0.10 gives -14%.
- **The BMOV.32.CLEAR B0-B15 block at the top of the patched mmvq is not caused by the probes.** probe_shape.sass has it in every kernel, including `none` and k_hold. It is excluded.
- **empty2 uses pt_empty.bpf.c, which declares no map.** Routing does not run for it, so its 0.018 is switch + calls + BSSY/BSYNC only, with no area.
- **What the rest of tg128 adds:** rms_norm and rope add about 0.015 ns-equivalent today [E: A gives 0.299, B gives 0.284; these are different passes].

======================================================================
1. TARGET SHAPE AND THE PIPELINE IT NEEDS
======================================================================
**What pt_lat should become (kernel view, inlined form).**
- Prologue: `ld.global.u32` of the switch, `setp`, then `st.local.u64 [__bpftime_tl+0], 0`.
- Entry: `@!on bra`; `mov.u64 w0, %globaltimer`; `st.local.u64 [tl+0], w0`.
- Exit:
  - `@!on bra`; `ld.local.u64 w0`; `mov.u64 t, %globaltimer`;
  - b = 63 - clz.b64((t-w0)|1); key = (w0==0) ? 65k+64 : 65k+b;
  - in = key < max_entries && base != 0;
  - shard = ((%smid & (S/32-1))<<5) | %laneid;
  - a = cvta.to.global(base) + (key*S + shard)*8;
  - `@in red.global.add.u64 [a], 1`.
- SASS model: r_smlane in probe_shape.sass (0160 plus 02a0-03d0) is 23 instructions.
  - Add: the switch (3), the no-start test (3), the bounds check (1-2), and the STL/LDL (2).
  - Total: about 33-36 [E].

**The pipeline this needs.**
- eBPF -> llvmbpf generateModule (GPU) -> O3.
- NEW `lower_gpu_helpers(M)`:
  - helper 502 becomes inline `mov %globaltimer`;
  - the new log2 helper (R6) becomes `llvm.ctlz`;
  - helper 1 with a constant fd of type 1531/1532 becomes an inline address in addrspace(1);
  - helper 1 on a 1530 map becomes a GEP into a per-call "mirror" of the thread-local words;
  - atomics on the mirror become plain read-modify-writes;
  - the probe function takes the words in and returns them.
- O3 again: GVN and SROA remove the key round trips and the eBPF stack.
- NVPTX codegen, then the PTX passes insert the calls.
- Routing:
  - loads the switch once per kernel;
  - threads the words through the calls, kept in a kernel `.local` at constant offsets (or in `.reg`);
  - splices leaf probe bodies into the kernel;
  - keeps `guard_generic_atomics` as a backstop.
- Trampoline wrap, then nvPTXCompiler.

**Map model.**
- 1530 keeps its meaning (the thread's own copy, zero at kernel start). Its storage becomes the kernel's words, passed by value. Areas over a cap (for example 64 B) keep today's pointer form.
- 1531/1532 keep their layout. "64-bit atomic adds only" becomes a compile-time check for constant-fd lookups.
- Map descriptors travel in the pass request. Device addresses are read at run time from map_info.

======================================================================
2. THE LADDER
======================================================================
**Common protocol for every rung.**
- Clocks fixed and checked; no leftover processes.
- tg128 with PT_SET_MMVQ, `-r 5` plus warmup, 2 shuffled passes. Items: base, empty2, timer, carryonly, count, lat.
- Count check: the sum map must hold exactly 15,711,207,424 for count and lat.
- pp512 lat on PT_SET_LLAMA4 (today 4.1-4.3%).
- ptxas_check on the four llama kernels (registers, frame, warps/SM).
- Correctness set:
  - pt_victim with carry, tlatomic, lat and hist: 0 mismatches, exact totals;
  - kmr's seven exit forms through the routing;
  - bpftime_nv_attach_tests and the runtime `[cuda-*]` tests.
- Keep rule, fixed now. A rung stays only if all of these hold in both passes:
  - the item it targets falls by at least half of the low end of its estimate;
  - the counts are exact;
  - no kernel loses warps/SM;
  - pp512 lat is not worse than today by more than 0.5 points.
  - Otherwise stop and attribute before the next rung.

---------------------------------------------------------------------
**R0. Measure the target on the real kernel first (no product change)**
- **What it removes:** nothing; it decides which rungs are needed.
- **Where and how:**
  1. **Attribution items.**
     - empty2-nogate: a debug switch that skips `gate_probe_function` (core.cpp:196-224). It gives the switch's share of 0.018.
     - carrynolog: a pt_lat variant whose bucket is replaced by a compare of the delta with a launch constant. It splits the 0.105 "carry + log2" item, which is not split today.
  2. **Hand-written target variants on the real mmvq.**
     - A debug-only hook, `BPFTIME_CUDA_PTX_OVERRIDE=<dir>`: if <dir>/<sha256 of the patched PTX>.ptx exists, compile that file instead. About 30-50 lines; never on the main path.
     - Hand-edit the kept patched PTX (BPFTIME_CUDA_KEEP_PTX=1) into three variants:
       - T-call: the section 1 shape with probe calls kept and words by value;
       - T-inline: spliced;
       - T-inline-reg: words in `.reg`.
     - Every variant keeps the 1531 adds, so the count check applies.
     - This edits bpftime's output, not llama.cpp.
- **Documented basis:** none needed; measurement only.
- **Expected saving:** 0.
- **Decision rules, fixed now:**
  - D1: T-inline > 0.05 ns in both passes means the shape itself misses the budget. Build R1-R5 only and report.
  - D2: T-call <= 0.031 means R7 (splice) is dropped.
  - D3: T-call > 0.031 >= T-inline means R7 is mandatory.
  - D4: if the log2 share (carryonly minus carrynolog) is < 0.015, R6 is deferred.
  - D5: R8 is taken only if T-inline-reg beats T-inline by >= 0.003 with warps/SM unchanged.

---------------------------------------------------------------------
**R1. Plumbing (request field and IR hook)**
- **What it removes:** nothing.
- **Where and how:**
  - New request field `gpu_maps`: fd, type, key_size, value_size, max_entries, area_offset, shards. It carries no addresses, so the PTX stays independent of the process. Filled in hack_fatbin (nv_attach_impl.cpp:1056-1125) from map_basic_info.
  - The kprobe_entry and kretprobe main.cpp pass it to compile_ebpf_to_ptx_from_words (core.cpp:226-274), which stores it in a new `vm.gpu_maps`.
  - New overlay patch vm/compat/llvm-vm/gpu-lowering.patch. In generate_ptx (gen-ctx:1131-1201), after optimizeModule: `if (lower_gpu_helpers(M)) optimizeModule(M, ...)`. This copies the CPU precedent at gen-ctx:722-726.
  - The field joins the patch_cache key automatically (in-memory cache only).
  - Size: 80-120 lines [E].
- **Documented basis:** needs nothing new.
- **Expected saving:** 0. Cumulative 0.284 [M, B].
- **Risks:** the cache key changes; one more O3 run per compile, only when a lowering fires.
- **Experiment:** with the lowering switched off, the patched PTX must be byte-identical for pt_lat, pt_hist, pt_carry, pt_tlatomic, latold, histold and empty. Unit test that the field reaches the vm.

---------------------------------------------------------------------
**R2. Inline 1531/1532 lookups and use `.global` red**
- **What it removes (exit, today's SASS):**
  - the STL of the bucket key (24b0), the argument moves and `CALL.REL 0x2800`;
  - helper 1's whole path, about 60 instructions: the map_type compare chain, the BRX jump table, the LDCs of map_info, the generic LD of the key, and the 1531 tail;
  - after the return: the null test, the routing's QSPC guard (2580, 2710-27e0), and ptxas's generic-atom expansion (2600-26f0).
- **What replaces it:** about 12-14 instructions: S2R smid, S2R laneid, 2-3 LOP/SHF, IMAD and LEA pairs with map_info constant operands, ISETP, and one RED.E.ADD.64. This is the r_smlane shape, 02a0-03d0.
- **Where and how (IR level):**
  - A `_bpf_helper_ext_0001` call with a constant 1531 fd becomes: `k = load i32 K` (GVN forwards the stored key), a bounds check, and a shard from `llvm.nvvm.read.ptx.sreg.smid`/`laneid` (present in LLVM 15, facts_code R2b).
  - base and shards are loaded from `map_info[fd]` at offsets 24 and 32. The result is NULL when out of range or when base is 0.
  - An atomicrmw add whose underlying object is such a pointer is addrspacecast to addrspace(1) (emits cvta.to.global). With an unused result it becomes `red.global.add.u64` (inline asm); otherwise `atom.global`.
  - Any other access through such a pointer is refused at compile time, so the module loads unprobed. This makes the 1531 contract checked.
  - 1532 gets the same lowering with getGlobalThreadId from sregs; the out-of-range warning stays a cold call.
  - Also:
    - strip the `.extern .const map_info` line, like the helper externs (core.cpp:280-283);
    - add static_asserts on the field offsets 24/32 (F3).
  - Size: 150-250 lines [E].
- **Documented basis:** the same as today's atom, with no new assumption.
  - The allocation is global memory:
    - drv-mem.txt:1513 (v13.2.2/text): cuMemAlloc "Allocates bytesize bytes of linear memory on the device ... The allocated memory is suitably aligned for any kind of variable";
    - cpg-writing-cuda-kernels.txt:242: "Global memory (also called device memory)";
    - ptx-isa.txt:5476, full sentence: "A generic address maps to global memory unless it falls within the window for const, local, or shared memory".
  - cvta.to.global is undefined only outside the window (P92:18780).
  - red on .global: P92:23623; defaults .relaxed and .gpu: P92:23629/23631; atomicity among atom/red: P92:23686.
  - The 8-byte alignment comes from drv-mem:1513.
  - Gap: "the address lies outside the const/local/shared windows" is implied, not stated. Today's generic atom ("where the address points to .global", P92:23363) needs exactly the same step.
  - Which shard is used cannot change a sum. `%smid & mask` is in range for any %smid value (P92:41436/41462), and %laneid is 0..31 (P92:41248).
- **Expected saving:** this rung attacks lookup + add, 0.135 [M, B].
  - Residual: 12-14 instructions, 0.01-0.03 [E, at 0.64-2.2 ps]. That is consistent with r_smlane minus r_carry on empty/mmvq, 0.010-0.018 [M, C].
  - Saving: 0.105-0.125 [E]. Cumulative: 0.16-0.18 [E].
- **Risks:**
  - Tools with dynamic fds keep helper 1.
  - The late path writes map_info at nv_attach_fatbin_record.cpp:263-291; the lowering reads the same table.
  - A map whose device copy belongs to another process has base 0, giving NULL at run time.
  - Tools without 1531/1532 maps must stay byte-identical.
  - [G]: whether ptxas folds constant-index map_info loads into instruction operands; whether NVPTX emits atom.global as RED without the inline asm.
- **Experiment:**
  - SASS acceptance: no CALL to helper 1 in the exit; exactly one RED.E.ADD.64; no QSPC and no ATOMS loop.
  - Keep if count and lat each fall by >= 0.05.
  - Also measure pt_hist (1532) on tg128 before and after (28.6% today [M, README]).

---------------------------------------------------------------------
**R3. Inline the timer**
- **What it removes, per probe:** `MOV R8` plus `CALL.REL 0x5170` plus the helper's 3 instructions; one CS2R takes their place. In the exit it also removes the start reloads `LD.E R9/R11` at 2260-2270.
- **Where and how (IR level):**
  - Calls to `_bpf_helper_ext_0502` become `asm sideeffect "mov.u64 $0, %globaltimer;"` with LLVM's `inaccessiblememonly`. Two reads are not merged, and the call no longer counts as a store, so GVN reuses the first load.
  - Use one 64-bit mov, not the _lo/_hi halves (T2).
  - About 30 lines [E].
- **Documented basis:** needs nothing new. The helper reads the same register, and the docs state no difference between inline and call (T2: P92:42056/42073). Placement relative to other instructions is not documented in either form (Q4c, Q5c).
- **Expected saving:** this rung attacks the timer item, 0.035 [M, B], which also includes that item's two compares.
  - Saving: 0.01-0.03 [E]. It is uncertain because the item's 2.2 ps per instruction suggests the call exposes CS2R latency, which may stay; r_carry is -0.001 to +0.009 [M, C].
  - Cumulative: 0.13-0.17 [E].
- **Risks:** low. Every user of helper 502 gets it.
- **Experiment:** SASS shows one CS2R per probe and no reload. Keep if the timer item and lat each fall by >= 0.005; the threshold is low on purpose because the estimate is uncertain.

---------------------------------------------------------------------
**R4. Load the switch once per kernel**
- **What it removes:** the switch sequence in each probe (MOV c[0x4][0x18], IMAD.MOV, ULDC, LDG.E, ISETP, @!P0 RET, at 1e50-1ec0 and 2040-20b0). One LDG and an ISETP in the prologue replace it. This also removes a predicated RET path in the callee, a step toward a single-exit callee.
- **Where and how (PTX level, routing kernel rewrite at nv_thread_local_routing.cpp:403-484):**
  - Prologue: `ld.global.u32 %bpon, [__bpftime_probes_enabled]; setp.ne`.
  - Each site: combine with its existing predicate using `and.pred` into a new predicate, or add `@!p bra skip`.
  - Remove the in-function gate only from functions that only rewritten kernel sites call. Stub-called functions, memcapture and bpf_main keep it.
  - Routing must now run for every probed kernel (F2: drop nv_attach_impl.cpp:997, 1007-1008).
  - Size: 60-80 lines [E].
- **Documented basis:** needs nothing new. It is the same plain load, and "launched after the write" rests on the same API ordering as today.
  - Never put `.uni` on a guarded branch or call. Lanes can load different values while the host writes, and the consequence of a false `.uni` is not stated (P92:22753, Q5b).
- **Contract change:** "every probe function loads the switch" becomes "every probed kernel loads it once; a thread's entry and exit always agree". Kernels launched after the write still run unprobed. CLAUDE.md and test_load_substitution need updating; the test still passes, because routing removes the gate after compile.
- **Expected saving:** the switch's share of empty2's 0.018 [M, B], measured in R0. Estimate 0.003-0.008 [E]. Cumulative 0.12-0.165 [E].
- **Risks:**
  - The predicate stays live across the kernel body. mul_mat_q is at 255 registers with 60 B of spills, so the predicate may itself spill. The fallback is to reload the switch at exit sites; it is a performance choice.
  - Routing now touches every probed module, so fix-matrix.sh and detach-switch.sh must run.
- **Experiment:** SASS shows one switch LDG per kernel and none in the probes. Keep if empty2 falls by >= 0.002 and detach-switch.sh still stops events at the detach.

---------------------------------------------------------------------
**R5. Pass thread-local words by value**
- **What it removes:**
  - Entry: the 1530 lookup (1ed0-1fc0: STL [R1+0x204], cvta, LD.E key, IMAD.WIDE, SELs, compares, RET guard) and the two 32-bit generic ST.E of the start (2000, 2020).
  - Kernel: the cvta at each call site and the generic ST.E.64 that zeroes the area.
  - Exit: the lookup (20c0-21c0) and the two LD.E of the start (21e0-21f0).
  - The 592 B eBPF stack depot: frame 640 to about 8 B [G: SROA].
  - It adds back one STL, one LDL and the parameter moves.
- **Where and how:**
  - **IR level** (in lower_gpu_helpers):
    - a mirror alloca [W x i64] of the whole area, with W capped (for example 8);
    - W new i64 parameters fill the mirror at function entry;
    - constant-fd 1530 lookups become bounds-checked GEPs into the mirror;
    - atomics whose underlying object is the mirror become plain read-modify-writes;
    - the r5 (area) argument of any remaining helper call gets the mirror's address, so dynamic fds and helpers 2/3 still work;
    - every `ret` returns the words in one `.param .align 8 .b8 retval[8W]` (single return value, PTX 7.1.1 per facts_code R7b);
    - the function is re-created with the new signature and the blocks spliced in.
  - **Routing:**
    - keeps `.local .align 16 .b8 __bpftime_tl[8W]` and accesses it only with `ld.local`/`st.local` at constant offsets;
    - zeroes it in the prologue;
    - before each site loads the words and calls with them;
    - after entry sites stores the returned words (predicated like the call);
    - after an exit site stores nothing, but only if the next instruction is that ret/exit with the same predicate;
    - for functions in this form, step 2 skips the area parameter and the r5 rewrite;
    - `guard_generic_atomics` stays as a backstop.
  - Areas above the cap, and programs with bpf-to-bpf calls, keep today's form.
  - Precondition: R4. An in-function `@off ret` would return words that were never set.
  - Size: 250-400 lines [E].
- **Documented basis:**
  - Per-thread register state (P92:1545); `.local` is "private memory for each thread" (P92:3023).
  - Arguments by value (P92:22755) and return values (P92:6696). Where parameters physically live is unspecified, which matters for performance only (P92:43032).
  - With an `.align 16` area (P92:4074) at 8-aligned offsets, the accesses are naturally aligned in `.local` space (P92:5451). This also ends today's reliance on the unstated alignment of the generic window for the zeroing `st.u64` (Q7c/T5).
  - A plain read-modify-write on the mirror is correct because the mirror is private (P92:3023).
- **Expected saving:** this rung attacks the carry part of the 0.105 item [M, B]; R0 measures the split.
  - About 38 of the item's ~81 instructions are the carry [E].
  - Saving: 0.04-0.055 [E]. Cumulative after this rung, endpoint A: 0.065-0.12 [E].
- **Risks:**
  - pt_tlatomic (a u32 add on half a u64, a fetch-add with its result used, an exchange) goes through the mirror's read-modify-write.
  - pt_carry passes stack pointers to helpers 503-505, so its stack stays. That is correct; an optional sub-rung lowers those helpers to sregs.
  - Stub-mode entries inside device functions throw as today, and the module loads unprobed.
  - The late path (#708, kprobe plus kretprobe on one kernel) is already broken; exit-only tools must not regress.
  - mul_mat_q gains no live register (local form).
- **Experiment:** frame <= 16 B; one STL at entry and one LDL at exit; tlatomic, carry and the kmr forms clean. Keep if carryonly and lat each fall by >= 0.02.

---------------------------------------------------------------------
**R6. clz (the only tool change)**
- **What it removes:** the 31-instruction shift and compare chain (2290-2490). About 6 instructions replace it (ISETP, LOP3 |1, FLO.U32, 2 IADD3, IMAD.MOV), as in probe_shape.sass 02d0-0320.
- **Where and how:**
  - New helper 512, `bpf_gpu_log2_u64(v) = 63 - clz(v|1)`. It equals pt_common.h's log2_u64 for every v, including 0.
  - Define it in default_trampoline.cu for the path without lowering, register it in core.cpp:234-253, and regenerate trampoline_ptx.h.
  - At IR level, lower it to `llvm.ctlz.i64` [G: NVPTX gives clz.b64].
  - pt_common.h's log2_u64 calls the helper: a one-line change.
  - A transparent exact-pattern matcher for known log2 code is possible but optional and off the main path, because it is fragile.
  - About 40 lines [E].
- **Documented basis:** clz.b64 (PTX 9.7.1.16, per facts_code R5). The helper's equality with the old function holds by definition; add a unit test over all bit lengths.
- **Expected saving:** this rung attacks the log2 part of the 0.105 item [M, B; split in R0].
  - Saving: 0.025-0.04 [E]. Cumulative, endpoint B: 0.035-0.08 [E].
- **Risks:** a new helper ABI to keep; unchanged tools gain nothing from this rung.
- **Experiment:** SASS has FLO and no chain. Keep if lat falls by >= 0.012.

---------------------------------------------------------------------
**R7. Splice leaf probe bodies into the kernel**
- **What it removes, per site:** BSSY/BSYNC (0170/01d0, 1ab0/1af0), the return-address MOV, CALL, RET and the argument moves. It also lets ptxas overlap the CS2R/S2R latency with kernel work.
- **Where and how (routing):** applies to a probe function with no `call`, no `.local` and no warp-synchronous instruction (shfl, vote, match, redux, bar, activemask).
  - Copy the body into each site inside `{ }`, renaming every register and label per site.
  - Its `ld.param` and `st.param retval` become `mov`; `ret` becomes `bra END_n`.
  - The site's predicate and the switch become `@!p bra SKIP_n` (no `.uni`).
  - Functions that do not qualify keep the call.
  - 150-250 lines plus golden tests on kmr's seven forms [E].
- **Documented basis:** a derivation from documented call semantics (P92:6694, 22755); the docs do not state that a call equals its inlined body (Q5c). Mark it as a derivation, not a quoted guarantee.
  - Renaming avoids the unstated rules on label scope and register shadowing.
  - Warp-synchronous bodies are excluded because copying changes "the same program point" (G132-cpp-language-extensions:2740).
- **Expected saving:** this rung attacks the call part of empty2 [M, B]. Saving 0.008-0.015 [E]. Cumulative, endpoint C: 0.025-0.055 [E].
- **Risks:** ptxas may move the timer reads relative to kernel instructions, so the measured interval may shift (open question 3); register pressure at the start and at exits [E: small live sets there]; code size per exit site.
- **Experiment:** no CALL and no BSSY at the sites. A script checks that the entry CS2R comes before the kernel's first LDG and the exit CS2R after its last STG. Keep if lat falls by >= 0.004. It is mandatory under D3.

---------------------------------------------------------------------
**R8. Words in registers (optional tuning)**
- **What it removes:** the STL/LDL of the words.
- **Where and how:** `.reg .b64` words instead of the `.local` area; optional `.maxnreg` set to the original register count (a performance directive). About 40 lines [E].
- **Documented basis:** P92:1545; spills affect performance only (P92:2980).
- **Expected saving:** -0.003 to +0.003 [E].
- **Risk:** mmvq uses 56 registers, exactly 7 x 256 per warp. Two more registers allocate 64, giving 32 warps/SM instead of 36 [E from the allocation rule; ptxas_check shows 36 today].
- **Experiment:** keep only under D5, with warps/SM unchanged.

======================================================================
3. EXPECTED ENDPOINT AGAINST THE 0.031 BUDGET
======================================================================
| endpoint | rungs | instructions per thread [E] | ns per thread [E] | tg128 loss |
|---|---|---:|---:|---:|
| A: transparent and documented only | R1-R5, call form, log2 chain kept | ~75 | 0.065-0.12 | about 10-17% |
| B: plus the clz helper (one tool line) | A + R6 | ~50 | 0.035-0.08 | about 5.5-12% |
| C: plus the splice (a derivation) | B + R7 | ~36 | 0.025-0.055 | about 4-8.5% |

- **The 0.031 budget is reached only at the low end of C. I do not claim it is met.** R0 replaces these estimates with real-kernel numbers for the target before most of the code is written.
- Today's lat costs 0.284 ns per thread [M, B]. Most of the expected gain is R2 (0.105-0.125) plus R5 and R6 (0.065-0.095) [E].

======================================================================
4. WHAT I WOULD NOT DO, AND WHY
======================================================================
- **Warp combine through activemask, match.any or redux.** Mask agreement is not documented, and C2 measured violations (39,143 and 116,089 of ~4.46M calls). The gain is small: r_warp is 0.006 vs 0.009-0.017 for r_smlane, and 0 on mem [M, C].
- **%clock64 durations.** They are cycles from a per-SM counter, and %smid may change (T6). That would change the values the tool reports.
- **Setting the NVPTX DataLayout globally before IR generation (R4a).** Misaligned 8-byte accesses become undefined for packed values and for value_size % 8 != 0.
- **A general PTX inliner (facts_code R6).** Depots, nested calls and warp-synchronous bodies make it high risk. Only the leaf splice.
- **eBPF-bytecode rewrites (R3 at bytecode level).** Fragile.
- **Relying on registers for parameter passing for correctness.** It is unspecified (P92:43032).
- **A cheaper switch read through `ld.global.nc` or the constant bank.** Whether host writes are visible on those paths is not in the collected docs. NOT documented.
- **Lane-only or SM-only shards.** Lane-only costs 0.28-0.30 ns and SM-only 0.35-0.37, against 0.15-0.16 for (SM, lane) [M, merge-cost].
- **Block-level aggregation in shared memory at exits.** It needs barriers on divergent, predicated exits. bar.warp.sync waits for "all threads" (P92:23065), so this risks a hang.
- **Sampling, dropping the bounds check on run-time keys, or dropping the no-start bucket without changing the tool.** Counts must stay exact.
- **Baking device addresses into the PTX.** It would work but is not needed, since map_info operands should be free [G].
- **Any change to llama.cpp.**

======================================================================
5. OPEN QUESTIONS
======================================================================
1. Does the real mmvq pay 0.64, 1.34 or 2.2 ps for each remaining probe instruction? R0's T-variants answer this.
2. Is the timer item's cost the call structure or the latency of the %globaltimer read itself? If the latter, R3 saves little and endpoint C rises by about 0.01-0.02 [E].
3. Timer placement after the splice. The order of a special-register read relative to other instructions is not documented, with or without a call. Counts are unaffected. You decide whether a SASS placement check on each patched kernel is acceptable as the check rule 1 allows.
4. On mul_mat_q (255 registers): keep the switch predicate live across the body, or reload it at exits?
5. [G] items to check in R1/R2/R6: SROA removing the [74 x i64] stack under LLVM 15's legacy pass manager; clz.b64 from llvm.ctlz.i64; RED for atom.global with an unused result.
6. With the switch loaded once, the no-start test (about 3 instructions) can only fire if %globaltimer reads 0. Keep it? This is the tool's choice.
7. The late path (#708) for entry plus exit pairs on one kernel.
8. The word cap W, and lowering helpers 503-505 for pt_carry-like tools.

Files read: everything listed in the task, plus:
- /home/jetson/src/results/gpu/map-model-proto-20261116/tools/pt_empty.bpf.c and pt_noop.bpf.c
- /home/jetson/src/results/gpu/map-model-proto-20261116/README.md and merge-cost/result-q1q2.txt
- /home/jetson/src/bpftime-gpu-work/proto/probe-shape/probe_shape.sass
- /home/jetson/src/bpftime-gpu-work/proto/design-20261117/mmvq-patched-lat.ins.txt
- in /home/jetson/src/bpftime-proto: nv_thread_local_routing.cpp, default_trampoline.cu, the generated llvm_jit_context_array_inline.cpp
- in the v13.2.2 doc text at /home/jetson/src/bpftime-gpu-work/step0b/c5-guarantees/docs/v13.2.2/text/: drv-mem.txt:1513, cpg-writing-cuda-kernels.txt:242/246, ptx-isa.txt:5476