# Step 0b, check C1: carrying state from the entry probe to the exit probe in the thread's own local memory

Follow-up round of step 0 (`/home/jetson/src/results/gpu/step0-checks-20261115/README.md`) for the per-thread map model (`/home/jetson/src/results/gpu/dir1-design-space-20261115/README.md`, sections 2 and 3).

**Status: built and smoke-tested only. No measurement has been taken.** Every smoke run shared the GPU with other agents and used reduced parameters. The real runs are `run_all.sh` on a quiet board, twice, by the main session.

**Adversarial review (2026-11-15, before any real run).** Changed: the verdict now needs every required trial at full parameters (rule 0), every GPU step's exit status 0, no spin-cap hit, and a deterministic mark-1 twin for the valid mark; a carry kernel failing (c)'s validation counts as FAILS; step timeouts are clipped to the 900-s total; leftover-shm cleanup touches only `step0b_c1_*`; SASS checks require kmr's 7 exit CALLs and `c_value`'s timer call; `build_d.py` checks its routing counts; `id_changes` is reported. The README now states the frame layout as compiled, kdiv's single exit site, and the fallback's documented weaknesses. All quotes in `docs/doc-quotes.txt` were re-fetched and found verbatim.

## Question

Can the entry probe write state and the exit probe read it back from the **thread's own local memory** (`.local`)? This must hold in the code the dev passes actually produce. If it does, a thread's temporary state needs no slot at all: storage is bounded by the hardware's resident threads, and nothing needs a tag.

It decides four things:

- **(a) Mechanism.** Which ways of injecting that storage PTX accepts, and which are sound.
- **(b) Correctness.** 0 wrong reads under every condition below, while controls that must fail do fail.
- **(c) Cost per thread execution**, timed by the main session. The variants are the carry, a `%smid`/`%warpid`/`%laneid` slot with an owner tag, and today's 1512 path.
- **(d) Compile.** Does a real llama.cpp module compile with the carry, and what does it do to registers, stack and occupancy?

**This round's rule (user, 11-15).** Correctness may rest only on documented guarantees. Behaviour that is only measured may affect performance only, or must be checked at run time with a counted fallback. Performance numbers are per platform and become parameters.

## What correctness rests on: documented versus measured

The exact quotes, fetched 2026-11-15, are in `docs/doc-quotes.txt`.

| link in the chain | source | status |
|---|---|---|
| `.local` is "private memory for each thread to keep its own data" | PTX ISA 5.1.5 | documented |
| With the ABI, `.local` variables "must be declared within function scope and are allocated on the stack". A kernel-scope `.local` therefore lives in the kernel's frame for the thread's whole run. Module scope is not allowed. | PTX ISA 5.1.5 | documented; ptxas confirms the module-scope rule with an error (form 2 below) |
| Local memory: scope **Thread**, lifetime **Kernel** (Table 1). "Local memory is used for thread local storage during the execution of a kernel." | CUDA Programming Guide 2.3.3, 2.3.3.4 | documented |
| `cvta` takes "the generic address of a variable declared in ... `.local`". A generic address inside the local window "maps to an address in the underlying state space". | PTX ISA 9.7.10.23, 6.4.1.1 | documented |
| Call arguments "may be register variables or constants"; a caller's arguments "may be `.param` variables, `.reg` variables, or constants". | PTX ISA 7.1 | documented |
| A thread's own write followed by its own read of the same bytes: the two are morally strong (program order, same proxy, complete overlap), and per location, communication order cannot contradict program order. | PTX ISA 8.7, 8.9.1, 8.10.5 | documented |
| "Unless otherwise specified, memory operations ... use generic method of memory access, i.e. a generic proxy". But "using distinct virtual addresses behaves as if using different proxies". The design meets this by touching the carry through **one** generic address only: the kernel's `cvta.local` of the variable, used for the valid mark and passed to both probes. It also uses the same 32-bit width the JIT emits for eBPF's 64-bit accesses (it splits them into halves), so every write and read overlaps completely. | PTX ISA 8.6, 8.3 | documented; the width is the JIT's (`build/*/..._module.ptx`) |
| No other thread, including an earlier thread on the same hardware slot, can change what this thread wrote there: `.local` is private to the thread, and "the side-effect of a memory operation in one state space can be observed directly only by operations that also have access to the same state space". Bytes the thread has not written yet are not covered (no document says what they hold; here they hold an earlier thread's data), hence the valid mark. | PTX ISA 5.1.5, 8.3 | documented. (A device-memory slot has no such guarantee: see the fallback below.) |
| The state survives compute preemption (time-slicing) while the thread sits between entry and exit. | No sentence names preemption. Local memory's documented lifetime is the kernel, with no exception. The device reports compute preemption support (CUdevice_attribute 90). | **not stated explicitly**; measured in (b), for time-slicing only, and on this board no warp moved (`id_changes_hog`, see Known limits) |
| By contrast, `%warpid` and `%smid` "may change during execution, e.g., due to rescheduling of threads following preemption", and `%warpid` is "a unique warp number within a CTA but not across CTAs within a grid". | PTX ISA 10.4, 10.8 | documented. A slot addressed by them is therefore neither stable nor documented to be unique among co-resident warps of different CTAs or kernels: it needs a tag that names the thread across everything co-resident, and a counted fallback |

Why the preemption link is reasonable even though it is not written down:

- ptxas spills registers to local memory.
- The JIT's own 512-B eBPF stack already lives there. Every existing probe passes the address of its key on that stack to helper 1.
- Local memory that did not survive a context switch would break any CUDA program that uses a stack.

Part (b) is still the evidence for it here, and it covers only time-slicing between two contexts.

**Making the carry rule-compliant anyway.** This round's rule allows measured-only behaviour if it is checked at run time with a counted fallback. The carry can do that cheaply: the entry writes the thread's id (and the kernel's identity) beside the payload, the exit compares it and, on a mismatch, discards the payload and counts a fallback. `PROG_CHK`'s id comparison (mismatch bit 1, with a per-kernel `SALT`) is this check; its comparison with the device-memory ground truth is test-only. With it, a GPU whose preemption did lose local memory would show counted fallbacks instead of wrong values, so correctness rests on the documented rows above plus the run-time check; only the fallback rate depends on the platform.

Three things are measured only, and nothing relies on them:

- **The same generic address in every thread.** The carry's generic address is the same number in every thread (`addr_distinct` = 1 in every smoke; SASS: `R1 + c[0x0][0x20]`). The generic-addressing rule makes it per thread; the value of the number is not relied upon.
- **ptxas's frame layout.** With this whole-module compile, ptxas puts every callee's frame inside the kernel's frame (callees report 0-byte frames): the 16-B carry at `R1+0`, and from `R1+0x10` the area that the probe functions' eBPF stacks, the helpers and `c1_clobber` share at the same offsets. That is why a callee's own frame (form 3) is overwritten by a later call, and the carry is not.
- **Local memory is not cleared between kernels.** A new thread's carry holds whatever an earlier thread on the same slot left (the no-mark control in (c)). The design does not rely on it either way: the prologue's valid mark covers it.

## (a) Mechanism

**How probes are built today (dev 52126f33).**
- `ptxpass_kprobe_entry` inserts `call __probe_func__<K>;` right after the kernel's `{`.
- `ptxpass_kretprobe` inserts `call __retprobe_func__<K>;` before every `ret;`/`exit;` of the kernel body, including predicated ones and ones with a label on the same line.
- Each probe function is the JIT's PTX of the eBPF program, with no parameters (`compile_ebpf_to_ptx_from_words(..., with_arguments = false)`). eBPF `r1` is therefore never initialized.
- The JIT knows only the helper ids that `pass/ptxpass_core/src/core.cpp:235-253` registers. An unknown id makes `generateModule` fail ("Ext func not found", `vm/llvm-jit/src/compiler_utils.cpp:368`), and `exitOnErr` ends the process (`llvm_jit_context.cpp:938`). A new helper id is therefore not usable without changing the dev tree.

**Forms tried** (`src/forms/*.ptx`, `tools/forms_check.sh`, output in `build/forms.txt`, ptxas 13.2, sm_87):

| form | ptxas | sound? |
|---|---|---|
| 1. kernel-scope `.local`, its `cvta.local` address passed as a `.reg` argument to every probe call, probe functions given one `.param .b64` | accepted | yes: every link is in the table above |
| 2. module-scope `.local` read by a helper | **rejected**: "Module-scoped variables in .local state space are not allowed with ABI" | n/a. PTX ISA 5.1.5 allows module scope only without the ABI ("legacy PTX code (ISA versions prior to 3.0)") |
| 3. a helper returning the address of a `.local` in its own frame | accepted | **no**: the frame ends when the helper returns. In (b) it reads back correctly until a call with a 2-KiB frame runs between entry and exit, then fails (kept as variant `frame`). |
| 4. "a helper that returns the address" in general | n/a | Only form 1 can feed it: a helper has no other way to reach the kernel's frame (form 3 is its own frame, form 2 is rejected). This is the "wrapper" access below. |

**Chosen: form 1.** It is the only form that is both accepted and documented, and it keeps the routing local to the kernel and its two probe functions. The routing (`tools/c1lib.py`) post-processes the real pass output:

- **Kernel** (`route_kernel`):
  - declares `.local .align 16 .b8 __c1_carry[N]` and a `.reg .b64`;
  - zeroes the first 8 bytes before the entry probe runs: `cvta.local.u64 %c1_carry, __c1_carry; st.u32 [%c1_carry], 0; st.u32 [%c1_carry+4], 0`, through the same generic address and width the probes use. This is the valid mark: an exit whose entry did not run never sees stale bytes as valid. Part (c)'s validation exercises it (mark 0: every exit-only thread sees "no entry"; a twin with mark 1: every one sees "valid", so the check can fail); its control without the mark shows stale bytes from an earlier kernel do reach the exit;
  - rewrites every `[@p] call <probe>;` into `cvta.local.u64 %c1_carry, __c1_carry; [@p] call <probe>, (%c1_carry);`.

  The address is recomputed at each call site, so no register stays live across the kernel body.
- **Probe functions** (`route_probe_function`) get `.param .b64 <name>_c1carry`. The eBPF program reaches the carry through an ordinary map lookup on a map of the new type 1540 (`bpf_map_lookup_elem(&carry, &key)`; key 0). The routing finds each helper-1 call whose map argument is the constant fd of such a map, and redirects it in one of two ways:
  - **helper form**: the call goes to `_bpf_helper_ext_0001_c1` (`src/c1_helpers.cu`), with eBPF `r5` (param 4, unused by every lookup) set to the carry address. The wrapper serves type 1540 and passes any other type to the trampoline's `_bpf_helper_ext_0001`. This needs no change to the passes, the JIT or the trampoline.
  - **direct form**: the call is removed and its result is the address itself. The direct form assumes a single-entry carry map (key 0). A real implementation would do this in the pass, or would give the JIT a carry helper.

  Every helper-1 call's fd must be a constant, and exactly one store lookup per probe must be routed. Otherwise the build stops: a change in the JIT's PTX shape fails loudly, never silently.
- **Not evaluated:** passing the address as eBPF `r1` (the JIT's `with_arguments` mode). That would change how the passes call the JIT.

**SASS evidence.** `tools/check_sass.py` runs `nvdisasm -c` on every module, which names every function `$<kernel>$<function>`. For every routed kernel it checks:
- the kernel reserves a frame (`IADD3 R1, R1, -X`);
- it calls the entry probe once and the exit probe at least once;
- it forms the generic address as `R1 + c[0x0][0x20]` and stores zero (the mark) before its first probe call;
- the PTX has the carry declaration and the two mark stores, and passes the carry at every probe call;
- in the helper form, both probe functions call the wrapper;
- in the direct form, neither does, and the entry probe has a generic `ST.E` and the exit probe a generic `LD.E`.

`build.sh` stops if any check fails. Pass statistics: `kmr` gets 7 exit-probe call sites, one per exit form.

## (b) Correctness

**Kernels.** The dev kprobe and kretprobe passes run on each kernel with its own eBPF; the routing then adds a 16-B carry.

- `kdiv` (CUDA; step0/e7-warp patterns):
  - absent low lanes, where lanes below a per-warp cut leave first;
  - data-dependent early returns;
  - nested divergent branches;
  - lane-dependent loops;
  - calls to a `__noinline__` function with a **2-KiB frame** between entry and exit. That array overwrites the part of the kernel's frame where the probe and helper frames were (`R1+0x10` upward); the carry sits at `R1+0` (kdiv's frame: 2,064 B = 16 + 2,048);
  - a lane-dependent spin before the end.

  nvcc merges kdiv's returns into a single `ret`, so its exit probe has **one** call site, reached by divergent lanes at different times. The several exit sites are kmr's.
- `kmr` (hand-written PTX, `src/kmr.ptx.in`). Every exit form the kretprobe pass handles, chosen per thread by data: `@p ret;`, `@p exit;`, `exit;`, `ret;`, `$L: ret;`, plus the 2-KiB call on one path, and a short spin (cap 2^22, cap hits counted like `kdiv`'s, `spin_caps` in each trial row). 7 exit-probe call sites; `build_b.py` stops if the pass gives another number, and `check_sass.py` requires 7 separate exit-probe `CALL`s in SASS.
- `kspin`, `ks2a`, `ks2b` (CUDA). Every thread spins for `spin_ms` between entry and exit (iteration cap 2^22, cap hits counted), sampling `%globaltimer` and sleeping 1 µs per iteration. Lane 0 logs every step longer than 50 µs (a gap).
  - **Detector control**, as step0/e1-ids: once, at half time, each warp stalls itself for 200 µs, and the next sample must see that gap. It is counted apart from preemption.
- Each of these also exists as `<name>_d`, the direct form.

**One eBPF source** (`src/probes.bpf.c`, `PROG_CHK`, one `SALT` per kernel family). The same probes serve every variant; only `map_info[store].map_type` changes, at run time, with the context synchronized first.

- **Entry.** Writes `(id ^ SALT, %globaltimer)` into the storage. It also writes the same pair into an **independent ground truth**: rec key 0, a per-thread array (1502) in device memory indexed by the thread's id, separate per kernel. It counts the entry.
- **Exit.** Compares the storage with:
  - the thread's own id (bit 1);
  - the ground truth's timer (bit 2: this launch's entry, not a stale or foreign one);
  - the ground truth's id (bit 4).

  It also checks the timer is not in the future (bit 8). It counts the exit, the mismatches and NULL storage, records the values and the address it used, then zeroes the storage and the ground truth (temporary state discarded).
- The id is the thread's linear id over all three grid dimensions, from helpers 503/504/505/508. It recurs in every launch, which is why the ground truth carries the timer.

**Variants** (map type of `store`):

| variant | storage | role |
|---|---|---|
| `carry` (1540), `carry_direct` | the kernel-scope `.local` carry | under test |
| `slot` (1543) | `(smid x nwarpid + warpid) x 32 + laneid` in device memory, no tag | control that **can** fail (if ids change, or are not unique across co-resident CTAs, which 10.4 does not promise); expected 0 here, as step0/e1 |
| `slottag` (1542) | the same slot, owner tag (linear thread id + 1) written at entry; at exit a foreign tag gives NULL, counted as fallback | the fallback design, as the task names it; its documented weaknesses are under Known limits |
| `shared` (1544) | one slot for every thread, device memory | **must fail** |
| `lane` (1545) | a slot per `%laneid` only | **must fail** |
| `frame` (1541) | the wrapper's own frame (form 3) | unsound; reported, not judged |

The must-fail controls live in global memory on purpose. A "shared" buffer in local memory cannot be built: the same generic address is each thread's own memory.

**Cases** (`build/c1check <case>`):
- `basic`: `kmr` and `kdiv`, every variant plus the direct form. Shapes:
  - `full512` 24 x 512;
  - `w48` 256 x 48 (partial warps);
  - `anchor` 512 x 512 = 262,144 threads, more than resident, so warps are replaced and local memory is reused.

  3 launches each, new input per launch.
- `graph`: `kmr` + `kdiv` captured into one graph, replayed 1,000 times per variant (`carry`, `slot`, `slottag`, `shared`). Then `kmr_d` + `kdiv_d`, also 1,000 times.
- `streams`: `ks2a` and `ks2b` on two non-blocking streams, each with half of its resident capacity, 30 ms spin, 3 reps per variant, plus the direct form. Both kernels' slot maps share one slot array. Overlap in time and shared SMs are measured from the warps' records.
- `spin` (`run_spin.sh`):
  - `kspin` (`carry`, `slot`, `slottag`, `shared`) and `kspin_d`, each at its own full residency (occupancy query), 2 launches of 300 ms.
  - `spin_ctl` runs alone. `spin_hog` runs while `build/hog`, copied from step0/e1-ids, keeps a second process's context busy, so the GPU time-slices while every warp sits between entry and exit.
  - Episodes and attribution follow step0/e1. An **episode** is a merged group of overlapping non-injected gap intervals that touches at least half the warps of the launch. **Attribution** is the share of our gap time during which the hog's warp 0 was running.

### Decision rules, part (b) (written before any real run; `tools/analyze_b.py`, `--selftest` checks them)

Rules 0 and the spin-cap rule were added by the adversarial review (2026-11-15), still before any real run.

- **Completeness (rule 0).** Every trial the verdict needs must be present with at least run_all.sh's parameters (`required_trials` in `analyze_b.py`):
  - basic: `kmr` and `kdiv` with `carry`, `shared` and `lane`, and `kmr_d`/`kdiv_d` with `carry_direct`, in each of `full512`, `w48`, `anchor`, 3 launches each;
  - graph: `kmr` and `kdiv` with `carry` and `shared`, `kmr_d`/`kdiv_d` with `carry_direct`, 1,000 replays;
  - streams: `ks2a`/`ks2b` with `carry` and `shared`, the `_d` ones with `carry_direct`, 3 reps, and 3 rows per carry variant in `streams.csv`;
  - spin, separately in the ctl and the hog run: `kspin` with `carry` and `shared`, `kspin_d` with `carry_direct`, 2 launches of at least 300 ms.

  Missing or short: INCONCLUSIVE. (`--quick`, used only by `QUICK=1`, relaxes these to QUICK's parameters; such a verdict is stamped as not a result.)
- **Count validity, per trial.** entries == exits == launches x threads, every thread's own counts equal the launches, no NULL storage at entry, no NULL rec. NULL at exit is allowed only for `slottag`, as its counted fallback.
- **carry FAILS** if any `carry` or `carry_direct` trial in any case has a mismatch or a count error.
- **carry WORKS** if all of the following hold:
  1. `carry` and `carry_direct` show 0 mismatches and valid counts in every trial of all four cases (`basic`, `graph`, `streams`, `spin`).
  2. Every must-fail trial (`shared` in every case, `lane` in `basic`) shows more than 0 mismatches. Otherwise the check could not fail there, and the verdict is INCONCLUSIVE.
  3. The preemption evidence holds, separately for `carry` and for `carry_direct`:
     - at least **10 episodes** over the hog launches;
     - attribution at least **0.5** in every hog launch that had a gap (a launch without one has no attribution);
     - the detector control seen in every warp, in both the ctl and hog runs;
     - in the control run alone, **no** non-injected gap above 50 µs (otherwise the threshold is invalid and the board was busy);
     - no capped warp;
     - the hog still alive when `c1check` ended.
  4. In the two-stream case, every rep overlaps in time and the two kernels share at least one SM.
  5. No spin-cap hit in `basic` or `graph` (`spin_caps`; a hit means a stuck timer).
- **INCONCLUSIVE** otherwise.
- **Reported, not judged:**
  - `slot` (mismatches here would mean ids changed, or two co-resident warps shared an index);
  - `slottag`: its mismatches, which must be 0 for the fallback design to be usable, and its fallbacks;
  - `frame`;
  - the number of distinct carry addresses;
  - `id_changes_hog`/`id_changes_ctl`: warps whose `(%smid, %warpid)` differed between the start and the end of their spin. 0 means preemption never moved a warp on this board, so the carry was not exercised across a move.

## (c) Cost (timed by the main session)

The harness follows step0/e2-cost exactly where it can: `build/c1bench`, adapted from e2's `bench2.cpp`.
- Driver API launches.
- The same launch gate `k_hold` (`src/gate.cu`, copied).
- Calibration to about 4 ms per batch.
- 10 rounds, each running every item once in the order of `mt19937_64(seed + round)`.
- CUDA events around N back-to-back launches.
- Batches whose gate hit its cap are dropped.
- Shapes: anchor 512 x 512, large 8,192 x 512, full 24 x 512. R = 1.

**Kernels** (`tools/build_c.py`). Empty bodies with bench2's signature. One eBPF source (`PROG_COST`): entry writes `%globaltimer` (one helper call) into its storage; exit reads it, and counts a zero or a NULL in a device counter (a branch only; the read is consumed).

| kernel | storage | memory |
|---|---|---|
| `c_none` | no probes: the delta reference (same module, same shape, same round) | |
| `c_empty` | both probes `return 0`: the empty-probe baseline | |
| `c_value` | entry calls the timer helper, stores nothing: the work (1)-(3) share | |
| `c_local_d` | (1) carry, direct form | local |
| `c_local` | (1') carry, via the lookup wrapper | local |
| `c_slottag` | (2) `%smid`/`%warpid`/`%laneid` slot, owner tag written at entry and checked at exit, via the wrapper | device |
| `c_1512` | (3) today's 1512 lookup, entry write + exit read; `membar.sys` in every lookup | registered (`step0b_` shm, prefaulted, `cuMemHostRegister`, as bpftime) |
| `c_ref` | e2's reference itself: threadhist's exit-only lookup-and-increment | registered |

**Validation** (once per kernel and shape, before timing):
- counters 0;
- the **sabotaged twins** `*_bad` (entry writes 0; not timed) must count a mismatch in every thread, which proves every exit read and compared;
- `c_1512` wrote every thread's slot;
- `c_ref` incremented every slot once;
- **the valid mark.** `c_local_noentry` and `c_local_d_noentry` are the carry kernels with the exit probe only. Each is launched right after a `c_local_d` launch, which leaves nonzero timers in the same frame slot (all of these kernels have a 656-B frame). Each must count a mismatch in every thread, because the exit finds the prologue's 0.
  - **The mark check can fail:** the twin `c_local_d_noentry_mark1` writes 1 instead of 0 in the prologue and must count **no** mismatch in any thread. Together the two show, deterministically, that the exit reads what the prologue wrote.
  - **Reported only:** the control `c_local_d_noentry_noinit` (no mark). Reading stale bytes as valid there shows that local memory is not cleared between kernels (measured, not documented), which is why a design needs the mark.

  All four are untimed.

**Derived** (`tools/analyze_c.py`):
- ns per thread execution = (item − `c_none`) / threads, median [min-max] over rounds;
- the same against `c_empty`;
- "resolved" if positive in every round;
- **[projection]** as e2: one carry per thread execution on every pp512 kernel, 192.56 M thread executions over 204.6 ms, overhead % = ns x 94.1.

**Comparability with e2.** `c_ref` / 0.433 ns (e2 pass 1) at anchor. Outside [0.8, 1.25] the table is marked "not comparable with e2". As in e2, if that happens, run a second time before reading it; do not widen the gate after the fact. Smokes gave 0.98-1.22, each with a very tight round spread. `c_ref` compiles to 32 registers and a 632-B stack, as e2's reference did, so the code path is the same.

**Decision.** The cost table is reported, not judged. These numbers are this board's parameters.

## (d) Compile: llama.cpp rms_norm

Inputs, read only: `step0/e8-package/ptx/rmsnorm-module-20261115-061844.{unpatched,1.patched}.ptx`, step 0's kept module.

Kernel: `rms_norm_f32<512,1,true,true,false,false>`, the one threadhist probed, launched with 512 threads per block. Every variant is compiled with `ptxas -arch=sm_87 -v`, the same compiler and target as bpftime's nvPTXCompiler (`tools/build_d.py`). The driver's register count, local size and `cuOccupancyMaxActiveBlocksPerMultiprocessor` come from `build/occ`: load only, no launch.

| variant | what |
|---|---|
| `U` | unpatched |
| `P0` | the kept patched module as is (today's exit probe) |
| `P0e` | `P0` + dev kprobe pass, empty program |
| `P0c` / `P0cd` | `P0` + dev kprobe pass with the cost program + routing (helper / direct): injection into the kept patched module itself |
| `E` | unpatched + trampoline + both passes, empty programs |
| `N` | unpatched + both passes with the cost program, **no routing**: the same probes without the carry |
| `H16` | `N` + routing, helper form, 16-B carry |
| `D16`, `D64`, `D256` | `N` + routing, direct form, 16/64/256-B carry |

### Decision rules, part (d) (`tools/analyze_d.py`)

- **PASS** if ptxas accepts every variant and the driver loads every cubin and finds the kernel (one `occ` row per variant).
- **FAIL** if ptxas rejects a variant, or `occ` reports a driver error from `cuModuleLoad` or `cuModuleGetFunction`.
- **INCONCLUSIVE** otherwise: rows missing without such an error (the occupancy step skipped, timed out or failed for another reason). (Changed by the review: before, a skipped step read as FAIL and so as "carry FAILS".)
- **The bound** is the change from the same probes without routing to the routed ones, in registers, cumulative stack and blocks per SM at 512 threads:
  - `N` -> `H16`/`D16`/`D64`/`D256`;
  - `P0e` -> `P0c`/`P0cd`, which also includes the entry program's own work, so it is an upper bound.

  An occupancy drop is reported as an occupancy cost, not a failure.
- **Memory** is cumulative stack x resident threads (8 SMs x 1,536), and whether the kernel stays within `CU_LIMIT_STACK_SIZE` (1,024 B here). [inferred] Below that limit the driver needs no larger per-thread stack; its sizing of local memory is not documented.

**From the compile (deterministic, not a measurement).**
- **Registers:**
  - Direct and helper forms: 0 extra registers (`N` 38 -> `D16`/`H16` 38).
  - The rise from the unpatched kernel's 32 to 38 comes from the cost program's probes (`U` -> `N`), not from the carry.
  - Into the kept patched module (`P0e` -> `P0c`/`P0cd`): +6, which includes the new entry program's own work.
  - In an earlier build, whose mark was a `st.local.u64`, the helper form had 40.
- **Headroom.** At 512 threads per block, 3 blocks/SM need at most 40 registers: 41 or more round up to 48 and give 2 blocks. The same probes without the carry sit at 38; the carry keeps that, so 2 registers of headroom remain.
- **Stack:** +24 B (16 B of carry plus 8 B of alignment padding to 16) = +288 KiB over 12,288 resident threads. D64 adds 72 B, D256 adds 264 B.
- **Occupancy:** 3 blocks/SM at 512 threads in every variant (threads per SM, 3 x 512 = 1,536, are the limit).
- **Stack limit:** every variant stays at or below 896 B < 1,024 B.

## Combined decision (`tools/verdict.py`, last line of `run_all.sh`)

- **carry FAILS** if (b) "FAILS", or (d) "FAIL" (a compile or load error, not a missing row), or a carry kernel of (c) (`c_local`, `c_local_d`) failed its validation (a mismatch or NULL storage). The fallback is then a `%warpid`-addressed slot plus an owner tag: the `slottag` variant. Its (b) result (0 wrong values, fallbacks counted) and its cost (c) are reported alongside, with the weaknesses listed under Known limits.
- **carry WORKS** otherwise, if all of:
  - (a) every routed module compiled and the SASS checks passed (`build_ok`);
  - every GPU step of `run_all.sh` exited 0 (`steps_rc.txt`);
  - (c)'s validation had no failure, the valid mark held (every `*_noentry` kernel with mark 0 counted every thread) and its mark-1 twin counted none;
  - (b) "carry WORKS" under the full parameter minimums (`b_summary.json` mode "full");
  - (d) "PASS".

  The no-mark control's result is printed next to it. The verdict file also prints `id_changes_hog` and repeats that the preemption link is measured, not documented.
- **INCONCLUSIVE** otherwise.
- A `QUICK=1` run is stamped "QUICK smoke, not a result".

## Commands (main session, quiet board, clocks checked)

```
cd /home/jetson/src/bpftime-gpu-work/step0b/c1-carry
./run_all.sh                 # -> runs/<date-time>/ ; or ./run_all.sh <OUT>
```

- **What it runs.** `run_all.sh` re-runs itself under `timeout -k 10 900` (the total timeout), and clips every step's own timeout to the time left before that deadline: a nested `timeout` runs in its own process group, so the outer one alone would not stop a running step. Each step's exit status goes to `OUT/steps_rc.txt`. In order:
  1. `build.sh` (CPU only, about 1 minute: eBPF objects, dev passes, routing, ptxas, SASS checks, part (d) compiles, host programs);
  2. part (b): basic, graph, streams, spin ctl, spin hog;
  3. part (c): 10 rounds;
  4. part (d) occupancy;
  5. the analyses and the combined verdict.

  Every GPU step has its own `timeout -k 5` (60-300 s) and every device spin has a cap.
- **Expected wall time:** about 2.5-3 minutes. Build 60 s. (b) about 75 s: basic 8 s, graph about 45 s (8 ms per replay), streams 1 s, spin ctl 4 s, spin hog about 12 s. (c) about 10-15 s. (d) 1 s.
- **Outputs** in `OUT/`:
  - `c1_verdict.txt` (last line: the C1 verdict);
  - `b_summary.txt/json`, `c/summary.md/json`, `d/d_summary.txt/json`;
  - the raw CSVs per case, `a_forms.txt`, the SASS checks, the ptxas logs;
  - `env.txt` (clocks and load before and after, the busiest processes) and `/dev/shm` before/after.

  POSIX shm segments are named `step0b_c1_<pid>` and are unlinked as soon as they are mapped. Leftover `step0b_c1_` segments (only this check's, never another step0b check's) and CUDA's `cuda.shm.*` for our PIDs (c1check, c1bench, hog) are removed at the end.
- **Single parts:**
  - `./build/c1check <basic|graph|streams|spin> OUT [key=value ...]`. Keys: launches, shapes, variants, iters, spin_ns, replays, spin_ms, gap_us, inj_us, logcap, reps.
  - `./run_spin.sh OUT hog|ctl HOG_S KERNEL_MS [c1check options]`.
  - `./build/c1bench --out OUT --rounds N`.
  - `python3 -I tools/analyze_b.py --selftest` must print `selftest ok`.
- **Development only:** `QUICK=1 ./run_all.sh OUT` runs every GPU step for a few seconds (about 45 s in all). It writes `OUT/quick`, analyses with relaxed minimums, and its verdict is stamped "not a result".

## Known limits

- **Preemption** is exercised only as time-slicing between two contexts (the hog), as in step0/e1. Not exercised: the debugger, checkpoint/restore, MPS, green contexts, compute-sanitizer, power gating. No document states that local memory survives preemption, only that its lifetime is the kernel.
- **Only calls in the kernel body are routed.** A probe called from inside a device function (the kprobe pass's stub mode, `__bpftime_cuda__kernel_trace` called from device code) would need the address threaded through that call chain. Not done. The routing stops the build if a probe's call count is not what it expects.
- **The routing parses LLVM NVPTX output:** callseq blocks, `mov.u64 %rd, <fd>`. A different JIT output shape stops the build; it never routes silently wrong. The direct form assumes a single-entry carry map and key 0. That is not checked at the PTX level.
- **No entry, no state.** If the entry probe did not run (probe switch flipped, no entry program), the exit sees the valid mark (0), not garbage. A tool must check it, as the cost program does. Local memory is **not** cleared between kernels: the no-mark control read the previous kernel's timers as valid in every thread. The mark is required, not optional.
- **Same eBPF, different kernels.** The (b) variants share one PTX, switched by map type; the direct form and (c)'s storages are separate kernels of one module with the same eBPF.
- **The fallback (`slottag`) rests on more measured behaviour than the carry.** This weakens the fallback design, not the carry; a fallback that meets this round's rule needs all of the following fixed:
  - *No launch or kernel identity in the tag.* It is the linear thread id + 1 (`%gridid` repeats across graph replays, step0/e1). A stale slot from an earlier launch, or a slot shared with a concurrent kernel's thread of the same linear id, passes the check.
  - *Slot uniqueness is measured only.* PTX ISA 10.4 promises `%warpid` unique within a CTA, not across CTAs, so two co-resident warps of different CTAs or kernels may share `(smid, warpid)` on another GPU or driver. Here the tag catches it within one grid (different block, different tag) but not across kernels (previous point).
  - *The slot array's size is a platform parameter.* "The SM identifier numbering is not guaranteed to be contiguous" (10.8), and `%nwarpid` need not equal threads per SM / 32. The harness sizes the array as SMs x (threads per SM / 32) x 32 (12,288 here; no slot variant saw a NULL in the smokes, whose slot kernels had at most 32 warps per SM resident, so this board's indices fit: measured, not derived from `%nsmid`/`%nwarpid`); an index beyond it gives NULL, which `slottag` counts as a fallback and the untagged controls count as NULL storage. A real design sizes it from `%nsmid` and `%nwarpid`.
  - *Tag and payload are not one access.* The tag is a plain store, then the payload; two claimants of one slot can leave one's tag beside the other's payload. Slot reuse by a later warp is also not ordered with the earlier owner's weak accesses by the PTX memory model (no synchronization between them), so per location the tag can be current while the payload is not.
- **Preemption never moved a warp here.** e1 found `%smid`/`%warpid` unchanged across time-slicing on this board, and (b) records the same (`id_changes_hog`). PTX ISA 10.4/10.8 allow a move; the carry's correctness then rests on the documented lifetime alone, or on the run-time tag check above.
- **The hog does not write local memory.** It shows that time-slicing does not lose the carry's writes; it would not show another context's local-memory writes leaking into ours.
- **A misconfigured map faults or spins.** A lookup on a map type the trampoline does not know goes to `host_call`, which these programs do not serve: with no communication area the kernel faults, otherwise its uncapped spins wait for a CPU server that is not there, and only the process timeouts bound them. Every map is configured before its kernel launches.
- **(c) times empty kernels:** the cost is fully exposed, as in e2. The `full` shape's deltas are a few µs, near the launch floor (e2's caveat). `c_ref` is compared with e2 across modules of different sizes.
- **(d) occupancy** is shown at 512 threads per block only, where threads per SM are the limit. Headroom there is 2 registers (38 of 40). At other block sizes, or with heavier probe programs, a register change could matter. The carry itself added 0 registers here; one build of the helper form added 2.
- **The kernels need PTX ISA 8.1.** `nvcc` 13.2 emits `ld.volatile.local` for volatile local arrays, which needs PTX ISA 9.1. The trampoline heads every patched module with `.version 8.1`, so the kernels here avoid it. Real application PTX with such instructions would not compile in bpftime's pipeline today. This is outside C1 and was noted while building.
- **Smoke runs** shared the GPU with other agents; their numbers are not measurements.
- **The ctl quietness rule** makes a run on a busy board (or with the desktop compositing) INCONCLUSIVE; that is intended, and the run should be repeated on a quiet board.

## Smoke runs (development, 2026-11-15; not results)

`smoke/`:

| run | what | finding (not a result) |
|---|---|---|
| `b1/`, `b2/`, `quick1/` | (b) cases | carry and carry_direct 0 mismatches. `shared` and `lane` mismatched in every trial. `frame` 0 mismatches with a 512-B clobber frame, mismatches with the 2-KiB one; the clobber was enlarged for that reason. Under the hog, about 66 episodes per 300-ms launch, attribution 0.80-0.81. |
| `quick1/` (c), earlier `c1/` | (c) with 1-2 rounds; the GPU was shared or idle, unknown | anchor, ns per thread: direct carry 0.104-0.107, wrapper 0.158-0.163, slot + tag 0.286-0.288, 1512 entry+exit 0.78-0.89, e2's reference 0.425-0.53 (ratio to e2 0.98-1.22). Valid mark: every `*_noentry` thread counted; the no-mark control read stale bytes as valid in every thread. |
| `d1/` | (d) occupancy query | as in (d) above |
| `review1/` | adversarial review, after its fixes: each GPU step alone (basic incl. anchor x 1 launch, graph x 20, streams, spin ctl and hog at 100 ms, c x 1 round, d), then `QUICK=1 run_all.sh` (`review1/quick/`, 47 s, of which build 40 s CPU) | carry and carry_direct 0 mismatches; must-fail controls failed; 22 episodes per 100-ms hog launch, attribution 0.82-0.83, `id_changes_hog` 0; mark-0 kernels "no entry" in every thread, mark-1 twin "valid" in every thread, no-mark control stale in every thread; `c_ref` ratio to e2 1.01. The quick-data analysed with the full minimums gives INCONCLUSIVE (42 required trials short), as it must. |
| `forms/` | (a) form tests | as in (a) above |

One development bug was found and fixed:
- **Symptom.** `kspin_d`'s gap log (and a marker written at kernel start) came back empty.
- **Cause.** `cuMemsetD8` runs on the legacy stream, which does not order against the non-blocking streams the kernels use, so the zeroing landed after the kernel's writes.
- **Fix.** `c1check` now synchronizes the context after every buffer setup. step0/e1-ids notes the same hazard.

## Files

| file | what |
|---|---|
| `src/kernels_b.cu`, `src/kmr.ptx.in` | (b) application kernels |
| `src/probes.bpf.c` | the eBPF programs (PROG_CHK, PROG_COST [+COST_BAD], PROG_VALUE, PROG_EMPTY) |
| `src/c1_helpers.cu` | the lookup wrapper `_bpf_helper_ext_0001_c1` (types 1540-1545). `-DC1_NO_FRAME` for (c) and (d). |
| `src/c1check.cpp` | (b) host program |
| `src/c1bench.cpp`, `src/gate.cu`, `src/step0b_regmem.h` | (c) host program, gate, registered memory (copies from e2 and step0/common; only the shm prefix changed) |
| `src/occ.cpp` | (d) occupancy query |
| `src/hog.cu` | second GPU process (copy of step0/e1-ids/hog.cu) |
| `src/forms/*.ptx`, `tools/forms_check.sh` | (a) form tests |
| `tools/c1lib.py` | ELF and relocations, pass invocation, assembly (as e2's `build_ref.py`), the routing |
| `tools/build_b.py`, `tools/build_c.py`, `tools/build_d.py` | module builders |
| `tools/check_sass.py` | PTX/SASS checks |
| `tools/analyze_b.py`, `tools/analyze_c.py`, `tools/analyze_d.py`, `tools/verdict.py` | analyses and decisions |
| `build.sh`, `build_bpf.sh`, `run_all.sh`, `run_spin.sh` | build and run |
| `docs/doc-quotes.txt` | the documentation quotes |
