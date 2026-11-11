# Survey: measured results on GPU per-thread buffers and maps (2026-11-11)

Background for direction 1 of `RESEARCH_DIRECTIONS.md` (main), "GPU maps sized by thread index".
The summary is in that file, section "What other systems measured (survey 2026-11-11)".

Read-only workflow (`workflow-script.js`): four readers, one per group of sources (Neutrino and
Xtrace; NVBit, SASSI, GT-Pin, CUPTI and compute-sanitizer; academic GPU tracing such as
hip-analyzer, PASTA, GVProf, BARRACUDA; eBPF on GPUs: bpftime, eGPU, gpu_ext and its revision
notes, plus the Linux bpf ringbuf), each followed by a reader that opened the sources again and
corrected, struck or added numbers.

- `workflow-results.json`: every reader's structured output (findings with sources, then the
  checker's corrections, struck items and additions).
- `notes.txt`: the same, flattened for reading (partly in Chinese, as returned).

Where the checker corrected a finding, the correction holds (e.g. Neutrino's memory figures are
computed with `--memusage`, not measured; hip-analyzer's global-atomic scheme does not converge
with the others at high concurrency; the 6000x of gpu_ext Fig. 12(b) is CPU-map kernel time over
the uninstrumented baseline). Not searched yet: per-thread state on GPUs (privatization plus
reduction, per-thread counters).
