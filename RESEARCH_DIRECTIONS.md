# Research directions

Directions chosen for the work after the official-vs-dev GPU comparison (2026-11-10/11). Each entry
says what the problem is, the evidence, where upstream stands, the known designs elsewhere, and the
open questions. An entry is a direction, not a plan: the plan is made when the direction is
started. Newest last.

## 1. GPU maps sized by thread index: per-thread arrays and the GPU ringbuf

Decided 2026-11-11.

### The problem

Two bpftime GPU map types keep one copy per GPU thread *index*:

| type | number | memory |
|---|---|---|
| `BPF_MAP_TYPE_PERGPUTD_ARRAY_MAP` / `_HOST_MAP` | 1502 / 1512 | value_size x max_entries x thread_count |
| `BPF_MAP_TYPE_GPU_RINGBUF_MAP` | 1527 | (header + max_entries x (8 + value_size)) x thread_count |

The slot of a thread is its linear global id in the launch (`getGlobalThreadId`,
`attach/nv_attach_impl/trampoline/default_trampoline.cu`). The other GPU maps (1501 hash, 1503/1513
shared array, 1504 array shared with a bpf map) are sized by entries only.

- **thread_count is fixed when the map is created.** The syscall server creates the map when the
  tool loads the program, before any kernel is launched and in another process, so it cannot know
  the grid. The default is 1024 (`bpf_map_attr::gpu_thread_count`, `runtime/include/bpftime_shm.hpp`);
  the only override is the env `BPFTIME_MAP_GPU_THREAD_COUNT` on the syscall server. Official reads it
  for 1502 and 1527, not for 1512 (the map type used on the Orin); dev 3823cac5 makes 1512 read it.
- **Threads past thread_count lose data.** Ringbuf: they return without writing, without a drop
  count or a warning (official and dev). Per-thread array: official returns an address past the map
  (writes into whatever follows in the shared-memory segment) and printfs once per thread; dev
  518f05b5 returns NULL / -E2BIG and warns once.
- **Memory grows with the grid, not with the GPU.** The micro benchmark's ring is 408 B per thread:
  about 400 MB at 1M threads, more than the default segment (official 50 MB, dev 128 MB); ringbuf
  creation then halves thread_count until it fits ("Clamped GPU ringbuf thread count"). The Orin Nano
  holds only 12,288 threads at a time (8 SMs x 1536, measured with `cudaGetDeviceProperties`).
- **Slots belong to a thread index, not to a running thread.** Two kernels running at the same time
  share slots (dev's ringbuf takes a busy word, 518f05b5, for this case).
- **The benchmark is affected.** In `gpu/compare-20261110` no thread count was set, so GPU Ringbuf
  large/xlarge have only the first 1024 threads writing; the xlarge number (+50-55 us) is likely
  mostly about 1M threads taking the `tid >= max_thread_count` return (inferred, not measured). Per-GPU-thread array medium and
  larger cannot be compared at all (official writes past the map).

### Evidence from upstream's own work

- CI needs a different thread count per example ([#490](https://github.com/eunomia-bpf/bpftime/pull/490)):
  threadhist 524288, kernel_trace 4096, kernelretsnoop 10 "to avoid bad_alloc", default 8192.
- gpu_ext revision notes (llama.cpp, RTX 5090,
  [device-array-runtime-notes.md](https://github.com/eunomia-bpf/gpu_ext/blob/main/workloads/llama.cpp/observability_overhead/revision-rq4/device-array-runtime-notes.md)):
  kernelretsnoop's payload is 22 MiB, but the ring allocates 524,288 thread slots, about 892 MiB. A
  single shared GPU array used as an event buffer cost 5.573% paired overhead; a collector still on
  the per-thread ring, 90.812%.

### Where upstream stands (checked 2026-11-11)

- **master:** no change to these paths between 4708f409 and 39cc1fce. The current behaviour came in
  as stopgaps: default 1000 ([#423](https://github.com/eunomia-bpf/bpftime/pull/423), whose
  description already says the server cannot know the thread count), 1024x1024 plus the env and the
  per-thread warning ([#501](https://github.com/eunomia-bpf/bpftime/pull/501)), back to 1024
  ([#515](https://github.com/eunomia-bpf/bpftime/pull/515)), the ringbuf silent return and halving
  ([#490](https://github.com/eunomia-bpf/bpftime/pull/490)), 1512 without the env
  ([#522](https://github.com/eunomia-bpf/bpftime/pull/522)).
- **issues:** [#530](https://github.com/eunomia-bpf/bpftime/issues/530) (threadhist counts only 1024
  threads) was answered with the env and "we will improve it", then closed;
  [#552](https://github.com/eunomia-bpf/bpftime/issues/552) (2026-03) lists the
  `exceeds max_thread_count` warning as pre-existing.
- **unmerged branches** `official/revision/*` (2026-09, no PR): thread-count validation and
  write-back to attr (201bb2dc, af759303), a ringbuf that counts every drop by reason (052513a5),
  opt-in warp-leader-only output (89a12447). The per-thread-index design is unchanged.
- **direction in the authors' papers:** gpu_ext ([arXiv 2512.12615](https://arxiv.org/abs/2512.12615))
  and LPC 2025: run eBPF once per warp, and hierarchical maps (GPU-local shards merged periodically).
  Not in bpftime master.
- Nothing upstream sizes these maps by the launch grid or by resident threads.

### Designs elsewhere

| system | buffer | when full |
|---|---|---|
| NVBit | one channel per context (mem_trace 1 MB), atomic reservation, one record per warp | GPU waits for the host |
| Neutrino (OSDI '25) | allocated per launch from the grid (per thread or per warp), passed as a kernel parameter | no bounds check |
| Xtrace (arXiv 2609.28769) | per launch, one slot per thread; per warp when registers are short | — |
| compute-sanitizer MemoryTracker sample | one global array, atomic index | drop, countable |
| CUPTI activity | per-context device buffers | drop, counted |
| GT-Pin | buckets <= hardware threads, hashed by hardware thread id | — |
| SASSI | no per-thread state; aggregated on the GPU | — |
| bpf ringbuf | one ring for all CPUs, reservation under a lock | reservation fails, drop |

PTX documents `%warpid` and `%smid` as able to change after preemption; Neutrino computes a virtual
warp index from `%ctaid`/`%tid` instead.

### Approaches to evaluate

1. **Size at launch.** The agent already hooks every launch and sees grid x block; allocate (or
   grow) the per-thread storage for that launch, as Neutrino and Xtrace do.
2. **Shared buffer with reservation, aggregated per warp.** One ring (or one per SM) with an atomic
   reservation by a warp leader; memory fixed, independent of the grid.
3. **Slots by hardware position** (SM x warp slot x lane, or buckets as in GT-Pin): 12,288 slots on
   the Orin. Fits accumulation and entry-to-exit scratch, not per-logical-thread results; needs
   `%warpid`/`%smid` to stay put between entry and exit.
4. **Cheaper fixes on the current design:** warn on the host at launch when grid x block exceeds a
   map's thread count; count every drop; make 1512 read the env and readback use the effective
   count (dev already has the last two through 3823cac5).

### Open questions

- Which real workloads need per-logical-thread results, and which only need accumulation or event
  streaming? That decides between approaches 1 and 2/3.
- Cost on the Orin of an atomic reservation in host-registered memory (uncached for the GPU) against
  per-thread rings.
- How concurrent kernels and CUDA graphs should share or separate these buffers.
- What the host-side `bpf()` view of a per-thread map should be when its size changes per launch.

### Noticed while reading, not verified

- `BPF_MAP_TYPE_GPU_HASH_MAP` (1501) creates `next_prime(max_entries)` buckets but a value array of
  `max_entries` entries (`nv_gpu_shared_hash_map.cpp:17-22`), so keys hashing to the last buckets
  may fail to store or read (official and dev alike).
