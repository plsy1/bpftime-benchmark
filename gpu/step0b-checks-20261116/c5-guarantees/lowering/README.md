# C5 lowering probe (compile only, no GPU)

**Questions.**
- Claim 4 of `../README.md` recommends writing the publication in documented CUDA C++ constructs, so that the documented C++ guarantee applies. What do those constructs compile to with CUDA 13.2 for sm_87? Is that the same code step 0 and C4 measured, so that their costs carry over?
- Claim 9 asks whether the cooperative-groups reservation (`cg::coalesced_threads()`) is different code from the raw `__activemask()` one.

**Method.** `probe.cu` holds three publication kernels and two reservation helpers. In each publication kernel, every lane stores a payload word, the warp calls `__syncwarp()`, and lane 0 publishes a flag:

- `pub_cxx_fence`: `cuda::atomic_thread_fence(memory_order_seq_cst, thread_scope_system)`, then `cuda::atomic_ref<u64, thread_scope_system>::store(v, memory_order_relaxed)`.
- `pub_cxx_release`: `atomic_ref<u64, thread_scope_system>::store(v, memory_order_release)`, with no separate fence.
- `pub_threadfence_system`: `__threadfence_system()`, then the relaxed system-scope store.

The two reservation helpers are:
- `reserve_raw`: `__activemask`, lowest-lane leader, `atomicAdd`, `__shfl_sync`. This is step 0 e7's `reserve_ok` without its trailing `__syncwarp`.
- `reserve_cg`: `cg::coalesced_threads()`, `thread_rank()`, `num_threads()`, `shfl()`. This is the documented cooperative-groups API.

`build_probe.sh` compiles the file to PTX and to a cubin and disassembles the cubin. Its output is in `probe.ptx`, `probe.sass` and the summary `lowering.txt`. Nothing is run.

**Result (CUDA 13.2, sm_87):**

| kernel | PTX | SASS | same as |
|---|---|---|---|
| `pub_cxx_fence` | `bar.warp.sync -1; fence.sc.sys; st.relaxed.sys.b64` | `NOP` (the warp sync); `MEMBAR.SC.SYS; ERRBAR; CCTL.IVALL; ST.E.64.STRONG.SYS` | e4 protocol A (`membar.sys; st.relaxed.sys`), except that the store is generic (`ST`) instead of global (`STG`) |
| `pub_cxx_release` | `bar.warp.sync -1; st.release.sys.b64` | `NOP`; `MEMBAR.ALL.SYS; ERRBAR; ST.E.64.STRONG.SYS` | e4 D1 (`st.release.sys`) |
| `pub_threadfence_system` | `bar.warp.sync -1; membar.sys; st.relaxed.sys.b64` | as `pub_cxx_fence` | e4 protocol A |
| `reserve_raw`, `reserve_cg` (separate `.func`s in PTX; in SASS, subroutines of `use_reserve` reached by `CALL.REL.NOINC` and left by `RET.REL.NODEC`) | `activemask`, `atom.global.add.u32`, `shfl.sync.idx` in both | `VOTE.ANY`, `ATOMG.E.ADD.STRONG.GPU`, `SHFL.IDX` in both | e7's helper |

**Reading:**
- The documented C++ forms produce the instructions step 0 measured, so step 0's and C4's costs carry over to them. Of the three publication kernels, only `pub_cxx_fence` and `pub_cxx_release` are the DOCUMENTED forms of claim 4. `pub_threadfence_system` compiles to the same SASS, but the guide never calls `__threadfence_system()` a C++ release fence, so that form is PARTLY (review, claim 4 of `../README.md`).
- `__syncwarp()` with a full mask compiles to a `NOP` here. The compiler meets the C++ guarantee without a barrier instruction, and the guarantee belongs to the C++ source, not to a SASS instruction. This is why a hand-written PTX or SASS equivalent is not covered by the same text.
- The cooperative-groups form costs the same instructions as the raw intrinsics, which also means it inherits the raw form's open question (claim 9): nothing documents that every member builds the same group, and C2 measured disagreeing `__activemask()` masks under churn.

**Limits.** One compiler version, one target. Small kernels with a full mask only. The helpers are separate `.func`s called with `call.uni` in the PTX, and ptxas kept them as subroutines placed in `use_reserve`'s code section (`CALL.REL.NOINC` at 0x0230 and 0x0280, `RET.REL.NODEC`; the review corrected an earlier "inlined"). bpftime's real helpers are separate functions called from patched PTX, so their code is not this code.
