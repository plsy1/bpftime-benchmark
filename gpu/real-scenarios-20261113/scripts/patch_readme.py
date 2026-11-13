import sys
P = sys.argv[1]
s = open(P).read()


def rep(a, b):
    global s
    n = s.count(a)
    assert n == 1, (n, a[:90])
    s = s.replace(a, b)


rep("""- Where a verifier corrected or struck a claim, the corrected version is used.
- The workloads verifier's output reached us cut off partway through the Chalmers item. Section 5 lists the claims that therefore have no verdict.""",
"""- Where a verifier corrected or struck a claim, the corrected version is used.
- The synthesizer saw the workloads verifier's output cut off. Both verifiers' full verdicts were read afterwards: scenarios 60 claims (47 confirmed, 12 corrected, 1 struck), workloads 41 claims (22 confirmed, 19 corrected). Corrections the synthesizer had missed are applied in this copy and listed in section 5, item 14.
- Raw output: `workflow-output.json` (`surveys[*].out` readers, `surveys[*].verify` verdicts, `synth`). Script: `scripts/survey-workflow.js`.""")

rep("""On the Orin each costs about 10 us, and the calls of different threads run one after another ([PROTO] 36-41).""",
"""On the Orin each costs 9.8-15.6 us with the shipped per-module lock, 11-22 us per call in 32-lane bursts, and about 70 us for launches the watcher is not told about, such as CUDA-graph launches. The calls of different threads run one after another ([PROTO] 36-41; [WP] 90, 113, 180).""")

rep("""roughly 113-120 s per call site [inferred from [PROTO]].""",
"""roughly 113-180 s per call site [inferred from [PROTO]].""")

rep("""   - cuBLAS and cuDNN cannot be patched here: they carry PTX only for sm_120/sm_121 ([CUOBJ]; [NVA] 2245-2263). They run 45-76% of GPU kernel time in PyTorch LLM inference on Orin ([ICDCS] §V Fig. 9).""",
"""   - cuBLAS, cuDNN and cuRAND cannot be patched here: they carry PTX only for sm_120 or sm_121 ([CUOBJ]; [NVA] 2245-2263). cuBLAS GEMMs run 45-76% of GPU kernel time in PyTorch LLM inference on Orin ([ICDCS] §V Fig. 9).
   - A few vendor entries would pass the pick rule: cuBLASLt 1 of 290 (`libcublasLt.268.sm_75.ptx`), cuSPARSE 8 sm_86 of 155, cuSOLVER 3 sm_80 + 3 sm_86 of 263, TensorRT's libnvinfer 85. Which kernels they hold is unknown ([CUOBJ]).""")

rep("""- **Vendor-library kernels (cuBLAS, cuBLASLt, cuDNN).** Their PTX targets only sm_120/sm_121 in this toolkit.""",
"""- **Vendor-library kernels (cuBLAS, cuBLASLt, cuDNN).** Apart from one sm_75 entry in cuBLASLt, their PTX targets only sm_120/sm_121 in this toolkit.""")

rep("""- **Host-call times** use dev's measured 9.8-10.4 us per call for warps ([PROTO] 36-41). At that cost the serialized ceiling is about 102,000 host calls per second [inferred].""",
"""- **Host-call times** use dev's measured 9.8-15.6 us per call ([PROTO] 36-41; 11-22 us in 32-lane bursts, [WP]). At that cost the serialized ceiling is about 64,000-102,000 host calls per second [inferred]. Launches under CUDA graphs would pay about 70 us per call ([WP] 180).""")

rep("""about 115M calls, roughly 19-20 min per prefill for RoPE alone [inferred]""",
"""about 115M calls, roughly 19-30 min per prefill for RoPE alone [inferred]""")
rep("""On RoPE at pp512 that is about 11.5M calls, roughly 113-120 s per prefill [inferred]. With a thread-0 guard""",
"""On RoPE at pp512 that is about 11.5M calls, roughly 113-180 s per prefill [inferred]. With a thread-0 guard""")
rep("""About 58M calls, roughly 9.5-10 min per prefill on RoPE at pp512 [inferred]""",
"""About 58M calls, roughly 9.5-15 min per prefill on RoPE at pp512 [inferred]""")
rep("""One per thread per launch is about 11.5M calls, roughly 113-120 s per prefill per printk site on RoPE at pp512 [inferred]""",
"""One per thread per launch is about 11.5M calls, roughly 113-180 s per prefill per printk site on RoPE at pp512 [inferred]""")

rep("""- At real geometry, the per-thread host calls of a CPU-style port cost minutes per prefill.""",
"""- At real geometry, the per-thread host calls of a CPU-style port cost minutes per prefill.
- So the per-tool shares are 100% or 0% by construction: a tool keeps its state in thread-indexed maps or it does not, and it uses CPU maps and printk or it does not. They do not decide how far to go. What does is measured per application: coverage (the share of GPU time in attachable kernels), firings per call site at real launch geometry, the cost share of each direction in the slowdown, and thread-indexed capacity against use and loss.""")

rep("""6. Two parts are unverified:
   - `nsys` and `ncu` are present at /usr/local/cuda/bin, but whether kernel tracing and SASS metrics work on this Tegra is unknown.
   - If they do not, a CPU uprobe on `cuLaunchKernel`/`cudaLaunchKernel` that reads the grid and block arguments can replace the census.""",
"""6. Tools:
   - `nsys` 2026.3.1 (`/opt/nvidia/nsight-systems/2026.3.1/bin/nsys`) records each launch with grid, block and GPU time on this board. Checked on a test program that exits on its own (`nsys-check/`).
   - `ncu` SASS metrics are untested.
   - If tracing fails on llama.cpp, a CPU uprobe on `cuLaunchKernel`/`cudaLaunchKernel` that reads the grid and block arguments can replace the census.""")

rep("""   - Use pp32: 720,896 thread executions per prefill, about 7 s per site and about 70 s for cuda-counter [inferred].""",
"""   - Use pp32: 720,896 thread executions per prefill, about 7-11 s per site and about 70-110 s for cuda-counter [inferred].""")
rep("""2. Bound every run. One per-thread call site at pp512 on RoPE takes about 113-120 s per prefill [inferred].""",
"""2. Bound every run. One per-thread call site at pp512 on RoPE takes about 113-180 s per prefill [inferred].
   - Run with `GGML_CUDA_DISABLE_GRAPHS=1`: the host-call watcher is not told about graph launches, which then pay about 70 us per call ([WP] 180).""")
rep("""4. Report host calls per second against the ceiling of about 102,000 per second [inferred].""",
"""4. Report host calls per second against the ceiling of about 64,000-102,000 per second [inferred].""")

rep("""- Before timing, check jetson_clocks: cpu5 should be at 1728000.""",
"""- Before timing, check jetson_clocks: cpu5 should be at 1728000 and the GPU at 1020000000.
- Never measure from 03:25 to 04:15 JST (the daily timer).""")

rep("""  - The RoPE kernel that gpu_ext probed launches 4 x pp x 256 threads, where pp is the prompt length in tokens: 524,288 threads at pp512. It runs 22 launches per prefill. The 44 launches gpu_ext counted are a warm-up pass plus one timed pass [inferred by the verifier from llama-bench defaults] ([GX-pre]; run_table1_perf.py:71-84).""",
"""  - The RoPE kernel that gpu_ext probed launches 4 x pp x 256 threads, where pp is the prompt length in tokens: 524,288 threads at pp512. It runs 22 launches per prefill. The 44 launches gpu_ext counted are a warm-up pass plus one timed pass [inferred by the verifier from llama-bench defaults] ([GX-pre]; run_table1_perf.py:71-84).
  - That kernel is the K-only RoPE (4 KV heads). In it, 224 of every 256 threads return at once (head dimension 64), yet each still passes the exit probe and owns a per-thread slot. The Q RoPE of the same model (32 heads) launches 8 times as many threads, 4,194,304 at pp512, so one fixed thread count cannot fit both kernels even at one prompt length [inferred from llama.cpp rope.cu].""")

rep("""7. **nsys and ncu kernel tracing on this Tegra** is unverified.""",
"""7. **nsys kernel tracing works on this board** (checked 11-13, `nsys-check/`). ncu SASS metrics are untested.""")

rep("""10. **Claims whose verifier verdict was not received** (the workloads verifier output is cut off). Other items from the workloads survey were re-checked by the row verifiers (cuBLAS/cuDNN PTX, `pick_patched_ptx`, the MMQ rule, TaxBreak, PASTA, PKA, FAISS, CUTLASS, Neutrino) or by the scenarios verifier. These four were not:
    - the PTX counts of cuRAND and cuSOLVER;
    - that cuBLAS, cuBLASLt and cuDNN do not link libcudart (libnvinfer's was checked);
    - NVIDIA's forum statement that Orin runs sm_80 SASS without JIT;
    - the workloads survey's closing synthesis.""",
"""10. **Claims the synthesizer listed as unverified are now settled** (full verifier output plus checks on 11-13):
    - cuRAND carries PTX only for sm_121 (10 entries). cuSOLVER carries 3 sm_80 and 3 sm_86 entries of 263 ([CUOBJ] `listings/summary.txt`).
    - cuBLAS, cuBLASLt, cuDNN (all five libraries) and libnvinfer do not link libcudart or libcuda (`readelf -d`, `listings/summary.txt`). Whether dev's driver-level `cuLibraryLoadData` hook sees their loads is still untested.
    - NVIDIA's forum statement that Orin runs sm_80 SASS without JIT: confirmed (forum 375642).
    - The workloads survey's closing synthesis: corrected to 11.5M thread executions per prefill (not 23M), and to "45-76% of kernel time is invisible", not "mostly invisible".""")

rep("""14. **Corrections applied from the verifiers:**""",
"""14. **Corrections applied from the verifiers.** Applied after the synthesis, from the full workloads verdicts:
    - Host-call cost is 9.8-15.6 us with the shipped per-module lock (11-22 us in 32-lane bursts), not 9.8-10.4 us. All per-prefill times above were recomputed.
    - "Vendor libraries carry no PTX usable for sm_87" overstated it: cuBLASLt, cuSPARSE, cuSOLVER and libnvinfer have a few entries at or below sm_87.
    - Under llama.cpp's default CUDA graphs, host calls would be served at about 70 us per call.
    - The K-only RoPE kernel and its 87.5% immediately-returning threads (W1).

    Applied by the synthesizer:""")

rep("""15. """, """16. """) if "\n15. " in s else None

rep("""## Sources (keys used above)""",
"""15. **Which launchlate gpu_ext's P40 number (14%) used** is not identified. gpu_ext's later RTX 5090 version (revision-rq4, 575-01) runs in thread 0 and makes no host calls. If the P40 run used a copy of bpftime's example, it made a CPU ARRAY lookup and a printk per thread, which would be the only published real-application cost of direction-2 operations. Check gpu_ext's history before citing either way.

## Sources (keys used above)""")

rep("""- [CUOBJ] `cuobjdump --list-ptx/--list-elf` outputs in /tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/scen-work/ and /tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/wl-survey/.""",
"""- [CUOBJ] `listings/` in this folder: `cuobjdump --list-ptx` output per library, and `summary.txt` (PTX and SASS entries by arch, entries at or below sm_87, NEEDED), made by `scripts/vendor_summary.py` from those listings and the ELF listings (not kept; regenerate with `cuobjdump --list-elf`).
- [WP] /home/jetson/src/results/gpu/watcher-policy-20261112/README.md:90, 113, 180 (32-lane bursts 11-22 us per call; about 70 us for launches the watcher is not told about).""")

rep("""- Checkout: /tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/survey-src/gpu_ext. Paths below are relative to workloads/llama.cpp/observability_overhead/ in that checkout.""",
"""- https://github.com/eunomia-bpf/gpu_ext at commit 50a51ad4 (2026-09-24, master head when read). Paths below are relative to workloads/llama.cpp/observability_overhead/ in that repository.""")

open(P, 'w').write(s)
print('saved')
