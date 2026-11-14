# step0 e8: packaging limits (compile and load only)

Check 8 of `/home/jetson/src/results/gpu/dir1-design-space-20261115/README.md` section 5 (decides for B and D):

> 8. **Packaging limits** (B, D; compile and load only).
>    - A `.global` appended to a patched llama.cpp module loads, and cuLibraryGetGlobal finds it.
>    - Constant memory left next to map_info's 40 KiB.

D's per-module scratch is 288 KiB, or 2.8 MiB for 10 modules. B's pools reach 32 MiB and more (note section 3). The sizes
tested are 288 KiB, 2.8 MiB, 32 MiB, 256 MiB and 1 GiB.

## Files

| file | what |
|---|---|
| `get_llama_ptx.sh` | one threadhist-llama run on llama-bench pp32 (graphs off) with `BPFTIME_CUDA_KEEP_PTX=1`, `BPFTIME_MAP_GPU_THREAD_COUNT=262144`, through run.sh; copies the patched PTX of the module with rms_norm, the trampoline and map_info to `ptx/` |
| `ptx/rmsnorm-module-smoke1.1.patched.ptx` | that module, as bpftime patched it (599,593 B; `.target sm_61` before bpftime's rewrite), plus the unpatched original |
| `e8_load.cpp` | standalone loader: driver API + nvPTXCompiler (static, CUDA 13.2), no bpftime |
| `run_all.sh`, `summarize_e8.py` | every case in its own process, then the decision rules |
| `Makefile` | builds `e8_load` |

## Method

`e8_load PTX MODE N` prepares the PTX as bpftime does:
- `rewrite_ptx_target`: the first `.target` becomes `sm_87`; `.version` is unchanged for sm_87
  (attach/nv_attach_impl/nv_attach_utils.cpp:31).
- It compiles with **nvPTXCompiler** and bpftime's options `--gpu-name=sm_87 --verbose -O3`
  (attach/nv_attach_impl/nv_attach_fatbin_record.cpp:135-162).
- It loads the resulting cubin with **cuLibraryLoadData**, as the agent's hook loads the patched image
  (nv_attach_impl.cpp:2411-2457).
- The base cubin is 355,776 B, exactly the size of the image the agent loaded in the llama run ("loaded the patched image
  ... (355776 bytes ...)" in `runs/step0-e8-llamaptx-smoke1/client.log`). So this path reproduces bpftime's.
- `--jit` instead hands the PTX text to cuLibraryLoadData, so the driver compiles it.

Modes:
- `base`: nothing appended. It is the reference: `map_info` must be found with 40,960 B. It also runs the **negative
  control**: `cuLibraryGetGlobal` on a bogus name must return CUDA_ERROR_NOT_FOUND (500).
- `plain`: `.global .align 8 .b8 __bpftime_step0_scratch[N];` appended, the string exactly as the task gives it (not
  `.visible`, and referenced by nothing).
- `visible`: the same with `.visible`.
- `kernel`: `.visible`, plus an appended kernel `__bpftime_step0_touch`, 32 x 128 threads, that takes the symbol's
  address (`mov.u64 %rd, __bpftime_step0_scratch`) and writes a pattern at 4,096 spread words, the first and last included.
  Thread 0 stores the address it saw.
- `constpad` (compile only): `.visible .const .align 8 .b8 __bpftime_step0_cpad[N];`, read by an appended kernel through
  `ld.const`.

For each found global, the program checks the size `cuLibraryGetGlobal` reports against N. Then:
1. `cuMemsetD8(0xA5)` over all N bytes, read back in 64 MiB chunks with `cuMemcpyDtoH`.
2. In `kernel` mode: launch the kernel, then read back the whole region again. The written words must carry the pattern,
   every other byte must still be 0xA5, and the address the kernel saw must equal the found pointer.

Constant memory comes from the verbose compile log (`ptxas info: ... bytes gmem, ... bytes cmem[3], ... bytes cmem[4]`,
plus the per-kernel cmem[0] and cmem[2]), `cuobjdump -res-usage` and `cuobjdump -elf`. The remainder of bank 3 is then
confirmed by compiling a `.const` of exactly 65,536 - cmem[3] bytes and one of 8 B more.

Before the 256 MiB and 1 GiB cases, `run_all.sh` checks MemAvailable >= N + 1 GiB and skips the size otherwise.

## Decision rules (written before the real run)

- **The test is valid** only if `base` loads, `map_info` is found with 40,960 B, and the negative control returns 500.
  Otherwise everything is **inconclusive**.
- **Packaging works at N** if `plain`, or `visible` + `kernel`, at N meets four conditions. It compiles (rc 0) and loads
  (rc 0). It is found with size == N. The host memset and readback show 0 mismatched bytes. In `kernel` mode, the kernel's
  symbol address equals the found pointer, and its readback has 0 wrong words.
  - A compile, load or lookup failure at N means **no** at N.
  - A skip for memory means **untested** at N.
- The answer is the largest working N. If `plain` fails where `visible` works, the declaration must be `.visible`.
- `summarize_e8.py` applies these rules: `decision.valid`, `decision.per_N` (yes / no / untested), the largest yes, and
  `constant_left_confirmed` (the two `constpad` compiles). With an invalid base or control, `per_N` is inconclusive
  (gating added in review, before the real run).
- **Constant memory left** = 65,536 - cmem[3] of `base`. The number stands if a `.const` of exactly that size compiles and
  8 B more fails.

## Commands (main session)

```bash
cd /home/jetson/src/bpftime-gpu-work/step0/e8-package
timeout -k 5 300 ./get_llama_ptx.sh   # only if a fresh module is wanted; ~35 s here (agent start-up ~4 s, pp32 ~17 s)
timeout -k 5 600 ./run_all.sh         # newest ptx/*.patched.ptx; ~30-60 s
```
Results go to `results/run-<TS>/`: `results.jsonl`, `base.info.txt` (the compile log), `base.res-usage.txt`,
`base.elf-sections.txt`, `summary.json` and `summary.txt`. `get_llama_ptx.sh` deletes only the
`/tmp/bpftime-fatbin-work.*` dirs its own agent log names, and only after copying.

## Smoke results (2026-11-15, board shared with other agents; load and size checks only, nothing timed)

`results/run-smoke2/summary.txt`. The test is valid: base loads, `map_info` has 40,960 B, and the bogus name returns 500.

| N | plain | visible | visible + kernel |
|---|---|---|---|
| 288 KiB (294,912) | found, size ok, r/w ok | same | same; kernel address matches, 4,096 words ok |
| 2.8 MiB (2,936,016) | ok | ok | ok |
| 32 MiB | ok | ok | ok (also through the driver-JIT path) |
| 256 MiB | ok | ok | ok |
| 1 GiB | ok | ok | ok |

- Packaging works up to 1 GiB, the largest size tested. The declaration exactly as given (not `.visible`, unreferenced) is
  kept and found.
- `cuLibraryLoadData` returns in about 0.3 ms, and free memory does not move until the first `cuLibraryGetGlobal`. With
  lazy loading the module, and its `.global`, are created at the first symbol lookup.
  - bpftime's hook does such a lookup at once (`write_trampoline_globals`), so the agent pays for the allocation at load.
  - The free-memory drops in `summary.txt` are informational: cuMemGetInfo on this iGPU reports system memory, which other
    processes move.

Constant memory of the patched rms_norm module (`base.info.txt`, `base.res-usage.txt`, `base.elf-sections.txt`):

| bank | bytes | what |
|---|---|---|
| cmem[3] (module `__constant__`, the 64 KiB bank) | 40,968 | `map_info[1024]` 40,960 + `constData` 8; llama.cpp's own norm module has no `.const` |
| cmem[4] | 144 | 18 relocated 8-B addresses of globals (`.rel.nv.constant4`, 0x120 B of relocations) |
| cmem[0], per kernel | at most 580 | kernel parameters |
| cmem[2], per kernel | at most 24 | compiler constants, in 3 of 40 kernels |
| gmem (`.nv.global.init` + `.nv.global`) | 27,055 | trampoline globals and strings |

- **Constant memory left in bank 3: 24,568 B.** A `.const` of 24,568 B compiles (cmem[3] = 65,536). One of 24,576 B fails
  with `ptxas error: File uses too much global constant data (0x10008 bytes, 0x10000 max)`.
- 24,568 B is room for 614 more 40-B `MapBasicInfo` entries, or a 3,071-entry table of 8-B pointers. B's
  64-entry per-module table fits; doubling `map_info` does not.

## Known limits

- One module: the one with rms_norm. Modules whose kernels use their own `__constant__` data have less room in bank 3 (this
  module has none).
- Load and lookup only. No probe in this program uses the appended global. The `kernel` mode shows that code in the same
  module reaches it through the symbol, including near 1 GiB.
- One process per size, so loading several modules with large globals, as 10 patched modules would, is not tested. Each
  `.global` is ordinary device memory taken from the shared 7.36 GiB.
- The driver-JIT path was tried at 32 MiB only.
- `get_llama_ptx.sh` ran once, and its selection pattern was then corrected (patched entries read `.entry`, without
  `.visible`). The review re-ran the corrected script end to end (`TS=review1`, 30 s, threadhist sum 705,536): it selected
  one module, whose patched and unpatched PTX are byte-identical to the hand-copied `smoke1` files, and the /tmp count came
  back to 196. `run_all.sh` takes the newest `ptx/*.patched.ptx`, now `review1` (the same bytes).
- No control shows that the load path can fail for size: a `.global` beyond physical memory was not tried, because on this
  shared board the allocation could start the OOM killer. Which call reports an allocation failure (load or the first
  `cuLibraryGetGlobal`) is therefore unknown.
- The 1 GiB case depends on free memory at the time; `run_all.sh` skips it below N + 1 GiB.
