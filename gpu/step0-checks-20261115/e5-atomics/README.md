# step0 e5: eBPF atomics as tools would write them

Check 5 of `/home/jetson/src/results/gpu/dir1-design-space-20261115/README.md` section 5 (decides for A, B and D):

> 5. **eBPF atomics as tools would write them** (A, B, D).
>    - Compile-only first: `__sync_fetch_and_add` with a used result at the default `-mcpu` and at v3; read the PTX.
>    - Then a run at 12,288 threads: every returned index must appear exactly once.
>    - Decides A's record path, and whether "use atomics" is a workable rule for tool authors in B and D.

The note also says (section 2) that CMPXCHG returns a plain load taken before the cmpxchg (vm/llvm-jit/src/compiler.cpp:1697-1718).
This experiment records that bug and does not fix it.

The code is the dev tree at `/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate` (dev 52126f33), run through its `build-cuda` and
`/home/jetson/src/bpftime-gpu-work/run.sh`. Nothing in the dev tree was changed or built.

## Files

| file | what |
|---|---|
| `atomics_cases.bpf.c` | compile-only cases, one translation unit per case (`-DCASE_<name>`) |
| `compile_only.sh` | case x `-mcpu` (default, v1, v2, v3) x clang-15/16 -> `results/compile-only/` |
| `decode_bpf_atomics.py` | decodes BPF atomic instructions from their bytes (opcode 0xdb/0xc3, imm) |
| `e5.bpf.c` | the probe, three builds: `e5_fadd.bpf.o`, `e5_plain.bpf.o` (control), `e5_cas.bpf.o` |
| `e5_loader.c` | loads one program (under the syscall server), waits for SIGINT, checks the maps, writes JSON |
| `e5_victim.cu` | `e5step0_atomics_kernel`, 48 x 256 threads, 10 serialized launches (also used by e6 part c) |
| `run_all.sh` | `make` (incremental), the three runs through run.sh with `BPFTIME_CUDA_KEEP_PTX=1`, PTX/SASS extraction, summary |
| `extract_probe_ptx.py`, `summarize_e5.py` | probe function PTX and its atomics; decisions |
| `Makefile` | builds everything; libbpf.a is copied from `example/gpu/threadhist-llama/.output` |

## Method

**Compile-only.** `compile_only.sh` compiles nine forms with clang-15 (the board's BPF compiler) and clang-16, at the default
`-mcpu` and at v1, v2 and v3. The forms are `__sync_fetch_and_add` on u64 and u32 with the result used and unused,
`__atomic_add_fetch(..., __ATOMIC_SEQ_CST)` (bpftime's cuda-counter example), `__sync_val_compare_and_swap` on u64 and u32,
the CAS retry loop, and xchg. It records whether each compiles and which BPF instruction results, decoded from the bytes
(llvm-objdump-15 prints the u32 fetch and non-fetch adds with the same text). It also records which `-mcpu` tool Makefiles use:
bpftime's examples pass none, and libbpf-tools (bcc v0.37.0) passes none except `-mcpu=v3` for softirqs.

**Run.** The victim launches `e5step0_atomics_kernel` 10 times with 48 x 256 = 12,288 threads, one launch at a time. The
kretprobe on it fires once per thread, 122,880 times in all. Every firing:
1. computes gid from helpers 503-505;
2. counts its firing in `seen[gid]`, a word only that thread touches;
3. builds a tag unique per (launch, thread): `(seen << 20) + gid + 1`;
4. claims an index `idx` from `ctl[0]` and writes the tag into `out[idx]`.

All maps are type 1503, which the server creates as 1513 (host memory registered with the GPU). Step 4 differs per build:
- **fadd** (the rule under test): `idx = __sync_fetch_and_add(ctr, 1)`.
- **plain** (negative control, it must lose): `idx = *ctr; *ctr = idx + 1`.
- **cas**: `for (i < 1024) { old = *ctr; if (cas(ctr, old, old + 1) == old) break; }`, then `idx = old`. Retries and cap hits
  are counted with separate atomic adds.

After the victim exits, the loader reads `ctl`, `seen` and `out` through `bpf()` and computes:
- `calls` = sum of `seen`;
- `gaps` = zero slots in `out[0 .. calls-1]`;
- `duplicate_indices` = calls - distinct valid tags found. Each duplicated index overwrites one tag, so this counts the
  duplicates exactly.

Every program name starts with `cuda__`: the agent runs a CUDA attach only for such programs, and the first smoke run, whose
probe was named otherwise, fired 0 times.

**PTX and SASS.** With `BPFTIME_CUDA_KEEP_PTX=1`, the agent keeps the extracted and patched PTX in
`/tmp/bpftime-fatbin-work.*` (attach/nv_attach_impl/nv_attach_impl.cpp:953-954 and :1140-1149). `run_all.sh` finds the dirs named in
its own run's agent log. It copies the patched module and cuts out `__retprobe_func__e5step0_atomics_kernel` (the JIT's PTX
for the probe), then lists every `atom`/`red` and fence. It compiles the module with `ptxas -arch=sm_87 -O3` after the same
`.target` rewrite bpftime applies, and cuts the kernel's SASS (`ptx/<mode>.kernel.sass`, its atomics in
`ptx/<mode>.kernel.atoms.sass.txt`). ptxas inlines the probe and every helper into the kernel (one SASS function), so the
probe's own atomics are those that differ between the three builds; the trampoline's are common to all three. Finally the
script deletes only those dirs.

Each run.sh run gives the loader a 120 s timeout and the victim 60 s, so the loader is always stopped by run.sh's SIGINT
after the victim, never by its own timeout while the victim runs. `run_all.sh` copies run.sh's `meta.txt`
(`<mode>.runmeta.txt`: `client_rc`, `server_rc`) and the victim's line (`<mode>.victim.json`) next to the loader's JSON.

## Decision rules (written before the real run; rule 0 and the PTX/SASS checks of rule 6 added in review, before it)

0. **A run counts** only if the probe fired on every thread of every launch (calls == 122,880), the loader was stopped by
   run.sh's SIGINT (`server_rc` 0, not 124 from its own timeout; `timed_out` 0), the victim exited 0 with 0 output
   mismatches, and every `bpf()` read of the maps worked. A run that does not count makes the rules that use it
   **inconclusive**, never "no" (`summarize_e5.py`: `validity`).
1. **"Fetch-add with a used result compiles at the `-mcpu` tools use"** holds per width when clang-15 at the default
   `-mcpu` emits BPF_ATOMIC with imm 0x01 (ADD|FETCH) for it. A backend error or a non-fetch instruction means no for that width.
2. **The plain control must lose**: `duplicate_indices > 0` with calls == 122,880. If it loses nothing, the run had no
   contention and rules 3 and 4 are **inconclusive**.
3. **fadd at runtime: yes** if calls == 122,880, counter == calls, all of these are 0 (gaps, duplicate indices, invalid tags,
   overflow, NULL lookups), and every thread fired exactly 10 times. Any duplicate or gap means **no**.
4. **CAS in eBPF is unusable** if the cas run shows `duplicate_indices > 0` with calls == 122,880. No duplicates means the bug
   did not show in this run, which is not evidence that CAS is safe; the PTX decides that case.
5. **"Use atomics" is a workable rule for tool authors** if 1 holds for u64 and 3 is yes. If 1 fails at the default `-mcpu`
   for a width, the rule needs `-mcpu=v3` for that width.
6. **PTX:** the CMPXCHG bug is confirmed when the `atom.cas` destination register is never read and the loop's compare uses
   the value loaded before the cas (`cmpxchg_bug_ptx`); the SASS agrees when the probe's `ATOM.E.CAS.64` writes `RZ`
   (`cmpxchg_sass_destination_RZ`). The counterpart for fetch-add: its `atom.add.u64` result register is read
   (`fadd_result_used_ptx`) and its SASS `ATOM.E.ADD.64` has a real destination register (`fadd_sass_destination_not_RZ`).
   Without kept PTX these are inconclusive.

## Commands (main session, quiet board)

```bash
cd /home/jetson/src/bpftime-gpu-work/step0/e5-atomics
timeout -k 5 120 ./compile_only.sh                  # ~20 s, no GPU
timeout -k 5 600 ./run_all.sh                       # make, then fadd plain cas: ~25-60 s (smoke: 22 s for all three)
```
Results go to `results/compile-only/` and `results/run-<TS>/`, which holds `{fadd,plain,cas}.json`, `ptx/`,
`summary.json` and `summary.txt`. The run logs are in `/home/jetson/src/bpftime-gpu-work/runs/step0-e5-<mode>-<TS>/`.
Nothing is timed, so a busy board changes only the wall time.

## Smoke results (2026-11-15, board shared with other agents; functional, nothing timed)

`results/compile-only/matrix.txt` (clang-15 and clang-16 agree):

| form | default / v1 / v2 | v3 |
|---|---|---|
| u64 `__sync_fetch_and_add`, result used | ADD\|FETCH (imm 0x01) | same |
| u64 / u32 fetch-add, result unused | ADD (imm 0x00) | same |
| u32 fetch-add, result used | **backend error** "Invalid usage of the XADD return value" | ADD\|FETCH |
| u64 `__atomic_add_fetch` seq_cst, result used | ADD\|FETCH | same |
| u64 CAS, CAS loop | CMPXCHG (imm 0xf1) | same |
| u32 CAS | **error** "Unsupported atomic operations, please use 64 bit version" | CMPXCHG |

So the note's "tools must be built with `-mcpu=v3`" holds only for 32-bit fetch with a used result and for 32-bit CAS.
- The u64 forms compile at the default `-mcpu`.
- libbpf-tools' histogram slots are `__u32` (14 headers in bcc v0.37.0), but its `__sync_fetch_and_add(&histp->slots[slot], 1)`
  ignores the result, so it compiles at the default `-mcpu`.
- A u32 cursor whose result is used (an index into a record array) fails loudly in clang ("Invalid usage of the XADD return
  value") and needs `-mcpu=v3`. It never miscompiles silently.
- On the GPU only u64 was run. The u32 lowering is the same `atomicrmw` at 32 bits (vm/llvm-jit/src/compiler_utils.cpp:372-400),
  untested here.

Runtime (`results/run-smoke5/summary.txt`, one full `run_all.sh`; the earlier single-mode runs run-smoke3 and run-smoke4,
combined in `results/smoke-combined/`, agree: plain counter 88, cas counter 92):

| program | calls | counter | gaps | duplicate indices | CAS retries | verdict |
|---|---|---|---|---|---|---|
| fadd | 122,880 | 122,880 | 0 | 0 | - | pass |
| plain (control) | 122,880 | 74 | 122,806 | 122,806 | - | loses, as it must |
| cas | 122,880 | 86 | 122,794 | 122,794 | 0 | unusable |

What the PTX and SASS show (`results/run-smoke5/ptx/`, identical to `results/smoke-combined/ptx/`):
- **fetch-add:** the JIT emits `atom.add.u64 %rd11, [%rd49], 1;` with a generic address and no `.sem` or `.scope`. ptxas
  makes it `ATOM.E.ADD.64.STRONG.GPU`, with an `ATOMS.CAST.SPIN` fallback for the shared window, and the result is used.
  - GPU scope suffices for GPU-GPU atomicity. The CPU reads the words only after the kernels end.
  - No fence surrounds the atomic itself. The 1513 lookup before it ends in `membar.sys` (`MEMBAR.SC.SYS` in the helper).
- **CMPXCHG:** `ld.u32 [%rd48]; ld.u32 [%rd48+4]; or.b64 %rd10 ...; add.s64 %rd63, %rd10, 1;
  atom.cas.b64 %rd64, [%rd48], %rd10, %rd63; setp.lt.u64 %p6, %rd10, 131072`.
  - The cas result `%rd64` is never read, and SASS confirms it: `ATOM.E.CAS.64.STRONG.GPU P1, RZ, ...` (destination RZ).
  - The "plain load before the cmpxchg" and the loop's own `old = *ctr` were merged into one load, so the retry compare
    `old == old` folded away. The loop never retries (cas_retries = 0), and every thread takes the value it read.
  - So a CAS-based claim behaves exactly like the plain control.
- **Not asked, seen on the way:** the JIT splits 64-bit plain loads and stores through map pointers into two 32-bit
  accesses: `ld.u32 [p]; ld.u32 [p+4]` and `st.u32 [p]; st.u32 [p+4]`, as above and for `seen[]`. A CPU or GPU reader
  racing a plain 64-bit store can therefore see a torn value. This matters for any record or flag word written without
  an atomic (A's drain, D's scratch tags).

Review re-smoke (`results/run-review1`, rule 0 applied, all three runs valid): fadd 122,880 calls, counter 122,880, 0 gaps,
0 duplicates; plain counter 127 (122,745 duplicate indices); cas counter 84 (122,796), 0 retries; `cmpxchg_bug_ptx`
confirmed, CAS SASS destination `RZ`; fetch-add result read in PTX and SASS. The older dirs lack the run meta, so
`summarize_e5.py` now calls their runs inconclusive under rule 0.

Under the rules above: rule 1 holds for u64 (no for u32 at the default `-mcpu`), rule 2 holds, rule 3 is yes, rule 4
gives unusable, and rule 6 is confirmed. "Use atomics" (64-bit fetch-add) is workable as written; CAS logic in eBPF is not.

## Known limits

- Only u64 was run on the GPU. u32 fetch-add was compiled, not run: the JIT's 32-bit path (trunc, `atomicrmw` i32, zext,
  vm/llvm-jit/src/compiler_utils.cpp:372-400) is untested on the device.
- The tag store into `out[idx]` is a plain 64-bit store, which the JIT splits into two 32-bit stores. Where two firings share
  an index (plain, cas), a torn tag can count as invalid instead of overwritten; the duplicate count stays exact for fadd
  (no shared index) and only shifts between columns for the controls.
- One kernel shape, with serialized launches. The tag scheme assumes serialized launches, because `seen[gid]` is a plain
  per-thread word. Concurrent launches and graph replays are not covered.
- Registered host memory (1513) only. On a discrete GPU the same programs would hit device memory (1503), which is untested.
- The PTX ISA text on default `.sem`/`.scope` was not re-read here (the docs page fetch stopped before the `atom` section).
  As the reviewer knows the ISA, an `atom` without `.sem` is `.relaxed` and without a scope is `.gpu`; ptxas's `STRONG.GPU`
  agrees. Quote the ISA before citing this.
- One pass of fadd shows no lost index among 122,880. It cannot exclude rare failures. The main session's rerun adds one more
  sample; loop `run_all.sh fadd plain` for more (rule 2 needs the plain control in the same results directory).
- kretprobe only. Entry probes and `__memcapture` call the same JIT output, but they were not run.
