# GPU eBPF in real applications: scenarios, workloads, and how to measure the two directions' share (2026-11-13)

The advisor asked two questions:
- Which mainstream usage scenarios exist for running code inside GPU kernels?
- In real applications, not microbenchmarks, how large a share do the operations of direction 1 and direction 2 take?

This report merges two verified surveys and proposes one measurement round on the Orin to answer the second question.

**Nothing in this report is a measured share.** None of the sources we read measures one. Every volume below is arithmetic from published launch geometry and is marked [inferred].

How the verifier results are used:
- Where a verifier corrected or struck a claim, the corrected version is used.
- The synthesizer saw the workloads verifier's output cut off. Both verifiers' full verdicts were read afterwards: scenarios 60 claims (47 confirmed, 12 corrected, 1 struck), workloads 41 claims (22 confirmed, 19 corrected). Corrections the synthesizer had missed are applied in this copy and listed in section 5, item 14.
- Raw output: `workflow-output.json` (`surveys[*].out` readers, `surveys[*].verify` verdicts, `synth`). Script: `scripts/survey-workflow.js`.

## Terms used in this report

**Probes and launches**
- **Probe**: an eBPF program that bpftime patches into a GPU kernel. It runs at one of three places ([PASS]):
  - kernel entry (kprobe);
  - kernel exit (kretprobe);
  - after matched load/store lines (`kprobe/__memcapture`).
  Each program attaches to one exact kernel name ([CORE] core.cpp:317-345, `m[2] == kernel`).
- **Firing**: one GPU thread running a probe once.
- **Thread execution**: one thread of one kernel launch. A launch of 524,288 threads gives 524,288 thread executions.

**Operation classes**
- **D1 op (direction 1)**: a read or write on a map whose storage is indexed by the logical thread id and sized when the map is created. In bpftime these maps are:
  - the 1502 and 1512 per-thread arrays;
  - the 1527 GPU ring buffer, which has one ring per thread.

  Other tools have fixed-size per-warp buffers with the same capacity problem; they are listed under D1 too.
- **D2 op (direction 2)**: a helper call that today makes a synchronous GPU-to-CPU round trip. These are CPU HASH maps, GPU_HASH 1501, printk and puts ([CS] class table, [CS-sum]). On the Orin each costs 9.8-15.6 us with the shipped per-module lock, 11-22 us per call in 32-lane bursts, and about 70 us for launches the watcher is not told about, such as CUDA-graph launches. The calls of different threads run one after another ([PROTO] 36-41; [WP] 90, 113, 180).
- **Neither**: GPU-local operations: the GPU arrays 1503, 1504 and 1513, the ordinary ARRAY in dev, and the index and time helpers ([CS] class table).

**Shares**
- **Operation share**: of the map and helper operations a tool executes, the fraction that are D1, D2 or neither. Each call site is counted per firing, then multiplied by the number of firings.
- **Cost share**: the part of a tool's slowdown (time) or extra memory that comes from D1 or D2 operations.
- **Coverage**: the fraction of an application's GPU time, or of its launches, that falls in kernels bpftime can patch.

**Kernels and tools**
- **Attachable**: a kernel whose fatbin carries PTX for compute capability 8.7 or lower, and whose load passes through a bpftime hook ([NVA] 2245-2293, 488-534).
- **Own kernels**: kernels compiled from the application's source, for example llama.cpp's `mul_mat_q`.
- **Vendor kernels**: kernels that ship precompiled in NVIDIA libraries (cuBLAS, cuDNN).
- **Tool port**: a change to a bpftime tool, never to the target application. Examples: the tool's `SEC` kernel name, its map types, a thread guard. Building the application with a PTX flag is build configuration, not a rewrite.
- **CPU-style tool**: a tool written the way one writes it for the CPU, with HASH maps and printk. bpftime's cuda-counter and threadscheduling are examples.

**Labels**
- **Tier A/B/C**: the groups used to rank scenarios in section 1.
- **S1-S10**: the scenarios.
- **W1-W6**: the workloads.
- **M1-M21**: the rows of the matrix.

## Short answer so far

1. **No real-application share has been measured.**
   - Our call-site survey is static.
   - gpu_ext and Neutrino report launch geometry and overheads, not operation shares per scenario ([CS] Limits; scenarios survey, gaps).
   - The only cost share we have comes from a microbenchmark. There, host calls are about 91-97% of cuda-counter's overhead on a 32-thread vectorAdd: 320 calls per launch x 9.8-10.4 us against 3,446 us of overhead [inferred arithmetic from [RD] numbers table and [CMP] line 75].
2. **Direction 1 operations appear in real use, whatever style the tool is written in.**
   - Five of the ten scenarios keep per-thread or per-warp storage: S1, S3, S5, S7, and S4 in the form bpftime has to use.
   - Every mainstream tool sizes that storage in advance and handles overflow its own way (see S7 and S5 in section 1).
   - The one real-application measurement is gpu_ext on llama.cpp's RoPE kernel at pp512 (a 512-token prompt), on an RTX 5090. A per-thread ring of 892 MiB held 22 MiB of records, and throughput fell 90.7%. A GPU-local array holding the same 720,896 records cost 5.6% ([GX-notes] 4-8, 20-28; [GX-5090] 1-14).
   - The RoPE kernel's thread count grows 16x from a 32-token to a 512-token prompt, so any fixed capacity either loses records or wastes memory ([GX-pre] 3-18).
   - The 90.7% vs 5.6% attribution is partly [inferred]: gpu_ext did not separate the ring's size from its write path.
3. **Direction 2 operations appear only when tools are written CPU-style.**
   - No mainstream tool we read makes a synchronous host call per event. The exception is hang debugging (CUTracer `--channel-records 1`, which needs SM90 or newer) ([CUT] readme.md:9, 99-106).
   - The tools that already run on real application kernels make no host calls: bpftime's four examples on real kernels (llama.cpp, PyTorch, FAISS, CUTLASS) and gpu_ext's 15 device-side tools ([CS] Results; scenarios verifier, "added").
   - All 59 of bpftime's host-call sites sit on vectorAdd demos or `__directly_run`.
   - When a CPU-style tool does run on a real kernel, host calls dominate. One host call per thread on RoPE at pp512 is about 11.5M calls per prefill, roughly 113-180 s per call site [inferred from [PROTO]].
   - So D2's share in real applications depends on which tools people write. It has to be measured with CPU-style tool ports.
   - The survey's selection-effect caveat stands: authors who know host calls are slow avoid them ([CS] Limits).
4. **On this Orin, coverage is limited to own kernels built with PTX.**
   - Covered: llama.cpp quantized inference, CUTLASS, FAISS's own kernels, Rodinia, and the ATen kernels of a source-built PyTorch.
   - cuBLAS, cuDNN and cuRAND cannot be patched here: they carry PTX only for sm_120 or sm_121 ([CUOBJ]; [NVA] 2245-2263). cuBLAS GEMMs run 45-76% of GPU kernel time in PyTorch LLM inference on Orin ([ICDCS] §V Fig. 9).
   - A few vendor entries would pass the pick rule: cuBLASLt 1 of 290 (`libcublasLt.268.sm_75.ptx`), cuSPARSE 8 sm_86 of 155, cuSOLVER 3 sm_80 + 3 sm_86 of 263, TensorRT's libnvinfer 85. Which kernels they hold is unknown ([CUOBJ]).
   - Triton cubins cannot be patched either. They are loaded through `cuModuleLoadData`, which dev does not hook ([TRITON] driver.c:316; [NVA] 488-534).
5. **The most common mainstream scenarios are mostly not expressible in bpftime today.**
   - S8 (correctness checking), S6 (instruction mix), S7 (region timing) and S5 (address traces) need PTX pass work: a per-instruction attach point, or the effective address as an argument ([PASS]; [MEMCAP] 45-147).
   - Today's bpftime can measure S1, S2, S3, S9 and S10 in full, and S4 only partly.
   - For S5-S8 the plan projects volumes from Nsight Compute counts.

## 1. Mainstream device-side scenarios, ranked

**How they are ranked.** Ranks follow the number and standing of independent tools that implement each scenario:
- **Tier A**: shipped by NVIDIA in the CUDA toolkit or its profilers.
- **Tier B**: several independent tools, used on real LLM or DL kernels in published work.
- **Tier C**: mostly bpftime and gpu_ext examples.

No source gives usage statistics, so the order is [inferred], and order within a tier is not significant. All of these are developer tools. We found no always-on production deployment of in-kernel probes (scenarios survey, gaps).

| Rank | Tier | Scenario | Evidence of real use | bpftime today | D1 ops (frequency) | D2 ops (frequency) | Neither (frequency) |
|---|---|---|---|---|---|---|---|
| 1 | A | S8 Correctness checking (NaN/Inf, out of bounds, races, uninitialized reads, hangs) | Compute Sanitizer (memcheck, racecheck, initcheck, synccheck) ships with every CUDA toolkit, this board included ([SAN]; sanitizer_patching.h:799-1051). GPU-FPX (NVBit, HPDC'23). CUTracer register and value traces and deadlock detection ([CUT]). DrGPUM and ValueExpert (search-level only) | Mostly not: no per-instruction attach point, and memcapture gives no address ([PASS], [MEMCAP]) | About none: shadow state is sized by allocations, not threads [inferred] | Rare: one report per error (Sanitizer prints the first 100 by default). Per record only when debugging hangs ([CUT] 99-106) | Per access per thread: the address plus a lookup in a CPU-written allocation table, mirrored to the GPU before launch [inferred] |
| 2 | A | S6 Instruction mix (instruction, opcode and tensor-op counts) | Nsight Compute sass_ metrics, collected by software patching ([NCU] §2.3.2). CUPTI SASS Metrics API ([CUPTI]). NVBit instr_count and opcode_hist ([NVBIT-T]). Neutrino tensorop_count ([NEU] Table 3). CUTracer opcode_only | Not: no per-instruction attach point ([PASS]) | None when counters are keyed by opcode or PC. In bpftime, a per-thread accumulator would be 1 D1 op per instruction per thread, because bpftime has no probe registers [inferred] | None in any tool | Per executed instruction per warp: one atomic add on an opcode-keyed array. This is the highest frequency of all scenarios |
| 3 | A | S4 Memory-access counts (bytes, memory instructions, coalescing) | Nsight Compute memory sass_ metrics ([NCU]). Neutrino gmem_bytes, checked against Nsight Compute ([NEU] §6.1). NVBit's address-divergence tool on five CNNs ([NVBIT] §6.1). bpftime mem_trace | Partly: counts only the ld/st lines its regex matches, and gets no address ([MEMCAP] 45-56) | bpftime: a 1502 per-thread accumulator per matched ld/st per thread; none with a shared 1503 counter. Neutrino: 8 B per thread, saved once at kernel end | None in any tool | Per memory instruction per thread. Memory instructions are 11.03% (attention) and 17.57% (GEMM) of executed instructions ([NEU] App. I) |
| 4 | B | S7 Intra-kernel region timing (per-warp region timers, pipeline stages) | KPerfIR/Proton in Triton: about 8% overhead, used to speed up FA3 on H100 by 24.1% ([KPERF] §5.2, §6.2.3, §6.3). NVIDIA IKET in the CUTLASS DSL ([IKET]). Triton-distributed profiler ([TD]). FlashInfer profiler (search-level). Neutrino program_progress ([NEU] §7) | Not: needs an attach point at a chosen instruction | Yes: a per-warp buffer whose capacity is guessed. Overflow handling differs: KPerfIR and Proton overwrite the oldest records, IKET runs a sizing pre-pass, Neutrino caps at 128 per thread, Triton-distributed leaves sizing to the user | None: buffers are copied after the kernel ([KPERF] §4.3) | Per region boundary or loop iteration per warp: tens to hundreds per warp per launch |
| 5 | B | S1 Kernel, block and warp timing | Neutrino block_sched found about 20% block-scheduling time in torch.zeros, a 24.69% tail in Triton FA-v2 and 50.93% in GEMM ([NEU] §4.6, §7, App. J). KPerfIR/Proton record per-warp start and end. gpu_ext kernelretsnoop on llama.cpp ([GX-paper] Table 2). bpftime kernelretsnoop, kernel_trace, cuda-counter | Yes | Yes: the entry timestamp sits in a per-thread map between the separate entry and exit programs [inferred from the pass design], and a per-thread 1527 record is written at exit | None in mainstream tools. bpftime cuda-counter makes about 10 host calls per thread per launch ([RD] numbers table) | 2 firings per thread per launch. 1 record per thread (bpftime) or per warp (Neutrino) |
| 6 | B | S5 Memory address traces | NVBit mem_trace: one 288-B record per warp access ([NVBIT-T]). Neutrino dmat: about 2.7x-13.2x slowdown, 7.12x on average ([NEU] Table 3, §6.2). CUTracer address and value traces (SM90+). GVProf GPU Patch ([GVP]). eGPU, evaluated on a microbenchmark only ([EGPU] §7.2). Used for simulation and access-pattern studies | Not: memcapture passes the PTX text, not the address ([MEMCAP] 64-147) | Dominant: storage grows as threads x accesses, which is not known in advance. Neutrino first runs a counting launch, which breaks for kernels that modify their inputs in place ([NEU-C] cuda.c:486-538) | None per event. NVBit drains once per buffer fill ([NVBIT-T] channel.hpp:60-120). GPU Patch signals when its buffer is full ([GVP]) | Per memory instruction per thread |
| 7 | C | S9 Device printing | CUDA device printf is common in hand-written debug code; it is buffered and flushed at sync points (search-level). As injected instrumentation, only bpftime examples use it: 21 printk sites, all on vectorAdd or `__directly_run` ([CS-sum]) | Yes | None | Every printk or puts is a synchronous host call today ([TRAMP] 129-136, 386-391, 486-501) | Per thread per launch in the examples |
| 8 | C | S3 Work distribution (per-thread histogram, SM/warp mapping) | bpftime threadhist (on llama.cpp, PyTorch and FAISS kernels) and threadscheduling. gpu_ext threadhist: 3% overhead vs NVBit's 87% on a P40 ([GX-paper] Table 2). Neutrino records the SM id per warp | Yes | threadhist: one 8-B 1502 slot per thread. gpu_ext's run used 2.1% of 1,048,576 slots ([RD] 147-149). Histograms keyed by SM or warp are sized by the hardware | threadscheduling: 5 host-call sites per thread per launch. One of them (`thread_mapping`) is per thread and cannot be aggregated per warp ([CS-sum]) | 1 update per thread per launch |
| 9 | C | S2 Kernel launch latency | bpftime launchlate. gpu_ext launchlate on llama.cpp: 14% vs NVBit's 93% on a P40, 0.22% vs 8.80% on a 5090 ([GX-paper] Table 2; [GX-5090]). Mainstream tools measure this on the host, e.g. Nsight Systems [inferred] | Yes | None | bpftime launchlate has no thread guard: one printk per thread per launch (launchlate.bpf.c:86-121). gpu_ext's version runs in thread 0 only and makes none ([CS-sum]) | Per launch when guarded, per thread as bpftime wrote it |
| 10 | C | S10 Device-side policy (block partition, work stealing, prefetch) | gpu_ext research: work stealing, and an L2 stride prefetch giving 1.34x ([GX-paper] §4.3, §6.2.1). bpftime atomizer. gpu_ext's warp-aggregated shards (§5.3) exist in the paper only, not in its code ([OV]) | Partly | None | atomizer: 6 printk sites per thread per launch ([CS-sum]). A device-to-host prefetch request would need no answer [inferred] | Per thread or per block per launch |

**Where the two directions come from.** These counts are our own classification of the verified records in `records.json` by map type, run with `python3 -I`. They count call sites, which is not the same as a share: the plan below weights them by firings.

**bpftime: 22 device-side programs, 168 call sites that run on the GPU**

| Kind of site | Sites | Programs |
|---|---:|---:|
| D1 (1502/1512/1527) | 15 | 8 |
| D2 (host calls) | 59 | 12 |
| Other GPU-local map sites | 34 | |
| Index, time and other helpers | 60 | |

**gpu_ext: 15 device-side programs, 80 sites**

| Kind of site | Sites | Programs |
|---|---:|---:|
| D1 | 9 | 7 |
| D2 | 0 | 0 |
| Other GPU-local map sites | 34 | |
| Index and time helpers | 37 | |

Two of gpu_ext's llama.cpp kernelretsnoop rewrites exist to avoid the per-thread ring: a banked 1503 buffer and a single-value array ([GX-notes]; [CS-sum]).

So the two directions come from different places:
- **D1** comes from what a scenario needs: per-thread state between separate programs, and per-thread or per-warp output.
- **D2** comes from tool style: CPU map types and printk.

## 2. Real workloads worth measuring on this Orin

**W1 llama.cpp, GGUF Q4_K_M (TinyLlama-1.1B; Llama-3.2-1B as a second model). Priority: first.**
- **Why it matters:**
  - It is the only real application on which device-side eBPF has been evaluated (gpu_ext) ([GX-paper] §6.4.2).
  - On sm_87, quantized matmuls run in llama.cpp's own MMQ kernels ([LCPP] mmq.cu `ggml_cuda_should_use_mmq`).
  - In Q4_K_M decode, the mul_mat_vec_q family takes 75.5% of kernel time, measured on an H100 ([CHAL] Table 5.2).
- **Attachable here:** MMQ, mul_mat_vec_q, flash_attn_ext, RoPE, rms_norm, softmax and the other own kernels. This needs a PTX build: `-DCMAKE_CUDA_ARCHITECTURES=87`, which gives real and virtual code, or `GGML_NATIVE=OFF`, which includes 80-virtual ([LCPP] CMakeLists; CMake docs).
- **Not attachable here:** the cuBLAS path, used for F16/BF16/F32 weights or with `GGML_CUDA_FORCE_CUBLAS` ([LCPP] build.md).
- **Feasibility and caveats:**
  - The default build is not attachable. CMake expands `native` to `87-real`, which is SASS only (/usr/share/cmake-3.28/Modules/Internal/CMakeCUDAArchitecturesNative.cmake:35; [ENV] fatbin.list.txt:5-8).
  - CUDA graphs are on by default. gpu_ext set `GGML_CUDA_DISABLE_GRAPHS=1`; graphs are untested with dev ([GX-readme] 46-55).
  - The model file is 0.67 GB, with 3.17 GB maximum RAM.
  - The RoPE kernel that gpu_ext probed launches 4 x pp x 256 threads, where pp is the prompt length in tokens: 524,288 threads at pp512. It runs 22 launches per prefill. The 44 launches gpu_ext counted are a warm-up pass plus one timed pass [inferred by the verifier from llama-bench defaults] ([GX-pre]; run_table1_perf.py:71-84).
  - That kernel is the K-only RoPE (4 KV heads). In it, 224 of every 256 threads return at once (head dimension 64), yet each still passes the exit probe and owns a per-thread slot. The Q RoPE of the same model (32 heads) launches 8 times as many threads, 4,194,304 at pp512, so one fixed thread count cannot fit both kernels even at one prompt length [inferred from llama.cpp rope.cu].

**W2 CUTLASS GEMM. Priority: second, as a control.**
- **Why it matters:** the templates compile into the application, so all kernels are its own. It is Neutrino's main GEMM workload ([NEU] Table 3), and a bpftime example exists.
- **Attachable here:** all kernels, with an sm_87 PTX build ([ENV] fatbin.list.txt:9-12).
- **Feasibility and caveats:** easy. It is a kernel benchmark, not an application, and the bpftime example is SIMT FP32, so it uses no tensor cores (example/gpu/cutlass/README.md).

**W3 FAISS IVF on SIFT1M. Priority: third.**
- **Why it matters:** vector search is a real application outside LLMs, and bpftime's faiss-test example exists.
- **Attachable here:** FAISS's own kernels: k-select, the IVF list scan and l2Norm ([FAISS] §4.2, §5.1, §5.3).
- **Not attachable here:** the distance GEMM, which runs in cuBLAS.
- **Feasibility:** a source build with the GPU enabled. SIFT1M is 1M x 128 floats, 512 MB [arithmetic], which fits.

**W4 Rodinia / Polybench. Priority: optional.**
- **Why it matters:** many small launches with different grids. Gaussian has 414 kernels and gramschmidt 6,411 ([PKA] Table 3).
- **Attachable here:** all kernels.
- **Feasibility and caveats:** trivial to build. Its inputs are sized for simulators and run in under 1 ms on real hardware ([PKA] §5.2.1), so it is weak as a real application. It is useful only as a D1 control with many kernels.

**W5 PyTorch eager (small Hugging Face Transformers LLMs). Priority: defer.**
- **Why it matters:** the mainstream framework. ICDCS'26 measured its kernel mix on Orin.
- **Attachable here:** only the ATen kernels, and only from a source build with `TORCH_CUDA_ARCH_LIST=8.7+PTX`.
- **Not attachable here:** the GEMMs: 76.0% of kernel time for Qwen-1.5B on Orin NX, 67.5% on AGX Orin, and 44.8% for Gemma-2B ([ICDCS] Fig. 9). The pip cu132 wheels target 8.0, 9.0, 10.0, 11.0 and 12.0 ([FORUM]; dev-discuss); their PTX content was not checked.
- **Feasibility:** the wheel runs, using sm_80 SASS. A source build on an 8 GB board is heavy [inferred]. Neutrino too needed `torch.compile` to reach GEMMs ([NEU] artifact notebook, via the workloads verifier).

**W6 TensorRT / TensorRT Edge-LLM. Priority: a cheap attach check only.**
- **Why it matters:** it is NVIDIA's supported path for LLMs on Orin.
- **Attachable here:** unknown. libnvinfer has 85 PTX entries at or below sm_86 and 55 sm_87 cubins, and it does not link cudart ([CUOBJ]; `readelf -d`).
- **Not attachable here:** probably the engine kernels, but where they come from at inference time is unknown. The builder-resource libraries "do not contain device code" to cuobjdump.
- **Feasibility:** installed; attaching is untested.

**What cannot be done on this Orin, and what it would need:**
- **Vendor-library kernels (cuBLAS, cuBLASLt, cuDNN).** Apart from one sm_75 entry in cuBLASLt, their PTX targets only sm_120/sm_121 in this toolkit. On an sm_120 GPU (RTX 5090 class), bpftime's `pick_patched_ptx` would accept sm_120 PTX [inferred from [NVA] 2245-2263]. This rests on two unchecked points: the x86 libraries' PTX was not checked, and these libraries do not link libcudart, so it is untested whether dev's driver-level hooks see their loads at all.
- **Large models and serving.** Examples are vLLM with 30B MoE models, gpu_ext's GPT-OSS-120B runs (host-side policies), and 8B BF16 models. These need a discrete GPU. 8 GB of unified memory fits roughly 1-3B quantized models [inferred].
- **Hopper and Blackwell kernels, where S7 matters most.** FA3, warp-specialized GEMM, and CUTracer all need SM90 or newer.
- **Triton, TorchInductor and vLLM kernels.** No machine helps: bpftime first has to hook `cuModuleLoadData*`. That is a bpftime change, not a target rewrite ([NVA] 530-534; candidate CLAUDE.md:2146-2147, cited by the verifier).
- **Collisions between concurrent kernels.** Two processes could be run here in principle, but gpu_ext's two-tenant runs were host-side policies on larger GPUs ([GX-paper] §6.3.2).

## 3. Scenario x workload matrix

How to read the matrix:
- **Feasible here** takes one of six values:
  - `yes: tool exists`: a bpftime tool exists and only needs retargeting;
  - `yes: tool port`: the scenario is expressible today, but a tool must be written or ported;
  - `partial: tool port, limited coverage`;
  - `no: needs bpftime pass work`;
  - `no: not attachable here`;
  - `unknown: untested`.
- **Shares** are per firing (the fraction of executed map and helper operations), with volume on the one known real geometry: RoPE at pp512 is 11,534,336 thread executions per prefill, or 360,448 warp executions.
- **Host-call times** use dev's measured 9.8-15.6 us per call ([PROTO] 36-41; 11-22 us in 32-lane bursts, [WP]). At that cost the serialized ceiling is about 64,000-102,000 host calls per second [inferred]. Launches under CUDA graphs would pay about 70 us per call ([WP] 180).
- **Grouped rows** name every workload they cover.

| # | Scenario | Workload | Feasible here | bpftime tool | D1 share (thread-indexed storage) | D2 share (host calls) |
|---|---|---|---|---|---|---|
| M1 | S1 kernel/block/warp timing | llama.cpp own kernels (RoPE, rms_norm, mul_mat_q, mul_mat_vec_q, flash_attn_ext ...), PTX build | yes: tool exists | kernelretsnoop / kernel_trace, SEC retargeted to each mangled kernel name (one program per kernel). Reference: gpu_ext's RoPE port. CPU-style variant: cuda-counter retargeted | kernelretsnoop: its 1 map op per firing is D1 (per-thread 1527 record), so 100%. Volume: 11.5M records per prefill from RoPE at pp512 alone, or 0.36M with a warp-leader guard [inferred]. gpu_ext measured an 892 MiB ring for 22 MiB of data (RTX 5090) | 0 as written. cuda-counter-style port: all ~10 map/print ops per thread are host calls, about 115M calls, roughly 19-30 min per prefill for RoPE alone [inferred] |
| M2 | S2 launch latency | llama.cpp own kernels, PTX build | yes: tool port | launchlate (bpftime, written for vectorAdd), retargeted. Reference: gpu_ext's llama.cpp launchlate (1513 FIFO, thread 0 only) | 0 | bpftime launchlate has no thread guard: 1 printk per thread, about 1 of 3-4 map/print ops per firing. On RoPE at pp512 that is about 11.5M calls, roughly 113-180 s per prefill [inferred]. With a thread-0 guard: at most 2 per launch. gpu_ext version: 0 |
| M3 | S3 work distribution | llama.cpp own kernels, PTX build | yes: tool exists | threadhist (llama-cpp-test on rms_norm_f32 exists); threadscheduling retargeted | threadhist: 100% (1 update of an 8-B per-thread 1502 slot per thread per launch). Slots are fixed at load: gpu_ext used 2.1% of 1,048,576 | threadhist: 0. threadscheduling: 5 host-call sites per thread, all its map ops. About 58M calls, roughly 9.5-15 min per prefill on RoPE at pp512 [inferred] |
| M4 | S4 memory-access counts | llama.cpp own kernels, PTX build | partial: tool port, limited coverage | mem_trace retargeted: counts ld/st lines that match the regex, into a shared non-atomic 1503 counter. The pass patches every function in the module | 0 as written (shared 1503). A per-thread accumulator variant: 1 D1 op per matched ld/st per thread [inferred] | 0. Frequency: per matched memory instruction per thread. The share of llama.cpp ld/st lines that match is unknown (vector, .nc, generic, predicated and cp.async forms are missed) |
| M5 | S5 address trace | llama.cpp own kernels, PTX build | no: needs bpftime pass work | Write after a pass change that passes the effective address and covers more ld/st forms: a per-access 1527 record, or a warp-leader record | About 100% of ops (one record per access per thread). About 800 MB per decode token in Neutrino's 16-B layout, under unmeasured assumptions [inferred] | 0 (mainstream tools drain once per buffer fill) |
| M6 | S6 instruction mix | llama.cpp own kernels, PTX build | no: needs bpftime pass work | Write after a per-instruction attach pass: opcode- or PC-keyed 1503 counters | 0 with opcode/PC keys. A per-thread accumulator would add 1 D1 op per instruction per thread [inferred] | 0 |
| M7 | S7 intra-kernel region timing | llama.cpp own kernels, PTX build | no: needs bpftime pass work | Write after an attach-at-chosen-instruction pass: a per-warp timestamp buffer | About 100% (per-warp appends; tens to hundreds per warp per launch; capacity guessed) [inferred] | 0 |
| M8 | S8 correctness checking | llama.cpp own kernels, PTX build | no: needs bpftime pass work | Write after an address-carrying pass: address plus a lookup in a CPU-written allocation table | About 0 (shadow state sized by allocations) [inferred] | Rare (one per error). Per record only when debugging hangs (CUTracer, SM90+, cannot run here) |
| M9 | S9 device printing | llama.cpp own kernels, PTX build | yes: tool port | Add bpf_printk to any probe. All 21 printk sites today are on vectorAdd or __directly_run | 0 | 100%: every printk is a host call. One per thread per launch is about 11.5M calls, roughly 113-180 s per prefill per printk site on RoPE at pp512 [inferred] |
| M10 | S10 device policy | llama.cpp own kernels, PTX build | yes: tool port (changes behaviour; not a share measurement) | atomizer-style block partition, retargeted | 0 | atomizer as written: 6 printk sites per thread, at least half of the executed ops [inferred]. 0 without the prints |
| M11 | S1 and S3 | FAISS own kernels (l2NormRowMajor, k-select, IVF scan), SIFT1M, PTX build | yes: tool exists | threadhist (faiss-test on l2NormRowMajor exists); kernelretsnoop and threadscheduling retargeted | Same per-firing split as the llama.cpp rows (100% D1 for kernelretsnoop and threadhist). Grid sizes unknown | 0 as written. CPU-style ports: as in the llama.cpp rows |
| M12 | S1 and S3 | CUTLASS GEMM (bpftime example, SIMT FP32 4096^3), sm_87 PTX build | yes: tool exists | cutlass_launch_counter (exists: thread 0, 1503); kernelretsnoop and threadhist retargeted | Launch counter: 0. kernelretsnoop and threadhist: 100%. Few launches (24+2) with large grids | 0 as written. CPU-style ports: as in the llama.cpp rows |
| M13 | S1 and S3 | Rodinia / Polybench (HotSpot, Gaussian, GEMM ...), PTX build | yes: tool port | kernelretsnoop and threadhist retargeted | 100% for per-thread tools. Many launches with differing grids (gaussian 414 kernels, gramschmidt 6,411), which tests one fixed capacity across kernels | 0 as written. CPU-style ports: as in the llama.cpp rows |
| M14 | S1 and S3 | PyTorch eager ATen kernels (source build, TORCH_CUDA_ARCH_LIST=8.7+PTX) | yes: tool exists (heavy build) | threadhist (pytorch-test on bitonicSortKVInPlace exists); kernelretsnoop retargeted | 100% for per-thread tools. The GEMMs (45-76% of kernel time on Orin) are not visible | 0 as written. CPU-style ports: as in the llama.cpp rows |
| M15 | S2, S9 and S10 | FAISS, CUTLASS, Rodinia and PyTorch-ATen own kernels | yes: tool port | launchlate, a printk probe and atomizer, retargeted | 0 | Same per-firing split as the llama.cpp rows: a per-thread printk means 1 host call per thread per launch; a thread-0 guard means at most 2 per launch |
| M16 | S4 | FAISS, CUTLASS, Rodinia and PyTorch-ATen own kernels | partial: tool port, limited coverage | mem_trace retargeted | 0 as written; 1 per matched ld/st per thread for a per-thread accumulator [inferred] | 0 |
| M17 | S5, S6, S7 and S8 | FAISS, CUTLASS, Rodinia and PyTorch-ATen own kernels | no: needs bpftime pass work | As in the llama.cpp rows | As in the llama.cpp rows (S5 and S7 about 100%, S6 and S8 about 0) [inferred] | 0 (S8: rare error reports) |
| M18 | S1-S10 | Vendor-library kernels: cuBLAS/cuBLASLt GEMMs (PyTorch linear layers, FAISS distance GEMM, llama.cpp F16/BF16 path), cuDNN conv (CNNs) | no: not attachable here | None on this board: PTX only for sm_120/sm_121, and no sm_87 PTX. Perhaps on an sm_120 GPU [inferred, not checked] | n/a here | n/a here |
| M19 | S1-S10 | Triton / TorchInductor kernels, vLLM serving | no: not attachable here | None: Triton loads cubins with cuModuleLoadData, which dev does not hook. vLLM also needs more memory and an aarch64/CUDA 13 build | n/a here | n/a here |
| M20 | S1-S10 | TensorRT 10.16 / TensorRT Edge-LLM | unknown: untested | Check first whether dev sees libnvinfer's loads (it has 85 PTX entries at or below sm_86 and 55 sm_87 cubins, and does not link cudart) | unknown | unknown |
| M21 | S1-S10 | MLPerf training (SSD, BERT ...), SPEC ACCEL | no: needs another machine or licence | None here: 8 GB is too small for training, SPEC ACCEL needs a licence and NVHPC, and both are vendor-library heavy | n/a here | n/a here |

**What the matrix says [inferred]:**
- Today's bpftime can measure the advisor's question on llama.cpp, FAISS, CUTLASS and Rodinia, for S1, S2, S3, S9 and S10, and for S4 partly.
- In the cells that are feasible here, D1 is close to 100% of the per-thread tools' map operations.
- D2 is 0 for the tools as they were written for real kernels, and close to 100% for the CPU-style ports.
- At real geometry, the per-thread host calls of a CPU-style port cost minutes per prefill.
- So the per-tool shares are 100% or 0% by construction: a tool keeps its state in thread-indexed maps or it does not, and it uses CPU maps and printk or it does not. They do not decide how far to go. What does is measured per application: coverage (the share of GPU time in attachable kernels), firings per call site at real launch geometry, the cost share of each direction in the slowdown, and thread-indexed capacity against use and loss.

## 4. Measurement plan for the next step

**What it answers.** For each measured cell there are four numbers:
- **Q1, coverage.** What share of the application's GPU time and launches is attachable?
- **Q2, operation share.** How many executions per call site fall in each direction, per prefill or token and per second?
- **Q3, cost share.** What part of the tool's slowdown comes from host calls? What part of its extra memory sits in thread-indexed maps?
- **Q4, D1 symptoms.** How do capacity and use compare, and how many records are lost?

**Cells.**
- Main: M1, M2, M3, M4 and M9 on llama.cpp.
- For generality: M11 and M12 (FAISS, CUTLASS); M13 is optional.
- M5-M8 are projected from Step 0 counts only, not measured with bpftime.
- M10 is left out: S10 changes behaviour, so it does not measure a share.

**Step 0: census of the application, without bpftime (the denominator)**
1. Build llama.cpp with `-DCMAKE_CUDA_ARCHITECTURES=87` and `-DGGML_CUDA_FA=ON`, the latter as gpu_ext did. Check with `cuobjdump --list-ptx` that compute_87 PTX is present.
2. Run `llama-bench` on TinyLlama Q4_K_M:
   - prefill: `-p 32`, `-p 128` and `-p 512` with `-n 0`;
   - decode: `-p 0 -n 128`;
   - each with `GGML_CUDA_DISABLE_GRAPHS=1`, and again with graphs on.
3. Run `nsys profile -t cuda`. It gives, per kernel, the launch count, grid x block (threads per launch) and GPU time. From these:
   - thread executions per prefill and per token, for each kernel;
   - Q1: the time share of own kernels versus cuBLAS.
4. Optionally, on the top five kernels, collect Nsight Compute sass_ metrics: executed instructions, and global load and store instructions. These project the volumes of S4, S5 and S6.
5. Statically, dump llama.cpp's PTX (`cuobjdump -ptx`) and count which ld/st lines the memcapture regex matches, using `python3 -I`. This gives S4's coverage.
6. Tools:
   - `nsys` 2026.3.1 (`/opt/nvidia/nsight-systems/2026.3.1/bin/nsys`) records each launch with grid, block and GPU time on this board. Checked on a test program that exits on its own (`nsys-check/`).
   - `ncu` SASS metrics are untested.
   - If tracing fails on llama.cpp, a CPU uprobe on `cuLaunchKernel`/`cudaLaunchKernel` that reads the grid and block arguments can replace the census.

**Step 1: tools as written, on real kernels (D1 and the probe floor)**
1. Tools: kernelretsnoop, kernel_trace, threadhist, and mem_trace.
   - Generate one program per target kernel, from the exact mangled names, for the top kernels by time in Step 0.
   - This per-kernel generation is a tool port and is untested.
2. Count executions per call site in two ways:
   - from geometry: threads x launches x firings per thread;
   - from a counting variant of each tool, with one atomic 1503 counter per site, as a check.
3. D1 measures:
   - the configured `max_thread_count` against the largest thread count per launch among the probed kernels;
   - 1502 slots touched (non-zero) against slots allocated;
   - 1527 bytes allocated against bytes delivered;
   - records lost, computed as expected minus received. A 1527 output past the capacity returns 1 and is not counted in `dropped` ([TRAMP] 425-442).
4. Keep one fixed capacity across pp32, pp128 and pp512. This shows directly how loss and waste change with prompt length.
5. Time: tool throughput against baseline, plus an empty-probe variant (entry and exit programs with no helpers), which measures the patching floor.

**Step 2: CPU-style ports, on the same kernels (D2)**
1. Tools:
   - cuda-counter: HASH maps plus printk, about 10 host calls per thread;
   - threadscheduling: 5 host calls per thread;
   - launchlate as written: 1 printk per thread.
2. Bound every run. One per-thread call site at pp512 on RoPE takes about 113-180 s per prefill [inferred].
   - Run with `GGML_CUDA_DISABLE_GRAPHS=1`: the host-call watcher is not told about graph launches, which then pay about 70 us per call ([WP] 180).
   - Use pp32: 720,896 thread executions per prefill, about 7-11 s per site and about 70-110 s for cuda-counter [inferred].
   - Use decode, whose kernels are small.
   - Put a total timeout on every run.
3. Count host calls:
   - with a per-helper counter in the CPU-side host-call handler (a measurement-only patch to bpftime);
   - otherwise by deriving them from geometry x per-site frequency.
4. Report host calls per second against the ceiling of about 64,000-102,000 per second [inferred].
5. Attribute the slowdown three ways:
   - (a) host calls x the measured per-call cost (9.8-10.4 us for warps, 15.6 us for a single thread; [PROTO] 36-41), divided by tool time minus baseline time;
   - (b) the same tool with each D2 site made GPU-local (HASH replaced by 1503, printk removed), which is the "eliminate/defer" form from [CS]; the time difference is the host-call share;
   - (c) the empty probe from Step 1, which gives the floor.
6. Report (a) and (b) per site class: eliminate, defer, printk.
7. Repeat with three frequency variants of each tool: as written (per thread), a warp-leader guard, and a thread-0 guard. This shows how a site's frequency class changes D2's cost.

**Step 3: generality (other workloads)**
- Run Steps 1-2 for S1 and S3 on CUTLASS (the example exists) and on FAISS IVF with SIFT1M, with the same fixed-capacity protocol.
- Optionally, run Rodinia gaussian and gramschmidt for the many-launch D1 case.
- TensorRT: one check of whether dev sees libnvinfer's modules. No measurement.

**Output: one table row per cell, with these columns**
- probed kernels and their share of application GPU time;
- firings per prefill or token;
- D1, D2 and neither operations;
- host calls per second;
- slowdown;
- host-call share of the slowdown, by (a) and by (b);
- D1 memory allocated and used;
- records lost.

**Build and download effort, all [inferred] except where cited**

| Workload | Download | Build on the Orin | Risk |
|---|---|---|---|
| llama.cpp + TinyLlama Q4_K_M | repository (shallow clone; size not checked) + 0.67 GB model | one CUDA arch; tens of minutes | CUDA graphs under dev; build memory (limit `-j`) |
| CUTLASS (bpftime example) | CUTLASS headers | minutes | low |
| FAISS + SIFT1M | repository + dataset (archive size not checked) | GPU build, about 1 h, needs BLAS | build time |
| Rodinia | small | minutes | weak realism |
| PyTorch source, 8.7+PTX | repository with submodules, several GB | many hours; risk of running out of RAM | defer |
| TensorRT | installed | none | whether it can be attached is unknown |

**Run rules from earlier rounds**
- Put a total timeout outside LD_PRELOAD on every run.
- Before timing, check jetson_clocks: cpu5 should be at 1728000 and the GPU at 1020000000.
- Never measure from 03:25 to 04:15 JST (the daily timer).
- Check `ps` for stray runs.
- Clean root-owned `/dev/shm` test segments.
- Remove containers after the runs.

**What this plan will not answer**
- Operation shares for S5-S8 as bpftime would execute them; these are projected from the census only.
- Anything about vendor kernels.
- How often these scenarios are used in production.

## 5. Open questions and what this survey could not establish

1. **No measured share exists.** All volumes are geometry arithmetic from one real kernel: llama.cpp RoPE, from gpu_ext's runs on other GPUs.
2. **How common each scenario is** in real use has no statistics. The ranking counts tools, and no always-on production use of in-kernel probes was found.
3. **The time split between own and vendor kernels for llama.cpp on the Orin** is unknown. Chalmers measured on an H100, and ICDCS'26 used PyTorch.
4. **Hooks on vendor libraries are untested.** It is unknown whether dev's driver-level hooks see cuBLAS, cuDNN or TensorRT loads; these libraries do not link libcudart. It is also untested whether the late path's retargeting of sm_120 PTX to sm_87 compiles.
5. **llama.cpp with its default CUDA graphs under dev** is untested.
6. **The share of llama.cpp ld/st PTX lines that the memcapture regex matches** has not been counted.
7. **nsys kernel tracing works on this board** (checked 11-13, `nsys-check/`). ncu SASS metrics are untested.
8. **D2 depends on which tools exist.** There is no collection of CPU-style GPU tools beyond bpftime's examples. GPU ports of libbpf-tools patterns (entry/exit pairs, histograms, counters) would be the next test of whether real tools need synchronous answers.
9. **Two figures rest on inference:**
   - The 44 launches in gpu_ext are warm-up plus one timed pass; this was inferred by the verifier from llama-bench defaults.
   - gpu_ext's 90.7% vs 5.6% does not separate the ring's size from its write path.
10. **Claims the synthesizer listed as unverified are now settled** (full verifier output plus checks on 11-13):
    - cuRAND carries PTX only for sm_121 (10 entries). cuSOLVER carries 3 sm_80 and 3 sm_86 entries of 263 ([CUOBJ] `listings/summary.txt`).
    - cuBLAS, cuBLASLt, cuDNN (all five libraries) and libnvinfer do not link libcudart or libcuda (`readelf -d`, `listings/summary.txt`). Whether dev's driver-level `cuLibraryLoadData` hook sees their loads is still untested.
    - NVIDIA's forum statement that Orin runs sm_80 SASS without JIT: confirmed (forum 375642).
    - The workloads survey's closing synthesis: corrected to 11.5M thread executions per prefill (not 23M), and to "45-76% of kernel time is invisible", not "mostly invisible".
11. **Sources read only through summaries:**
    - WebFetch summaries, not verbatim: KPerfIR, Proton, IKET, Triton-distributed, Compute Sanitizer and the Sanitizer API guide, the Nsight Compute guide, eGPU, gpu_ext arXiv v2.
    - Search snippets only: GPU-FPX details, the FlashInfer profiler, CUPTI SASS details, CUDA printf FIFO behaviour.
    - Re-check their numbers against the PDFs before citing them in a paper.
12. **Values not extracted:** Neutrino Fig. 12 (probe memory against model memory). Grid sizes of FAISS, CUTLASS and llama.cpp decode kernels.
13. **The papers contradict themselves:**
    - gpu_ext's §6.4.2 text says NVBit costs 85-87%, while its Table 2 says 93% for launchlate.
    - Neutrino's App. I gives 2.75x for FA-v2, while its Table 3 gives 2.94x (Conv2D is 2.75x).
14. **Corrections applied from the verifiers.** Applied after the synthesis, from the full workloads verdicts:
    - Host-call cost is 9.8-15.6 us with the shipped per-module lock (11-22 us in 32-lane bursts), not 9.8-10.4 us. All per-prefill times above were recomputed.
    - "Vendor libraries carry no PTX usable for sm_87" overstated it: cuBLASLt, cuSPARSE, cuSOLVER and libnvinfer have a few entries at or below sm_87.
    - Under llama.cpp's default CUDA graphs, host calls would be served at about 70 us per call.
    - The K-only RoPE kernel and its 87.5% immediately-returning threads (W1).

    Applied by the synthesizer:
    - The 5090's 90.7% is the warp-leader configuration; the original per-thread ring still loses about 99.5%.
    - bpftime's launchlate and atomizer make per-thread printk calls.
    - threadscheduling has 5 host-call sites per thread, and its `thread_mapping` site cannot be aggregated per warp.
    - All 21 output sites are printk, none is puts, and all are on demo kernels.
    - memcapture calls after the matched instruction. It also misses predicated and cache-hinted forms, and it counts ld.param and ld.local.
    - CUTracer needs SM90 or newer.
    - gpu_ext's Fig. 1 is struck as evidence for in-kernel tracing.
    - gpu_ext's §5.3 shards exist in the paper only.
    - Neutrino's copy-back is in cuda.c, not App. C.

15. **Which launchlate gpu_ext's P40 number (14%) used** is not identified. gpu_ext's later RTX 5090 version (revision-rq4, 575-01) runs in thread 0 and makes no host calls. If the P40 run used a copy of bpftime's example, it made a printk per thread (and, on bpftime versions where an ordinary ARRAY lookup was still a host call, one more per thread). That would be the only published real-application cost of direction-2 operations. Check gpu_ext's history before citing either way.

## Sources (keys used above)

**Local results and code**
- [CS] /home/jetson/src/results/gpu/callsite-survey-20261113/README.md (class table, Results, Limits). [CS-sum] summary.txt in the same folder. Per-tool splits come from records.json in the same folder, classified by map type with `python3 -I`.
- [OV] /home/jetson/src/results/gpu/overlap-survey-20261113/README.md and workflow-output.json.
- [PROTO] /home/jetson/src/results/gpu/fixes-20261109/protocol/README.md:36-41 (host-call cost by geometry: dev 15.6 / 10.4 / 9.9 / 9.8 us).
- [RD] `git -C /home/jetson/src/bpftime show main:RESEARCH_DIRECTIONS.md`, lines 147-149 and 225-240 (numbers table, including CUDA Counter: 10 calls per thread, 3,486 us, 87x).
- [CMP] /home/jetson/src/results/gpu/compare-20261110/summary.md:74-75.
- [ENV] /home/jetson/src/results/gpu/feasibility-20261109/gpu-env/fatbin.list.txt.
- bpftime dev worktree /home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate:
  - [TRAMP] attach/nv_attach_impl/trampoline/default_trampoline.cu:129-136, 181-216, 386-391, 425-442, 486-501, 504-588.
  - [MEMCAP] attach/nv_attach_impl/pass/ptxpass_kprobe_memcapture/main.cpp:45-56, 64-147, 166-178.
  - [PASS] attach/nv_attach_impl/pass/: only ptxpass_kprobe_entry, ptxpass_kretprobe and ptxpass_kprobe_memcapture.
  - [CORE] attach/nv_attach_impl/pass/ptxpass_core/src/core.cpp:317-345 (exact kernel name match); ptxpass_kprobe_entry/main.cpp:46-93.
  - [NVA] attach/nv_attach_impl/nv_attach_impl.cpp:488-534 (hooks), 841-873 (`--extract-ptx`), 2245-2293 (`pick_patched_ptx`, no-PTX path).
  - The examples' README and .bpf.c files under example/gpu/.
- [CUOBJ] `listings/` in this folder: `cuobjdump --list-ptx` output per library, and `summary.txt` (PTX and SASS entries by arch, entries at or below sm_87, NEEDED), made by `scripts/vendor_summary.py` from those listings and the ELF listings (not kept; regenerate with `cuobjdump --list-elf`).
- [WP] /home/jetson/src/results/gpu/watcher-policy-20261112/README.md:90, 113, 180 (32-lane bursts 11-22 us per call; about 70 us for launches the watcher is not told about).
- [SAN] /usr/local/cuda/compute-sanitizer/include/sanitizer_patching.h:799-1051, and https://docs.nvidia.com/compute-sanitizer/ComputeSanitizer/index.html.

**gpu_ext (checkout at 50a51ad)**
- https://github.com/eunomia-bpf/gpu_ext at commit 50a51ad4 (2026-09-24, master head when read). Paths below are relative to workloads/llama.cpp/observability_overhead/ in that repository.
- [GX-notes] revision-rq4/device-array-runtime-notes.md:1-28.
- [GX-5090] revision-rq4/results-table1-warp-plt-575-06/README.md:1-14.
- [GX-pre] preflight-575-noncross-clock-01-capacity-diagnosis.md:3-18, and run_table1_perf.py:71-84.
- [GX-readme] README.md:46-55.
- [GX-paper] https://arxiv.org/html/2512.12615v2: Table 2, §4.3, §5.3, §6.2-6.4.

**Papers, repositories and documentation**
- Neutrino:
  - [NEU] https://open-neutrino.github.io/Neutrino-Preprint.pdf: §3, §4.6, §6.1-6.3, §7, Table 3, App. A-C, I, J.
  - [NEU-C] https://github.com/open-neutrino/neutrino/blob/main/neutrino/src/cuda.c:486-538, 583-621.
- NVBit:
  - [NVBIT] https://d1qx31qr3h6wln.cloudfront.net/publications/MICRO_2019_NVBit.pdf: §6.1, §6.2.
  - [NVBIT-T] https://github.com/eunomia-bpf/nvbit-tutorial: tools/*/inject_funcs.cu, core/utils/channel.hpp:60-120.
- Instrumentation and profiling tools:
  - [KPERF] https://arxiv.org/html/2505.21661v1: §4.3, §5.2, §6.2-6.3.
  - [PROTON] https://triton-lang.org/main/dialects/ProtonGPUOps.html.
  - [IKET] https://docs.nvidia.com/cutlass/4.6.0/media/docs/pythonDSL/cute_dsl_general/iket_profiling.html.
  - [TD] https://triton-distributed.readthedocs.io/en/latest/getting-started/profiler/intra_kernel_profiler.html.
  - [CUT] https://github.com/facebookexperimental/CUTracer/blob/main/readme.md: lines 9, 80-106.
  - [GVP] https://gvprof.readthedocs.io/en/stable/manual.html and preface.html.
  - [EGPU] https://camps.aptaracorp.com/ACM_PMS/PMS/ACM/HCDS25/10/13a8f7c0-0a7e-11f0-ada9-16bb50361d1f/OUT/hcds25-10.html: §6.1, §7.
- NVIDIA documentation:
  - [NCU] https://docs.nvidia.com/nsight-compute/ProfilingGuide/index.html: §2.2.6, §2.3.2.
  - [CUPTI] https://docs.nvidia.com/cupti/api/group__CUPTI__SASS__METRICS__API.html.
- Workload studies:
  - [CHAL] https://odr.chalmers.se/bitstreams/206b6310-8f25-4b83-a853-6526778c8518/download: Tables 5.1-5.2.
  - [ICDCS] https://www.weisongshi.org/papers/liu26-ICDCS.pdf: §V, Fig. 9.
  - [PKA] https://par.nsf.gov/servlets/purl/10302224: Table 3, §5.2.1.
  - [FAISS] https://ar5iv.labs.arxiv.org/html/1702.08734: §4.2, §5.1, §5.3.
- Workload code and builds:
  - [LCPP] https://raw.githubusercontent.com/ggml-org/llama.cpp/master/ggml/src/ggml-cuda/mmq.cu, ggml/src/ggml-cuda/CMakeLists.txt, CMakeLists.txt, docs/build.md, tools/llama-bench/llama-bench.cpp.
  - [TRITON] https://github.com/triton-lang/triton/blob/main/third_party/nvidia/backend/driver.c, line 316.
  - [FORUM] https://forums.developer.nvidia.com/t/jetpack-7-2-jetson-orin-nano-super-8gb-pytorch-for-compute-capability-8-7/379752 and https://forums.developer.nvidia.com/t/pytorch-container-26-06-py3-missing-compute-capability-8-7-kernels-for-jetson-orin-nano/375642.

## Files in this folder

- `README.md`: this report.
- `workflow-output.json`: raw workflow output (two readers, two verifiers, synthesis). `scripts/survey-workflow.js`: the workflow.
- `listings/`: `cuobjdump --list-ptx` per vendor library on this board, and `summary.txt` from `scripts/vendor_summary.py`.
- `nsys-check/`: the 11-13 check that Nsight Systems records kernels here (`README.md`, `fin.cu`).
- `scripts/patch_readme.py`, `scripts/patch_readme2.py`: the edits that applied the late verifier corrections to the synthesizer's text.
