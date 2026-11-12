# What GPU eBPF call sites need from the host (2026-11-13)

Direction 2 of `RESEARCH_DIRECTIONS.md` (main), "Framing for a paper": the framing holds only if most
helper call sites of real GPU eBPF tools can avoid a synchronous round trip to the CPU. This survey
classifies every helper call site of the eBPF programs that bpftime runs on the GPU, in bpftime's
own GPU examples and in gpu_ext's device-side tools.

## Sources (`sources.txt`)

- bpftime dev `52126f33` (`dev/uprobe-official-4708f4092d1e`): the 20 programs under
  `example/gpu/` and the 2 under `benchmark/gpu/micro/`, with their loaders.
- gpu_ext `50a51ad` (eunomia-bpf/gpu_ext, a blobless sparse checkout of the eBPF sources and the
  sources next to the device-side ones; the full repository is several GB). Of its 135 `.bpf.c`
  files outside result snapshots (`gpu_ext-all-bpf-files.txt`), most are host-kernel policies for the
  GPU driver (eviction, prefetch, scheduling: not device-side). Device-side candidates were picked by
  their sections (`kprobe/<mangled kernel>`, `cuda__/...`), GPU helpers (ids 5xx) or GPU map types
  (15xx), and copies were dropped by content hash (`gpu_ext-device-candidates.txt`: 37 files, 15
  distinct), plus three borderline files read by hand. 15 of them are device-side programs with
  call sites (6 microbenchmarks and workloads, 9 llama.cpp observability variants of kernelretsnoop,
  threadhist and launchlate); `xsched/bpftime_hpf.bpf.c` runs on the CPU (ubpf), and
  `sass-kretprobe` and `hummingbird/hostdev` have no helper calls.

## Method (`scripts/workflow.js`)

Four groups of files; for each, one agent recorded every helper call site (file, line, helper, map
and map type, whether it is a host call in dev 52126f33, how the result is used, the key's scope,
the operation, whether the user-space loader reads the map, an idiom, a class, whether lanes of a
warp share the target, how often it runs), and a second agent checked every record against the
sources, the loaders and the trampoline (`default_trampoline.cu`), adding missed sites and
correcting fields (`records.json`: first pass, verified pass and the verifier's notes; no class
changed in the verification, a few fields and reasons did). Classes:

| class | meaning |
|---|---|
| gpu_local_already | no host call even today (GPU map types 1502/1503/1504/1512/1513/1527, ordinary ARRAY since dev 1b3d9c99, pid as a constant since aa6eb973, index and time helpers) |
| eliminate | a host call today, but the data or answer can live on the GPU (only GPU probes touch the map) |
| defer | a host call today, but the probe does not need the result now (unused, or a counter idiom whose pointer only adds), so it can be buffered on the GPU and applied later with an exact final state; printk/puts too (ordering caveat) |
| sync | the probe needs the result now and the map is shared with the CPU |
| cpu_side | uprobe sections in the same files, not run on the GPU |

## Results (`summary.txt`)

| source | device-side programs | call sites run on the GPU | already GPU-local | host calls today | eliminate | defer | sync |
|---|---:|---:|---:|---:|---:|---:|---:|
| bpftime examples and benchmark | 22 | 168 | 109 | 59 | 20 | 39 | **0** |
| gpu_ext device-side tools | 15 | 80 | 80 | **0** | – | – | – |

- **No surveyed call site needs a synchronous answer from the CPU.**
- bpftime's 59 host-call sites: 21 printk (output only); 20 map operations on maps only GPU probes
  touch (entry/exit timestamp maps such as cuda-counter's `start_ts`, a per-launch sum
  `total_time_ns`, the benchmark's hash map): eliminate; 18 map operations the CPU reads but the
  probe does not need back (counters and histograms through lookup + atomic add + `BPF_NOEXIST`
  insert, per-thread records written with `BPF_ANY`, a leader thread publishing a value): defer.
  51 of the 59 have the same target across a warp (aggregatable); 49 run once per thread per
  launch, 10 once per launch.
- gpu_ext's tools make no host call at all: they were written around host calls by hand, with
  GPU-side map types (1502/1503/1513), the GPU ringbuf, warp-leader writes and banked device
  buffers; several are explicit rewrites of bpftime's kernelretsnoop/threadhist/launchlate.
- Found while classifying (from the code, not measured): the counter idiom loses counts in its
  synchronous form. Lanes take turns through the request slot, so every lane of the first warps
  can miss on the lookup, and of their `BPF_NOEXIST` inserts only one lands; the other increments
  are lost. An exact add applied later (defer) or per warp (aggregate) would keep them.

## What this means for direction 2

The per-call-site framing holds on these programs: every host call in them could be eliminated or
deferred, and most could also be aggregated per warp. gpu_ext's authors reached the same place by
rewriting their tools with GPU-specific map types. The open contribution is to do this
automatically: tools keep ordinary maps and helpers (as written for the CPU), and the runtime picks
each call site's execution while keeping results exact, where gpu_ext's rewrites are manual and its
asynchronous shards approximate.

## Limits

- Examples, benchmarks and research tools, not production tools; tool authors who know host calls
  are slow avoid synchronous patterns (a selection effect). GPU ports of CPU tools (libbpf-tools'
  entry/exit patterns) would be the next test.
- Static counts; frequency only as a class (per thread per launch, per launch).
- "eliminate" rests on these loaders never reading the map; printk ordering is not modelled.
- rocm-counter is classified by analogy (no ROCm device attach in this tree).
