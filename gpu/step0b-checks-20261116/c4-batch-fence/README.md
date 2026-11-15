# C4: one system fence for many warps' records (batched publication)

Step 0b, check C4. Design note: `/home/jetson/src/results/gpu/dir1-design-space-20261115/README.md` (sections 2 and 3). Step 0: `/home/jetson/src/results/gpu/step0-checks-20261115/README.md`, point 4 of "What this means for a per-thread map model": "Covering many warps' records with one fence (per chunk, per SM, at kernel end) is the open mechanism; step 0 did not test it."

**Status: built, PTX/SASS-checked and smoke-tested only. No measurement has been taken.** The smokes ran on a board shared with other agents' GPU programs, with the CPU clocks not fixed (cpu5 at 729,600 kHz). The real run is `run_all.sh full` on a quiet board with fixed clocks, twice, serially, by the main session.

## Questions

1. **Cost (decided).** How many ns per warp record does publication cost when G warps share one system-scope fence, for G = 1, 2, 4, 8, 16, 32, 48, 96, 384, 1,536 and 8,192 (the last two exist only at the anchor and large shapes)? What is the smallest G that brings the cost under the threshold T (below), and its [projection] for 6.02M warp records in 204.6 ms (step 0's pp512 figure)?
2. **Correctness (reported, not decided).** Does the CPU read every record of a chunk whose flag it has acquired? As in step 0's check 4, a system fence of several to 80 us can hide races, so the expected verdict is "consistent with the model / no violation in N chunk-rounds", never "proven", and not attributable to the fence's ordering.

The user's requirement for this round decides how the two answers are used. **Correctness must rest only on documented guarantees** (PTX ISA, the PTX memory consistency model, the CUDA documentation); behaviour measured here may only set performance parameters, or be checked at run time with a counted fallback. So the protocol is argued from the model (section "Why the protocol is correct in the model"), the data can only fail to contradict it, and the controls that pass on this GPU although the model forbids relying on them are reported as such, not as permission to drop a fence.

## The protocol

Records go into chunks in registered memory, made as bpftime makes its segment (`common/step0b_regmem.h`, a copy of step 0's `step0_regmem.h` with `step0b_` names: `shm_open`, `ftruncate`, `mmap MAP_SHARED`, read-prefault, `cuMemHostRegister(DEVICEMAP|PORTABLE)`).

1. Each contributing warp writes its record (layouts below).
2. It orders the record for the GPU with a gpu-scope release.
3. It increments its chunk's completion counter (a relaxed gpu-scope `atom.add`, **device memory**: the CPU never reads it).
4. The warp whose increment completes the chunk acquires at gpu scope, issues **one** system-scope fence, and stores the chunk's flag (`st.relaxed.sys`, registered memory).
5. The CPU acquire-loads the flag (LDAR), then reads every record of the chunk.

**Completion test.** The counters are cumulative and are never reset while a run lasts. The u-th use of a chunk completes at u·E increments, where E is the chunk's size (32·E for the ar32 variants). The completer is the one whose `atom` returns u·E − 1. No modulo is needed.

- In the cost program, the host zeroes the counters before each batch and passes the launch number as u.
- In the correctness program, u = (r − 1)/S + 1 for round r and S slots.

**`red.release.gpu` cannot be the counter operation.** It returns nothing, so no warp would learn that it completed the chunk. §8.11.1 also says reductions cannot be the read half of an acquire pattern. The value-returning form `atom.release.gpu.add` is used instead (variants `rel_*`).

**Layouts:**
- `w32`: one 32-B record per warp, written by lane 0 as 4 × u64, as in step 0's `rec_lane0`.
- `l8`: one 8-B word per lane, 256 B per warp, one coalesced store.

**Variants.** The SASS is checked for every kernel by `sass_check.py` (`build/sass_check.txt`). The model status is argued in the next section.

| variant | contributor (after the record) | completer | SASS (sm_87, CUDA 13.2) | model status |
|---|---|---|---|---|
| `ar_aa` | `fence.acq_rel.gpu; atom.relaxed.gpu.add` | `fence.acq_rel.gpu; fence.acq_rel.sys; st.relaxed.sys` | `MEMBAR.ALL.GPU; ERRBAR; CCTL.IVALL; ATOMG` / `MEMBAR.ALL.GPU; ...; MEMBAR.ALL.SYS; ERRBAR; CCTL.IVALL; STG.STRONG.SYS` | **literal (GPU side)**: every GPU-side link is a §8.8 pattern as written, each pattern in its own instructions; its acquire fence is `fence.acq_rel` (next section) |
| `rmw_ar` | `atom.acq_rel.gpu.add` (release for every warp, acquire for the completer) | `fence.acq_rel.sys; st.relaxed.sys` | `MEMBAR.ALL.GPU; ERRBAR; ATOMG; CCTL.IVALL` / `MEMBAR.ALL.SYS; ...` | **literal (GPU side)**; its acquire is an acquire operation, so it needs no reading of `fence.acq_rel` |
| `ar32_aa` (l8) | every lane: `fence.acq_rel.gpu; atom.relaxed.gpu.add` (32 increments per warp) | as `ar_aa` | 32 lanes' `ATOMG` on one word (not aggregated) | **literal (GPU side), without `bar.warp.sync`**; acquire fence as `ar_aa` |
| `ar_ar` | `fence.acq_rel.gpu; atom.relaxed.gpu.add` | `fence.acq_rel.sys; st.relaxed.sys` | `MEMBAR.ALL.GPU; ERRBAR; CCTL.IVALL; ATOMG` / `MEMBAR.ALL.SYS; ERRBAR; CCTL.IVALL; STG.STRONG.SYS` | literal patterns, but **one fence closes the acquire and opens the release** (needs the endpoint reading, below) |
| `rel_ar` | `atom.release.gpu.add` | `fence.acq_rel.sys; st.relaxed.sys` | `MEMBAR.ALL.GPU; ERRBAR; ATOMG` (no CCTL) / as above | as `ar_ar` |
| `ar32_ar` (l8) | as `ar32_aa` | as `ar_ar` | | as `ar_ar`, without `bar.warp.sync` |
| `ar_sc` | `fence.acq_rel.gpu; atom.relaxed.gpu.add` | `fence.acq_rel.gpu; fence.sc.sys; st.relaxed.sys` | `... MEMBAR.ALL.GPU ... MEMBAR.SC.SYS; ERRBAR; CCTL.IVALL; STG.STRONG.SYS` | relies on reading `fence.sc` as at least an acq_rel fence |
| `tf_sc` | `fence.sc.gpu` (= `membar.gl` = `__threadfence()`); `atom.relaxed.gpu.add` | as `ar_sc` | `MEMBAR.SC.GPU; ERRBAR; CCTL.IVALL; ATOMG` | same reading, twice |
| `rel_sc` | `atom.release.gpu.add` | as `ar_sc` | | same reading, once |
| `ar32_sc` (l8) | as `ar32_aa` | as `ar_sc` | | same reading, once |
| `nofence` (cost), `none` (checker) | `atom.relaxed.gpu.add` | `st.relaxed.sys` only | no MEMBAR, no CCTL | control: no synchronization at all |
| `sysonly` | `atom.relaxed.gpu.add` (no fence) | as `ar_sc` | | control: contributors without a release |
| `nosys` | `fence.acq_rel.gpu; atom.relaxed.gpu.add` | `fence.acq_rel.gpu; st.relaxed.sys` | no SYS MEMBAR | control: completer without the system fence |
| `early` (checker) | `atom.relaxed.gpu.add`, the completer's flag store, **then** the record | | flag `STG.STRONG.SYS` before the record `STG`s | control: must fail |
| cost references | `rec` = record only; `rf_{scgpu,argpu,scsys,arsys}` = record + one fence (`rf_scsys` = step 0's `rec_lane0` f1); `pw_sc` / `pw_ar` = per-warp publication (record, one system fence, flag; step 0's protocol A); `fonly_*` = lane 0 executes a fence alone; `ar_sc` with the counter in registered memory | | | |

The SASS lowering, from a probe compiled with the same nvcc:

| PTX | SASS |
|---|---|
| `fence.sc.gpu`, `membar.gl`, `__threadfence()` | `MEMBAR.SC.GPU; ERRBAR; CCTL.IVALL` |
| `fence.acq_rel.gpu` | `MEMBAR.ALL.GPU; ERRBAR; CCTL.IVALL` |
| `atom.release.gpu` | `MEMBAR.ALL.GPU; ERRBAR; ATOMG` |
| `atom.acq_rel.gpu` | `MEMBAR.ALL.GPU; ERRBAR; ATOMG; CCTL.IVALL` |
| `red.release.gpu` | `MEMBAR.ALL.GPU; ERRBAR; RED` |
| `fence.sc.sys`, `membar.sys` | `MEMBAR.SC.SYS; ERRBAR; CCTL.IVALL` |
| `fence.acq_rel.sys` | `MEMBAR.ALL.SYS; ERRBAR; CCTL.IVALL` |
| `st.release.sys` | `MEMBAR.ALL.SYS; ERRBAR; STG.E.64.STRONG.SYS` |
| `ld.acquire.gpu` | `LDG.E.64.STRONG.GPU; CCTL.IVALL` |

The variants differ in SASS only in SC against ALL, GPU against SYS, and whether a `CCTL.IVALL` follows.

## Why the protocol is correct in the model

Quotes are from PTX ISA 9.5 and are verbatim. `docs/ptx-isa-9.5-excerpts.txt` holds the quoted sections as fetched on 2026-11-15. A few `> CUDA-PG §` quotes come from the CUDA Programming Guide (`docs/cuda-programming-guide-excerpts.txt`). `check_quotes.py` checks every quote line of this file, indented or not, against the section it names: `python3 -I check_quotes.py README.md docs/ptx-isa-9.5-excerpts.txt docs/cuda-programming-guide-excerpts.txt`. It reports 75 pieces with 0 missing. The review also checked the 66 PTX pieces against a fresh fetch of the page (same md5), with the same result.

### Scope of the model

The model applies to this GPU:

> PTX §8.1: "The constraints specified under this model apply to PTX programs with any PTX ISA version number, running on sm_70 or later architectures."

The host is inside `.sys` scope, and outside `.gpu` scope:

> PTX §8.5: "The set of all threads in the current program executing on the same compute device as the current thread." ... "The set of all threads in the current program, including all kernel grids invoked by the host program on all compute devices, and all threads constituting the host program itself."

The warp is not a scope:

> PTX §8.5: "Note that the warp is not a scope; the CTA is the smallest collection of threads that qualifies as a scope in the memory consistency model."

Fences and `.relaxed`/`.release` accesses are strong operations:

> PTX §8.4: "A membar, fence.sc or fence.acq_rel instruction." ... "A memory fence operation, or a memory operation with a .relaxed, .acquire, .release, .acq_rel, .volatile, or .mmio qualifier."

An `atom` both reads and writes; a `red` only writes:

> PTX §8.4: "All variants of ld instruction and atom instruction (but not red instruction)." ... "All variants of st instruction, and atomic operations if they result in a write."

### The patterns the protocol uses

Contributor, `fence.acq_rel.gpu; atom.relaxed.gpu [cnt]`, and the completer's flag store, `fence.acq_rel.sys; st.relaxed.sys [flag]`, are release patterns:

> PTX §8.8: "Or a release or acquire-release memory fence followed by a strong write on M in program order" ... "E.g.: fence.release; st.relaxed [M]; or fence.release; atom.relaxed [M];"

Contributor `atom.release.gpu [cnt]` (`rel_*`) and `atom.acq_rel.gpu [cnt]` (`rmw_ar`) are release operations, so each is a release pattern by itself. The completer's `atom.acq_rel.gpu` is also an acquire operation, so it is an acquire pattern by itself:

> PTX §8.4: "A memory operation with .acquire or .acq_rel qualifier." ... "A memory operation with .release or .acq_rel qualifier."

> PTX §8.8: "A release operation on M" ... "E.g.: st.release [M]; or atom.release [M];" ... "An acquire operation on M"

The completer's `atom.relaxed.gpu [cnt]` followed by an acq_rel fence is an acquire pattern:

> PTX §8.8: "Or a strong read on M followed by an acquire memory fence in program order" ... "E.g.: ld.relaxed [M]; fence.acquire; or atom.relaxed [M]; fence.acquire;"

That an acq_rel fence serves as the "acquire memory fence" is shown by the model's own MP litmus test (§8.10.6). There, `F2: fence.sys` (no `.sem`, hence `.acq_rel`) closes the acquire pattern `R1; F2`:

> PTX §9.7.15.4: "If the optional .sem qualifier is absent, .acq_rel is assumed by default."

> PTX §8.10.6: "R1: ld.global.relaxed.sys.u32 %r0, [flag];" ... "F2: fence.sys;" ... "This establishes the causality order W1 -> F1 -> W2 -> R1 -> F2 -> R2."

On sm_87 there is no other candidate: `fence.acquire` does not exist there.

> PTX §9.7.15.4: ".acquire and .release qualifiers for fence instruction require sm_90 or higher."

So `ar_aa` and `ar32_aa` rest on this litmus test for their gpu-level acquire. `rmw_ar` does not: its completer acquires with `atom.acq_rel`, which is an acquire operation (§8.4, quoted above), so its acquire pattern is the §8.8 first form.

The patterns' reach:

> PTX §8.8: "Any memory synchronization established by a release pattern only affects operations occurring in program order before the first instruction in that pattern." ... "Any memory synchronization established by an acquire pattern only affects operations occurring in program order after the last instruction in that pattern."

> PTX §8.8: "For both release and acquire patterns, this effect is further extended to operations in other threads through the transitive nature of causality order."

### The chain for `ar_aa` (w32: one thread writes, releases and counts)

Contributor thread Tx: record stores W (plain `st.global`) → F_x = `fence.acq_rel.gpu` → A_x = `atom.relaxed.gpu.add [cnt]`.
Completer thread Tc: A_c = `atom.relaxed.gpu.add [cnt]`, which returns u·E − 1 → F_a = `fence.acq_rel.gpu` → F_c = `fence.acq_rel.sys` → S = `st.relaxed.sys [flag]`.
CPU thread H: R_f = LDAR of flag, which reads S → R_rec = reads of the records.

1. **A_x precedes A_c in observation order.** Every increment of the chunk is a morally strong RMW on one word. A_c returns u·E − 1, so it reads, through a chain of RMWs, from every increment of use u, A_x included. (By the RMW atomicity axiom, each increment reads the value of the one before it in coherence order.)

   > PTX §8.9.2: "A write W precedes a read R in observation order if:" ... "R and W are morally strong and R reads the value written by W, or" ... "For some atomic operation Z, W precedes Z and Z precedes R in observation order."

   > PTX §8.10.3: "When an atomic operation A and a write W overlap and are morally strong, then the following two communications cannot both exist in the same execution, for the set of bytes accessed by both A and W:" ... "A reads any byte from a write W’ that precedes W in coherence order." ... "A follows W in coherence order."

   > PTX §8.7: "The operations are related in program order (i.e, they are both executed by the same thread), or each operation is strong and specifies a scope that includes the thread executing the other operation." ... "Both operations are performed via the same proxy."

2. **(F_x; A_x) synchronizes with (A_c; F_a).** A_x precedes A_c in observation order. The first operation of the release pattern (F_x) and the last operation of the acquire pattern (F_a) are both `.gpu` fences on the same GPU, so each scope includes the other thread, and they are morally strong.

   > PTX §8.9.4: "A release pattern X synchronizes with an acquire pattern Y, if a write operation in X precedes a read operation in Y in observation order, and the first operation in X and the last operation in Y are morally strong."

3. **(F_c; S) synchronizes with R_f.** R_f reads S, and F_c (`.sys`) and R_f are morally strong. This assumes that the host's LDAR acts as a strong `.sys` acquire on the flag (H1 a), and that the CPU's mapping counts as the GPU's address (H1 b). Without H1 b, steps 3, 5 and 7 do not hold literally (section "Virtual aliases"). The acquire pattern's effect reaches F_c, because F_c follows F_a in program order. The release pattern's effect covers what precedes F_c, and that includes W through causality order (§8.8 footnote). Each pattern's reach sentence applies as written, because no instruction belongs to both patterns.

4. **Cumulativity.** W precedes F_x in program order, F_x synchronizes with F_a, F_a precedes F_c in program order, F_c synchronizes with R_f, and R_f precedes R_rec in program order. So W precedes R_rec in base causality order:

   > PTX §8.9.5: "An operation X precedes an operation Y in base causality order if:" ... "X precedes Y in program order, or" ... "X synchronizes with Y, or" ... "X precedes Z in base causality order and Z precedes Y in base causality order."

   > PTX §8.9.5: "The transitivity of base causality order accounts for the" ... "of synchronizing operations."

5. **No stale read.** R_rec cannot return a value older than W in coherence order: not the previous round's record, and not the CPU's poison (whose ordering before W is item 7).

   > PTX §8.10.6: "If a write W precedes an overlapping read R in causality order, then for any byte accessed by both R and W, R cannot read from any write W’ that precedes W in coherence order."

6. **The completer's own record**, when Tc = Tx, is ordered by program order.

7. **The harness's recycling** is modelled too. The CPU poisons the records, then STLRs the ack. On the GPU, lane 0 polls the ack with `ld.relaxed.sys`. Then **every lane that writes a record** issues `ld.acquire.sys` of the ack until it reads the expected value (capped). That is an acquire operation (§8.8, first form), so the CPU's poison stores precede the next round's record stores in causality order. Step 0's pub4 waited with `ld.volatile` only.

### Variants

**`rmw_ar`.** The same chain with single-instruction patterns. A_x = `atom.acq_rel.gpu` is the contributor's release pattern. A_c, the same instruction in the completer, is its acquire pattern, and its last instruction is A_c itself. F_c then opens a separate release pattern. Again, no instruction belongs to two patterns.

**`ar_ar`, `rel_ar`, `ar32_ar` (one completer fence).** Here F_c = `fence.acq_rel.sys` is both the last instruction of the acquire pattern (A_c; F_c) and the first of the release pattern (F_c; S). §8.8's reach sentences speak of operations *after* the last and *before* the first instruction, so they do not say whether synchronization passes through the shared instruction itself. The chain then needs the endpoint reading: §8.9.4's synchronizes-with relates exactly the patterns' first and last operations (its morally-strong clause names them), so F_x synchronizes with F_c and F_c synchronizes with R_f, and base causality is transitive through F_c. The MP litmus puts the fences themselves in its causality chain ("W1 -> F1 -> W2 -> R1 -> F2 -> R2"), which supports the reading, but it has no shared instruction. `ar_aa` pays one more gpu-scope fence per chunk to need no reading. For `rel_ar`, the release pattern is A_x = `atom.release.gpu` itself (the §8.8 first form).

**`*_sc`, `tf_sc` (what `__threadfence()` and `membar.sys` give).** In `ar_sc` and `rel_sc`, the gpu-level link is literal: the completer's `fence.acq_rel.gpu` closes the acquire pattern. The system-level release, however, is `fence.sc.sys; st.relaxed.sys`. The §8.8 release forms name "a release or acquire-release memory fence". §8.4 lists `fence.sc` and `fence.acq_rel` separately as memory fence operations. No sentence in 9.5 calls `fence.sc` an acquire-release fence. The text says:

> PTX §9.7.15.4: "fence.acq_rel is a light-weight fence that is sufficient for memory synchronization in most programs." ... "fence.sc is a slower fence that can restore sequential consistency when used in sufficient places, at the cost of performance." ... "On sm_70 and higher membar is a synonym for fence.sc"

The only synchronization rule that names `fence.sc` is Fence-SC order, and its direction is fixed at run time:

> PTX §8.9.4: "A fence.sc operation X synchronizes with a fence.sc operation Y if X precedes Y in the Fence-SC order."

The host does no `fence.sc`, so that rule cannot carry the flag to the CPU. The `*_sc` variants are therefore correct only under the reading "`fence.sc` is at least an acq_rel fence". NVIDIA's libcu++ in CUDA 13.2 implements C++ `seq_cst` thread fences, which are both acquire and release fences in C++, with `fence.sc`: `/usr/local/cuda-13.2/targets/sbsa-linux/include/cccl/cuda/std/__atomic/functions/cuda_ptx_generated.h:58-71` (`seq_cst` fence -> `fence.sc.{cta,cluster,gpu,sys}`) and `:78` (`__ATOMIC_SEQ_CST` dispatch on sm_70+). That is implementation evidence, not a PTX guarantee. `tf_sc` relies on the same reading at the contributors too. **A design that must avoid the `fence.sc` and endpoint readings uses `ar_aa`, `rmw_ar` or `ar32_aa`** (or, accepting the endpoint reading, `ar_ar`, `rel_ar`, `ar32_ar`). Even these keep smaller dependencies, listed under the decision rules: `ar_aa` and `ar32_aa` rely on the MP litmus for the acquire fence; the l8 lane-0 variants rely on `bar.warp.sync`; every variant relies on H1. Step 0's protocol A (`membar.sys` then `st.relaxed.sys`) is in the same position as `pw_sc`. Its literal twin, `pw_ar`, is step 0's D2.

**`l8` with a lane-0 release (`ar_*`, `rel_*`, `tf_sc`).** Lanes 1-31's stores reach lane 0's release only through `__syncwarp` (`bar.warp.sync`). §8.9.4 lists `bar{.cta}` barriers, `barrier.cluster`, fence.sc pairs, release/acquire patterns and API events, not `bar.warp.sync`:

> PTX §8.9.4: "A bar{.cta}.sync or bar{.cta}.red or bar{.cta}.arrive operation synchronizes with a bar{.cta}.sync or bar{.cta}.red operation executed on the same barrier."

The instruction's own description does give the ordering, so it is documented, but not in the formal model:

> PTX §9.7.15.2: "bar.warp.sync also guarantee memory ordering among threads participating in barrier. Thus, threads within warp that wish to communicate via memory can store to memory, execute bar.warp.sync, and then safely read values stored by other threads in warp."

The `ar32_*` variants avoid the question: every lane releases its own store with its own increment, so every link is a §8.8 pattern. The cost of each choice is measured. On sm_87 a converged `__syncwarp` compiles to no barrier instruction (`BRA.CONV` only), as in step 0.

### Why the controls are not correct in the model

- **`none`** has no release or acquire pattern anywhere, so no causality order between W and R_rec.
- **`sysonly`**: the contributors write W, then `atom.relaxed` with no fence. That is not a release pattern, so W is not ordered before anything in Tc. The completer's `fence.sc.sys` covers only Tc's own prior accesses:

  > PTX §9.7.15.4: "The membar instruction guarantees that prior memory accesses requested by this thread (ld, st, atom and red instructions) are performed at the specified level, before later memory operations requested by this thread following the membar instruction."

- **`nosys`**: the completer's `fence.acq_rel.gpu; st.relaxed.sys [flag]` is a release pattern. Its first operation, however, has `.gpu` scope, which does not include the host thread (§8.5). So it is not morally strong with the CPU's acquire (§8.7), and §8.9.4 gives no synchronization with the CPU.
- **`early`**: the flag is stored before W in program order.

### Virtual aliases: the GPU-to-host link is outside the literal model (added by review)

The chain above treats the GPU's flag store S and the CPU's flag load R_f as accesses to one address. They are not. The GPU uses the segment's device pointer and the CPU readers use their own mapping. pubc prints the three addresses (`va_device`, `va_registered`, `va_reader`), and on this board all three differ, for example `0x1021c4000`, `0xffffa0ce0000` and `0xffffa0be0000` (`results/review-smoke/pubc/`). The device attribute `CAN_USE_HOST_POINTER_FOR_REGISTERED_MEM` is 0 (pubc's JSON field `attr_can_use_host_pointer_for_registered_mem`), so even the agent's own registered mapping is not the GPU's address. The CUDA guide says the same for registered memory on systems without ATS or HMM (`PAGEABLE_MEMORY_ACCESS` is 0 here: `attr_pageable_memory_access`):

> CUDA-PG §2.6.3.1.2: "Unlike memory created with CUDA APIs, however, the memory cannot be accessed from the kernel using the host pointer. A pointer in the device’s memory region must be obtained using cudaHostGetDevicePointer(), and that pointer must be used for accesses in kernel code."

The PTX model's addresses are virtual, and distinct virtual addresses of one location are aliases that behave as different proxies:

> PTX §8.2: "The memory consistency model specification uses the terms “address” or “memory address” to indicate a virtual address, and the term “memory location” to indicate a physical memory location."

> PTX §8.2.1: "Two memory operations are said to overlap when the range of virtual addresses accessed by the two operations intersect."

> PTX §8.2.2: "Two distinct virtual addresses are said to be aliases if they map to the same memory location."

> PTX §8.6: "Although virtual aliases use the generic method of memory access, since using distinct virtual addresses behaves as if using different proxies, they require a proxy fence to establish memory ordering."

Moral strength needs the same proxy and complete overlap:

> PTX §8.7: "Both operations are performed via the same proxy." ... "If both are memory operations, then they overlap completely."

Read literally, then, S and R_f are not morally strong. R_f reading S puts S before R_f in no observation order (§8.9.2 needs moral strength), and §8.9.4 gives no synchronization. The records' causality clause also needs the same address, or an alias fence on the path:

> PTX §8.9.5: "X and Y are performed to the same address, using the generic proxy, or" ... "X and Y are aliases and there is an alias proxy fence along the base causality path from X to Y."

> PTX §8.10.6: "Virtual aliases require an alias proxy fence along the synchronization path."

**No GPU-side instruction repairs the flag link.** `fence.proxy.alias` exists on sm_70 and later. It could put an alias fence on the records' causality path, but it acts within one thread, so it cannot make S and R_f morally strong:

> PTX §9.7.15.4: "Bi-directional proxy fences do not directly synchronize with other fences in the sense that fences with .release or .acquire semantics do. Instead, bi-directional proxy fences take effect within a single thread and therefore do not have a .scope qualifier."

> PTX §9.7.15.4: ".acquire and .release qualifiers with .alias qualifier require sm_90 or higher."

On sm_87 it lowers to `MEMBAR.SC.SYS` or `MEMBAR.SC.GPU`, chosen at run time by a branch on the constant-bank word `c[0x0][0x128]`, then `CCTL.IVALL` (`build/lowering_probe.full.sass`, kernel `k_fproxy_alias`). It is not costed here. Across aliases the model promises only eventual visibility:

> PTX §8.6.1: "eventually become observable by strong accesses to the same location performed via a different proxy in that list, if modification and access are:" ... "system-scope - even if these used distinct memory addresses - or"

**So "literal" in this README means literal on the GPU side.** The GPU-to-host link of every variant rests on H1 below. The same holds for step 0's protocols and for any bpftime design whose reader is the loader process. H1 therefore does not change the ranking.

The one GPU-to-host route that §8.9.4 documents across aliases is API synchronization: kernel end followed by a stream or event synchronization, then the CPU read.

> PTX §8.9.4: "Completion of the last task queued to a stream, if any, synchronizes with return from cudaStreamSynchronize." ... "In addition to establishing a synchronizes relation, the CUDA API synchronization mechanisms above also participate in proxy-preserved base causality order"

This still needs a reading. The causality axiom (§8.10.6) speaks of an *overlapping* read, and overlap is defined by virtual address (§8.2.1). So even this route covers aliases only if the API sentence above is read as reaching aliased reads. The direct documented support is the guide's own `cudaHostRegister` example. The kernel writes through `devA`, and after `cudaDeviceSynchronize` the host reads through `a`, the registered host pointer: an alias in PTX terms.

> CUDA-PG §2.6.3.1.2: "copyKernel<<<blocks, threads>>>(devA, devB);" ... "CUDA_CHECK(cudaDeviceSynchronize());" ... "checkAnswer(a, b);"

A design that reads records only after its kernel's stream or event has completed rests on that documented example (same process, registered pointer) and on a reading of §8.9.4. It needs no H1. Publication while the kernel runs, which is what this check measures, rests on H1 b.

What the CUDA documentation does say about the host side (`docs/cuda-programming-guide-excerpts.txt`; web guide v13.5, fetched 2026-11-15; the board has CUDA 13.2):

> CUDA-PG §5.4.4.3: "All writes to all memory made by the calling thread before the call to __threadfence_system() are observed by all threads in the device, host threads, and all threads in peer devices as occurring before all writes to all memory made by the calling thread after the call to __threadfence_system()."

> CUDA-PG §5.4.4.3: "These functions differ in the thread scope in which orderings are enforced, but are independent of the accessed memory space, including shared memory, global memory, page-locked host memory, and the memory of a peer device."

> CUDA-PG §5.8.1.1: "Each thread in the system is related to each other thread in the system by the system thread scope:"

> CUDA-PG §5.8.2: "Types in namespaces std:: and cuda::std:: have the same behavior as corresponding types in namespace cuda:: when instantiated with a scope of cuda::thread_scope_system."

> CUDA-PG §5.8.3: "it is a load or store that affects a naturally-aligned object of sizes 1, 2, 4, 8, or 16 bytes on mapped memory [2], or"

Taken together:
- Host threads are documented as observers of a system fence.
- Host `std::` atomics are documented as system-scope atomics of the CUDA C++ model.
- Aligned 8-B loads and stores on mapped memory are documented as atomic at system scope.

The guide's `__threadfence_system()` sentence covers only the calling thread's own writes. Other warps' records reach the host only through cumulativity, which is stated in the PTX model (§8.9.5) and in the CUDA C++ model, not in that sentence. None of these texts speaks of a second virtual mapping.

### Assumptions outside the PTX model

- **H1.** Two parts.
  - (a) The CPU's `__atomic_load_n(ACQUIRE)` (LDAR) and `__atomic_store_n(RELEASE)` (STLR) act as strong `.sys`-scope acquire and release operations. The model covers host threads only as members of `.sys` scope (§8.5) and does not say how host instructions map to its operations. The CUDA C++ model documents host `std::` atomics at system scope (§5.8.2 above). The harness uses the GCC builtins on a plain `uint64_t`, which `std::atomic_ref` would make formal.
  - (b) Accesses through the CPU's own mapping count as same-address, same-proxy accesses to the GPU's device address of the same pages (previous section). Neither PTX nor the CUDA guide documents this, and no GPU-side instruction can supply it. It is the assumption under which any GPU-to-host publication through registered memory works on this board.
  - Step 0's check 4 stated part (a) and relied on part (b) without stating it.
- **H2.** Per §8.1.1, some system-scope strong operations may not be atomic on some systems:

  > PTX §8.1.1: "When communicating with the host CPU, certain strong operations with system scope may not be performed atomically on some systems."

  The protocol needs no system-scope RMW. The only RMW is the counter, in device memory at `.gpu` scope. The flag and the ack are aligned 8-B stores and loads.
- **H3.** The CUDA API orders the host's counter reset (`cuMemsetD32Async` on the same stream) before the kernel:

  > PTX §8.9.4: "Completion of a task enqueued in a CUDA stream synchronizes with the start of the following task in the same stream, if any."

  > PTX §8.9.4: "Start of a CUDA kernel synchronizes with start of all threads in the kernel. End of all threads in a kernel synchronize with end of the kernel."

- **H4.** A run-time check stands in for anything else. A design that must also hold on GPUs whose behaviour differs keeps only what the chain above needs. Everything this check measures beyond that, such as `nosys` or `sysonly` passing here, is a fact about this GPU and may be used only for performance.

## Method

### A. Cost: `build/bfc` (`src/bfc.cpp`, `src/kernels.cu`)

Derived from step 0's bench2 (`step0/e2-cost/src/bench2.cpp`), so the numbers stay comparable:
- Driver API, `cuLaunchKernel`, one module.
- The `k_hold` launch gate: a one-thread kernel holds the stream until the host has queued all N launches and the second event (capped at 200 ms and 2^24 polls; a capped batch is dropped).
- N calibrated to about 4 ms per batch (2-200).
- 10 interleaved rounds, ordered by `mt19937_64(seed + round)`.
- Delta against the empty kernel of the same module and shape, paired per round.
- Medians with [min-max] over rounds.

**Shapes** (step 0's):
- anchor = 512 × 512 threads (rms_norm's pp512 launch; decides);
- large = 8,192 × 512;
- full = 24 × 512 (12,288 resident threads).

R = 1: one record per warp per launch, the shape of an exit probe.

**Chunks.** Warp w (logical: global thread index / 32) is in chunk w / G, computed as `(w * ceil(2^40 / G)) >> 40`; the host checks the division for every warp. The last chunk of a launch may be partial: E = min(G, warps − c·G). Chunks are contiguous: for G ≤ 16 a chunk lies inside one 512-thread CTA, and therefore on one SM. G up to 384 is swept at every shape (full has 384 warps: one chunk at G = 384). 1,536 and 8,192 are swept at anchor and large (anchor has 8,192 warps: one chunk at G = 8,192). No warp waits for another, so the kernels cannot hang.

**Memory:**
- records: registered, 32 MiB region, warp w at 4w (w32) or 32w (l8) words;
- flags: registered, one 128-B line per chunk;
- counters: device memory, one 32-B sector per chunk (the `ar_sc|reg` rows put them in registered memory).

**Items** (283 at anchor and at large, 235 at full; 801 in all, every one validated in `results/dev-validate/`, and again with the reviewed bfc in `results/review-validate/`):
- empty and 4 `fonly`;
- per layout: `rec`, 4 `rf_*`, `pw_sc`, `pw_ar`;
- per layout and G: `nofence`, `nosys`, `sysonly`, `tf_sc`, `ar_sc`, `rel_sc`, `ar_ar`, `rel_ar`, `ar_aa`, `rmw_ar`;
- l8 adds `ar32_sc`, `ar32_ar` and `ar32_aa`;
- w32 adds `ar_sc` with the counter in registered memory.

**Validation, once per item before timing** (`items.json`):
- Two launches (epochs 1 and 2) on zeroed memory.
- Checked:
  - every record word of every warp;
  - every counter (2·E, or 64·E);
  - every flag (epoch 2, written by a warp of its chunk);
  - no flag beyond the last chunk;
  - completers = 2 × chunks and increments = 2 × warps (× 32 for ar32), counted in device memory in validation mode only;
  - the host's view of the chunk division.

**Timing:**
1. counters zeroed;
2. one untimed launch (epoch 1);
3. the gate;
4. N launches (epochs 2..N+1) between two events;
5. a post-check of the first and the last chunk's counter and flag, outside the timed window (`post_ok`; a failed row is dropped and counted).

**Derived:** ns per warp record = delta / warps. For l8, ns per lane record = delta / threads. Fence share = protocol − `nofence` at the same G. [projection] = 6.02M × ns / 204.6 ms.

### B. Correctness: `build/pubc` (`src/pubc.cu`)

Derived from step 0's pub4 test 1: the same registered memory with a second unregistered mapping for 4 CPU reader threads, the same payload values `(r << 32) | (w << 16) | (lane << 8) | j`, the re-reads of violating words after 1, 2, 4 ... 512 ms (healed = arrived late = an ordering violation; never healed = a harness bug), and the same launch chunking.

**Geometry.** 384 warps (48 × 256, all resident; the program checks occupancy and refuses otherwise, because a chunk waits for all its warps). G divides 384.

**Chunk membership.** Interleaved by default: chunk c = w mod (384/G), so a chunk's warps come from different blocks and SMs. `--contiguous` makes chunks of consecutive warps.

**Slots and the counter.** S = 4 slots per chunk; round r uses slot (r − 1) mod S. The per-(chunk, slot) counter is cumulative.

**Round r of a warp:**
1. Lane 0 waits for ack = r − S (`ld.relaxed.sys` polls with `nanosleep(64)`, capped at 2^26 polls and a 5-s globaltimer deadline).
2. Every record-writing lane acquires the ack (`ld.acquire.sys`, capped at 2^16).
3. The warp writes its record, then the variant's release and increment.
4. The completer runs the variant's acquire and system fence, then stores `flag = r << 20 | completer warp`.

Every cap or deadline hit stops the warp and is counted. A dead warp ends the run, because its chunk can never complete.

**CPU readers.** Each reader takes its share of the chunks and, for each chunk-round:
1. acquire-loads the flag;
2. reads every record word of the chunk in reverse order (last warp, last word first);
3. checks each word, classifying mismatches as stale, poison, future or other, and attributing them to the completer's warp or another warp;
4. overwrites every word with poison;
5. STLRs the ack.

A flag other than r or r − S is an anomaly.

**Launch chunking.** `--chunk-rounds 4`: a new launch every 4 rounds. In step 0 almost all reorders were in a launch's first round, and that round is counted separately.

**`--time-fence`.** The completer brackets its system fence with `%globaltimer` reads (histograms for a launch's first round and the other rounds; 64-ns bins).

**Rates** (records/s) include the CPU readers. They are rates of the whole loop, not GPU costs.

**Stop condition.** Each run is bounded by `--budget-s`, counted from the first launch; `--records` is set high. The reported N is what was verified.

## Decision rules (written before any real run)

### A. Cost (decides)

- **Data.** Medians over rounds at R = 1, registered records, counters in device memory. Shape anchor decides; large and full are reported and do not decide. Items that fail validation are excluded and listed. Rows with a capped gate or a failed post-check are dropped and counted.
- **Partial runs (added by review).** If bfc completed fewer rounds than requested (max-wall or timeout), the summary says PARTIAL with the round count. The decision is still computed, but on fewer rounds than these rules ask for. If a layout's `rec` item is invalid, that layout has no T and no decision.
- **Gate.** At anchor, `rec` (w32) and `rf_scsys` (w32) must each be within [0.75, 1.33] × step 0's 1.61 and 10.06 ns. Otherwise the run is "not comparable to step 0": it is still reported, and the rules below still apply with this run's own T.
- **Threshold.** T = 2 × this run's `rec` cost, per layout and shape (the task's "unfenced 1.6 ns per warp record × 2"). Secondary thresholds: 3.22 ns (2 × step 0's 1.61) and 3.40 ns (10% of 204.6 ms over 6.02M records).
- **Per protocol variant** (`ar_aa`, `rmw_ar`, `ar_ar`, `rel_ar`, `tf_sc`, `ar_sc`, `rel_sc`, and for l8 `ar32_aa`, `ar32_ar`, `ar32_sc`):
  - **G\*** is the smallest swept G whose median is ≤ T.
    - It is "stable" if the round maximum is also ≤ T, otherwise "marginal".
    - If a larger G is above T again, the curve is "non-monotone" and those G are listed.
    - The [projection] at G\* is 6.02M × Q / 204.6 ms.
  - If no G qualifies: "not under T in the sweep", with the lowest point (its G, Q and projection).
  - If `nofence`'s lowest point is itself above T, add: "the counter alone exceeds T". No fence choice could then reach it, and the asymptote is the counter's cost, not the fence's.
- **Ranking.** Variants that reach T come first, ordered by the smallest G\*, then by ns at G\*. Variants that do not reach T follow, ordered by their lowest ns.
- **The decided output** per layout:
  1. G\* (or the lowest point) and its projection for every protocol;
  2. **the best literal protocol** by that ranking (`ar_aa`, `rmw_ar`, `ar32_aa`; literal on the GPU side, see "Virtual aliases"), and next to it `rmw_ar`'s own result. `rmw_ar` is the only variant whose acquire needs no reading of `fence.acq_rel` as the acquire fence (added by review; reported, not ranked separately);
  3. the best protocol with a shared completer fence (`ar_ar`, `rel_ar`, `ar32_ar`);
  4. the best overall.

  Under the user's requirement, (2) is the set that needs neither the endpoint nor the `fence.sc` reading. Its remaining dependencies are documented text, though not all of it is in the formal model. The exception is H1 b, which every variant shares and no document states:
  - `ar_aa` and `ar32_aa` need the MP litmus (§8.10.6) for `fence.acq_rel` as the acquire fence; `rmw_ar` does not.
  - (3) needs the endpoint reading, and the `*_sc` variants need the `fence.sc` reading.
  - The l8 lane-0 variants (`rmw_ar` included) additionally rest on `bar.warp.sync`'s instruction-level guarantee; the `ar32_*` ones do not. No l8 variant avoids both this and the `fence.acq_rel` acquire reading: a per-lane `atom.acq_rel` variant was not built.
  - Every variant's GPU-to-host link rests on H1 alike (H1 b: undocumented; section "Virtual aliases").
- **Reported, not decided:**
  - the fence share per G;
  - `fonly_*` (the cost of a lone `MEMBAR.SC.GPU`, `ALL.GPU`, `SC.SYS` and `ALL.SYS` against each other: "membar.gl vs membar.sys alone");
  - `rf_*`;
  - `pw_sc` / `pw_ar` (step 0's per-warp fence);
  - the registered-counter rows.

### B. Correctness (reported)

Per (layout, G):

- **Validity.** The mode is INVALID if `early` shows no violation (the checker cannot see a flag published ahead of its records). A run is INVALID if it has any of the following:
  - a never-healed violation (harness bug);
  - a flag anomaly;
  - a future or other word;
  - a dead warp, cap hit or deadline hit;
  - published chunk-rounds that were not all verified;
  - nothing verified.
- **Power reference.** p_none = V_none / N_none.
- **Protocol variant with V violating chunk-rounds in N:**
  - **V > 0**: "VIOLATION observed". The model forbids it, so the compiled protocol or the harness is wrong, and it must be investigated before any design uses it.
  - **V = 0**: "consistent with the model: no violation in N chunk-rounds (N·G warp records); 95% upper bound 3/N". It is "with power" if p_none × N ≥ 3, otherwise "under-powered". It is always "not attributable to the fences' ordering". Under load a system fence lasted 2-80 us in the smokes, and a fence's duration alone may hide the race, as step 0's calibrated-delay control showed. No delay control is run here.
- **`sysonly`** (contributors without a release):
  - V > 0: "needed in practice too".
  - V = 0 with p_none × N ≥ 3: "not needed in practice on this GPU; the remaining fences, or their duration, hide the race; its necessity rests on the model; a design keeps it".
  - Otherwise: under-powered.
- **`nosys`** (completer without the system fence): the same three outcomes. "V = 0 with power" is exactly the kind of measured-only behaviour the user's requirement excludes from correctness: the model gives no synchronization with the CPU without a `.sys` pattern.
- **`none`:** V > 0 means "the check can fail at this G". V = 0 means "inconclusive-but-consistent".
- No "proven", no rate-based claim, and no attribution of a zero to the fence's ordering.

## Commands (main session; quiet board; clocks fixed; nothing else on the GPU)

```
cd /home/jetson/src/bpftime-gpu-work/step0b/c4-batch-fence
./build.sh                 # CPU only, about 20 s: cubin, bfc, pubc, PTX/SASS check (must print ALL OK)
./run_all.sh full          # results/run-<time>-full/{meta.txt,cost/,pubc/}; refuses unless cpu5 is at 1728000 kHz
python3 -I compare_runs.py results/run-<pass1> results/run-<pass2> results/compare.md
python3 -I check_quotes.py README.md docs/ptx-isa-9.5-excerpts.txt docs/cuda-programming-guide-excerpts.txt
```

- `run_all.sh` refuses to start if `build/sass_check.txt` is not ALL OK, or if any binary or the SASS check is older than a source (run `./build.sh` first).

- Do not wrap `run_all.sh` in an outer `timeout`; it would orphan a running program. It bounds itself:
  - `TOTAL_S`: full 1,500 s, quick 600, smoke 150.
  - Every call runs under `timeout -k 5`: bfc `max-wall + 120 s`, pubc `budget + 30 s`.
  - Budgets are cut to what is left, and calls are skipped and logged `SKIPPED` once too little remains.
  - A pubc call that fails at start-up with "busy or unavailable" (another process holding the GPU) is retried once.
- **Expected wall time:** full about 17 minutes; quick about 6; smoke 51 s (measured: `results/run-20261115-224327-smoke`). The breakdown of full:

| part | budget | runs | expected |
|---|---|---|---|
| A. cost | `--max-wall-s 480` | 801 items × 10 rounds | about 2-4 min; validation and calibration took 0.3 s (full), 1.0 s (anchor) and 11 s (large) in `dev-validate`, and the large shape's registered-counter items take 0.1-0.2 s per launch |
| B. `early` | 4 s | 2 layouts × G ∈ {1, 8, 48, 384} | |
| B. `none`, `sysonly`, `nosys` | 8 s each | 2 layouts × G ∈ {1, 8, 48, 384} | |
| B. `ar_sc` (the task's protocol), `ar_aa` (the literal one) | 20 s each | 2 layouts × G ∈ {1, 8, 48, 384} | about 10 min for the eight (layout, G) groups |
| B. at G = 48 | 10 s each | `tf_sc`, `rel_sc`, `ar_ar`, `rel_ar`, `rmw_ar`, plus l8 `ar32_sc`, `ar32_ar`, `ar32_aa` | about 2.5 min |
| B. `--time-fence` | 5 s each | `ar_sc`, `ar_aa`, G = 48, both layouts | 0.5 min |
| B. `--contiguous` | 5 s each | w32 `none` / `ar_aa`, G = 8 | 0.2 min |

- **Single calls:**
  - `timeout -k 5 50 ./build/pubc --layout w32 --variant ar_aa --G 48 --records 100000000000 --budget-s 20 --out x.json`
  - `timeout -k 5 300 ./build/bfc --out DIR --rounds 10 --shapes anchor --max-wall-s 180`
  - bfc options: `--shapes`, `--gs`, `--only REGEX`, `--rounds`, `--target-ms`, `--max-wall-s`, `--seed`.
- POSIX shm names are `step0b_c4_<pid>` (bfc) and `step0b_c4p_<pid>` (pubc). They are unlinked as soon as they are mapped, and `meta.txt` lists `/dev/shm/step0b_*` before and after.

## Smoke results (not a measurement)

The board was shared with other agents' GPU programs, with GPU load at 999‰ during parts of the runs. The CPU clocks were not fixed (cpu5 at 729,600 kHz; GPU at 1,020 MHz). Runs:
- `results/dev-smoke2/`: single pubc calls.
- `results/dev-smoke3/`: bfc, anchor, 2 rounds, GPU load 0 at start, before `ar_aa`, `rmw_ar`, `ar32_aa` and G = 1,536 / 8,192 existed.
- `results/dev-validate/`: every one of the 801 items validated, no rounds.
- `results/run-20261115-222806-smoke/`: the first smoke profile. Superseded: its l8 G = 48 rows include a run that hit "device busy" and one whose 1-s budget was spent in set-up.
- `results/run-20261115-224327-smoke/`: the final scripts, GPU load 0 at start and end.

**Cost** (`dev-smoke3`, anchor, ns per warp record; not comparable to step 0, gate ratios 1.35 and 1.22):

- **References:**
  - record only (`rec`): 2.18 (w32), 2.15 (l8);
  - lone fences (`fonly`): `MEMBAR.SC.GPU` and `ALL.GPU` 0.88, `SC.SYS` and `ALL.SYS` 7.6;
  - step 0's per-warp publication (`pw_sc`): 13.0.
- **The batched curve, w32** (`ar_sc`):

  | G | 1 | 48 | 384 |
  |---|---:|---:|---:|
  | ns per warp record | 21.3 | 7.2 | 4.3 |

  - The literal `rel_ar` reached 4.1 ns at G = 384.
  - `nofence` (the counter and the flag, no fence) cost 2.5-4.8 ns, and `nosys` cost as much as `nofence`: the gpu-scope fences are cheap.
  - The cost is the system fence, plus the counter atomic's latency.
  - At G = 1 the batched protocol costs more than the per-warp fence (21 against 13 ns), because the completer's system fence waits behind the atomic's return.
- **The batched curve, l8:**
  - `ar_ar` reached 3.38 ns at G = 384.
  - `ar32_*` (every lane its own increment) cost 19-33 ns at every G: 32 lanes' atomics on one word serialize.
  - The formally clean l8 variant is therefore far dearer than the lane-0 one that rests on `bar.warp.sync`.
- **Counter in registered memory:** 111-692 ns per warp record. Placing the counter in device memory is required, not optional.
- **Under these smoke conditions** every protocol in `dev-smoke3` reached T (about 4.3 ns) only at G = 384, the edge of the sweep then, marginally, with a [projection] of about 10-13%.
- The final smoke (1 round, anchor, G ∈ {1, 48, 384, 8,192}) ranked `ar_aa` as the best literal protocol: w32 G\* = 8,192 at 4.58 ns (13.5%), l8 G\* = 384 at 4.55 ns (13.4%). Its gate ratios were 1.58 and 1.39 (not comparable). The real run decides.

**Correctness** (single runs of 1-4 s):
- **`none`** (no fence) violated in this share of chunk-rounds:

  | G | 1 | 8 | 48 | 384 |
  |---|---:|---:|---:|---:|
  | violating | 0 (w32), 3-3.5 × 10^-6 (l8) | 12% (interleaved), 17-22% (contiguous) | 27-49% | 34% (w32) |

  At G = 1 this is close to step 0's C in normal mode, so G = 1 is under-powered.
- **`early`** failed at every (layout, G) tried: w32 and l8 at G = 1 and 48, and w32 at G = 384.
- **Every protocol variant** showed 0 violations, about 3.5M warp records per G = 48 run.
- **`sysonly` and `nosys`** were run at G = 1 and 48 only. At G = 48 both showed 0, with power: on this GPU a gpu-scope release/acquire alone, or a completer's system fence alone, hid the race. At G = 1 they were under-powered, except as follows.
- **l8 G = 1 `nosys` failed in both smoke profiles:** 2 of 543,744 chunk-rounds while another process time-sliced the GPU, and 21 of 8,223,744 on an otherwise idle GPU. All were healed: the completer's own words, read as the CPU's poison. `none` failed 28 of 8,094,720 there. If the real run repeats this, the system fence is needed in practice too, not only by the model.
- **The completer's system fence** lasted p50 69-84 us in a launch's first round and 2-3 us in other rounds (p99 up to 145 us, max 166 us).

## Known limits

- **Machine, not model.** The checker can only fail to contradict the model on this GPU, driver and CPU. Its zeros are "consistent with the model", and the fence durations (2-80 us, up to 145 us) prevent attributing them to the fences' ordering.
- **Empty kernels.** As in step 0, the cost is fully exposed. A warp's lifetime here is its record, its fence and its atomic's round trip, so the counter's latency directly limits warp turnover. Real kernels overlap it with work, or saturate memory, so the projection may err either way. It is labelled [projection].
- **Membership differs between the two programs.** The cost program's chunks are contiguous (inside one CTA for G ≤ 16), while the checker's are interleaved across blocks and SMs by default. One contiguous checker pair (`none`, `ar_aa`, G = 8) is run.
- **The sweep's edge.** In the smokes G\* sat at the largest swept G. G = 1,536 and 8,192 extend the sweep at anchor and large, but a curve still falling at 8,192 means the asymptote is not reached. Larger chunks also mean that a record waits for up to G − 1 other warps before the CPU may read it.
- **Static membership.** Chunk = warp index / G, with E known from the grid. A real map does not know which warps will write. It would need a claim (a cursor `atom`, about 1.1 ns in step 0: `comp_rec` dev f0 − `rec_lane0` dev f0 at anchor). It would also need a flush of the last partial chunk at kernel end, or a CPU read after the kernel. The latter needs no device fence. It rests on the API rules (§8.9.4, H3) and on the guide's `cudaHostRegister` example, and needs no H1 (section "Virtual aliases").
- **One record per warp per launch** (R = 1). Two record sizes (32 B per warp, 256 B per warp).
- **Counter placement.** Only one registered-counter variant (`ar_sc`, w32) is costed. The checker always uses device-memory counters.
- **The checker's geometry.** 384 resident warps, S = 4, chunk-rounds of 4 per launch, and 4 readers (one reader at G = 384). Reader speed bounds the rates. G = 384 runs are lock-stepped through one reader.
- **One process.** A second mapping stands for the loader process (H1).
- **Virtual aliases (H1 b).** Every CPU access in this check, and in bpftime, goes through a virtual address distinct from the GPU's (on this board even the registered mapping's address differs from the device pointer). Read literally, the PTX model then gives no GPU-to-host synchronization at all. The check measures behaviour under that assumption and cannot test it. `fence.proxy.alias` cannot repair the flag link, and it is not costed.
- **Time-slicing.** Another GPU process changes rates, fence durations and race windows (the l8 G = 1 `nosys` smoke). The real run must be alone on the GPU.
- **The model.** The reading of `fence.sc` and the `bar.warp.sync` question are stated, not resolved. The PTX ISA text is 9.5 as fetched on 2026-11-15; the CUDA 13.2 toolchain's ISA may differ in wording.

## Files

- `src/kernels.cu`, `src/bfc.cpp`: the cost kernels and host.
- `src/lowering_probe.cu` → `build/lowering_probe.sass`: the PTX-to-SASS lowering table above (built, never run).
- `src/pubc.cu`: the correctness harness.
- `common/step0b_regmem.h`: bpftime-style registered memory.
- `sass_check.py`: the PTX and SASS checks, which fail on a missing or misplaced fence, atomic, flag or record store. The negative test, editing one SASS instruction in 3 kernels, failed as it must.
  - The review added order checks for the checker's kernels: the ack acquire before the warp vote; the record stores between it and the counter atomic; exactly the variant's contributor fences between the last record store and the atomic; exactly its completer fences between the atomic and the flag store; `rmw_ar`'s `CCTL.IVALL` before the completer fence.
  - Their negative test edited 4 kernels in a copy of `build/pubc.sass` (a record store removed, `ALL.SYS` weakened to `ALL.GPU`, the acquire's `CCTL` removed, `rmw_ar`'s `CCTL` removed). All 4 failed.
- `analyze_cost.py`, `analyze_pub.py`, `compare_runs.py`: analysis.
- `check_quotes.py`, `docs/ptx-isa-9.5-excerpts.txt`, `docs/cuda-programming-guide-excerpts.txt`: the verbatim-quote and section-attribution check. Its negative test (one misattributed PTX quote, one misspelt one, one misattributed guide quote) failed as it must.
- `build.sh`, `run_all.sh`: build and run.
- `build/`: the cubin, PTX (`build/keep/kernels.ptx`, `build/pkeep/pubc.ptx`), SASS, `sass_check.{txt,json}`.
- `results/`: smoke and validation output only (above).

## Changes made after the smokes (before any real run)

Disclosed because some change decision rules after smoke data was seen:
- **Variants `ar_aa`, `rmw_ar`, `ar32_aa` added.** Review found that `ar_ar`, `rel_ar` and `ar32_ar` share one completer fence between the acquire and the release pattern, which needs a reading. The "literal" set of the decision rules moved from those three to the new ones. The older three are kept as "literal patterns, shared fence".
- **G = 1,536 and 8,192 added** at anchor and large, because every smoke protocol reached T only at G = 384, the edge of the sweep then.
- **The ranking rule** changed from "cheapest ns at its own G\*" to "smallest G\*, then ns", since the question asks for the smallest batch size. This also changed the smoke's "best" variant.
- **pubc's budget** now counts from the first launch, not from program start: a 1-s smoke budget had been spent in CUDA set-up. `run_all.sh` retries a pubc call once if the device was busy at start-up.
- The checker's main runs moved from `ar_ar` to `ar_sc` + `ar_aa`.

## Changes made by the adversarial review (before any real run)

No decision threshold, sweep or variant changed. The changes:

- **Model text.**
  - New section "Virtual aliases": the GPU-to-host link is outside the literal PTX model for every variant. H1 is split into (a), host atomics, and (b), aliasing.
  - "Literal" now reads "literal on the GPU side".
  - `ar_aa` and `ar32_aa` rest on the MP litmus for the gpu-level acquire fence (sm_87 has no `fence.acquire`); `rmw_ar` does not. Decided output (2) now also reports `rmw_ar`.
  - The CUDA guide's documented host-side guarantees are quoted.
  - Excerpts added: PTX §8.2, §8.2.1, §8.2.2, §8.6, §8.6.1 and the 9.7.15.4 Target ISA Notes (re-fetched, same md5), and `docs/cuda-programming-guide-excerpts.txt`.
- **`check_quotes.py`.**
  - It skipped indented quotes: 20 of 50 pieces were never checked.
  - It did not check the section named. It now checks both, for PTX and CUDA-PG quotes.
- **`bfc`.**
  - The post-check read flags never reset between batches, so an earlier batch of the same item (same last epoch) could satisfy it. The flags of the first and the last chunk are now zeroed before each batch, outside the timed window.
  - `items.json` is written after validation and after every round, so a run cut short can still be analyzed.
- **`pubc`.**
  - It records the three virtual addresses and two device attributes (`attr_can_use_host_pointer_for_registered_mem`, `attr_pageable_memory_access`).
  - The readers' verified count is now an atomic store, because the main thread reads it while they run.
- **`sass_check.py`.** Order checks for the checker's kernels (Files).
- **`analyze_cost.py`.**
  - It reports rounds done against rounds requested (PARTIAL).
  - T and the gate are not taken from an invalid `rec` item.
  - It adds the `rmw_ar` line.
- **`analyze_pub.py`.**
  - `sysonly` and `nosys` verdicts now respect mode validity.
  - The power reference comes only from a harness-clean `none` run.
  - Timed and contiguous runs get verdicts, with harness checks.
  - It lists calls that failed or left no result, and gives an "any protocol violation" flag.
- **`run_all.sh`.** A guard against binaries older than their sources, and header times that match this README.
- **`lowering_probe.cu`.** Now includes `fence.proxy.alias`, and `build.sh` keeps the unfiltered disassembly (`build/lowering_probe.full.sass`).
- **Wording.**
  - The literal set is said to avoid the endpoint and `fence.sc` readings, not every reading.
  - The read after kernel end is labelled as resting on the guide's `cudaHostRegister` example plus a reading of §8.9.4.
- **Review runs** (GPU load 0, cpu5 at 1,728,000 kHz; none is a measurement):
  - `results/review-smoke/`: short bfc and pubc calls.
  - `results/review-validate/`: all 801 cost items validated again with the reviewed bfc, 0 failures, 10 s in all.
  - `results/run-20261115-233208-smoke/`: `TOTAL_S=60 ./run_all.sh smoke`, past the new build guard. bfc ran, 13 pubc calls returned rc 0, the rest were `SKIPPED` by the bound, the whole run took 16 s, and no `step0b_` segment was left.
- **Not tested.** The bfc post-check's flag reset has no negative test (a kernel that skips its flag store); its logic was reviewed only.
