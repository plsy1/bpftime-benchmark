export const meta = {
  name: 'gpu-buffer-designs-measured',
  description: 'Survey which GPU instrumentation systems measured their per-thread/event buffer designs and what the numbers show',
  phases: [{ title: 'Research' }, { title: 'Verify' }],
}
const CTX = `Read-only research task (no edits, no GPU runs, no posting anything anywhere).
Problem we study ("direction 1"): a GPU instrumentation runtime must store or ship per-thread data from up to millions of GPU threads. Known designs: (a) buffers sized per launch from the grid (Neutrino OSDI'25, Xtrace arXiv 2609.28769), (b) one shared buffer/ring with atomic reservation, often aggregated per warp (NVBit channel, Linux BPF ringbuf), (c) slots per hardware thread / buckets (GT-Pin), (d) aggregate on the device, no per-thread data (SASSI, gpu_ext GPU-local shards), (e) hardware sampling (CUPTI PC sampling), (f) bpftime today: per thread index, fixed count (default 1024) chosen at map creation.
We want MEASURED results about the buffering/data-collection design specifically: memory footprint, runtime overhead (and how much of it is due to the buffer/collection vs the probe), data loss/drops, scaling with thread count or event rate, blocking/stalls when full, and any head-to-head comparison between designs. For every number give: the system and design, workload, GPU, the number with units, and the exact source (paper section/table/figure, or file+line in a repo, with URL). Say explicitly when a paper reports only end-to-end overhead without isolating the buffer design, and list what was NOT measured. Prefer primary sources (paper PDFs, repos); open them, do not rely on abstracts alone.`
const OUT = { type: 'object', properties: {
  systems: { type: 'array', items: { type: 'object', properties: {
    system: { type: 'string' }, design: { type: 'string' },
    measurements: { type: 'array', items: { type: 'object', properties: {
      what: { type: 'string' }, workload_gpu: { type: 'string' }, result: { type: 'string' }, source: { type: 'string' },
      isolates_buffer_design: { type: 'boolean' } }, required: ['what', 'result', 'source', 'isolates_buffer_design'] } },
    not_measured: { type: 'string' } }, required: ['system', 'design', 'measurements', 'not_measured'] } },
  comparisons: { type: 'array', items: { type: 'string' }, description: 'head-to-head comparisons between buffer designs found, with numbers and source' },
  notes: { type: 'string' } }, required: ['systems', 'comparisons', 'notes'] }
const GROUPS = [
  { key: 'neutrino-xtrace', p: 'Sources: Neutrino (OSDI 2025 paper https://www.usenix.org/conference/osdi25 and preprint https://open-neutrino.github.io/Neutrino-Preprint.pdf, repo https://github.com/open-neutrino/neutrino) and Xtrace (arXiv 2609.28769, https://arxiv.org/abs/2609.28769, site https://g-watch.github.io). Find their evaluation of map/trace buffer memory, overhead breakdown, any dynamic (two-pass counting) sizing cost, per-warp vs per-thread buffers, and Xtrace\'s measurements of Neutrino and NVIDIA IKET.' },
  { key: 'nvbit-sassi-gtpin-cupti', p: 'Sources: NVBit (MICRO 2019 paper, release tarball/tools mem_trace channel), SASSI (ISCA 2015), GT-Pin (Intel; papers e.g. ISPASS/CGO and docs), CUPTI (PC sampling, activity buffers; NVIDIA docs), compute-sanitizer MemoryTracker sample. Find measured overhead of their data collection (e.g. NVBit mem_trace slowdowns, channel throughput, stalls when the channel is full), memory use, sampling drop rates.' },
  { key: 'academic-tracing', p: 'Sources: hip-analyzer / Darche & Dagenais (ACM TOPC 2024 "Low-Overhead Trace Collection and Profiling on GPU Compute Kernels", DOI 10.1145/3649510, and their follow-up comparing global-atomic buffer, per-CU buffer, chunk allocator, per-thread preallocation), PASTA (arXiv 2602.22103), CUDAAdvisor (CGO 2018), GPA / GPUscout / other GPU tracing papers that compare trace-buffer allocation strategies (search the literature, e.g. "GPU trace buffer allocation atomic per-warp", "GPU instrumentation event buffer overhead"). This group is the most likely to contain HEAD-TO-HEAD comparisons of buffer designs; extract them in detail.' },
  { key: 'ebpf-gpu', p: 'Sources: bpftime GPU / eGPU (HCDS 2025 paper), gpu_ext (arXiv 2512.12615, its tables/figures on overhead of observability tools such as threadhist, kernelretsnoop, launchlate vs NVBit), its revision notes in https://github.com/eunomia-bpf/gpu_ext/tree/main/workloads/llama.cpp/observability_overhead/revision-rq4 (device-array-runtime-notes.md, full-record-device-buffer-plan.md etc.: per-thread ring vs single GPU array, 892 MiB vs 22 MiB payload, 5.573% vs 90.812% overhead), upstream bpftime benchmark tables (benchmark/gpu in https://github.com/eunomia-bpf/bpftime), and for the CPU analogue the Linux BPF ringbuf vs perf buffer benchmarks (Andrii Nakryiko blog "BPF ring buffer", kernel selftests benchs). Extract what they measured about buffer design.' },
]
const res = await pipeline(GROUPS,
  g => agent(`${CTX}\n\n${g.p}`, { label: `research:${g.key}`, phase: 'Research', schema: OUT }),
  (r, g) => r ? agent(`${CTX}\n\nCheck another agent's findings against the sources. For every measurement, open the source and confirm the number, units, workload/GPU and whether it really isolates the buffer design. Correct or strike anything not supported; add important measured results it missed from the same sources.\n\nFindings:\n${JSON.stringify(r, null, 1)}`, {
    label: `verify:${g.key}`, phase: 'Verify',
    schema: { type: 'object', properties: {
      corrections: { type: 'array', items: { type: 'string' } },
      struck: { type: 'array', items: { type: 'string' } },
      added: { type: 'array', items: { type: 'string' } } }, required: ['corrections', 'struck', 'added'] } }).then(v => ({ key: g.key, research: r, verify: v })) : null)
return res.filter(Boolean)
