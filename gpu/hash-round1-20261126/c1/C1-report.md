# C1: what the hash-map programs do through lookup results, in bytecode and in llvmbpf's PTX (2026-11-26)

Rules: `../RULES-round1.md`, section C1 (written before any run). Design: results `gpu/hash-design-20261126/DESIGN.md`.
Compile level. The GPU runs only dumped PTX; nothing was timed.

## Answer

- **Stage-one material** (every access through a lookup result is a 64-bit atomic add with an unused result, plus null tests; no delete; only `BPF_NOEXIST` inserts):
  - cuda-counter `call_count`;
  - threadscheduling `sm_histogram` and `warp_histogram`, and the same two maps in threadscheduling_dynamic_hook (1501);
  - th-hostcall `call_count`;
  - pt_hash `h` in the cnt and pre variants.
- **Not stage-one:** every other hash map, apart from two pt_hash builds: in miss, `h` meets the rule vacuously, and none never uses `h`. The reasons, by frequency:
  - plain (non-atomic) u64 loads or stores through the lookup result;
  - `BPF_ANY` or `BPF_EXIST` updates;
  - deletes;
  - in two programs, the lookup pointer passed to `bpf_map_update_elem` as the value.
- **Atomics in the bytecode:** only one form, the no-fetch atomic add (`lock *(u64 *)(rX + 0) += rY`, opcode 0xdb, imm 0).
  - It comes from `__atomic_add_fetch` (cuda-counter) as well as from `__sync_fetch_and_add` (everything else), because no program uses the result.
  - No fetch, xchg or cmpxchg form appears in any object.
  - Compiled with clang-15's default CPU (no `-mcpu` flag; clang passes no `-target-cpu`).
- **Atomics in the PTX:** that add becomes `atom.add.u64 %rdD, [%rdP], 1;`.
  - It is 64-bit, on a **generic** address (no `.global`, no `cvta.to.global`), with no `.sem` or `.scope`. The PTX ISA (local copy) defines those defaults: "If the .sem qualifier is absent, .relaxed is assumed by default", and "If the .scope qualifier is absent, .gpu scope is assumed by default".
  - Its destination register is never read again. This holds at all 30 `atom` sites in the 7 dumps, the side-count map's included (`analysis/ptx-atom-dest-check.txt`).
- **Plain u64 accesses in the PTX:** every plain u64 load or store through a lookup result comes out as **two 32-bit generic accesses**.
  - Loads: `ld.u32 [p]` and `ld.u32 [p+4]`, then shl/or.
  - Stores: `st.u32 [p]` and `st.u32 [p+4]`.
  - This holds in all 9 cases seen, across cuda-counter, cudagraph and pt tid.
  - Stack accesses stay `ld.local.u64` / `st.local.u64`.
  - Likely cause (inference, IR not dumped): llvmbpf builds its loads without an alignment (`vm/llvm-jit/src/compiler_utils.cpp:291`, `builder.CreateLoad(srcTy, addr)`). It sets the NVPTX data layout only after the module is generated (`llvm_jit_context.cpp:933-946`), and LLVM's default layout gives i64 a 4-byte ABI alignment. Atomics are given `MaybeAlign(8)` explicitly (`compiler_utils.cpp:388`) and stay whole.
- **Escapes:** besides the two lookup pointers passed to helper 2 as the value, there are none.
  - No lookup pointer is stored to memory or spilled to the stack.
  - No pointer arithmetic (every access is at offset 0).
  - The pointer is compared only against 0.
- **Update flags:** constant in every update call, in the bytecode (`r4 = imm`) and in the PTX (param3 a `mov.u64` constant).

## Verdicts per (program, map)

Types: 1 = HASH, 1501 = GPU_HASH_MAP (created as an ordinary HASH on the Orin: results `gpu/hash-current-20261126`). Updates: `NOEXIST` = 1, `ANY` = 0, `EXIST` = 2. "XADD" is the no-fetch atomic add, result unused. "plain" is a non-atomic u64 access. Every access is u64 at offset 0. In the PTX column:
- "u32 pair" means a plain u64 access emitted as two 32-bit generic accesses;
- "dest dead" means the `atom`'s destination register is not read anywhere in its function.

Run names below: cc = cuda-counter, ts = threadscheduling, cg = cudagraph, th = th-hostcall, ptcnt / ptrmw / pttid = the pt_hash builds hs_cnt / hs_rmw / hs_tid_thread_kc_test.

| program | map (type) | lookups: accesses through the result | updates (flags) | deletes | escapes | PTX | verdict |
|---|---|---|---|---|---|---|---|
| cuda-counter | call_count (1) | kprobe: null test, XADD | NOEXIST | none | none | `atom.add.u64`, generic, dest dead | **stage-one** |
| cuda-counter | start_ts (1) | kretprobe: plain load (pc 19), plain load again at pc 105, after the delete and other helper calls | ANY (kprobe) | 1 (kretprobe, key pid, not the lookup key x) | none | u32 pairs, both reads | not: plain loads, ANY, delete |
| cuda-counter | total_time_ns (1) | kretprobe: plain load, add, plain store (non-atomic `*total += delta`) | EXIST with the lookup pointer as the value; NOEXIST on a miss | none | pointer passed to helper 2 (r3) | load u32 pair, store u32 pair | not: plain load and store, EXIST, escape |
| cudagraph | call_count (1) | kprobe: plain load (`*cnt + 1`) | ANY | none | none | u32 pair | not: plain load, ANY |
| cudagraph | start_ts (1) | kretprobe: plain load | ANY | 1 | none | u32 pair | not: plain load, ANY, delete |
| cudagraph | total_time_ns (1) | kretprobe: plain load | ANY | none | none | u32 pair | not: plain load, ANY |
| threadscheduling | sm_histogram (1) | null test, XADD | NOEXIST | none | none | `atom.add.u64`, generic, dest dead | **stage-one** |
| threadscheduling | warp_histogram (1) | null test, XADD | NOEXIST | none | none | `atom.add.u64`, generic, dest dead | **stage-one** |
| threadscheduling | thread_mapping (1) | no lookup | ANY (80-byte value) | none | none | update call only | not: ANY, multi-word value |
| th-hostcall | call_count (1) | kretprobe: null test, XADD | none from the GPU (the loader inserts key 0 from the CPU before attaching) | none | none | `atom.add.u64`, generic, dest dead | **stage-one** |
| threadscheduling_dynamic_hook | sm_histogram (1501) | null test, XADD | NOEXIST | none | none | not dumped | **stage-one** |
| threadscheduling_dynamic_hook | warp_histogram (1501) | null test, XADD | NOEXIST | none | none | not dumped | **stage-one** |
| threadscheduling_dynamic_hook | thread_mapping (1501) | no lookup | ANY (80-byte value) | none | none | not dumped | not: ANY, multi-word value |
| gpu_shared_map | counter_per_thread (1501) | no lookup | ANY (constant 0), from the thread whose index sum is 0 | none | none | not dumped | not: ANY |
| micro cuda_probe | hash_map (1) | cuda__probe_hash_lookup: plain load, add 1, plain store (`(*val)++`) | ANY (cuda__probe_hash_update) | 1 (cuda__probe_hash_delete) | none | not dumped | not: plain load and store, ANY, delete. The verdict covers all of the object's programs together; each of the three programs alone also fails |
| pt_hash cnt (all 7 builds: thread kc, thread ksm, block kc, block kblk, leader kc test, leader kc llama4, block kc rms) | h (1) | null test, XADD | NOEXIST | none | none | `atom.add.u64`, generic, dest dead (thread kc) | **stage-one** |
| pt_hash pre (thread kc test, thread kc rms) | h (1) | null test, XADD | none (the loader prefills) | none | none | not dumped | **stage-one** |
| pt_hash rmw | h (1) | null test, XADD | EXIST with the lookup pointer as the value; NOEXIST on a miss | none | pointer passed to helper 2 (r3) | `atom.add.u64`, dest dead; update param2 = the lookup register | not: EXIST, escape |
| pt_hash tid | h (1) | exit: plain load (`s && *s`) | ANY (entry) | 1 (exit) | none | u32 pair, or-ed for the nonzero test | not: plain load, ANY, delete |
| pt_hash tidnd | h (1) | exit: plain load | ANY (entry) | none | none | not dumped | not: plain load, ANY |
| pt_hash upd | h (1) | no lookup | ANY | none | none | not dumped | not: ANY |
| pt_hash del | h (1) | no lookup | none | 1 | none | not dumped | not: delete |
| pt_hash miss | h (1) | null test only; the key is never inserted | none | none | none | not dumped | meets the rule vacuously: no access, no insert |
| pt_hash none | h (1) | no h operation at all | none | none | none | not dumped | not applicable |

pt_hash's `chk` is a GPU array (1503, created as 1513), not a hash map. Its accesses are all XADD (`atom.add.u64`, dest dead in the PTX) and are left out of the table. gpu_shared_map's `counter` (1503) is left out too.

## Evidence

### Bytecode (`llvm-objdump-15 -dr`, in `bytecode/`)

Built objects are the examples' `.output/*.bpf.o` (clang 15.0.7 per the `.tmp.bpf.o` DWARF producer). The three objects not built here were compiled with `bytecode/compile-flags.txt`: build_hs.sh's flags, with bpftime-proto-70f7's `third_party` headers.

- **cuda-counter.** `call_count`, an XADD from `__atomic_add_fetch`:

  ```
  26: call 1                      (r1 = call_count)
  27: if r0 == 0 goto +2
  28: lock *(u64 *)(r0 + 0) += r6
  37: call 2                      (r1 = call_count, r4 = 1)
  ```

  `total_time_ns` and `start_ts`, in the kretprobe:

  ```
  14: call 1   (start_ts, key r10-8 = block x)   15: r6 = r0
  19: r1 = *(u64 *)(r6 + 0)
  27: call 3   (start_ts, key r10-28 = pid)
  31: call 1   (total_time_ns, key pid)
  33: r1 = *(u64 *)(r0 + 0)   34: r1 += r7   35: *(u64 *)(r0 + 0) = r1
  40: r3 = r0   41: r4 = 2   ... 76: call 2       (miss path: r3 = r10-40, r4 = 1)
  105: r5 = *(u64 *)(r6 + 0)                  (printk argument, after the delete)
  ```

- **threadscheduling, and threadscheduling_dynamic_hook** (`__sync_fetch_and_add`). The two objects have identical instructions: a diff of the disassembly without relocations prints nothing. Only the map types (1 against 1501) and the kernel name differ.

  ```
  56: call 1 (sm_histogram)    57: if r0 == 0 goto +3   58: r1 = 1   59: lock *(u64 *)(r0 + 0) += r1   69: r4 = 1  70: call 2
  79: call 1 (warp_histogram)  80: if r0 == 0 goto +3   81: r1 = 1   82: lock *(u64 *)(r0 + 0) += r1   92: r4 = 1  93: call 2
  49: r4 = 0  50: call 2 (thread_mapping, value r10-136, 80 bytes)
  ```

  The raw encoding of the add is `db 10 00 00 00 00 00 00`: BPF_STX | BPF_ATOMIC | BPF_DW with imm 0, which is BPF_ADD without BPF_FETCH.

- **th-hostcall:**

  ```
  6: call 1   7: if r0 == 0 goto +2   8: r1 = 1   9: lock *(u64 *)(r0 + 0) += r1
  ```

- **cudagraph:**
  - kprobe: `43: call 1 (call_count)`, `46: r1 = *(u64 *)(r0 + 0)`, `47: r1 += 1`, then `55: r4 = 0  56: call 2`.
  - kretprobe: `33: call 1 (start_ts)`, `38: r8 = *(u64 *)(r7 + 0)`, `44: call 3`, `49: call 1 (total_time_ns)`, `51: r1 = *(u64 *)(r0 + 0)`, `61: r4 = 0  62: call 2`.
- **gpu_shared_map:** `63: r1 = counter_per_thread ll  65: r4 = 0  66: call 2`. No lookup of it.
- **micro:**
  - cuda__probe_hash_lookup: `70: call 1  72: r1 = *(u64 *)(r0 + 0)  73: r1 += 1  74: *(u64 *)(r0 + 0) = r1`;
  - cuda__probe_hash_update: `60: r4 = 0  61: call 2`;
  - cuda__probe_hash_delete: `83: call 3`.
- **pt_hash cnt** (hs_cnt_thread_kc_test, kdiv exit; the other cnt builds differ only in the key and the rate filter):

  ```
  14: r1 = h ll   16: call 1   17: if r0 == 0 goto +12   20: lock *(u64 *)(r0 + 0) += r1
  45: r1 = h ll   47: r4 = 1   48: call 2
  ```

  The remaining calls are on `chk`.
- **pt_hash pre** (hs_pre_thread_kc_test, kdiv exit): `16: call 1 (h)  17: if r0 == 0 goto +11  20: lock *(u64 *)(r0 + 0) += r1`. The object has no `call 2` and no `call 3` at all; its other calls are lookups of `chk`.
- **pt_hash rmw** (kdiv):

  ```
  16: call 1 (h)  17: r6 = r0  18: if r6 == 0 goto +28  21: lock *(u64 *)(r6 + 0) += r1
  34: r3 = r6   35: r4 = 2   36: call 2 (h)          (miss path 64: r4 = 1  65: call 2)
  ```

  pt_hash tid: entry `35: r4 = 0  36: call 2 (h)`; exit `41: call 1 (h)  43: r1 = *(u64 *)(r0 + 0)`, then `69: call 3 (h)`.

**Analyzer.** `tools/analyze_bpf.py` follows every path of each function and tracks lookup pointers through moves and stack spills. Its outputs are in `analysis/` and `analysis/pt_hash/`.
- It reports each access, null test, escape, update (with its flags) and delete.
- It reported no unmodelled instruction, no fetch/xchg/cmpxchg, no spill, no store of a pointer and no pointer arithmetic in any of the 23 objects (7 examples and 16 pt_hash builds), for any map, `chk` and the arrays included.
- Its only escapes are the three helper-2 value arguments: cuda-counter pc 76, and rmw pc 36 in each of its two kernels. This was checked with `grep -rh "escape\|arith\|note" analysis/`.
- The objdump listings above were read by hand as well.

### PTX (dumps of the patched modules, build tvf-7d02d677, `.target sm_87`, in `ptx/`; probe functions extracted to `ptx-probes/`)

Helper calls are `_bpf_helper_ext_000N(param0..4)`, with param0 = the map fd as a `mov.u64` constant. Fds come from libbpf's lines in `runs/c1-{cc,cg}/server.log` (for example `libbpf: map 'call_count': created successfully, fd=13`) and from the agent's "Copying map fd" lines (`runs/c1-*/client.log`), told apart by key size, value size and entries. In every function the sequence of helper calls also matches the bytecode one to one.
- cc and cg: start_ts 3, call_count 13, total_time_ns 14;
- ts: thread_mapping 3 (value 80), sm_histogram 13 (128 entries), warp_histogram 14;
- th: call_count 3;
- pt: h 4 (type 1, 262144 entries), chk 14 (1513).

Line numbers are lines of the dump file. For a call, the number is the line of its `// callseq` block.

- **cuda-counter** (`ptx/c1-cc/c8a3f290…ptx`):
  - `__probe_func__`, call_count:

    ```
    3005 mov.u64 %rd35, 13;   3006 call _bpf_helper_ext_0001 -> %rd37
    setp.eq.s64 %p1, %rd37, 0;  @%p1 bra $L__BB0_2;
    3032 atom.add.u64 %rd38, [%rd37], 1;              (%rd38 not used again)
    3036 call _bpf_helper_ext_0002(%rd35=13, ..., %rd34=1)
    ```

  - `__retprobe_func__`, start_ts (fd 3) and total_time_ns (fd 14):

    ```
    2620 call _bpf_helper_ext_0001(fd 3) -> %rd26
    2670 ld.u32 %rd32, [%rd26];  2671 ld.u32 %rd33, [%rd26+4];  shl.b64/or.b64
    2677 call _bpf_helper_ext_0003(fd 3)
    2702 call _bpf_helper_ext_0001(fd 14) -> %rd88
    2728 ld.u32 %rd45, [%rd88];  2729 ld.u32 %rd46, [%rd88+4];  ... add.s64
    2733 st.u32 [%rd88], %rd49;  2734 shr.u64;  2735 st.u32 [%rd88+4], %rd50;
    2736 mov.u64 %rd87, 2;  ...  2785 call _bpf_helper_ext_0002(fd 14, key, %rd88, %rd87)   (miss: %rd88 = stack, %rd87 = 1)
    2829 ld.u32 %rd64, [%rd26];  2830 ld.u32 %rd65, [%rd26+4];    (start_ts re-read after the delete, printk)
    ```

- **threadscheduling** (`ptx/c1-ts/dfd6935f…ptx`):

  ```
  2738 mov.u64 %rd88, 0;   2739 call _bpf_helper_ext_0002(fd 3 thread_mapping, ..., %rd88)
  2766 call _bpf_helper_ext_0001(fd 13) -> %rd67   2792 atom.add.u64 %rd69, [%rd67], 1;
  2795 mov.u64 %rd88, 1;   2798 call _bpf_helper_ext_0002(fd 13, ..., %rd88)
  2829 call _bpf_helper_ext_0001(fd 14) -> %rd80   2855 atom.add.u64 %rd81, [%rd80], 1;
  2858 mov.u64 %rd82, 1;   2861 call _bpf_helper_ext_0002(fd 14, ..., %rd82)
  ```

- **th-hostcall** (`ptx/c1-th/a69b9455…ptx`, the rms_norm module of llama.cpp):

  ```
  2561 mov.u64 %rd5, 3;  2562 call _bpf_helper_ext_0001 -> %rd9
  2588 atom.add.u64 %rd10, [%rd9], 1;
  ```

- **cudagraph** (`ptx/c1-cg/4aa12824…ptx`):
  - probe: `2990 call ext_0001(fd 13) -> %rd46`, then `3017 ld.u32 %rd47, [%rd46]` and `3018 ld.u32 %rd48, [%rd46+4]`, then `3025 call ext_0002(fd 13, …, %rd29=0)`.
  - retprobe: `2665 ext_0001(fd 3) -> %rd36`, `2715/2716 ld.u32 [%rd36]`, `[%rd36+4]`, `2719 ext_0003(fd 3)`, `2745 ext_0001(fd 14) -> %rd51`, `2771/2772 ld.u32 [%rd51]`, `[%rd51+4]`, `2779 ext_0002(fd 14, …, %rd15=0)`.
- **pt cnt** (`ptx/c1-ptcnt/ab261ae0…ptx`, kdiv retprobe):

  ```
  2829 mov.u64 %rd18, 4;  2830 call _bpf_helper_ext_0001(h) -> %rd22
  2856 atom.add.u64 %rd23, [%rd22], 1;
  2917 call _bpf_helper_ext_0002(%rd18=4, ..., %rd13=1)
  ```

  The kflat retprobe is the same at 2597/2623/2685.
- **pt rmw** (`ptx/c1-ptrmw/5263a474…ptx`, kdiv): `2886 ext_0001(fd 4) -> %rd23`, `2912 atom.add.u64 %rd24, [%rd23], 1;`, `2942 mov.u64 %rd32, 2;`, `2943 ext_0002(%rd19=4, %rd5, %rd23, %rd32)`. The lookup register is the value argument.
- **pt tid** (`ptx/c1-pttid/5f550aa7…ptx`, kdiv): `3194 ext_0001(fd 4) -> %rd46`, `3220 ld.u32 %rd47, [%rd46+4];`, `3221 ld.u32 %rd48, [%rd46];`, `3222 or.b64`, `3286 ext_0003(fd 4)`. Entry: `3379 mov.u64 %rd7, 0;`, `3491 ext_0002(fd 4, …, %rd7)`.
- **Tally over the extracted probe functions:**
  - no `ld.u64` or `st.u64` on a generic address, no `ld.u8`/`ld.u16`, no `red.`, no `cvta.to.global`;
  - the only `.global` access is the probe gate `ld.global.u32 %bpftime_on, [__bpftime_probes_enabled]`;
  - `ld.u32`: cc 6, cg 6, tid 4; `st.u32`: cc 2;
  - `atom.add.u64`: cc 1, ts 2, th 1, ptcnt 8, ptrmw 10, pttid 8. Two sites each in ptcnt and ptrmw are on `h` (one per kernel), none in pttid; the rest are on `chk`.
  - The destination of each `atom` has no other mention in its function. This was checked on the raw dump text by `tools/atom_dest_check.py`; its output is `analysis/ptx-atom-dest-check.txt` (30 sites, all 0).

### The runs

`tools/run_dumps.sh`. Each case ran through `run.sh` with `GPU_BUILD=proto/builds/tvf-7d02d677`, `SPDLOG_LEVEL=info` and `BPFTIME_CUDA_PTX_DUMP_DIR=c1/ptx/<run>`, under `timeout 400`. Commands:
- cc, ts, cg: copied from fix-matrix.sh (`cc-legacy`, `ts-legacy`, `cg-default`);
- th: the llama-bench pp32 command of `llama-census-20261113/runs/llama-hostcall-pp32`;
- pt: as `hash-current` h1, i.e. `./pt_victim 1 0` and `./pt_loader bpf/<obj> runs/<name>`, with `BPFTIME_MAP_GPU_THREAD_COUNT=131072` for both server and victim.

Results:

| run | client_rc | client_ms | server_rc | dumps | sanity |
|---|---:|---:|---:|---:|---|
| c1-cc | 0 | 685 | 0 | 1 | C[1] 3.00 |
| c1-ts | 0 | 536 | 0 | 1 | |
| c1-cg | 0 | 5,604 | 0 | 1 | calls 6 |
| c1-th | 0 | 29,759 | 0 | 1 | count 1,411,072 |
| c1-ptcnt | 0 | 2,103 | 0 | 1 | h sum 95,233 |
| c1-ptrmw | 0 | 3,127 | 0 | 1 | h sum 50,299 |
| c1-pttid | 0 | 4,105 | 0 | 1 | |

Afterwards there were no bpftime segments in `/dev/shm` and no leftover loader or victim processes. Run dirs: `runs/c1-{cc,ts,cg,th,ptcnt,ptrmw,pttid}`.

## Not determined

- **PTX for gpu_shared_map, threadscheduling_dynamic_hook and micro:** not dumped, because none of them is built here.
  - dynamic_hook's instructions equal threadscheduling's, so its PTX is very likely the same as c1-ts's. That is an inference.
  - micro's `(*val)++` is very likely a u32-pair load and store, as cudagraph's. Also an inference.
- **PTX for the pt_hash builds not run** (cnt other than thread kc, pre, tidnd, upd, del, miss, none): bytecode only. Their h accesses are the same instruction forms as the dumped ones.
- **The cause of the u32 pairs** is inferred from the source, not checked in an IR dump.
- **What a fetch form or a used result would become:** no program has one, so it was not generated. A synthetic test was not made (not asked).
- **SASS:** what ptxas makes of the dead-destination `atom` (`RED` or `ATOM`), and of the u32 pairs, is C3's part. It was not looked at here.
- **Not covered:**
  - rocm-counter (ROCm);
  - cuda_api_trace (a CPU uprobe);
  - micro's `cuda_probe_kernel_gpu.bpf.c`: its map is `kernel_gpu_shared_array`, not a hash.

## Files (all under `proto/hashdesign/c1/`)

- `bytecode/*.objdump.txt`, `bytecode/pt_hash/*.objdump.txt`: disassembly.
- `bytecode/{gpu_shared_map,threadscheduling_dynamic_hook,micro_cuda_probe}.bpf.o` and `bytecode/compile-flags.txt`: the three objects compiled here.
- `analysis/*.txt`, `analysis/pt_hash/*.txt`: the analyzer's per-access report.
- `ptx/c1-*/<sha256>.ptx`: the dumped patched modules. `ptx/c1-*.runsh.txt`: run.sh output.
- `ptx-probes/c1-*.txt`: the probe `.func` bodies, with call sequences condensed and dump line numbers.
- `analysis/ptx-atom-dest-check.txt`: every `atom` in the probe functions of the 7 dumps, with its destination's other mentions.
- `tools/analyze_bpf.py`, `tools/extract_probes.py`, `tools/atom_dest_check.py`, `tools/run_dumps.sh`.
