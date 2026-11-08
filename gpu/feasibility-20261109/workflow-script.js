export const meta = {
  name: 'gpu-orin-understand',
  description: 'Map bpftime CUDA attach support and its feasibility on Jetson Orin (sm_87, CUDA 13.2, aarch64)',
  phases: [
    { title: 'Read', detail: 'five independent readers: attach pipeline, build, examples, board environment, upstream' },
    { title: 'Synthesize', detail: 'feasibility map, ranked blockers, experiment plan' },
    { title: 'Critic', detail: 'what is missing, unverified or contradictory' },
  ],
}

const REPO = '/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate'
const SCRATCH = '/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/gpu-env'

const COMMON = `
Context: we are evaluating whether bpftime's GPU (CUDA) support works on this NVIDIA Jetson Orin board.
Board facts already known: aarch64, Orin integrated GPU (nvgpu driver), compute capability 8.7 (sm_87), L4T R39.2.1,
CUDA toolkit 13.2 at /usr/local/cuda-13.2 (/usr/local/cuda -> it), nvcc 13.2.86, driver 595.78. bpftime builds here use
GCC 13 and LLVM 15 (/usr/lib/llvm-15), Frida-gum for hooking. The bpftime source tree to study is ${REPO}
(our dev branch = upstream eunomia-bpf/bpftime at 177a1dc plus our own runtime patches; the GPU code under
attach/nv_attach_impl, runtime/src/bpf_map/gpu and example/gpu is essentially upstream's). The upstream remote is
named "official" in that repo (official/master = 25e250d6 is fetched). Nothing has ever been built with
-DBPFTIME_ENABLE_CUDA_ATTACH=ON on this board.
Rules: read-only on the repository (no edits, no commits, no builds inside ${REPO}). Put any scratch files under
${SCRATCH} (create it). Every command you run must have a timeout (e.g. \`timeout 60 ...\`). Do not run anything
heavy or long; do not use LD_PRELOAD. Cite evidence as file:line (or command + output). Say "not found" or
"unverified" rather than guessing. Your final answer is data for another agent, not a message to a human.`

const FINDINGS = {
  type: 'object',
  properties: {
    summary: { type: 'string', description: 'one paragraph: what this area is and the bottom line for Jetson Orin' },
    findings: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          topic: { type: 'string' },
          detail: { type: 'string' },
          evidence: { type: 'string', description: 'file:line or command and output' },
          jetson_risk: { type: 'string', enum: ['none', 'low', 'medium', 'high', 'blocker', 'unknown'] },
          why_risky: { type: 'string' },
        },
        required: ['topic', 'detail', 'evidence', 'jetson_risk'],
      },
    },
    commands_to_try: { type: 'array', items: { type: 'string' }, description: 'concrete commands for the next step, with timeouts' },
    open_questions: { type: 'array', items: { type: 'string' } },
  },
  required: ['summary', 'findings', 'commands_to_try', 'open_questions'],
}

const READERS = [
  {
    key: 'pipeline',
    prompt: `${COMMON}
Your area: the CUDA attach pipeline itself, end to end. Read attach/nv_attach_impl/ (nv_attach_impl.cpp,
nv_attach_impl_frida_setup.cpp, nv_attach_fatbin_record.*, nv_elf_introspect.*, nv_attach_impl_patcher.cpp,
memory_utils.hpp, pass/*, ptx_compiler/*, trampoline/*, README.md, README-passes.md, configs/), the GPU maps under
runtime/src/bpf_map/gpu/ and wherever the runtime/agent/syscall-server reference BPFTIME_ENABLE_CUDA_ATTACH
(grep), and how an eBPF program becomes PTX (llvmbpf generate_ptx in vm/llvm-jit, the NVPTX target and the
target CPU string it passes, e.g. "sm_60").
Answer: (1) the data flow from the application's CUDA registration calls to the instrumented kernel running:
which functions are hooked (and in which library: static cudart inside the app, libcudart.so, libcuda.so), how the
fatbin is parsed, whether it needs PTX inside the fatbin or can use SASS/cubin only, how the PTX is rewritten,
how it is recompiled (nvPTXCompiler library? a spawned nvcc/ptxas? which arch flag), how the new module is loaded
(cuModuleLoadData? replacing the fatbin pointer?), how eBPF maps and the host<->device channel are set up (which
CUDA memory APIs: cudaHostRegister, cuMemHostAlloc, cudaMallocManaged, cuMemHostGetDevicePointer, ...; does it
assume a discrete GPU, UVA, pageable memory access, or host-registering shared memory from boost shm/mmap?),
how the host side reads results; (2) every hard-coded architecture, PTX ISA version, CUDA version, path, or
x86-only assumption (grep for sm_, compute_, .version, .target, cuda-12, lib64, x86); (3) every place likely to
break on an integrated-GPU aarch64 Jetson with CUDA 13.2 and sm_87, with severity.`,
  },
  {
    key: 'build',
    prompt: `${COMMON}
Your area: building bpftime with -DBPFTIME_ENABLE_CUDA_ATTACH=ON here. Read the CMake logic: top-level
CMakeLists.txt and cmake/*.cmake (BPFTIME_ENABLE_CUDA_ATTACH, BPFTIME_CUDA_ROOT, CUDA_PATH), attach/CMakeLists.txt,
attach/nv_attach_impl/CMakeLists.txt, attach/nv_attach_impl/ptx_compiler/CMakeLists.txt, test/CMakeLists.txt,
runtime/CMakeLists.txt (and runtime/agent, runtime/syscall-server, runtime/unit-test) for every CUDA-related line,
vm/ for NVPTX/LLVM requirements, the trampoline Makefile and how trampoline_ptx.h is produced, and the example
Makefiles under example/gpu/ (arch flags such as -arch=sm_XX / -gencode). Check against this board's CUDA layout
(list /usr/local/cuda-13.2/lib64, /usr/local/cuda-13.2/targets/aarch64-linux/lib, include dirs, stubs/libcuda.so,
libnvptxcompiler_static.a, libcudart_static.a, libnvrtc*, nvdisasm, cuobjdump, ptxas) and LLVM 15
(llvm-config-15 --targets-built, and whether llc-15 -march=nvptx64 -mcpu=help lists sm_87).
Answer: the exact cmake configure command to try (based on the dev build used here: RelWithDebInfo, gcc-13/g++-13,
LLVM_DIR=/usr/lib/llvm-15/lib/cmake/llvm, -DBPFTIME_LLVM_JIT=ON, unit tests ON), every library/path/flag the CUDA
build expects and whether this board has it, predicted configure/compile/link failures (with the CMake line),
CUDA 13 incompatibilities you can see (removed APIs, header changes, deprecated arch flags), and how long a CUDA
build is likely to take relative to the existing build (count of extra translation units).`,
  },
  {
    key: 'examples',
    prompt: `${COMMON}
Your area: what to run to prove GPU probes really fire. Read example/gpu/README.md and every example directory
under example/gpu/ (cuda-counter, kernel_trace, threadhist, mem_trace, launchlate, gpu_shared_map, host_map_test,
kernelretsnoop, cudagraph, directly_run_on_gpu, threadscheduling, atomizer, cutlass, faiss-test, llama-cpp-test,
pytorch-test, rocm-counter, ...), benchmark/gpu/ (README.md, micro/, workload/, nvbit/, host/, run_cuda_bench.py),
and the GPU tests (attach/nv_attach_impl/test/*, runtime/unit-test cuda tests, e.g. cuda/test_cuda_compile.cpp).
For each example: what it demonstrates, which eBPF program and which CUDA program it uses, how it is built
(nvcc flags, arch), how it is run (bpftime load / start, env vars), what output proves the probe ran inside the
kernel (vs the tool merely loading), and external dependencies (PyTorch, llama.cpp, faiss, cutlass, ROCm: are
they realistic on this Jetson?). Rank the 3-5 smallest examples to try first, with the exact commands adapted to
this board (sm_87, /usr/local/cuda-13.2, timeouts outside LD_PRELOAD), and the expected output for each. Also list
which unit tests can run here and what they need.`,
  },
  {
    key: 'board',
    prompt: `${COMMON}
Your area: this board's CUDA environment, measured. Write tiny CUDA programs under ${SCRATCH} and compile them with
/usr/local/cuda-13.2/bin/nvcc -arch=sm_87 (or -gencode arch=compute_87,code=[sm_87,compute_87]), each run bounded
by timeout 60, to report: (1) device attributes: integrated, unifiedAddressing, canMapHostMemory,
pageableMemoryAccess, pageableMemoryAccessUsesHostPageTables, concurrentManagedAccess, hostRegisterSupported,
canUseHostPointerForRegisteredMem, managedMemory, computePreemptionSupported, cooperativeLaunch, l2CacheSize, SM
count, max threads; (2) whether cudaHostRegister works on (a) malloc memory, (b) an mmap MAP_SHARED of a file in
/dev/shm (as boost interprocess shared memory is), with and without cudaHostRegisterMapped, and whether a kernel
can then read/write it through cudaHostGetDevicePointer, and whether a second process mapping the same /dev/shm
file sees the GPU's writes; (3) whether cuModuleLoadData accepts PTX text targeting sm_87 and also PTX targeting an
older .target (sm_60) with an older .version, i.e. JIT forward compatibility through the driver; (4) the PTX ISA
version ptxas 13.2 accepts and the default .version nvcc 13.2 emits for sm_87; (5) whether the fatbin of a program
built with -arch=sm_87 contains PTX (cuobjdump --list-ptx / --list-elf), and with which nvcc flags it does;
(6) whether CUDA programs link cudart statically by default here (and which libcuda.so the process loads at run
time: check /proc/<pid>/maps of a short-lived run, or ldd); (7) LLVM 15 NVPTX: does llc-15 accept -mcpu=sm_87 and
which max PTX version it emits (try a tiny .ll); (8) anything in dmesg-free user-visible logs that suggests the
nvgpu driver restricts any of the above (no sudo needed; do not change system settings). There may be earlier GPU
demos (mem.cu, vec_add.cu) from another session under /tmp/claude-2002/-home-jetson-src/*/scratchpad; reuse ideas
but write your own files. Report raw numbers and outputs.`,
  },
  {
    key: 'upstream',
    prompt: `${COMMON}
Your area: what upstream and the literature say. (1) In ${REPO}, run \`git log --oneline 177a1dc465ff..official/master
-- attach/nv_attach_impl runtime/src/bpf_map/gpu example/gpu benchmark/gpu vm/llvm-jit\` (and look at the diffs of
the GPU-relevant commits) to list what changed in GPU support upstream since our base, and \`git log --oneline -40 --
attach/nv_attach_impl\` on our branch for its recent history. (2) Use web search/fetch for eunomia-bpf/bpftime
GitHub issues, PRs and discussions about Jetson, Orin, Tegra, aarch64/arm64 CUDA, integrated GPU, CUDA 13, sm_87,
"unified memory", and for the bpftime GPU work (the eGPU paper / "bpftime GPU" arXiv papers, the docs site
eunomia.dev pages on GPU) to find which platforms and CUDA versions are claimed or tested, and known limitations.
(3) Check NVIDIA documentation for CUDA 13 changes that matter here: architectures supported by CUDA 13.x for Jetson
Orin (sm_87), removed/deprecated APIs (e.g. legacy driver APIs, cudaHostRegister on Tegra, __cudaRegisterFatBinary
internals, fatbin format changes), and Jetson/Tegra memory model specifics (pinned vs pageable, I/O coherence,
cudaHostRegister support on Tegra). Report each item with its URL or commit id.`,
  },
]

phase('Read')
const reads = await parallel(READERS.map(r => () =>
  agent(r.prompt, { label: `read:${r.key}`, phase: 'Read', schema: FINDINGS })
    .then(res => res ? { key: r.key, ...res } : null)))
const got = reads.filter(Boolean)
log(`readers returned: ${got.map(r => r.key).join(', ')} (${got.length}/${READERS.length})`)

phase('Synthesize')
const SYNTH = {
  type: 'object',
  properties: {
    verdict: { type: 'string', description: 'can bpftime GPU support plausibly work on this Orin, and what is the biggest unknown' },
    architecture_summary: { type: 'string' },
    blockers: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          rank: { type: 'integer' },
          title: { type: 'string' },
          detail: { type: 'string' },
          evidence: { type: 'string' },
          confidence: { type: 'string', enum: ['confirmed', 'likely', 'possible'] },
          possible_fix: { type: 'string' },
        },
        required: ['rank', 'title', 'detail', 'evidence', 'confidence', 'possible_fix'],
      },
    },
    experiment_plan: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          step: { type: 'integer' },
          goal: { type: 'string' },
          commands: { type: 'array', items: { type: 'string' } },
          success_criterion: { type: 'string' },
          if_it_fails: { type: 'string' },
        },
        required: ['step', 'goal', 'commands', 'success_criterion', 'if_it_fails'],
      },
    },
    contradictions: { type: 'array', items: { type: 'string' } },
    open_questions: { type: 'array', items: { type: 'string' } },
  },
  required: ['verdict', 'architecture_summary', 'blockers', 'experiment_plan', 'contradictions', 'open_questions'],
}
const synth = await agent(`${COMMON}
You are combining five independent investigations of bpftime's CUDA support on this Jetson Orin into one
feasibility map. Their reports (JSON) follow. Cross-check them against each other and, where they disagree or a
claim matters a lot, against the source (${REPO}) or a quick bounded command yourself. Produce: a verdict; a short
architecture summary of how the CUDA attach works; the blockers ranked by how likely they stop us (confirmed /
likely / possible, with evidence and a possible fix each); and an ordered experiment plan for the next session
(build configuration, the first example to run, how to verify a probe really ran inside the GPU kernel, and what to
do when a step fails), keeping the board's rules (timeouts, no measuring 03:25-04:15 JST, builds in a new build
directory such as ${REPO}/build-cuda, not build-probecheck). List contradictions between the reports and open
questions.

REPORTS:
${JSON.stringify(got, null, 1)}`, { label: 'synthesize', phase: 'Synthesize', schema: SYNTH })

phase('Critic')
const CRITIC = {
  type: 'object',
  properties: {
    gaps: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          gap: { type: 'string' },
          why_it_matters: { type: 'string' },
          how_to_close: { type: 'string' },
          severity: { type: 'string', enum: ['high', 'medium', 'low'] },
        },
        required: ['gap', 'why_it_matters', 'how_to_close', 'severity'],
      },
    },
    claims_to_doubt: { type: 'array', items: { type: 'string' } },
  },
  required: ['gaps', 'claims_to_doubt'],
}
const critic = await agent(`${COMMON}
You are a skeptical reviewer of a feasibility study. Below are the five reader reports and the synthesis. Find what
is missing (an area nobody read, a CUDA API path nobody traced, a claim with no evidence, a step of the experiment
plan that cannot work as written, a board fact assumed but not measured), and which claims are doubtful. Spot-check
two or three of the most important claims directly in ${REPO} or with a bounded command. Be concrete.

READER REPORTS:
${JSON.stringify(got, null, 1)}

SYNTHESIS:
${JSON.stringify(synth, null, 1)}`, { label: 'critic', phase: 'Critic', schema: CRITIC })

return { readers: got, synthesis: synth, critic }
