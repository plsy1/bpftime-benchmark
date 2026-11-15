# C5: guarantee audit for the GPU map model (step 0b)

Follow-up round of step 0 (`/home/jetson/src/results/gpu/step0-checks-20261115/README.md`) for the per-thread map model (`/home/jetson/src/results/gpu/dir1-design-space-20261115/README.md`, sections 2 and 3).

**Status: the document audit is finished.** Every quote below was checked mechanically against the saved page text (`run_all.sh`, section "Commands").

**One optional GPU check** (`capture-remedy/`, claim 10) is built and smoke-tested twice (the second time after the review tightened its rules). Its real runs are for the main session. The audit needs no GPU.

**Adversarial review (2026-11-15), before any real run.** All quotes were re-verified against freshly downloaded pages. Several readings were corrected; the list is in "Review changes" at the end.

**This round's rule (user, 11-15).** Correctness may rest only on documented guarantees: the PTX ISA, the PTX memory consistency model, the CUDA Programming Guide and the CUDA Driver API reference. Behaviour that is only measured may affect performance only, or must be checked at run time with a counted fallback. Performance numbers are per platform and become parameters.

## Questions

1. For each of the ten claims made in conversation (section "The ten claims"), what do NVIDIA's documents actually guarantee?
2. For each step-0 finding and each mechanism that the follow-up checks C1-C4 rely on: is it DOCUMENTED, PARTLY DOCUMENTED or NOT FOUND?
3. For each step-0 verdict: could a different NVIDIA GPU legitimately behave differently?
4. From 1-3: what may the design rely on, and what needs a start-up check, a run-time check with a counted fallback, or a per-platform parameter?

## Method

- **Sources.** All were fetched on 2026-11-15 with `curl`, not WebFetch, so that the quotes come from the raw HTML and not from a summary. `docs/SOURCES.tsv` lists the 62 pages, and `docs/MANIFEST.tsv` gives the URL, sha256, size and version of each. The sources are:
  - PTX ISA 9.5, one page;
  - the CUDA Programming Guide v13.5, all 47 chapter pages plus the index (last updated Oct 31, 2026);
  - the archived CUDA C++ Programming Guide v12.9;
  - CUDA for Tegra v13.5;
  - CUDA Driver API reference v13.5: memory management, stream management, virtual memory management, stream memory operations, data types, and the two synchronization-behaviour pages;
  - four clauses of the C++ working draft (eel.is): [atomics.lockfree], [atomics.fences], [atomics.order] and [intro.races]. The last three were added by the review, so that claim 4's C++ steps are quoted rather than recalled. The working draft is not the published standard that CPG §5.8.4 amends.
  - the archived **CUDA 13.2.2** versions of the 21 cited NVIDIA pages (`docs/v13.2.2/`, with their own `SOURCES.tsv` and `MANIFEST.tsv`; PTX ISA 9.2). The table "Document tags" maps every tag to its title, version and URL.
- **Review (2026-11-15, adversarial).** All 78 pages the audit had saved (59 current, 19 of 13.2.2) were downloaded again and were byte-identical to the saved copies. The review added 5 pages, fetched the same day: three C++ clauses and the 13.2.2 versions of `drv-va` and `drv-api-sync`. Every quoted fragment was then searched for again with a second, independent HTML-to-text extraction (regular expressions, block boundaries marked). All were found, none crosses a block boundary, and the nearest heading of each matches its cited section. The changes the review made are listed in "Review changes" at the end.
- **Versions.**
  - The board runs CUDA 13.2.2 (`/usr/local/cuda-13.2/version.json`) with driver 595.78. The quotes come from the current 13.5 pages and PTX 9.5.
  - **Every quote was also checked against the 13.2.2 pages** (`run_all.sh` step 5). All are present there except one wording difference: `::cudaMalloc` (13.5) against `cudaMalloc` (13.2.2) in the `cuThreadExchangeStreamCaptureMode` text.
  - Section numbers differ between the versions; for example, the C++ memory model is §5.7 in 13.2.2 and §5.8 in 13.5. The numbers below are 13.5's.
  - **Quotes tagged `cpg-legacy-12.9` come only from the archived v12.9 guide**, because the v13.5 guide no longer contains that text: compute preemption, cross-context time-slicing and NVIDIA's discovery-pattern example. They are weaker evidence, and the rules below treat them that way.
- **Text.** `tools/html2text.py` converts each saved page (`docs/raw/*.html`) to text (`docs/text/*.txt`). Superscripts are kept as `^{...}`, so footnote markers are never glued to the words: `fence.sc^{1}`, not `fence.sc1`.
- **Quotes.** Every quote in this file has the form `[<doc> §<section>] "<verbatim text>"`, where `<doc>` names a file in `docs/text/`. The text inside the quotation marks is copied, not paraphrased, and ` ... ` separates fragments that are each verbatim.
  - `tools/check_quotes.py` checks every fragment against the page text, normalizing whitespace and nothing else.
  - For a numeric section, it also checks that the nearest numbered heading before the quoted text is that section, or its ancestor or descendant.
  - A quote that fails the checker invalidates its classification.
- **Searches for NOT FOUND.** Every page in `docs/text/` was grepped (case-insensitive) for the terms named in each row: 48 pages of the current guide, the v12.9 guide, PTX, Tegra and driver API. A NOT FOUND covers these 62 pages only. It does not cover other NVIDIA documents: the CUDA Runtime API reference, libcu++ (the CCCL page that PTX §8.1.1 links for "CUDA Atomicity Requirements" returned 404; CPG §5.8.3 holds that text), the Tuning Guides, or the inline PTX guide.
- **What was read in our own trees** (pinned by sha256 at reading time, because C1-C4 were still being edited by their agents):
  - `step0b/c1-carry/README.md` and `src/c1_helpers.cu` (3fe7a2c3);
  - `step0b/c1-carry/tools/c1lib.py` (9dc7a3f0; re-read by the review at bc646b10, README at 998ea13b: it still emits `cvta.local` before every probe call and still has `MT_SLOTTAG`);
  - `step0b/c2-claim/src/c2.cuh` (67c23966, unchanged at the end of this audit) and `step0b/c2-claim/README.md` (c396a2cc, written during this audit; it reports the measured `__activemask()` mask disagreement used in claim 9);
  - `step0b/c3-costparts/README.md` (d4c23a26; re-read by the review at ec204e7b, no row below changes);
  - `step0b/c4-batch-fence/README.md` (5e332dbb);
  - step 0's `e4-publish/README.md` (0780e5c8), `e7-warp/warp.cu` (f888e023) and `e3-copyout/partB/step0_copier.c` (eba331aa).

## Decision rules (fixed before classifying)

- **DOCUMENTED.** A fetched NVIDIA document states the property, or states rules that give it in the document's own terms, with every premise quoted. An example is chaining PTX §8.9.4 edges by §8.9.5 transitivity. Where CPG §5.8 keeps standard C++ semantics, quoted C++ clauses count as premises (added by the review for claim 4; they come from the working draft, see Known limits). Conditions such as a device attribute or a compute capability are listed, and each becomes a start-up check.
- **PARTLY DOCUMENTED.** Part of the claim is stated, and the missing part is named. The missing part is one of:
  - an informal reading (prose not tied to the formal model, for example `bar.warp.sync`);
  - text found only in the archived v12.9 guide;
  - an extension beyond the documented setting (for example, a reader in another process);
  - a CUDA C++ guarantee whose PTX counterpart is not formal.
- **NOT FOUND.** No fetched text states the property; the search (pages and terms) is listed. Marked **contradicted** when a document states the opposite or a strictly weaker property.
- **Not a GPU guarantee.** Performance numbers, bugs in bpftime, and properties of the application or of the hooks. They become platform parameters or bpftime items and get no documentation class.
- **What the design may do with each class:**
  - DOCUMENTED: rely on it, after checking its conditions at start-up.
  - PARTLY: rely only on the documented part. The rest may affect performance only, or must be checked at run time with a counted fallback.
  - NOT FOUND: performance only, or a run-time check with a counted fallback. Never correctness.
- **Measured agreement never upgrades a class.** Step 0's zero violations stay evidence about this board.
- **Control.** `tests/altered.md` holds four quotes: one has a single word changed, and one is verbatim but carries a wrong section number. The checker must reject exactly those two. If it rejects neither, the checker cannot fail and no class in this file stands.

## Commands

```
timeout -k 5 600 ./run_all.sh            # about 15 s; no GPU, no network
timeout -k 5 600 ./run_all.sh --refetch  # adds a re-download of all 62 URLs (about 25 s) and lists pages that changed
```

`run_all.sh` prints PASS only if every step passes:
1. verifies the sha256 of every saved page against `docs/MANIFEST.tsv` and `docs/v13.2.2/MANIFEST.tsv`;
2. regenerates the text from the raw HTML and compares it with the saved text;
3. checks every quote and section number in this README;
4. runs the control, which must reject exactly 2, and exactly the expected two: the altered word (`tests/altered.md` line 7, NOT FOUND) and the wrong section (line 13, WRONG SECTION);
5. checks every quote against the 13.2.2 pages, expecting exactly the 1 known wording difference, and exactly that one (the `::cudaMalloc` fragment).

Not checked by the tools: text in double quotes that has no `[doc §section]` tag in front of it. The review listed all such strings. Those that name document text were grepped and are verbatim; those that paraphrase a claim or a step-0 finding are written without quotation marks.

Its temporary files stay in a `.tmp_run.*` directory here, which it removes. The compile-only lowering probe of claims 4 and 9 is `lowering/build_probe.sh` (about 20 s, no GPU).

With `--refetch` it also downloads every URL into a temporary directory, reports which pages changed, and keeps the saved copies unchanged. A changed page means its quotes should be rechecked against the new text: run the checker with the temporary directory as the text directory.

## Document tags

Every quote's tag names one saved page. The URL is the page as fetched on 2026-11-15. The last column says whether the quote check also ran against that page's CUDA 13.2.2 version.

| tag | document | version | URL | 13.2.2 copy |
|---|---|---|---|---|
| `cpg-advanced-host-programming` | CUDA Programming Guide 3.1. Advanced CUDA APIs and Features | v13.5 | https://docs.nvidia.com/cuda/cuda-programming-guide/03-advanced/advanced-host-programming.html | yes |
| `cpg-advanced-kernel-programming` | CUDA Programming Guide 3.2. Advanced Kernel Programming | v13.5 | https://docs.nvidia.com/cuda/cuda-programming-guide/03-advanced/advanced-kernel-programming.html | yes |
| `cpg-asynchronous-execution` | CUDA Programming Guide 2.5. Asynchronous Execution | v13.5 | https://docs.nvidia.com/cuda/cuda-programming-guide/02-basics/asynchronous-execution.html | yes |
| `cpg-cooperative-groups` | CUDA Programming Guide 4.4. Cooperative Groups | v13.5 | https://docs.nvidia.com/cuda/cuda-programming-guide/04-special-topics/cooperative-groups.html | yes |
| `cpg-cpp-language-extensions` | CUDA Programming Guide 5.4. C/C++ Language Extensions | v13.5 | https://docs.nvidia.com/cuda/cuda-programming-guide/05-appendices/cpp-language-extensions.html | yes |
| `cpg-cuda-cpp-execution-model` | CUDA Programming Guide 5.9. CUDA C++ Execution model | v13.5 | https://docs.nvidia.com/cuda/cuda-programming-guide/05-appendices/cuda-cpp-execution-model.html | yes |
| `cpg-cuda-cpp-memory-model` | CUDA Programming Guide 5.8. CUDA C++ Memory Model | v13.5 | https://docs.nvidia.com/cuda/cuda-programming-guide/05-appendices/cuda-cpp-memory-model.html | yes |
| `cpg-cuda-graphs` | CUDA Programming Guide 4.2. CUDA Graphs | v13.5 | https://docs.nvidia.com/cuda/cuda-programming-guide/04-special-topics/cuda-graphs.html | yes |
| `cpg-device-callable-apis` | CUDA Programming Guide 5.7. Device-Callable APIs and Intrinsics | v13.5 | https://docs.nvidia.com/cuda/cuda-programming-guide/05-appendices/device-callable-apis.html | yes |
| `cpg-green-contexts` | CUDA Programming Guide 4.7. Green Contexts | v13.5 | https://docs.nvidia.com/cuda/cuda-programming-guide/04-special-topics/green-contexts.html | yes |
| `cpg-legacy-12.9` | CUDA C++ Programming Guide (archived), single page | v12.9 | https://docs.nvidia.com/cuda/archive/12.9.0/cuda-c-programming-guide/index.html | no |
| `cpg-memory-sync-domains` | CUDA Programming Guide 4.16. Memory Synchronization Domains | v13.5 | https://docs.nvidia.com/cuda/cuda-programming-guide/04-special-topics/memory-sync-domains.html | yes |
| `cpg-understanding-memory` | CUDA Programming Guide 2.6. Unified and System Memory | v13.5 | https://docs.nvidia.com/cuda/cuda-programming-guide/02-basics/understanding-memory.html | yes |
| `cpg-writing-cuda-kernels` | CUDA Programming Guide 2.3. Writing SIMT Kernels | v13.5 | https://docs.nvidia.com/cuda/cuda-programming-guide/02-basics/writing-cuda-kernels.html | yes |
| `cuda-for-tegra` | CUDA for Tegra (application note) | v13.5 | https://docs.nvidia.com/cuda/cuda-for-tegra-appnote/index.html | yes |
| `cxx-draft-atomics-lockfree` | C++ working draft, [atomics.lockfree] | - | https://eel.is/c++draft/atomics.lockfree | no |
| `cxx-draft-atomics-fences` | C++ working draft, [atomics.fences] | - | https://eel.is/c++draft/atomics.fences | no |
| `cxx-draft-atomics-order` | C++ working draft, [atomics.order] | - | https://eel.is/c++draft/atomics.order | no |
| `cxx-draft-intro-races` | C++ working draft, [intro.races] | - | https://eel.is/c++draft/intro.races | no |
| `drv-api-sync` | CUDA Driver API 2. API Synchronization behavior | v13.5 | https://docs.nvidia.com/cuda/cuda-driver-api/api-sync-behavior.html | yes |
| `drv-mem` | CUDA Driver API 6.27. Memory Management | v13.5 | https://docs.nvidia.com/cuda/cuda-driver-api/cuda_driver_api/group__CUDA__MEM.html | yes |
| `drv-memop` | CUDA Driver API 6.38. Stream Memory Operations | v13.5 | https://docs.nvidia.com/cuda/cuda-driver-api/cuda_driver_api/group__CUDA__MEMOP.html | yes |
| `drv-stream` | CUDA Driver API 6.37. Stream Management | v13.5 | https://docs.nvidia.com/cuda/cuda-driver-api/cuda_driver_api/group__CUDA__STREAM.html | yes |
| `drv-types` | CUDA Driver API 6.5. Data types used by CUDA driver | v13.5 | https://docs.nvidia.com/cuda/cuda-driver-api/cuda_driver_api/group__CUDA__TYPES.html | yes |
| `drv-va` | CUDA Driver API 6.48. Virtual Memory Management | v13.5 | https://docs.nvidia.com/cuda/cuda-driver-api/cuda_driver_api/group__CUDA__VA.html | yes |
| `ptx-isa` | PTX ISA (Parallel Thread Execution), single page | PTX ISA 9.5 documentation | https://docs.nvidia.com/cuda/parallel-thread-execution/index.html | yes |

## Summary table

Classes: **D** = DOCUMENTED, **P** = PARTLY DOCUMENTED, **NF** = NOT FOUND, **NF-c** = NOT FOUND, contradicted. Section numbers are those of the page named in brackets.

| # | claim | class | key quote | source | the design may rely on | run-time check or fallback needed |
|---|---|---|---|---|---|---|
| 1 | `%warpid` is an SM-wide slot; (SM, slot) is unique among resident warps | **NF-c** (a strictly weaker property is stated, not the opposite) | [ptx-isa §10.4] "unique warp number within a CTA but not across CTAs within a grid" | PTX §10.4 | nothing for correctness; (SM, slot) only as a hint that spreads writers | exclusivity from a claim (an atomic owner word, CAS 0 -> tag), with a failed claim counted. A tag compared at exit is not enough (claim 1). |
| 2 | `%smid`/`%warpid` do not change while a warp runs | **NF-c** | [ptx-isa §10.8] "its value may change during execution, e.g. due to rescheduling of threads following preemption" | PTX §10.4, §10.8; CPG §5.7.4.3 | nothing | carry the slot index from entry to exit (C1's carry); never re-derive storage from the exit's ids |
| 3 | `%gridid` is unique per launch except graph replays | **P** | [ptx-isa §10.10] "This variable provides the temporal grid launch number for this context." | PTX §10.10 | a diagnostic field in records | never a correctness key; launch identity, if needed, from a counter the design owns |
| 4 | one leader fence after `__syncwarp` publishes the other lanes' stores to the CPU | **P** (**D** for the CUDA C++ forms with a libcu++ fence or a release store, read in the same process) | [cpg-cpp-language-extensions §5.4.4.2] "the call to __syncwarp(mask) strongly happens before" | CPG §5.4.4.2, §5.4.4.3, §5.8.3-5.8.4; PTX §8.5, §8.9.4, §9.7.15.2 | yes, if it is written in CUDA C++ (`__syncwarp`, then the leader's libcu++ system-scope fence and atomic store, or its system-scope release store) **with a valid mask** (claim 9); it compiles to step 0's protocol A or D1 (`lowering/`); `__threadfence_system()` instead of the libcu++ fence, and hand-written PTX, rest only on prose; with arbitrary lane groups, a per-lane release instead | start-up: compute capability >= 7.0; a reader in another process is outside the text (claim 8) |
| 5 | flags first, then payload, as two copies on one stream: the payload is at least as new as the flags | **NF** for copy-vs-kernel order, **D** for copy-after-copy | [cpg-cpp-language-extensions §5.4.4.3] "Reading from or writing to the same memory location without memory fences or synchronization results in undefined behavior." | CPG §2.5.5, §5.4.4.3; PTX §8.7, §8.9.4; Driver API 6.38 | nothing while the writer runs; a copy ordered after the writer by a stream or event edge | no fence scope makes it sound; default to registered memory for CPU-read data; keep copy-out only as a performance path with per-record validation and counted rejects |
| 6 | registers and local memory survive preemption, so a pointer kept there survives migration | **P** | [cpg-writing-cuda-kernels §2.3.3.4] "Local memory is used for thread local storage during the execution of a kernel." | CPG §2.3.3, §2.3.3.4; PTX §5.1.5, §6.4.1.1; Driver API 6.5 (the preemption attribute only); what preemption does only in `cpg-legacy-12.9` §4.4 | thread state, as every CUDA program does; C1 recomputes the carry's address before every probe call, so no stored pointer is needed | none needed beyond C1's own tag; preemption is never named |
| 7 | atomics on registered memory are correct GPU-GPU and GPU-CPU | **D** with conditions, split by scope | [cpg-cuda-cpp-memory-model §5.8.3] "it affects an object in mapped memory and hostNativeAtomicSupported is 1" | CPG §5.8.1.1, §5.8.3, §5.8.4; PTX §8.10.3, §9.7.15.5; Tegra §4.7 | GPU-GPU at `.gpu` scope; GPU-CPU only at `.sys` scope with `hostNativeAtomicSupported` = 1 | start-up: check that attribute (1 on the Orin) and the memory sync domain count; today's eBPF fetch-add is `.gpu` and races, in the model's terms, with any CPU access during the run; CPU reads during the run must be atomic loads (`std::atomic_ref`), not plain loads |
| 8 | kernel completion makes a kernel's writes visible to the host after stream synchronization | **D** for the synchronizing thread; **P** for bpftime's reader process | [ptx-isa §8.9.4] "Completion of the last task queued to a stream, if any, synchronizes with return from cudaStreamSynchronize." | PTX §8.5, §8.9.4; CPG §5.8.1.1 | yes, in the thread that synchronizes (or whose query returns success) | the reader is another process, not one of the program threads that CPG §5.8.1.1 relates: let an in-process thread acquire, then hand over to the reader through a CPU release; that hop rests on the CPU's memory model, not on a fetched text |
| 9 | `__activemask`/`__shfl_sync`/`__syncwarp` are correct with partial masks after divergent early returns | **NF** for an `__activemask()` mask (C2 measured disagreeing masks under churn); **D** for a mask known by construction | [cpg-cpp-language-extensions §5.4.6.6] "All non-exited threads specified in the mask must execute the intrinsic with the same mask value." | CPG §3.2.2.1.1, §3.2.2.2, §5.4.6.1, §5.4.6.5, §5.4.6.6, §5.7.3.1.5; `cpg-legacy-12.9` §9.5.2.2.1; C2 README | warp collectives only with a constructed mask where the whole warp reaches the call (runtime claim at kernel entry); `cg::coalesced_threads()` has the same gap | none can repair a mask mismatch; use a per-lane path in exit probes and `__memcapture`; records per call are a workload property |
| 10 | in global capture mode another thread's async copy invalidates the capture; in relaxed mode it does not | **P** | [drv-stream §6.37 cuThreadExchangeStreamCaptureMode] "this thread is prohibited from potentially unsafe API calls" | Driver API 6.37 (`cuThreadExchangeStreamCaptureMode`); CPG §4.2.2.1.2.2-3 | the per-thread mode rules; never synchronize the context or use the legacy stream | the list of "potentially unsafe" calls is not published; the documented remedy (the copier thread switches itself to RELAXED) is prepared as `capture-remedy/`, and two smokes agree; main-session runs pending |

## The ten claims

### 1. "`%warpid` is an SM-wide hardware slot; (SM, slot) is unique among resident warps" — NOT FOUND, contradicted

The ISA promises less, and says what the register is for:

> [ptx-isa §10.4] "The warp identifier provides a unique warp number within a CTA but not across CTAs within a grid. The warp identifier will be the same for all threads within a single warp."
> [ptx-isa §10.4] "%warpid is intended mainly to enable profiling and diagnostic code to sample and log information such as work place mapping and load distribution."
> [ptx-isa §10.5] "A predefined, read-only special register that returns the maximum number of warp identifiers."
> [ptx-isa §10.8] "The SM identifier ranges from 0 to %nsmid-1. The SM identifier numbering is not guaranteed to be contiguous."
> [ptx-isa §10.9] "The SM identifier numbering is not guaranteed to be contiguous, so %nsmid may be larger than the physical number of SMs in the device."

- **Missing:** uniqueness across CTAs on one SM, and any relation between `%nwarpid` and the number of resident warps per SM. Searched: all pages, for `warpid`, `nwarpid`, `smid`. Only PTX §10.x, CPG §5.7.4.3 (claim 2) and the archived v12.9 guide mention them.
- **Why "contradicted".** The rule above counts a strictly weaker stated property as contradicting. The text promises uniqueness within a CTA only. It does not say that two CTAs on one SM ever share a value, so it is weaker than the claim, not its opposite. The design consequence is the same either way.
- **What is documented:**
  - `%smid` < `%nsmid`. Sizing an array by `%nsmid` x `%nwarpid` x 32 is safe as long as a larger `%warpid` is bounds-checked; `%warpid` < `%nwarpid` is implied by "maximum number of warp identifiers" but never stated as a range. Step 0 found that ptxas emits `%nwarpid` as an immediate (48 for sm_87), so it is a per-target constant of the compiled code, not a value read from the running GPU; `%nsmid` became a load from a constant bank that the driver writes per launch (`step0/e1-ids/README.md`, SASS table).
  - The lane is documented: [ptx-isa §10.3] "A predefined, read-only special register that returns the thread’s lane within the warp. The lane identifier ranges from zero to WARP_SZ-1."
  - So is how a block is cut into warps: [cpg-advanced-kernel-programming §3.2.2.2] "The way a block is partitioned into warps is always the same; each warp contains threads of consecutive, increasing thread IDs with the first warp containing thread 0."
- **Design:**
  - Exclusivity of a storage slot must come from an atomic claim. Example: at entry, CAS the slot's owner word from 0 to the thread's tag (acquire); at exit, release-store 0; a failed CAS is a counted miss (NULL or a spill path).
  - Exclusivity then follows from RMW atomicity, which is documented (PTX §8.10.3, quoted in claim 7). The hardware slot only makes collisions rare: step 0 measured none, which sets the expected miss rate, a performance parameter.
  - C2 tests a pool-based form of the same idea. C3 (b) costs a claim-based form.
  - **A claim whose thread never reaches an exit probe stays taken.**
    - The kretprobe pass puts an exit probe before every `ret`/`exit` (design note, section 2), and a kernel that traps loses its context. So a leaked claim means a pass bug or an exit form the pass misses.
    - Count claims and releases per slot as a diagnostic.
    - Taking a slot back is safe only after a documented edge proves that no thread of the owner's launch still runs, and the agent has none in general (claim 8). A leaked slot therefore stays a counted, permanent miss: it costs capacity, not correctness.
- **Why a tag checked at exit is not enough** (C1's `MT_SLOTTAG` fallback, D's scratch): if two resident threads share a slot, then
  - their plain stores to the value race, which is undefined by CPG §5.4.4.3 (claim 5);
  - an interleaving "A tags, B tags, B writes, A writes through the pointer it already holds, B checks" passes B's check with A's value;
  - an owner tag without a launch id cannot tell two launches with the same linear thread id apart (claim 3).

  The tag catches only some sharing, so it is a detector, not a guarantee.

### 2. "`%smid`/`%warpid` do not change while a warp runs" — NOT FOUND, contradicted

> [ptx-isa §10.4] "Note that %warpid returns the location of a thread at the moment when read, but its value may change during execution, e.g., due to rescheduling of threads following preemption. For this reason, %ctaid and %tid should be used to compute a virtual warp index if such a value is needed in kernel code"
> [ptx-isa §10.8] "Note that %smid returns the location of a thread at the moment when read, but its value may change during execution, e.g. due to rescheduling of threads following preemption."
> [cpg-device-callable-apis §5.7.4.3] "Note that in PTX %smid and %warpid are defined as volatile values. The device runtime may reschedule thread blocks onto different SMs in order to more efficiently manage resources. As such, it is unsafe to rely upon %smid or %warpid remaining unchanged across the lifetime of a thread or thread block."

- **Missing:** nothing. The documents state the opposite of the claim. (The CPG note sits in the device-runtime chapter, but its last sentence is general.)
- **Step 0's 0 changes under time-slicing** only describe this board, with this driver, under this one kind of preemption.
- **Design:**
  - Compute the slot index once, at entry, and carry it to exit in the thread's own state: a register or C1's kernel-scope `.local`.
  - With the claim of claim 1, the exit's ids no longer matter: the slot is named by the carried index and owned through the claim.
  - Re-reading ids at exit and comparing them is allowed only as a counted diagnostic.

### 3. "`%gridid` is unique per launch except graph replays" — PARTLY DOCUMENTED

> [ptx-isa §10.10] "A predefined, read-only special register initialized with the per-grid temporal grid identifier. The %gridid is used by debuggers to distinguish CTAs and clusters within concurrent (small) grids."
> [ptx-isa §10.10] "During execution, repeated launches of programs may occur, where each launch starts a grid-of-CTAs. This variable provides the temporal grid launch number for this context."
> [ptx-isa §10.33] "A predefined, read-only special register initialized with the identifier referring to the CUDA device graph being currently executed. This register is 0 if the executing kernel is not part of a CUDA device graph."

- **Documented:** a per-context "temporal grid launch number", meant for debuggers.
- **Missing:**
  - uniqueness;
  - monotonicity;
  - the 64-bit range in use (the text gives the range per target, without saying it is never reused);
  - what graph replays get.

  Searched: all pages, for `gridid`, `graph exec`, `replay`. `%current_graph_exec` identifies *device* graphs, meaning graphs launched from the device. Nothing says what it holds for host-launched graph replays, and step 0 did not read it.
- **Design:** `%gridid` may label records for people to read. It must not be part of an owner tag, a region key or any other correctness key. A design that needs a launch id must make one itself, and the claim-based ownership of claim 1 needs none.

### 4. "One leader fence after `__syncwarp` publishes the other lanes' stores to the CPU" — PARTLY DOCUMENTED (DOCUMENTED for code written in CUDA C++ with a libcu++ fence or a release store, and read in the same process)

**In the PTX model (formal), the chain is not there:**

> [ptx-isa §8.5] "Note that the warp is not a scope; the CTA is the smallest collection of threads that qualifies as a scope in the memory consistency model."
> [ptx-isa §8.9.4] "A bar{.cta}.sync or bar{.cta}.red or bar{.cta}.arrive operation synchronizes with a bar{.cta}.sync or bar{.cta}.red operation executed on the same barrier."
> [ptx-isa §9.7.15.2] "bar.warp.sync also guarantee memory ordering among threads participating in barrier. Thus, threads within warp that wish to communicate via memory can store to memory, execute bar.warp.sync, and then safely read values stored by other threads in warp."
> [ptx-isa §9.7.15.4] "The membar instruction guarantees that prior memory accesses requested by this thread (ld, st, atom and red instructions) are performed at the specified level, before later memory operations requested by this thread following the membar instruction."

- `bar.warp.sync` is not among the synchronizes-with edges of §8.9.4.
- Its guarantee is prose about lanes of the warp reading each other's stores. It does not say that a fence in one lane then covers them for a third party.
- A fence covers "this thread". The other lanes' stores reach the CPU only through causality order, which needs a synchronizes-with edge from them to the leader.

**In the CUDA C++ memory model, the chain exists:**

> [cpg-cpp-language-extensions §5.4.4.2] "Calling __syncwarp(mask) provides memory ordering among the participating threads within a warp named in mask: the call to __syncwarp(mask) strongly happens before (see C++ specification [intro.races]) any warp thread named in mask is unblocked from the wait or exits."
> [cpg-cuda-cpp-memory-model §5.8] "CUDA C++ extends the standard C++ memory model and concurrency facilities in the cuda:: namespace with thread scopes, retaining the syntax and semantics of standard C++ by default."
> [cpg-cuda-cpp-memory-model §5.8.4] "A release fence A synchronizes with an atomic operation B that performs an acquire operation on an atomic object M if there exists an atomic operation X such that A is sequenced before X, X modifies M, and B reads the value written by X or a value written by any side effect in the hypothetical release sequence X would head if it were a release operation, and each operation (A, B, and X) specifies a scope that includes the thread that performed each other operation."
> [cpg-cuda-cpp-memory-model §5.8.3] "it is a load or store that affects a naturally-aligned object of sizes 1, 2, 4, 8, or 16 bytes on mapped memory"
> [cpg-cuda-cpp-memory-model §5.8.2] "Types in namespaces std:: and cuda::std:: have the same behavior as corresponding types in namespace cuda:: when instantiated with a scope of cuda::thread_scope_system."

The C++ clauses that the chain uses (working draft; the review fetched them so that these steps are quoted, not recalled):

> [cxx-draft-atomics-fences §atomics.fences p1] "Fences can have acquire semantics, release semantics, or both." "A fence with release semantics is called a release fence."
> [cxx-draft-atomics-fences §atomics.fences p5] "is a sequentially consistent acquire and release fence" (the case `order == memory_order::seq_cst` of `atomic_thread_fence`)
> [cxx-draft-intro-races §intro.races p7] "An evaluation A happens before an evaluation B (or, equivalently, B happens after A) if either" "A is sequenced before B, or" "A synchronizes with B, or" "A happens before X and X happens before B."
> [cxx-draft-intro-races §intro.races p8] "An evaluation A strongly happens before an evaluation D if, either" "A is sequenced before D, or" "A synchronizes with D, and both A and D are sequentially consistent atomic operations ([atomics.order]), or" "there are evaluations B and C such that A is sequenced before B, B happens before C, and C is sequenced before D, or" "there is an evaluation B such that A strongly happens before B, and B strongly happens before D."
> [cxx-draft-intro-races §intro.races p9] "A happens before B and" "there is no other side effect X on M such that A happens before X and X happens before B."

Each of the four cases of p8 is a case of p7 (sequenced before; synchronizes with) or a chain of them (the third case by p7's transitivity; the fourth by induction), so "strongly happens before" implies "happens before".

The chain for lane i's payload store W_i, step by step:
1. W_i is sequenced before lane i's `__syncwarp`.
2. That call strongly happens before the leader is unblocked (§5.4.4.2), hence happens before it (p7, p8).
3. The leader then executes a system-scope release fence: `cuda::atomic_thread_fence(memory_order_seq_cst, thread_scope_system)`, a release fence by p5 and p1, carried over to CUDA C++ by §5.8.
4. Then it stores the flag through a system-scope `cuda::atomic_ref`. An 8-B atomic store to mapped memory is atomic at system scope with no attribute needed (§5.8.3).
5. The CPU acquire-loads the flag through `std::atomic_ref` (system scope, §5.8.2). By §5.8.4, the fence synchronizes with that load.

By p7, W_i happens before the CPU's later payload read, and by p9 the read sees W_i (or a later write to the same location).

**`__threadfence_system()` is not the same step.** The guide shows it next to the libcu++ fence as its "Intrinsics" form with the same "ensures that" text, but it never calls it a C++ release fence, so §5.8.4 does not apply to it by name. Its own text covers only the calling thread's writes (item (d) below). The guide itself points to the libcu++ form: [cpg-cpp-language-extensions §5.4.4.3] "It is suggested to use cuda::atomic_thread_fence provided by libcu++ whenever possible for safety and portability reasons." So only two forms are DOCUMENTED: the libcu++ fence followed by an atomic store, and a release store. The `__threadfence_system()` form is PARTLY (it compiles to the same SASS, `lowering/`).

**Missing:**
- **(a) The C++ guarantee covers C++ code.**
  - bpftime's helpers are CUDA C++ (`default_trampoline.cu`), so the helper must be written with these C++ constructs (`__syncwarp`, a libcu++ fence or `atomic_ref`). Inline PTX would leave it resting on §9.7.15.2's prose again.
  - Step 0 used inline `membar.sys; st.relaxed.sys` (e4), and C4 uses inline PTX throughout.
  - Which PTX nvcc emits for the C++ form is the compiler's business, and the guarantee stays with the C++ source.
- **(b) Whether `fence.sc` is a release fence in PTX terms.** PTX §8.8 lists "a release or acquire-release memory fence"; `fence.sc` is neither by name, and PTX's list of operation kinds names "A membar, fence.sc or fence.acq_rel instruction" only as a memory fence. In C++, a `seq_cst` fence is "a sequentially consistent acquire and release fence" ([atomics.fences] p5, quoted above), and CPG §5.8 keeps "the syntax and semantics of standard C++ by default". So the release reading holds for the C++ source and not for hand-written `fence.sc`.

  > [ptx-isa §9.7.15.4] "fence.sc is a slower fence that can restore sequential consistency when used in sufficient places, at the cost of performance."
  > [ptx-isa §9.7.15.4] "On sm_70 and higher membar is a synonym for fence.sc^{1}, and the membar levels cta, gl and sys are synonymous with the fence scopes cta, gpu and sys respectively."

- **(c)** The CPU reader must be in the same program (claim 8).
- **(d)** The `__threadfence_system()` text itself covers only the calling thread's writes: [cpg-cpp-language-extensions §5.4.4.3] "All writes to all memory made by the calling thread before the call to __threadfence_system() are observed by all threads in the device, host threads, and all threads in peer devices as occurring before all writes to all memory made by the calling thread after the call to __threadfence_system()." The other lanes are covered only through `__syncwarp`'s happens-before.

**Design:**
- One leader per warp: `__syncwarp(mask)`, then a system-scope release by the leader (the libcu++ fence and an atomic store, or a release store). This is allowed, provided it is written in CUDA C++ **and the mask is valid**.
  - A valid mask is one that every named lane holds and reaches (claim 9): the full or constructed mask where the whole warp calls at one program point.
  - With an `__activemask()` mask, the chain inherits claim 9's gap. In exit probes and `__memcapture`, where lanes arrive in any grouping, every lane therefore publishes its own record with its own release.
  - That per-lane form needs no warp collective. Step 0 found that a converged warp's per-lane fences cost about what one fence per warp costs (check 2; check 4's A and B ran at the same rate).
- The fence-free alternative, a release store (`atomic_ref<..., thread_scope_system>::store(v, memory_order_release)`), is equally documented. Steps 3-5 become one standard C++ edge, which CPG §5.8 keeps unmodified: [cxx-draft-atomics-order §atomics.order p1] "a store operation performs a release operation on the affected memory location." [cxx-draft-atomics-order §atomics.order p2] "An atomic operation A that performs a release operation on an atomic object M synchronizes with an atomic operation B that performs an acquire operation on M and takes its value from any side effect in the release sequence headed by A."
- **What the documented forms compile to** (`lowering/`, compile only, CUDA 13.2, sm_87):

  | C++ form | PTX | SASS | equals |
  |---|---|---|---|
  | `__syncwarp()`; `cuda::atomic_thread_fence(seq_cst, system)`; relaxed system `atomic_ref` store | `bar.warp.sync -1; fence.sc.sys; st.relaxed.sys` | `NOP; MEMBAR.SC.SYS; ERRBAR; CCTL.IVALL; ST.E.64.STRONG.SYS` | e4 protocol A |
  | `__syncwarp()`; `__threadfence_system()`; same store (PARTLY: see above) | `bar.warp.sync -1; membar.sys; st.relaxed.sys` | same | e4 protocol A |
  | `__syncwarp()`; system `atomic_ref` release store | `bar.warp.sync -1; st.release.sys` | `NOP; MEMBAR.ALL.SYS; ERRBAR; ST.E.64.STRONG.SYS` | e4 D1 |

  Step 0's and C4's costs therefore apply to the documented forms. The full-mask `__syncwarp()` becomes a `NOP` in SASS: the compiler meets the C++ guarantee without a barrier instruction. That is one more reason the guarantee belongs to the C++ source and does not transfer to hand-written PTX.
- Step 0's "safe in 10^9 rounds" is not evidence for the ordering, because the fence's duration hides it (e4).

### 5. "Flags first and payload second, in two copies on one stream, guarantees the payload is at least as new as the flags" — NOT FOUND (for what matters); the copy-after-copy part is DOCUMENTED

**Documented: the second copy starts after the first completes.**

> [cpg-asynchronous-execution §2.5.5] "Most importantly CUDA streams are what are known as in-order streams. This means that the order of execution of the operations in a stream is the same as the order in which those operations were enqueued. An operation in a stream cannot leap-frog other operations. Memory operations (such as copies) are tracked by the runtime and will always complete before the next operation in order to allow dependent kernels safe access to the data being transferred."
> [ptx-isa §8.9.4] "Completion of a task enqueued in a CUDA stream synchronizes with the start of the following task in the same stream, if any."

**Not documented: anything that relates a copy's reads to a kernel that writes the same memory at the same time on another stream.** The API edges the model defines are stream order, events, cross-stream barriers of the legacy stream, kernel start and end, graph node order, the start of the enqueueing API call, synchronization calls, and successful queries:

> [ptx-isa §8.9.4] "An event record task synchronizes with matching event wait tasks, and a barrier arrival task synchronizes with matching barrier wait tasks."
> [ptx-isa §8.9.4] "Start of a CUDA kernel synchronizes with start of all threads in the kernel. End of all threads in a kernel synchronize with end of the kernel."
> [ptx-isa §8.9.4] "Start of a CUDA API call to enqueue a task synchronizes with start of the task."
> [ptx-isa §8.9.4] "Returning cudaSuccess from an API to query a CUDA handle, such as a stream or event, behaves the same as return from the matching synchronization API."

None of these edges runs from a store of a kernel that is still running to a copy task. A copy is not a thread of the model, and the model does not say what its reads are. It performs no strong operation, has no scope and forms no acquire pattern. If its reads are taken as plain (weak) reads, which is the only reading the definitions offer, they and the writer's stores are a data race:

> [ptx-isa §8.7] "The operations are related in program order (i.e, they are both executed by the same thread), or each operation is strong and specifies a scope that includes the thread executing the other operation."
> [ptx-isa §8.7.1] "Two conflicting memory operations are said to be in a data-race if they are not related in causality order and they are not morally strong."
> [cpg-cpp-language-extensions §5.4.4.3] "Reading from or writing to the same memory location without memory fences or synchronization results in undefined behavior."

Searched: all 62 pages, for `copy engine`, `DMA`, `while a kernel`, `memcpy` with `concurrent`, `copy` with `data race`, `memory model` with `memcpy`, and `AccessOrder`. The hits concern peer copies, unified-memory rules, staging, `cooperative_groups::memcpy_async` and the batched-copy source order. `cooperative_groups::memcpy_async` is a device-side copy "as-if performed by another thread", not the copy engine. None relates a copy-engine read to concurrent kernel stores. The closest texts:

- The copy's own reference page says nothing about memory order: [drv-mem §6.27 cuMemcpyDtoHAsync] "Copies from device to host memory. dstHost and srcDevice specify the base pointers of the destination and source, respectively. ByteCount specifies the number of bytes to copy."
- The driver's synchronization page defers the question to the guide, which does not answer it: [drv-api-sync §Kernel Launches] "Details of concurrent kernel execution and data transfers can be found in the CUDA Programmers Guide."
- The only documented source-access orders for a copy are stream order or the duration of the API call: [drv-types §6.5 CUmemcpySrcAccessOrder] "These flags allow applications to convey the source access ordering CUDA must maintain." "Indicates that access to the source pointer must be in stream order." These relate the copy to its own stream, not to a kernel on another stream.
- **On Tegra the copy mechanism itself is the driver's choice:** [cpg-advanced-host-programming §3.1.5] "On Tegra platforms one has the choice of using SMs or Copy Engines (CEs)" ... "The choice of which is currently specified by a heuristic in the driver." So step 0's 0 violations describe whichever mechanism the heuristic picked for those copies on this board and driver.

**Which fence scope would make it sound? None.**
- A fence in the writer, `__threadfence()` (gpu) or `membar.sys` (sys), helps only when it is half of a release/acquire pair, or of a Fence-SC pair, with an operation on the other side.
- The copy has no operation that can be that other half.
- The gpu-scope fence of step 0's writer is no weaker *for this purpose* than a system-scope one: neither reaches the copy.

The other documented way for a stream to wait on memory does not give it either:

> [drv-memop §6.38] "Synchronization ordering established through these APIs is not visible to CUDA."
> [drv-types §6.5 CU_STREAM_WAIT_VALUE_FLUSH] "This means that, if a remote write operation is guaranteed to have reached the device before the wait can be satisfied, that write is guaranteed to be visible to downstream device work. The device is permitted to reorder remote writes internally."
> [drv-types §6.5 CU_STREAM_WAIT_VALUE_FLUSH] "Support for this operation is restricted to selected platforms and can be queried with CU_DEVICE_ATTRIBUTE_CAN_FLUSH_REMOTE_WRITES."

That flush concerns remote writes, not a local kernel's. The Orin reports `CAN_FLUSH_REMOTE_WRITES` = 0 (`/home/jetson/src/results/gpu/feasibility-20261109/gpu-env/attrs.out.txt`, `cudaDevAttrCanFlushRemoteWrites`).

**Documented alternatives:**
- **(a) Write CPU-read data straight into registered memory.** Publish it with the protocol of claim 4 or C4. Step 0 found plain stores cost the same in both memories on the Orin, so this is the default.
- **(b) An API edge from the writer to the copy.** For example, an event recorded after the writer's kernel and waited on by the copier's stream, or a copy enqueued after the kernel in its own stream. This is documented, but it needs a host step after each launch, and graph replays get none. It is not concurrent with the writer.
- **(c) A copier kernel instead of the copy engine.** Its threads acquire each chunk's flag (`ld.acquire` at gpu scope pairs with the writer's gpu-scope release), copy the payload, and publish to the CPU with a system-scope release. Every link is in the model. Whether the copier kernel runs concurrently with the application is not guaranteed: [cpg-asynchronous-execution §2.5.1] "While the programming interfaces are asynchronous, the actual ability to carry out various operations concurrently will depend on the version of CUDA and the compute capability of the hardware being used". Latency is therefore not bounded.
- **(d) Keep the measured two-step copy only as a performance path, checked at run time.**
  - Each record carries a sequence number in its first and last word, plus a checksum written by the same thread.
  - The reader accepts a record only if the two sequence words match each other, the checksum matches, **and the sequence equals the value the flag copy promised** for that slot.
    - The first two catch torn records.
    - The third catches a record that is whole but stale, which was step 0's failure mode (an old payload under a new flag).
  - The reader counts every reject.
  - Above a reject threshold it falls back to (a).
  - This does not make the copy ordered. It makes the result checked, which is what the user's rule allows for measured-only behaviour.

### 6. "Registers and local memory are saved and restored with a thread on preemption, so a pointer kept there survives migration" — PARTLY DOCUMENTED

**Documented: compute preemption exists, and ids may change after it** (the latter in claim 2). The current documents name it only as a device attribute; what it does is described only in the archived guide:

> [drv-types §6.5 CU_DEVICE_ATTRIBUTE_COMPUTE_PREEMPTION_SUPPORTED] "Device supports compute preemption."
> [cpg-legacy-12.9 §4.4] "This allows compute tasks to be preempted at instruction-level granularity, rather than thread block granularity as in prior Maxwell and Kepler GPU architecture, with the benefit that applications with long-running kernels can be prevented from either monopolizing the system or timing out."

**Documented: local memory is per-thread storage for the whole kernel, with no exception named, and it is addressable.**

> [ptx-isa §5.1.5] "The local state space (.local) is private memory for each thread to keep its own data."
> [ptx-isa §5.1.5] "When compiling to use the Application Binary Interface (ABI), .local state-space variables must be declared within function scope and are allocated on the stack."
> [cpg-writing-cuda-kernels §2.3.3.4] "Local memory is used for thread local storage during the execution of a kernel."
> [ptx-isa §6.4.1.1] "The state spaces .const, Kernel Function Parameters (.param), .local and .shared are modeled as windows within the generic address space."
> [cpg-advanced-kernel-programming §3.2.2.1.1] "With independent thread scheduling, the GPU maintains execution state per thread, including a program counter and call stack, and can yield execution at a per-thread granularity"

(CPG §2.3.3, Table 1, also gives local memory scope "Thread" and lifetime "Kernel".)

**Missing:** no sentence says that registers or local memory are saved and restored on preemption, or that a generic address of a `.local` variable stays valid when a thread is rescheduled. Searched: all pages, for `preempt`, `save`, `restore`, `context switch`, `time slic`. The current guide mentions preemption only for stream priorities and cluster launch control.

**Reading:**
- The ISA and the guide define a thread's local variables and registers as lasting for the kernel, and the only documented visible effect of rescheduling is on `%smid`/`%warpid`.
- A preemption that lost them would break every CUDA program that spills registers or keeps an array on its stack.
- So the design may treat carried state like any automatic variable. It relies on nothing more than any CUDA program does.

**The pointer part does not arise in C1.**
- C1's routing recomputes the carry's generic address with `cvta.local` right before every probe call (`c1lib.py`, `route_kernel`: `cvta.local.u64 %c1_carry, __c1_carry;` emitted before each call).
- A generic address is therefore held only within one probe call, like any pointer to a stack variable in C++.

**Run-time check:** none beyond what C1 already has. C1's carry is validated by its own checker, and preemption is exercised only as time-slicing (C1 README, limits).

### 7. "Atomics on registered host memory are correct between GPU threads, and between GPU and CPU" — DOCUMENTED with conditions, split by scope

> [cpg-cuda-cpp-memory-model §5.8.3] "An atomic operation is atomic at the scope it specifies if:" "it specifies a scope other than cuda::thread_scope_system, or" "it affects an object in mapped memory and hostNativeAtomicSupported is 1, or" "it is a load or store that affects a naturally-aligned object of sizes 1, 2, 4, 8, or 16 bytes on mapped memory"
> [cpg-cpp-language-extensions §5.4.5.1] "Atomic APIs without a suffix, for example atomicAdd, are atomic at scope cuda::thread_scope_device." "Atomic APIs with the _system suffix, for example, atomicAdd_system, are atomic at scope cuda::thread_scope_system if they meet particular conditions."
> [ptx-isa §8.1.1] "When communicating with the host CPU, certain strong operations with system scope may not be performed atomically on some systems."
> [ptx-isa §9.7.15.5] "If the .scope qualifier is absent, .gpu scope is assumed by default."
> [ptx-isa §8.10.3] "Atomicity is guaranteed when the operations are morally strong." "Atomicity is not guaranteed if the operations are not morally strong."
> [cpg-cuda-cpp-memory-model §5.8.4] "The execution of a program contains a data race if it contains two potentially concurrent conflicting actions, at least one of which is not atomic at a scope that includes the thread that performed the other operation, and neither happens before the other"
> [cpg-understanding-memory §2.6.3.1.3] "Mapped memory makes CPU memory accessible from the GPU, but does not guarantee that all types of access, for example atomics, are supported on all systems."
> [cuda-for-tegra §4.7] "System wide atomics are not supported on Tegra devices with compute capability less than 7.2."

- **GPU-GPU, `.gpu` scope (atomicAdd, eBPF fetch-add):** DOCUMENTED for any memory, registered memory included, by §5.8.3's first case.
  - The §2.6.3.1.3 sentence quoted above names no scope, and it says mapped memory does not guarantee atomics "on all systems". Read strictly, it puts device-scope atomics on mapped memory in doubt on some other system. The review's conservative reading: rely on GPU atomics in registered memory only where `hostNativeAtomicSupported` = 1 (true on the Orin), which the start-up check already requires for GPU-CPU atomics. Device memory is not affected.
  - On GPUs with memory sync domains, device scope is limited to one domain: [cpg-cuda-cpp-memory-model §5.8.1.1] "Each GPU thread is related to each other GPU thread in the same CUDA device and within the same memory synchronization domain by the device thread scope: cuda::thread_scope_device." (See C4 for what that means.)
- **GPU-CPU read-modify-write:** DOCUMENTED only at `.sys` scope (`atomicAdd_system`, `atom.sys`) and only with `hostNativeAtomicSupported` = 1.
  - The Orin reports 1 (`/home/jetson/src/results/gpu/feasibility-20261109/gpu-env/attrs.out.txt`), so this becomes a start-up check. The fallback is no CPU-side RMW on shared words; the CPU then uses atomic loads and stores only.
  - **Atomic** loads and stores (system-scope `cuda::atomic_ref`, or `std::atomic_ref` by §5.8.2) of naturally aligned 1-8 B objects are atomic at system scope on mapped memory with no attribute needed. The §5.8.3 list sits under "An atomic operation is atomic at the scope it specifies if", so it says nothing about **plain** loads and stores. A plain CPU load concurrent with a GPU store is a data race by §5.8.4, whatever its size. 16-B objects carry a footnote: [cpg-cuda-cpp-memory-model §5.8.3] "If hostNativeAtomicSupported is 0, atomic load or store operations at system scope that affect a naturally-aligned 16-byte wide object in system allocated memory or mapped memory require system support."
- **What this means for today's code:**
  - The eBPF fetch-add lowers to `atom.add.u64` with no scope, which is `.gpu` (step 0: `ATOM.E.ADD.64.STRONG.GPU`).
  - Its scope does not include a host thread. A CPU that reads that counter *while kernels run* is therefore in a data race by §5.8.4, and in PTX terms the two are not morally strong, so §8.10.3 gives no single-copy atomicity.
  - Reading after a documented edge (claim 8) is fine.
  - Counters the CPU reads during the run need `.sys`-scope atomics on the GPU and atomic loads on the CPU. **Their cost was measured in one form**: step 0's `atom_warp_leader_sys` (lane 0 of each warp adds to its own word with `atom.sys`, SASS `ATOMG.E.ADD.64.STRONG.SYS`) cost the same as the `.gpu` form in registered memory in both passes (anchor, R = 1: 50.55 against 50.54 ns per warp op in pass 1, 50.07 against 50.04 in pass 2; `step0/e2-cost/results/run-20261115-*/summary.csv`). Contended `.sys` atomics (many warps on one word) were not timed; step 0's per-SM atomics were `.gpu`.
- **Step 0's CPU-GPU atomics** (check 4, test 2: the CPU release-stores FREE, the GPU claims the slot with a CAS) used `atomicCAS_system` (`casfence`) and `atom.acquire.sys.cas` (`casacq`), SASS `ATOMG...STRONG.SYS` (`step0/e4-publish/README.md`). The GPU side falls under the `.sys` + attribute row (attribute 1 here). The CPU side used the GCC builtin `__atomic_store_n(..., RELEASE)` on a plain object, not a `std::` or `cuda::std::` atomic type, so that §5.8.2 covers it is a reading (the GCC builtins are not in any fetched text). A design that wants the documented form uses `std::atomic_ref` on the CPU. The `.gpu`-scope `cas` variant is not covered. Step 0 had no test of CPU read-modify-writes against GPU ones.

### 8. "Kernel completion makes a kernel's writes visible to the host after stream synchronization" — DOCUMENTED for the synchronizing thread; PARTLY for bpftime's reader

> [ptx-isa §8.9.4] "End of all threads in a kernel synchronize with end of the kernel."
> [ptx-isa §8.9.4] "Completion of the last task queued to a stream, if any, synchronizes with return from cudaStreamSynchronize. Completion of the most recently queued matching event record task, if any, synchronizes with return from cudaEventSynchronize."
> [ptx-isa §8.9.4] "Start of a CUDA graph synchronizes with start of all source nodes in the graph. Completion of all sink nodes in a CUDA graph synchronizes with completion of the graph."
> [ptx-isa §8.9.4] "Returning cudaSuccess from an API to query a CUDA handle, such as a stream or event, behaves the same as return from the matching synchronization API."

These edges compose by §8.9.5 into causality order from every store of the kernel to everything the synchronizing thread does after the call returns. That covers registered memory read directly, and device memory read through a later copy. A thread that polls an event with a query, instead of blocking, gets the same edge when the query succeeds. (The text names the runtime's `cudaSuccess`; that the driver's `CUDA_SUCCESS` from `cuEventQuery` counts the same is a reading.)

**The gap: bpftime's reader is not that thread.**
- The map reader is the loader, a separate process. It reads the shared segment through intercepted `bpf()` and never synchronizes the application's streams.
- The scope texts differ in reach:

  > [ptx-isa §8.5] "The set of all threads in the current program, including all kernel grids invoked by the host program on all compute devices, and all threads constituting the host program itself."
  > [cpg-cuda-cpp-memory-model §5.8.1.1] "Each program thread is related to each other program thread by one or more thread scope relations:"
  > [cpg-cuda-cpp-memory-model §5.8.1.1] "Each thread in the system is related to each other thread in the system by the system thread scope: cuda::thread_scope_system."

  PTX's `.sys` is "the current program". CUDA C++ says "the system", but its scope relations are introduced for each "program thread". Neither text names another process that maps the same pages without CUDA.
- The same gap applies to every CPU-read row in claims 4 and 7, and to C4: the GPU side is documented up to "host threads", and the reader is in another process.

**A way across (the CUDA part is documented, the CPU part is not):**
- An in-process agent thread is the acquiring party, and it is in the program by any reading. Up to that thread, everything is documented (claims 4, 7 and the edges above).
- It then hands over to the reader with a CPU release store and the reader's acquire load, on an atomic in the shared segment.
- That second hop is ordinary CPU shared-memory communication between processes. No fetched page guarantees it: C++ addresses it only as recommended practice, and the CPU architecture's memory model (the Arm ARM here) was not fetched. Every shared-memory IPC on Linux relies on the same thing, so it moves the remaining gap from the GPU documents to the CPU's.

  > [cxx-draft-atomics-lockfree §atomics.lockfree p5] "Operations that are lock-free should also be address-free." "This restriction enables communication by memory that is mapped into a process more than once and by memory that is shared between two processes."

- Whether the GPU's stores, which the agent saw, are then visible to the reader through the agent's release (cumulativity across the CPU hop) also rests on the CPU's model, not on a fetched text.
- The cost is one CPU thread hop, not a copy: the agent can forward an index, not the payload.
- If the reader keeps reading the GPU's flags directly, the cross-process part stays PARTLY. It may then affect correctness only through a validation with counted rejects, like 5(d).

### 9. "`__activemask`/`__shfl_sync`/`__syncwarp` are correct with partial masks after divergent early returns" — NOT FOUND for an `__activemask()` mask; DOCUMENTED for a mask known by construction

> [cpg-cpp-language-extensions §5.4.6.1] "The function returns a 32-bit integer mask representing all currently active threads in the calling warp." "Threads that have exited the program are always marked as inactive."
> [cpg-cpp-language-extensions §5.4.6.1] "__activemask() cannot be used to determine which warp lanes execute a given branch. This function is intended for opportunistic warp-level programming and only provides an instantaneous snapshot of the active threads within a warp."
> [cpg-cpp-language-extensions §5.4.6.1] "Note that threads convergent at an __activemask() call are not guaranteed to remain convergent at subsequent instructions unless those instructions are warp synchronizing intrinsics (__sync)."
> [cpg-advanced-kernel-programming §3.2.2.1.1] "The threads of a warp that are participating in the current instruction are called the active threads, whereas threads not on the current instruction are inactive (disabled)."
> [cpg-cpp-language-extensions §5.4.6.6] "Each calling thread must have its corresponding bit set in the mask." "Each non-calling thread must have its corresponding bit set to zero in the mask. Exited threads are ignored." "All non-exited threads specified in the mask must execute the intrinsic with the same mask value." "Warp threads may call the intrinsic concurrently with different mask values, provided the masks are disjoint. Such condition is valid even in divergent control flow."
> [cpg-cpp-language-extensions §5.4.6.6] "The behavior of warp __sync functions is invalid, such as kernel hang, or undefined if:" "A non-exited thread specified in the mask fails to either eventually exit or call the intrinsic at the same program point with the same mask value." "In conditional code, all conditions must evaluate identically across all non-exited threads specified in the mask."
> [cpg-cpp-language-extensions §5.4.6.5] "Threads may only read data from another thread that is actively participating in the intrinsics. If the target thread is inactive, the retrieved value is undefined." "These intrinsics do not imply a memory barrier. They do not guarantee any memory ordering."
> [cpg-legacy-12.9 §9.5.2.2.1] "Commonly developers need to work with the current active set of threads. No assumption is made about the threads that are present, and instead developers work with the threads that happen to be there." "unsigned int writemask = __activemask();" "base_offset = __shfl_sync(writemask, base_offset, elected_lane);"

- **Documented:**
  - the constraints of §5.4.6.6;
  - what an active thread is;
  - that exited threads are inactive and ignored;
  - that warp-level intrinsics with **a mask every named lane holds** are valid even in divergent code.
- **Not found: that the lanes named in one lane's `__activemask()` result obtained the same result.**
  - Every `__sync` intrinsic called with an `__activemask()` mask needs exactly that.
  - The guide calls the result "an instantaneous snapshot" for "opportunistic warp-level programming", and no text ties the snapshots of different lanes together. Searched: all pages, for `activemask`, `coalesced`, `snapshot`, `same mask`.
  - It looked like a one-step reading of the definition of active threads ("participating in the current instruction"). **C2 measured that the reading is false on this board.**
    - With a loop in the leader-only region, 39,143 (noinline) and 116,089 (inlined) of about 4.46M helper calls had a lane that was not in its own leader's mask, so lanes reached one `__shfl_sync` with masks that disagreed.
    - With a single atomic in that region, 0.
    - Source: `step0b/c2-claim/README.md`, sha256 c396a2cc, section "What development found"; smoke counts, reproducer `x_mask_*`.
  - Under this round's rule, a reading that the hardware contradicts cannot carry correctness.
- **What that means for step 0 and NVIDIA's example.**
  - Step 0's check 7 and NVIDIA's own warp-aggregated atomic both have the single-atomic shape: [cpg-legacy-12.9 §9.5.2.2.1] "unsigned int writemask = __activemask();" "base_offset = __shfl_sync(writemask, base_offset, elected_lane);". That example exists only in the archived v12.9 guide.
  - Their 0 violations hold for that code shape on this board. They are not a guarantee, and C2 found no documented property that separates the shapes.
- **The cooperative-groups form does not close the gap.**
  - The v13.5 guide documents `coalesced_threads()` per calling thread: [cpg-device-callable-apis §5.7.3.1.5] "Constructing the group handle via coalesced_threads() is opportunistic. It returns the set of active threads at that point in time, and makes no guarantee about which threads are returned (as long as they are active) or that they will stay coalesced throughout execution (they will be brought back together for the execution of a collective but can diverge again afterwards)."
  - Its collectives are defined on the group: [cpg-device-callable-apis §5.7.3.1.5] "unsigned long long thread_rank() const: Rank of the calling thread within [0, num_threads)" "T shfl(T var, unsigned int src_rank) const: Refer to Warp Shuffle Functions"
  - Nothing says that every member builds the same group.
  - `lowering/` shows it compiles to the same instructions as the raw form (`VOTE.ANY`, `ATOMG.E.ADD.STRONG.GPU`, `SHFL.IDX`). C2's churn failure would therefore be expected to reach it too. That is untested: a cooperative-groups variant of C2's `x_mask` reproducer would decide it, but not for correctness.
- **The current guide's warning, read correctly.**

  > [cpg-cpp-language-extensions §5.4.6.1] "For example, the compiler could reorder instructions, and the set of active threads might not be preserved:" "unsigned mask = __activemask();" "int result = __any_sync(mask, predicate); // Active threads might not be preserved"

  The threads that execute a later instruction together need not be the snapshot's threads. The `__sync` intrinsic waits for every lane named in its mask, so the example is fine only if every named lane holds the same mask, which is exactly the part not promised.
- **Documented alternatives:**
  - **A mask known by construction.** This is C2's `oc_lane`: the lanes that exist in the warp, from `blockDim` [cpg-advanced-kernel-programming §3.2.2.2] "each warp contains threads of consecutive, increasing thread IDs with the first warp containing thread 0". It is valid only where every existing lane of the warp reaches the intrinsic at the same program point, for example a claim the runtime makes at kernel entry, before any branch (§5.4.6.6 also requires every condition around the call to evaluate the same in all named lanes). It is not valid in exit probes or in `__memcapture`, where lanes arrive in any grouping.
  - **A per-lane path.** Every lane does its own atomic, with no warp collective. It is correct by RMW atomicity alone (claim 7), and its cost is a parameter (step 0: per-lane atomics on one word are dear in registered memory).
  - No run-time check can rescue the `__activemask()` form: a disagreeing mask makes the intrinsic undefined (possibly a hang) before any check runs.
- **Our code, against the documented constraints:**
  - **e7 `reserve_ok`:** an `__activemask()` mask, so it is not covered. Its shape (one atomic) is the one that showed 0.
  - **C2 `c2_entry`, `__activemask()` variants:** not covered, and they failed under churn (C2's own finding). The constructed-mask variant (`oc_lane`) is covered where its precondition holds.
  - **C2 `c2_entry`, ordering:** after `C2_SPIN_CAP` a lane counts `CT_CAP_SPIN` and goes on to use the slot without having acquired it. By the model that use is unordered, so a cap hit should end in a counted NULL, not a use (still so in c2.cuh 67c23966).
  - **C2 `c2_exit` (AGG):** `__match_any_sync(__activemask(), s)` has the same uncovered mask. The `__syncwarp(peers)` ordering that follows rests on claim 4's C++ text. C2 labels AGG informational.
- **Shuffles order nothing.** [cpg-cpp-language-extensions §5.4.6.5] says so (quoted above). Any hand-off of memory between lanes needs `__syncwarp` or acquire/release, which C2 does per lane.
- **Step 0's finding that every llama.cpp exit-probe call had 32 lanes is not a guarantee.**
  - [cpg-cooperative-groups §4.4.3] "The coalesced_threads() operator returns the set of active threads at that point in time, and makes no guarantee about which threads are returned (as long as they are active) or that they will stay coalesced throughout execution."
  - [cpg-advanced-kernel-programming §3.2.2.1.1] "A schedule optimizer determines how to group active threads from the same warp together into SIMT units."
  - Records per call is a performance parameter.
- **Before sm_70** the rules differ: [ptx-isa §9.7.15.2] "For .target sm_6x or below, all threads in membermask must execute the same bar.warp.sync instruction in convergence". The PTX memory model also applies only from sm_70 on (C4).

### 10. "In global capture mode another thread's async copy invalidates the capture; in relaxed mode it does not" — PARTLY DOCUMENTED

> [drv-stream §6.37 cuThreadExchangeStreamCaptureMode] "During stream capture (see cuStreamBeginCapture), some actions, such as a call to ::cudaMalloc, may be unsafe."
> [drv-stream §6.37 cuThreadExchangeStreamCaptureMode] "CU_STREAM_CAPTURE_MODE_GLOBAL: This is the default mode. If the local thread has an ongoing capture sequence that was not initiated with CU_STREAM_CAPTURE_MODE_RELAXED at cuStreamBeginCapture, or if any other thread has a concurrent capture sequence initiated with CU_STREAM_CAPTURE_MODE_GLOBAL, this thread is prohibited from potentially unsafe API calls."
> [drv-stream §6.37 cuThreadExchangeStreamCaptureMode] "CU_STREAM_CAPTURE_MODE_THREAD_LOCAL: If the local thread has an ongoing capture sequence not initiated with CU_STREAM_CAPTURE_MODE_RELAXED, it is prohibited from potentially unsafe API calls. Concurrent capture sequences in other threads are ignored."
> [drv-stream §6.37 cuThreadExchangeStreamCaptureMode] "CU_STREAM_CAPTURE_MODE_RELAXED: The local thread is not prohibited from potentially unsafe API calls. Note that the thread is still prohibited from API calls which necessarily conflict with stream capture, for example, attempting cuEventQuery on an event that was last recorded inside a capture sequence."
> [cpg-cuda-graphs §4.2.2.1.2.2] "It is also invalid to query the execution status of or synchronize a broader handle which encompasses an active stream capture, such as a device or context handle when any associated stream is in capture mode."
> [cpg-cuda-graphs §4.2.2.1.2.2] "When any stream in the same context is being captured, and it was not created with cudaStreamNonBlocking, any attempted use of the legacy stream is invalid."
> [cpg-cuda-graphs §4.2.2.1.2.3] "When an invalid operation is attempted during stream capture, any associated capture graphs are invalidated."

**Documented:**
- The mode that matters is **the calling thread's own** mode, together with whether any thread captures in GLOBAL mode.
- A thread left in the default GLOBAL mode may not make "potentially unsafe" calls while any other thread captures in GLOBAL mode.
- A thread whose mode is RELAXED may.
- Synchronizing the context during any capture is invalid in every mode, and so is touching the legacy stream next to a blocking capture stream. That matches B0's crashing and invalidating controls.

**Missing:**
- **The list of "potentially unsafe" calls is not published.** The only example is `cudaMalloc`. Searched: all 62 pages, for `potentially unsafe`, `unsafe API` and `prohibited`. Only `drv-stream` has the first two, and the `prohibited` hits in the graph, interop, Tegra and PTX pages concern other things. Whether `cuMemcpyDtoHAsync` or `cuStreamSynchronize` on a private non-blocking stream is on it is not documented. Step 0 found that the copier's calls invalidated GLOBAL captures, which may change with the driver. The capture-remedy smokes narrow down which call the driver refused. Every `cuMemcpyDtoHAsync` returned success. In the sync variant, the copier's `cuStreamSynchronize` on its own stream returned error 900 (`CUDA_ERROR_STREAM_CAPTURE_UNSUPPORTED`). In the event variant the shim counts the record, the query and its fallback sync together, so it is one of those.
- **Step 0's B0 varied only the capturing thread's mode.** `step0_copier.c` never calls `cuThreadExchangeStreamCaptureMode`, so its own thread was always in the default GLOBAL mode.
  - Step 0's conclusion that an asynchronous copier breaks applications that capture in global mode is therefore a result for a copier in the default mode.
  - The documented remedy was not tested in step 0: the copier thread calls `cuThreadExchangeStreamCaptureMode(CU_STREAM_CAPTURE_MODE_RELAXED)` once.
  - **`capture-remedy/`** is that check: step 0's B0 harness with this one call added.
    - Decision rule: 0 errors of any kind in the capturing application and in the copier with the remedy, while the copier left in the default mode, and one set explicitly to GLOBAL, still break captures in the same run.
    - Two smokes (1 s per case, shared GPU) agree. In the second (`capture-remedy/smoke/run2`), the default-mode controls invalidated 601 of 1,700 and 2,886 of 2,886 captures, the explicit-GLOBAL control 580 of 1,647, and the remedy saw 0 errors of any kind in 1,373 and 1,413 iterations, with 5,923 and 5,149 copies during open captures. Its README has the rules and commands.
- **Design:**
  - Any agent thread that calls CUDA while the application may capture switches itself to RELAXED.
  - It never synchronizes the context or the device, and never uses the legacy stream.
  - It counts any capture-related error it gets.

## Mechanisms the follow-up checks C1-C4 rely on

| check | mechanism | class | basis (quotes above or below) | remark |
|---|---|---|---|---|
| C1 | kernel-scope `.local` carry from entry probe to exit probe, address by `cvta.local` before each call | **D** (lifetime, privacy, generic window); preemption **P** | claim 6 | as C1's own README says; no stored pointer across the kernel |
| C1 | `MT_SLOTTAG` fallback: (SM, warp slot, lane) slot plus owner tag checked at exit | **NF** | claims 1-3 | a detector, not a guarantee; replace by a claimed owner word (claim 1) |
| C1 | `MT_FRAME` (a helper's own frame) | documented **unsound** | [ptx-isa §5.1.5] ".local state-space variables must be declared within function scope and are allocated on the stack" | deliberate control; a function's stack frame ends with the call |
| C2 | slot claims by atomics on one word (bitmap `atom.or`, versioned stack CAS, `live` counter) | **D** | [ptx-isa §8.10.3] "When an atomic operation A and a write W overlap and are morally strong" | all `.gpu`, global, one device; sync-domain caveat on CC >= 9.0 |
| C2 | hand-off of the slot's data between owners: release `atom.add` and acquire load (`ORD` = 1) | **D** | [ptx-isa §8.9.2] "Observation order relates a write W to a read R through an optional sequence of atomic read-modify-write operations." | `ORD` = 0 is, as C2 says, not backed |
| C2 | leader election, broadcast and match in a `__noinline__` helper | **NF** with `__activemask()` (C2 measured the failure itself); **D** with C2's constructed mask (`oc_lane`) where the whole warp calls | claim 9 | the spin-cap path should return a counted NULL; per-lane pools (`ln*`, `lf*`) need no warp collective |
| C2 | lanes spin (capped) on their own claim's generation | **D** | [cpg-cuda-cpp-execution-model §5.9.2] "Otherwise, all device threads in its thread-block cluster shall eventually make progress." | the wait is on the same warp's leader; the cap is kept anyway |
| C2 | `%smid` as a pool hint only | n/a (performance) | claim 1 | correct by construction |
| C3 | `membar.gl` = `fence.sc.gpu` = `__threadfence()`; `membar.sys` = `fence.sc.sys` | **D** | [ptx-isa §9.7.15.4] "On sm_70 and higher membar is a synonym for fence.sc^{1}" | |
| C3 | `fence.acq_rel` is cheaper than `fence.sc` | **P** (qualitative) | [ptx-isa §9.7.15.4] "fence.acq_rel is a light-weight fence that is sufficient for memory synchronization in most programs." | how much cheaper is a platform parameter |
| C3 | variant N, the 1512 path without its fence, as a deployable form | **NF** for CPU-read data | claims 4, 7, 8 | a cost reference only, unless the CPU reads after a documented edge |
| C4 | contributors: gpu-scope release (`fence.acq_rel.gpu; atom.relaxed.gpu` or `atom.release.gpu`); completer: gpu-scope acquire through the counter's RMW chain | **D** | [ptx-isa §8.8] "Or a release or acquire-release memory fence followed by a strong write on M in program order" | literal variants |
| C4 | `l8` lane-0 variants (`ar_*`, `rel_*`, `tf_sc` on `l8`): lanes 1-31 reach lane 0's release only through `__syncwarp` | **P** | claim 4 | **D** if written in CUDA C++ with a valid mask (C4's kernels run full warps). In a probe with arbitrary lane groups only the `ar32_*` form is covered, in which every lane releases its own store, as C4 itself notes. |
| C4 | cumulativity: the completer's system-scope release also publishes the contributors' records | **D** | [cpg-memory-sync-domains §4.16.1] "The CUDA memory consistency model guarantees that the asserted condition will be true, so the write to x from thread 1 must be visible to thread 3, before the write to b from thread 2." | NVIDIA's example is C4's pattern: device-scope release and acquire between two GPU threads, then a system-scope release that a CPU thread (thread 3 in the example) acquires |
| C4 | `fence.sc` read as a release fence (the `*_sc` variants) | **P** | claim 4 (b) | literal in C++ (`seq_cst`), not in PTX §8.8 |
| C4 | `red.release` cannot be the counter | **D** | [ptx-isa §8.11.1] "Atomic reduction operations like red do not form acquire patterns with acquire fences." | as C4 says |
| C4 | contributors in kernels of different memory sync domains | **D** that gpu scope is **not** enough | [cpg-memory-sync-domains §4.16.2] "When using domains, code must abide by the rule that ordering or synchronization between distinct domains on the same GPU requires system-scope fencing. Within a domain, device-scope fencing remains sufficient." | 1 domain below CC 9.0; on CC >= 9.0, check the count at start-up and use system-scope contributor releases if it exceeds 1 |
| C4 | CPU acquires the flag in another process | **P** | claim 8 | as claim 8 |

The memory-sync-domains page also states cumulativity in words: [cpg-memory-sync-domains §4.16.1] "The system-scope ordering provided by release and acquire of b, therefore, needs to ensure not only writes issued from thread 2 itself are visible to thread 3, but also writes from other threads that are visible to thread 2. This is known as cumulativity." The domain count is a documented query: [cpg-memory-sync-domains §4.16.3] "To facilitate portable code, domains functionality can be used on all devices and CUDA will report a count of 1 on devices prior to compute capability 9.0." The ISA's formal counterpart is [ptx-isa §8.9.5] "The transitivity of base causality order accounts for the “cumulativity” of synchronizing operations."

Two more texts bear on all four:
- The model applies only from sm_70 on: [ptx-isa §8.1] "The constraints specified under this model apply to PTX programs with any PTX ISA version number, running on sm_70 or later architectures." This becomes a start-up check: compute capability >= 7.0.
- **Registered memory has a second virtual address.** [cpg-understanding-memory §2.6.3.1.2] "Unlike memory created with CUDA APIs, however, the memory cannot be accessed from the kernel using the host pointer. A pointer in the device’s memory region must be obtained using cudaHostGetDevicePointer(), and that pointer must be used for accesses in kernel code." The Orin reports `CAN_USE_HOST_POINTER_FOR_REGISTERED_MEM` = 0.
  - PTX treats distinct virtual addresses of one location as different proxies: [ptx-isa §8.6] "Although virtual aliases use the generic method of memory access, since using distinct virtual addresses behaves as if using different proxies, they require a proxy fence to establish memory ordering."
  - That rule is stated for PTX operations. The CPU's accesses are not PTX operations, and CUDA C++ §5.8.3 states its mapped-memory guarantee per object, not per address.
  - So the design needs no alias fence as long as **all GPU code uses one device pointer** for a given segment. A second GPU mapping of the same pages (for example, a VMM import next to a registration) would need `fence.proxy.alias` on the path.

## Step-0 verdicts: class, and whether a different NVIDIA GPU may differ

"May differ" means a different NVIDIA GPU, or the same one with another driver or configuration, may legitimately behave otherwise without violating any fetched document.

| step-0 verdict | class | may another NVIDIA GPU differ? | why (documented switch) |
|---|---|---|---|
| 1a. `%nsmid` = 8 = SMs, `%nwarpid` = 48 | sizes: **P** (`%nsmid` >= the maximum `%smid` + 1 is documented; equal to the SM count is not; `%nwarpid` versus resident warps is not stated) | **yes** | [ptx-isa §10.9] "%nsmid may be larger than the physical number of SMs in the device"; the CPG Table 33 row "Maximum number of resident warps per SM" differs by compute capability (32, 48 and 64 appear) |
| 1b. (SM, slot) exclusive among co-resident warps, also across two grids | **NF-c** | **yes** | claim 1 |
| 1c. ids stable across time-slicing | **NF-c** | **yes**, also on this GPU under another kind of preemption (debugger, checkpoint) or another driver | claim 2: PTX §10.4 and §10.8 allow a change "due to rescheduling of threads following preemption", whatever the kind of preemption. (The original audit also cited CPG §4.7.1 on MPS here; that text is about which SMs an MPS client may use over time, not about a running warp moving, so the review dropped it.) |
| 1d. `%gridid` unique per launch, repeated by graph replays | **P** / **NF** | **yes** | claim 3 |
| 1e. same-context stream priority never preempted a running kernel | **D** | no | [cpg-advanced-host-programming §3.1.3.1] "Higher-priority tasks do not preempt already running lower-priority tasks." |
| 2. costs: the fence dominates, placement barely matters for plain stores, registered atomics about 80 ns and serialized | not a guarantee: **platform parameters** | **yes**, strongly | [cuda-for-tegra §3.2] "Full Coherency is supported on Tegra devices starting with Thor SoC." "GPU L2 caching is enabled for cudaHostRegister()/cuMemHostRegister() only on platforms which support sysmem full coherency." On a discrete GPU, mapped memory crosses PCIe: [cpg-understanding-memory §2.6.3.1] "accessing mapped memory in a kernel requires transactions across the CPU-GPU interconnect, PCIe, or NVLink C2C". Placement is then the main cost, and step 0's conclusion that copy-out is not required for records may flip. |
| 3A. two-step copy, flags first, 0 violations | **NF** | **yes**, even on this board | claim 5; copy engines and caches are not specified, and on Tegra the driver picks SMs or copy engines by a heuristic (CPG §3.1.5, quoted in claim 5) |
| 3B0. capture invalidation by the copier in global mode; context sync invalid | **P** (mode rules, invalid context sync); the unsafe-call list **NF** | **yes** (driver version) | claim 10 |
| 3B. copier cost in llama-bench | parameter | yes | performance |
| 3C. POSIX-FD export of `cuMemCreate` memory, imported and read in a second process | **D** (API); CPU mapping of the dmabuf **NF** | **yes**: Thor and later get CUDA IPC | [drv-va §6.48 cuMemExportToShareableHandle] "Given a CUDA memory handle, create a shareable memory allocation handle that can be used to share the memory with other processes." [drv-types §6.5 CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR] "Allows a file descriptor to be used for exporting." [cuda-for-tegra §4.7] "Starting CUDA 13.0, memory-sharing CUDA IPC APIs would be supported on Tegra platforms with open-source GPU driver (i.e. Jetson Thor and beyond) GPU drivers. For all other Tegra platforms, EGLStream, NvSci, or the cuMemExportToShareableHandle() / cuMemImportFromShareableHandle() APIs can be used to communicate between CUDA contexts in two processes." The stale CPU mapping is consistent with [cuda-for-tegra §3.1] "The GPU cache management operations still need to be performed because the coherency is one way." The importer's reads concurrent with the writer inherit claim 5 (a DtoH copy: **NF**) and claim 8 (another process, its own context: **P** at best); only reads after a documented edge in the writer's process are ordered. |
| 4. one leader fence per warp: 0 violations, not attributable | **P** (claim 4) | **yes**: the measured safety rests on the fence's duration, which differs per platform | claim 4 |
| 4'. CPU release-store, then GPU CAS: 0 stale reads | **D** with `.sys` scope and `hostNativeAtomicSupported` = 1, for a CPU side written with `std::atomic_ref`; step 0's `casfence` and `casacq` were `.sys` (`atomicCAS_system`, `atom.acquire.sys.cas`), but its CPU side used the GCC builtin `__atomic_store_n`, which no fetched text covers (a reading) | yes, where the attribute is 0 | claim 7 |
| 5. 64-bit fetch-add usable from eBPF; CAS broken | fetch-add atomicity **D** (`.gpu` default scope); the CAS failure is **a bpftime bug** (`compiler.cpp:1697-1718`), not a GPU property | no for the bug (it is in the JIT); the default scope is the same everywhere | claim 7 |
| 6a. one reader drains about 7M records/s through `bpf()` | parameter | yes (CPU, DRAM) | performance |
| 6b. 2,047 MiB registers, 2,048 MiB fails | **NF** (no size limit is documented; `cuMemHostRegister` lists only error codes) | **yes** (driver, platform) | Read in full: `drv-mem` `cuMemHostRegister`, Tegra §3.3 and CPG §2.6.3; grepped all pages for `limit` near `register`. Fallback: treat `CUDA_ERROR_OUT_OF_MEMORY` at registration as "segment too large", retry smaller, and report it. |
| 7a. warp instructions correct under partial masks | **NF** for `__activemask()` masks (C2 found disagreeing masks in a churn shape on this very GPU); step 0's 0 violations hold for its single-atomic shape only | **yes**: even this GPU differs by code shape | claim 9 |
| 7b. every llama.cpp exit-probe call had 32 lanes | not a guarantee: **workload and compiler property** | **yes** (compiler, architecture) | claim 9 |
| 8a. a `.global` up to 1 GiB appended to a patched module loads | **NF** in the fetched pages (no size limit found) | yes | Searched `global` + `size limit`/`maximum size`, and `cuLibraryLoadData` (only `drv-types` mentions it). The driver API's Library Management page, which holds `cuLibraryLoadData` itself, was **not fetched**. |
| 8b. 24,568 B of constant memory left next to map_info | total **P**: [cpg-writing-cuda-kernels §2.3.3.5] "Constant memory is small relative to other memories, typically 64KB per device." (CPG Table 34 also lists a constant memory size); map_info's 40,968 B are bpftime's | yes (Table 34 per compute capability) | parameter |
| 9. hooks see every geometry before graph replays (llama.cpp) | not a guarantee: **bpftime and application property** | yes (another application or CUDA version may use `cuLaunchKernelEx` or `cudaGraphExecUpdate` with new geometry) | design note section 2 |

**What-this-means items of step 0, reclassified:**
1. Thread-to-slot mapping is sound within a context → **NF-c**. Correctness needs a claim (claim 1); the hardware slot stays as the spreading hint.
2. `%gridid` cannot tell replays apart → stands. It is also not documented as unique otherwise (claim 3).
3. Per-slot state the CPU never reads belongs in device memory → may rely on it. The atomics are documented (claim 7), and the cost is a parameter.
4. A thread's own slot is cheap and the fence is not → parameters. "State the CPU never reads needs no system fence" is **D**: gpu scope covers all GPU readers, within one sync domain. Batched publication is C4 (**D** for the literal variants).
5. Per-thread records to the CPU do not fit → parameter (cost).
6. Delivery is bounded by the reader → parameter.
7. Device memory for CPU-read data is possible by two-step copy-out → **NF** for correctness (claim 5). The capture interaction is claim 10.
8. Warp-level reservation is correct → **NF** with an `__activemask()` mask (C2: disagreeing masks under churn). It is **D** only with a mask known by construction where the whole warp calls, so exit probes and `__memcapture` need a per-lane path. Whole warps at llama.cpp exits → workload.
9. Tool-written CAS is broken → a bpftime bug.

## What the design may rely on: start-up checks, run-time checks, parameters

**Start-up checks.** Each is a documented query, and a failed check disables the dependent feature with a logged reason:

| check | needed by |
|---|---|
| compute capability >= 7.0 | memory model (PTX §8.1), independent thread scheduling (claim 9) |
| `CU_DEVICE_ATTRIBUTE_HOST_NATIVE_ATOMIC_SUPPORTED` = 1 | any GPU-CPU read-modify-write on the segment (claim 7); in the review's conservative reading, any GPU atomic on registered memory at all (claim 7, first bullet) |
| `CU_DEVICE_ATTRIBUTE_HOST_REGISTER_SUPPORTED` = 1, `CAN_MAP_HOST_MEMORY` = 1 | the registered segment |
| memory sync domain count (`cudaDevAttrMemSyncDomainCount`) | gpu-scope chains across kernels (C4); if > 1, contributor releases at system scope |
| `%nsmid`, `%nwarpid` (read on the device), `MULTIPROCESSOR_COUNT`, `MAX_THREADS_PER_MULTIPROCESSOR` | sizing; ids beyond them are bounds-checked and counted |
| `CAN_USE_HOST_POINTER_FOR_REGISTERED_MEM` | always use `cuMemHostGetDevicePointer`'s pointer on the GPU, one pointer per segment (alias rule) |
| `cuMemHostRegister` result | an `OUT_OF_MEMORY` at a given size is a platform limit; retry smaller and report |

**Run-time checks with counted fallbacks:**
- a failed slot claim → counted miss (claim 1);
- a tag mismatch → counted diagnostic only;
- copy-out record validation → counted rejects, then fallback to registered memory (claim 5);
- capture-related errors in agent threads → counted (claim 10);
- C2's spin cap → counted NULL, not a use (claim 9);
- none for `__activemask()` masks: a disagreement is undefined before it can be checked, so the design avoids them (claim 9).

**Per-platform parameters, never correctness:**
- every timing number of step 0 and of C3/C4 (fence cost, atomic cost in each memory, copy speed, drain rate);
- the slot collision rate;
- records per warp call;
- the largest registrable segment;
- the constant memory left.

**Never, for correctness:**
- `%warpid`/`%smid` uniqueness or stability;
- `%gridid` uniqueness;
- copy-engine order against a running kernel;
- the duration of a fence;
- whole warps at a probe;
- lanes agreeing on an `__activemask()` mask (claim 9);
- plain (non-atomic) CPU loads of words the GPU writes while kernels run, and `.gpu`-scope GPU atomics on words the CPU accesses during the run (claim 7);
- `__threadfence_system()` as the release half of a cross-lane publication; use the libcu++ fence or a release store (claim 4);
- `%globaltimer`. It is fine for ordering records for people to read, but not for anything else: [ptx-isa §10.28] "Special registers intended for use by NVIDIA tools. The behavior is target-specific and may change or be removed in future GPUs."

## Known limits

- **Only the 62 fetched pages (and the 21 of 13.2.2) were searched.** NOT FOUND means "not in these pages". The driver API's other sections (Library and Module Management among them), the CUDA Runtime API reference, libcu++/CCCL documentation (its atomicity page, linked from PTX §8.1.1, returned 404), the PTX interoperability guide and the Tuning Guides were not read.
- **The documents are v13.5 and PTX 9.5, and the board runs 13.2.2.** Every quote was found in the 13.2.2 pages as well, with one wording difference. The archive's 13.2.2 driver API pages (6 of them) print "CUDA Toolkit v13.2.1" in their own header; they are what the 13.2.2 archive URL serves. (13.2.2's §3.1.5 also has a typo, "(CEs)o perform", so that quote is cut before it.) Text that 13.2.2 has and 13.5 dropped was not searched for.
- **Text found only in the archived v12.9 guide** (what compute preemption does, cross-context time-slicing, the discovery pattern) is weaker: NVIDIA dropped it from the current guide without saying it no longer holds. The cross-context text is [cpg-legacy-12.9 §4.2.8.2] "A kernel from one CUDA context cannot execute concurrently with a kernel from another CUDA context. The GPU may time slice to provide forward progress to each context."
- **The classes rest on reading where the texts must be combined.** Examples: claim 4 derives "happens before" from NVIDIA's "strongly happens before" by the C++ definitions; claim 8 counts the driver's `CUDA_SUCCESS` from a query like the runtime's `cudaSuccess`. The C++ clauses come from the **working draft** at eel.is, not from the published standard that CPG §5.8.4 amends; no published-standard text was fetched.
- **The CPU side of the cross-process hop (claim 8) rests on no fetched text.** The CPU architecture's memory model (the Arm ARM) was not fetched.
- **GPU work here is limited to the optional `capture-remedy/` check**, smoke only. The cost of `.sys`-scope atomics on registered memory (claim 7) was measured by step 0 only in the uncontended form (one word per warp); contended `.sys` atomics are an open cost item for C3 or the main session.
- **C1 and C2 were being edited during this audit.** The rows above refer to the files pinned in "Method". `c1lib.py` changed afterwards (bc646b10 at the review), and it still emits `cvta.local` before every probe call. Claim 9 uses C2's smoke counts, which C2's real runs may revise. A real run cannot turn the `__activemask()` form into a guarantee, but it may also show failures in more shapes.
- **The HTML-to-text conversion is ours** (standard library parser). Table cells are joined with " | ", so table contents are cited by row name rather than quoted.

## Files

- `README.md`: this audit.
- `run_all.sh`: the checks above (manifest, text regeneration, quotes, control, optional refetch).
- `docs/SOURCES.tsv`, `docs/MANIFEST.tsv`: URLs, sha256, sizes, versions, fetch date.
- `docs/raw/*.html`: the fetched pages, unchanged. `docs/text/*.txt`: their text, as produced by `tools/html2text.py`.
- `tools/html2text.py`, `tools/check_quotes.py`, `tools/manifest.py`: standard-library Python, run with `python3 -I`.
- `tests/altered.md`: the control input.
- `capture-remedy/`: the optional check for claim 10 (README, `build.sh`, `run_all.sh`, `analyze.py`, `selftest_analyze.py`, `src/`, `common/`, `smoke/run1/` (8 cases, before the review), `smoke/run2/` (9 cases, after it)).
- `lowering/`: the compile-only probe of claims 4 and 9 (`probe.cu`, `build_probe.sh`, `probe.ptx`, `probe.sass`, `lowering.txt`, README).
- `docs/v13.2.2/`: the CUDA 13.2.2 versions of the cited pages (raw, text, SOURCES, MANIFEST).

## Review changes (adversarial review, 2026-11-15)

Made before any real run; the classes of claims 1-3, 5, 6, 9 and 10 are unchanged.
- **Re-verification.** All 78 pages the audit had saved were downloaded again: byte-identical. Every quoted fragment was found again by a second, independent text extraction, with no fragment crossing a block boundary and every section matching. Untagged double-quoted strings were listed: those naming document text are verbatim; three step-0 paraphrases in quotation marks were reworded, and one non-verbatim phrase ("standard C++ semantics") was replaced by the verbatim CPG §5.8 text.
- **Decision rule DOCUMENTED.** One sentence added: quoted C++ clauses count as premises where CPG §5.8 keeps standard C++ semantics.
- **Claim 4.** The original chain recalled two C++ rules from memory: that a `seq_cst` fence is a release fence, and that "strongly happens before" implies "happens before". The review fetched [atomics.fences], [atomics.order] and [intro.races] and now quotes them, including the release-store edge. The review also found that `__threadfence_system()` is never called a C++ release fence, so only the libcu++ fence form and the release-store form are DOCUMENTED; the `__threadfence_system()` form is PARTLY.
- **Claim 5.** Two §8.9.4 edges were missing from the list (the start of the enqueueing call; a successful query), and the data-race sentence is now stated as a reading, since the model does not define a copy's reads. The search now quotes CPG §3.1.5 (on Tegra the driver picks SMs or copy engines by a heuristic), the copy source-access orders and the driver API's pointer to the guide.
- **Claim 6.** The current driver API does name compute preemption, as a device attribute; that quote was added.
- **Claim 7.**
  - The audit said plain aligned CPU loads and stores of up to 16 B are atomic at system scope on mapped memory. That was wrong: §5.8.3 covers atomic loads and stores only, so plain CPU loads concurrent with GPU stores are data races. 16-B atomics carry footnote [2].
  - The audit said the cost of `.sys` atomics was unmeasured. That was wrong: step 0's `atom_warp_leader_sys` (SASS `STRONG.SYS`) cost the same as `.gpu` in registered memory in both passes. Only contended `.sys` atomics are untimed.
  - Step 0's CPU side used GCC's `__atomic_store_n` on plain objects; §5.8.2 names only `std::` and `cuda::std::` types, so that part is marked as a reading (claim 7 and row 4').
  - The review added a conservative reading: CPG §2.6.3.1.3 doubts atomics on mapped memory on some systems without naming a scope, so GPU atomics in registered memory are relied on only where `hostNativeAtomicSupported` = 1.
  - The two step-0 results quoted as "a GPU CAS lock worked" and "CPU adds against GPU adds lost 0" are not in the step-0 README. The first is check 4's test 2, which used `.sys` atomics. Step 0 had no test matching the second.
- **Claim 8.** Added PTX's successful-query edge and CPG §5.8.1.1's "program thread". The in-process agent hop was called documented; its CPU half rests on no fetched text, which the claim now says.
- **Claim 9.** Added §5.4.6.6's lead-in and its rule that conditions around the call must evaluate the same in every named lane.
- **Claim 1.** Kept NF-c under the rule's "strictly weaker" arm, with a note that the text is weaker than the claim, not its opposite. Added that `%nwarpid` is a ptxas immediate (step 0).
- **Step-0 table.** Row 1c no longer cites the MPS text from CPG §4.7.1, which concerns which SMs a client uses, not thread migration. Row 3C now quotes the driver API's export function. Row 4' records that step 0's CAS variants were `.sys`.
- **Lowering probe.** Its README said ptxas inlined the helpers. It did not: they are subroutines (`CALL.REL.NOINC`/`RET.REL.NODEC`).
- **`run_all.sh`.** Steps 4 and 5 now check *which* fragments are rejected, not only how many.
- **`capture-remedy/`.**
  - The remedy rule ignored the copier's wait errors, which is where the controls fail (error 900; the copies themselves succeed). It also ignored captest's launch, instantiate, update, graph-launch, sync and status errors. All are now required to be 0, plus `prev_mode` = GLOBAL, no set-up error and no poll cap hit.
  - An explicit-GLOBAL control (`async-global-global`) was added, so that control and remedy differ only in the mode value.
  - The sanity rule now also requires the logged copier mode of every control and remedy case to match its name. A crashed remedy case FAILS, a crashed control gives UNDECIDED, and a crashed reported-only case changes no verdict.
  - `selftest_analyze.py` feeds altered copies of `smoke/run1` to the analyzer and checks that each verdict can come out FAILS, UNDECIDED or BROKEN HARNESS (no GPU).
  - A second smoke (`smoke/run2`) was run with the new rules.
