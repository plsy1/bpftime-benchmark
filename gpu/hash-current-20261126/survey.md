# Read-only survey of GPU hash maps (2026-11-26, four readers, no runs)

## Reader
Reader D: what our results and docs already say about GPU hash maps. Everything here was read only; nothing was run.

Abbreviations used below:
- pp512 = llama-bench 512-token prefill; tg128 = 128-token decode.
- "minimal" = the upstream micro benchmark's 1 block x 32 threads, one call per thread per launch.
- D/N = hostcall-l1's two builds: D = dev c115b33d, N = dev with the layer-1 commits that took array maps and pid off the host path.
- AC3 = the host-call watcher waiting rule kept in dev 52126f33. While a probed kernel may run it polls in a window that doubles from 10 to 50 us, then sleeps 100 us; otherwise it parks. WFE (busy-wait on the CPU's wait-for-event) is opt-in.
- UnitFold = the direction-2 design for deferring hash-map host calls. It was never built.

1. What a GPU hash operation is today (from our docs, old dev base).
- Every operation on BPF_MAP_TYPE_HASH (and on PERCPU_*/LRU/LPM maps) from a GPU probe is a synchronous host call. Every delete on anything but an ordinary ARRAY is one too (/home/jetson/src/bpftime origin/main:RESEARCH_DIRECTIONS.md:159-162).
- On the Orin, 1501 (GPU_HASH_MAP) is created as an ordinary HASH. The current proto tree does this in /home/jetson/src/bpftime-proto-70f7/runtime/include/bpftime_shm.hpp:160-173: "for the GPU hash map, whose every device operation is a host call, an ordinary hash map". The docs' `:137-154` is stale.
- So the 1501 implementation (nv_gpu_shared_hash_map) has never been exercised on this board. Not found: any run of a 1501 map on the Orin.
  - gpu_shared_map and threadscheduling_dynamic_hook were blocked on 11-09 (/home/jetson/src/results/gpu/examples-20261109/README.md:221-224).
  - The later fix matrix did not include them (/home/jetson/src/results/gpu/fixes-20261109/protocol/runs/san1-summary.txt).
- Per call, each GPU thread:
  1. queues on a per-module lock in GPU memory;
  2. takes occupy_flag in the single per-process CommSharedMem in the registered shared segment;
  3. writes the request, issues membar.sys, sets flag1 and spins on flag2;
  4. clears flag2, issues membar.sys, reads the response, issues membar.sys and releases.
  - Lanes of a warp take turns (a 32-step loop). There are four membar.sys per call.
  - The watcher takes flag1 with an acquire CAS and answers with a release store of flag2. All calls of all threads are served one after another (RESEARCH_DIRECTIONS.md:163-167; /home/jetson/src/results/gpu/dir2-design-20261113/DESIGN.md:58-66).
  - A lookup hit also costs the watcher address translation: cuPointerGetAttribute, 0.21 us (/home/jetson/src/results/gpu/fixes-20261109/protocol/README.md:43-45). The docs list "two driver calls per hit" as open.
- Not supported:
  - no device path for 1501;
  - MAP_GET_NEXT_KEY has neither a GPU sender nor a host handler (fce97abb:RESEARCH_DIRECTIONS.md:604-606);
  - CAS written by a tool is unusable, because the JIT drops the cmpxchg result: 102 counted vs 122,880 claims (/home/jetson/src/results/gpu/step0-checks-20261115/README.md:99,145).

2. Measured host-call costs. All are from the old base (d670246b / c115b33d / aa6eb973 / 52126f33). On the 70f7 base only the unit tests were rerun (`[cuda-host-call]` etc. pass, gpu/map-model-move-70f7bc1f-20261126/README.md:20); no host-call timing was taken. The same commits exist there as 220fe2a7, e59b70e2, 9292e0d3 and 885aae24.
- **llama.cpp (the 307x).** dev 52126f33, rms_norm kretprobe. Map: BPF_MAP_TYPE_HASH, max_entries 1, key 0 pre-inserted; each thread does a lookup (a hit) plus __sync_fetch_and_add (probes/th-hostcall/threadhist.bpf.c). Results (/home/jetson/src/results/gpu/llama-census-20261113/README.md:131-149):
  - 32-token prefill 7.63 s against 24.9 ms without the agent, 307x;
  - 10.8 us per call, about 93k calls/s served one after another;
  - 1,411,072 = 2 x 705,536 counted, none lost.
- **Protocol test**, update + lookup per thread on a hash map, dev d670246b (protocol/README.md:36-41):
  - 15.6 / 10.4 / 9.9 / 9.8 us per call with 1 thread / 1 warp / 8 warps / 32 warps;
  - with only the host-memory lock (no module lock in front): 74 / 300 us at 8 / 32 warps;
  - the bare round trip is about 5 us.
- **Upstream micro benchmark, CPU Hash cases, minimal geometry.**
  - official 4708f409: about 33.7 ms per launch (about 1 ms per call, the watcher's sleep);
  - dev c115b33d: update 323, lookup 337, delete 319 us per launch, against a 39.7 us baseline (/home/jetson/src/results/gpu/compare-20261110/README.md:81-83);
  - unchanged by layer 1: D 324.3 / 339.0 / 318.1 against N 327.5 / 341.1 / 317.7 us; still 128 host requests per case (/home/jetson/src/results/gpu/hostcall-l1-20261111/summary.md:8-10; README.md:27,41).
  - Caveat: lookup misses on an empty map, delete removes a missing key, and update writes key 0 from 32 threads (benchmark/gpu/micro/cuda_probe.bpf.c:158-184; DESIGN.md:839).
- **CUDA Counter** (HASH maps plus printk; 8 host calls per thread after aa6eb973, 10 before):
  - 3,486 to 2,877.5 us per launch, about 11 us per call (DESIGN.md:72-75);
  - RESEARCH_DIRECTIONS gives the rounded 3,487 to 2,929 us.
- **Watcher wake-up**, one HASH update per launch, aa6eb973 (hostwake-20261111/README.md:29-41):
  - 14-15 us while the watcher polls;
  - 60-950 us after gaps over 200 us.
- **With AC3** (dev 52126f33, watcher-policy-20261112/dev/README.md:63-73):
  - a sparse call takes about 69-71 us (WFE build: about 14 us);
  - 32-call bursts cost 11.6-11.8 us per call without load and 15.8 us under full load;
  - calls during graph launches take 70 us.
- **gpu_ext's Fig. 12(b):** 34 ms per CPU array/hash op, "6000x". That is official's serialized RPC with its 1 ms sleep, not PCIe (buffer-survey-20261111/notes.txt:160; fce97abb:RESEARCH_DIRECTIONS.md:336).

3. Correctness found with hash maps on the GPU.
- CUDA Counter prints calls=97 where 128 probes ran. All 32 lookups of a warp are answered before any lane branches; of the first-touch BPF_NOEXIST inserts only one lands; a plain `*v+=d` collapses to about one update per warp (DESIGN.md:77-80).
- On 11-09: cuda-counter calls=51 and threadscheduling SM=51 / warps=24 instead of 60 (examples-20261109/README.md:332-334).
- thread_mapping held 1 key of 10 before the per-thread-arguments fix; afterwards 10 (examples README:58; san1-summary).

4. Survey counts (/home/jetson/src/results/gpu/callsite-survey-20261113/README.md:43-62 and summary.txt:130-197).
- **bpftime, 22 device-side programs:** 168 GPU call sites, 59 of them host calls, classed 20 eliminate / 39 defer / 0 sync.
  - Of the 59, 38 are hash-map operations and 21 are printk.
    - 32 sites on HASH: micro 3, cuda-counter 8, rocm-counter 9, cudagraph 7, threadscheduling 5;
    - 6 sites on 1501: gpu_shared_map 1, threadscheduling_dynamic_hook 5;
    - so 7 of 22 programs use a hash map (my count from summary.txt).
  - Patterns:
    - counter: lookup + atomic add, else a NOEXIST insert;
    - entry/exit timestamp keyed per block (start_ts);
    - per-launch sum (total_time_ns);
    - per-thread record written with BPF_ANY (thread_mapping);
    - leader-thread publish;
    - SM / warp histograms.
  - Key scopes over all 59 sites: constant 26, per_block 14, none 11, per_thread 4, per_warp 4. 51 sites are aggregatable per warp.
  - Upstream threadscheduling keys by sm_id (128 entries) and (sm<<16|warp) (1024); the dynamic_hook copy adds (block_x<<16|thread_x) (4096) (overlap-survey-20261113/workflow-output.json).
- **gpu_ext:** 15 device programs, 80 sites, 0 host calls and no HASH; every key is a bounded index (RESEARCH_DIRECTIONS.md:238-239).
- **libbpf-tools:** HASH in 48 of 57 tools, PERCPU_HASH in 1 (RESEARCH_DIRECTIONS.md:202; fce97abb:627-628).
- **Counter and histogram uses** (25 uses / 52 sites): keys are 0, the pid, the SM or warp id, tid%10, or a latency bin (dir1-design-space-20261115/readers/needs.md:23).

5. Design-space notes on hash maps.
- RESEARCH_DIRECTIONS.md:125-126: "Not in focus: hash (arbitrary-key) maps on the GPU... no GPU-native tool is known to need one; gpu_ext's tools use fixed-bin arrays."
- fce97abb:753-755 and 790: moved into direction 1; "left out until a need shows".
- Noticed, not verified (RESEARCH_DIRECTIONS.md:145-147): 1501 makes next_prime(max_entries) buckets but a value array of max_entries, so keys in the last buckets may fail.
- UnitFold (dir2 DESIGN.md; never built):
  - a per-HASH ledger in the shared segment;
  - slots created only by the host, so a key's first touch stays synchronous;
  - per-(slot, shard) cumulative records the GPU writes with plain stores and one membar.sys;
  - records folded into the map under its stripe lock at every host access;
  - a hashed lock table in device memory and warp sums with shfl.sync;
  - its numbers are expectations only (DESIGN.md:839-844).
- Step 0 atomic costs, relevant to any device-side table (step0-checks README:76,139):
  - contended atomics in registered memory serialize at about 80 ns (82-106 ns), against about 1.3 ns in device memory at 48 warps;
  - a CAS loop on one word per SM is 45x dearer in registered memory.
- Not found: per-thread hash copies, sum-like hash maps, or hash capacity in dir1-design-space, map-model-proto (1530/1531/1532 are arrays only), probe-cost, map-lowering or byvalue. The grep hits there are hashlib, %gridid hashing and GT-Pin thread hashing.
- Projections, not measured (llama README:103-129):
  - cuda-counter on every kernel would take 4.2-6.7 h per pp512;
  - one host call per launch would cost 26-42% of decode GPU time.

Open questions:
- No host-call timing on the current 70f7bc1f base (proto 7d02d677). Are the old-base numbers still valid there (10.8 us per call, the 323-341 us micro hash cases, the 69 us sparse call)?
- Every GPU hash timing so far uses one key on a tiny or empty map. Not measured:
- distinct keys per thread, per warp or per block;
- a populated map;
- inserts near capacity (E2BIG);
- deletes of present keys;
- lookups that hit and pay address translation, apart from the llama run's single key.
- Is the 1501 next_prime(max_entries) buckets vs max_entries values mismatch real? It was noticed, not verified. 1501 is never created on the Orin (it becomes HASH), so it cannot be checked here as is.
- Lookup address translation: how much of a hit's cost is the two driver calls (cuPointerGetAttribute and the device-pointer lookup)? Only cuPointerGetAttribute's 0.21 us was timed.
- How large is the counter-idiom loss (calls=97 of 128) with HASH at larger grids and distinct keys? It was observed only at 32 threads x 4 launches and 10 threads x 6 launches.
- Do GPU-native tools need arbitrary-key maps at all? The surveys found only bounded keys (0, pid, SM, warp, tid%10, bins) outside CPU-style examples. RESEARCH_DIRECTIONS keeps hash maps 'not in focus' until a need shows.
- What does a device-side hash insert cost? It needs CAS, which must sit in helpers or the trampoline because tool-written CAS is broken. Step 0 measured atomics at about 80 ns in registered memory against about 1 ns in device memory, but no hash-shaped workload.
- No per-thread-copy, sum-like or capacity design for hash maps exists in dir1-design-space or the prototype. UnitFold (dir2, never built) is the only hash-specific design, and it keeps the host as the owner of keys.

## Reader
READER C: how GPU tools use hash maps (tree /home/jetson/src/bpftime-proto-70f7 @ 7d02d677, tools in /home/jetson/src/bpftime-gpu-work)

== 0. What a device-side hash op is on the Orin today ==
- A program counts as a GPU (device) program only if its name starts with `cuda__` (runtime/include/bpftime_prog.hpp:103-106: `return name.starts_with("cuda__");`). Its SEC is kprobe/kretprobe/<mangled CUDA kernel>.
- The trampoline has no hash fast path. In default_trampoline.cu the constant `BPF_MAP_TYPE_GPU_HASH_MAP = 1501` (line 72) is never used. _bpf_helper_ext_0001/0002/0003 handle only 1530/1531/1532/1502/1503/1504/1512/1513 and ARRAY(2)-with-extra_buffer. Plain HASH (1) and 1501 fall through to `host_call` (lookup 342-350, update 436-447, delete 462-469).
- On this board 1501 is never built as nv_gpu_shared_hash_map_impl. The syscall server sees an integrated GPU and creates it as a plain BPF_MAP_TYPE_HASH (syscall_context.cpp:873-881 -> bpftime_shm.hpp:160 gpu_map_type_without_cuda_ipc: `case BPF_MAP_TYPE_GPU_HASH_MAP: return BPF_MAP_TYPE_HASH`). That map is bpftime_hash_map in the shared segment. On this board, GPU_HASH and HASH therefore behave the same.
- What one GPU thread does per op (host_call, default_trampoline.cu:140-180):
  1. A 32-turn loop per warp, so the lanes are serialized.
  2. The lane whose turn it is spins on atomicCAS of `__bpftime_module_comm_lock` (GPU global memory), then on atomicCAS of `comm->occupy_flag` (host memory, one slot per process).
  3. membar.sys. It writes request_id and map_id, then copies key_size bytes (plus value_size bytes and flags for an update) with a byte loop (simple_memcpy, 181-185).
  4. membar.sys. It sets flag1=1 and spins on flag2 in host memory until the CPU watcher answers.
  5. It clears flag2, membar.sys, reads resp, membar.sys, releases both locks. `__syncwarp(active)` runs after every turn.
- Host side, the watcher's serve_cuda_request (runtime/src/attach/bpf_attach_ctx_cuda.cpp:126-165) runs bpftime_map_{lookup,update,delete}_elem_with_origin on the CPU hash map.
- A lookup answers with gpu_address_of_map_value (bpf_attach_ctx_cuda.cpp:581-595). That makes two driver calls (cuPointerGetAttribute, then cuMemHostGetDevicePointer) and returns a device pointer into the registered segment. The GPU then dereferences that pointer or does an atomic add through it.
- Staging buffers hold up to 1 MiB per key and per value (gpu_helper_protocol.h:15,32-43), so the 80-byte thread_mapping value fits.
- Measured cost, from results not re-run on 7d02d677: 9.8-15.6 us per call, 11-22 us per call in 32-lane bursts, about 70 us for launches the watcher is not told about (CUDA graphs). All calls of the process are serialized (results gpu/real-scenarios-20261113/README.md:33).
- Not supported on the device: a device-resident hash map of any kind; get_next_key (no helper; loaders iterate from the CPU); delete without a host call.

== 1. Per-tool table: device-side hash uses ==
All keys are u32 (4 B). Values are u64 except thread_mapping (struct thread_map, 10 x u64 = 80 B).

(a) benchmark/gpu/micro/cuda_probe.bpf.c
- Map: hash_map, HASH, 1024 entries, u32->u64 (33-38).
- Probes, all on kprobe/_Z9vectorAddPKfS0_Pf, every thread:
  - hash_update: key 0, BPF_ANY (158-165).
  - hash_lookup: key 0, `(*val)++` non-atomic on a hit (167-176).
  - hash_delete: key 0 (178-184).
- Keys: 1 (constant 0).
- The loader cuda_probe.c (modes at 100-104) never inserts. So "hash-lookup" measures a miss (NULL) and "hash-delete" an ENOENT.
- Config: micro_vec_add_config.json:360-376, `minimal` workload only (vec_add 32 3 32 1). That is 1 warm-up launch (vec_add.cu:79) plus 3 iterations (90-92), x 32 threads.

(b) example/gpu/cuda-counter/cuda_probe.bpf.c
- Maps, all HASH, 1024 entries, u32->u64: start_ts (7-12), total_time_ns (15-20), call_count (23-28).
- kprobe (every thread), lines 36-58:
  - update start_ts, key=&x (the low 4 bytes of the u64 blockIdx.x), BPF_ANY (44).
  - lookup call_count[pid] (48); on a hit `__atomic_add_fetch` (50), on a miss a BPF_NOEXIST insert (52).
  - printk (55).
- kretprobe, lines 60-91:
  - lookup start_ts[&x] (66).
  - delete start_ts with &pid (70). This is a key mismatch: the delete always misses unless pid==blockIdx.x, and the entry is never removed.
  - lookup total[pid] (73), then non-atomic `*total += delta` and update BPF_EXIST passing the device pointer as the value (75-77), or a NOEXIST insert (79-80).
  - printk (82-87).
- Keys: call_count 1 (pid); total_time_ns 1 (pid); start_ts = number of blocks (1 with the victim's <<<1,10>>>, vec_add.cu:81).
- My count of host calls per thread per launch (pid is not a host call: `__bpftime_pid_tgid`, default_trampoline.cu:496-507): entry 3 (4 at first touch), exit 5. Steady state 8, first touch 9. The results doc says "about 10".

(c) example/gpu/cudagraph/cuda_probe.bpf.c
- Maps: the same three HASH maps (7-28).
- Only the leader thread acts: block (0,0,0), thread (0,0,0) (filters at 42-47 and 67-72).
- kprobe: update start_ts[pid] BPF_ANY (53), lookup call_count (56), update BPF_ANY (58).
- kretprobe: lookup (75), delete (78, key pid, hits), lookup total (79), update BPF_ANY (81).
- Keys: 1 (pid) per map.
- Host calls: 7 per launch, from 1 thread. The launches are CUDA-graph launches, which the watcher is not told about, so about 70 us per call.

(d) example/gpu/threadscheduling/threadscheduling.bpf.c (HASH) and threadscheduling_dynamic_hook (1501, which becomes HASH on the Orin)
- Maps:
  - thread_mapping: 4096 entries, value struct 80 B (24-29).
  - sm_histogram: 128 entries, u64 (32-37).
  - warp_histogram: 1024 entries, u64 (40-45).
  - dynamic_hook declares the same three at 27-48.
- Probe: kprobe on vectorAdd, every thread (55-109; dynamic_hook 58-112 on _Z9vectorAddPKfS0_Pfi):
  - update thread_mapping, key (block_x<<16)|(thread_x&0xFFFF), BPF_ANY (71, 86). blockIdx.y/z and threadIdx.y/z are ignored.
  - lookup sm_histogram[%smid] (90), then `__sync_fetch_and_add` (92) or a NOEXIST insert (95).
  - lookup warp_histogram[(smid<<16)|%warpid] (99-100), then add (102) or a NOEXIST insert (105). The helpers read %smid and %warpid (default_trampoline.cu:654, 663).
- Distinct keys:
  - thread_mapping: one per thread (default 4x64 = 256, vec_add.cu:28-29). Inserts past 4096 fail.
  - sm_histogram: at most the SM count (8 on this Orin per the results).
  - warp_histogram: at most SMs x resident warp slots.
- Host calls: 3 per thread in steady state, 5 at first touch.

(e) example/gpu/gpu_shared_map/gpu_shared_map.bpf.c
- Map: counter_per_thread, 1501, 2 entries, u32->u64 (23-28).
- kretprobe: one thread (sum of indices == 0, line 50) does update[pid] BPF_ANY (55). That is 1 host call per launch, plus 1 printk.
- Keys: 1.
- The other map, counter (1503, created as 1513), is device-direct and not a hash.

Not device-side:
- example/gpu/cuda_api_trace/cuda_api_trace.bpf.c is CPU side.
  - Map: HASH, 1024 entries, key pid (6-11).
  - Probe: uprobe on cudaLaunchKernel (13-30): lookup (18), insert 0 NOEXIST on a miss (20), lookup again (21), update with BPF_EXIST (27).
  - Keys: 1.
- example/gpu/rocm-counter/rocm_probe.bpf.c is not a device-side use in this tree.
  - It has the same 3 maps and the same pid/&x key mismatch (44, 67, 71), plus `*cnt += 1` with a BPF_EXIST update (50-51).
  - Its programs are named probe/retprobe, not cuda__, and attach/ has no ROCm attach impl (not found).
- Cross-check: 18 hash-typed declarations in 8 files. 14 are device-side, 1 CPU-side (cuda_api_trace), 3 not runnable (rocm).

Other examples (atomizer, directly_run, threadhist*, launchlate*, mem_trace, kernel_trace, kernelretsnoop, host_map_test, cutlass, faiss/llama/pytorch-test, micro cuda_probe_kernel_gpu.bpf.c) declare no hash map. They use ARRAY, 1502, 1503, 1504, 1512, 1513 or 1527.

== 2. Our tools in /home/jetson/src/bpftime-gpu-work ==
- example/gpu/th-hostcall/threadhist.bpf.c:
  - Map: HASH, 1 entry, u32->u64 (10-15).
  - Probe: kretprobe on rms_norm_f32<512,...>. Every thread does lookup key 0 (21), then `__sync_fetch_and_add` (23).
  - The loader inserts key 0 (threadhist.c:76), so every lookup hits.
  - Measured (llama-census-20261113/README.md:131-147): 10.8 us per call, 705,536 calls per prefill, 307x slower, no increment lost.
- hostwake/probe/hostwake.bpf.c and load/probe/hostspin.bpf.c:
  - Map: HASH, 16 entries (11-16).
  - Probe: every thread does update key 0 BPF_ANY (34), timed with globaltimer (33, 35). hostwake is on kprobe vectorAdd, hostspin on kretprobe _Z10spinKernelyPy.
  - These are host-call latency probes; the hash map is only the carrier.
- example/gpu/{cuda-counter,cudagraph,threadscheduling} are byte-identical copies of the repo files (diff).
- proto/tools (pt_common.h:9-14: 1502, 1503, 1530, 1531, 1532), step0 (e5: 1503; e7: 1513) and step0b (1540 as a placeholder the host sets, 1502, 1503) use no hash map. Type 1540: not found in this tree's enum.
- The other .bpf.c files under gpu-work (ablate, xlarge, policy, sleep, compare) are copies of the bpftime source tree.

== 3. Patterns ==
- P1, count per key from every thread: lookup, then an atomic add through the returned pointer, or on a miss a NOEXIST insert.
  - Used by: cuda-counter call_count (pid), threadscheduling sm/warp histograms (SM, SM+warp), th-hostcall (constant key).
  - Few keys (1 to a few hundred), all threads hitting the same key.
  - Flaw: lanes take turns inside each host_call, so all 32 lanes finish the lookup before any lane inserts. On a new key the whole warp misses; one NOEXIST insert succeeds and 31 increments are lost.
  - Evidence: CUDA Counter's checked count is calls=97 for 4 launches x 32 threads = 128 firings (results gpu/compare-20261110/summary.md:75; events=128 in attribution.md:26). 97 = 1 + 3x32.
- P2, per-thread or per-block record keyed by thread or block index, overwritten with BPF_ANY.
  - Used by: threadscheduling thread_mapping (80 B value), cuda-counter start_ts (per block).
  - Distinct keys = threads or blocks, capped by max_entries (4096).
- P3, entry/exit pairing: a timestamp at kprobe, then lookup, delete, accumulate the delta into total[pid].
  - Used by: cuda-counter, cudagraph, rocm.
  - The accumulation is a non-atomic RMW followed by an update. With all threads on one pid key, concurrent lanes lose deltas (inference, not measured).
- P4, leader-thread-only state per launch, keyed by pid (cudagraph, gpu_shared_map): one thread, a few host calls per launch.
- P5, constant-key single-op benchmarks: micro update/lookup/delete, hostwake/hostspin.
- P6, CPU-side per-pid counter on a host CUDA API uprobe: cuda_api_trace.
- Keys come only from: pid, blockIdx.x, (block_x, thread_x), %smid, (%smid, %warpid), and constant 0.
- In every program the key is u32 and max_entries is at most 4096.
- No device program iterates (get_next_key); deletes appear only in the P3 pairing and the micro delete.

== 4. Stored Orin numbers (all predate 7d02d677 and the AC3 watcher) ==
- Columns are "official us / dev us", per launch.
- compare-20261110/summary.md:63-65, official 4708f409 (A) vs dev c115b33d (B):
  - CPU Hash update: A 33,671.5 / B 322.8 us.
  - CPU Hash lookup: A 33,708.2 / B 336.8 us.
  - CPU Hash delete: A 33,691.9 / B 319.4 us.
  - Baseline minimal: about 39.7 us.
  - So about 9 us per thread-op.
- CUDA Counter: A 337,373 / B 3,486 us (summary.md:75); dev aa6eb973: 2,928.9 us (baseline-sub-20261111/summary.md:22).
- The hash micro cases were dropped from our 24-case subset. CUDA Counter (minimal) is the only device-side hash case still in it (sub/gen_sub.py:39).
- No measurement of hash ops on the current tree was found.

Open questions:
- Nothing measures device-side hash ops on the current tree (7d02d677, AC3 watcher). The stored numbers come from dev c115b33d and aa6eb973, and the hash micro cases were dropped from our 24-case subset (only CUDA Counter (minimal) remains).
- The PTX scope of the atomic that llvmbpf's GPU lowering emits for __sync_fetch_and_add / __atomic_add_fetch on a host-call-returned pointer (gpu vs sys) was not checked. th-hostcall shows no lost increments among GPU threads; whether the CPU sees the values while the kernel runs is not established.
- The first-touch loss (whole warp misses, then 31 of 32 NOEXIST inserts fail) is inferred from host_call's lane turns and matches CUDA Counter's calls=97. It has not been measured for threadscheduling's sm and warp histograms.
- The non-atomic RMW in cuda-counter's total_time_ns (lookup, *total += delta, update BPF_EXIST) should lose deltas across lanes. Not measured.
- Updating an existing key moves its value to a new element (spare-element replacement). After that, a pointer a GPU thread got from an earlier lookup points to the retired element and its writes are lost. Not exercised by any test here.
- The x86-only 1501 implementation (nv_gpu_shared_hash_map_impl) was not examined on hardware. Its lookup returns a single shared staging copy (nv_gpu_shared_array_map.cpp:71-133, value_buffer), and its delete empties a linear-probing slot without a tombstone (nv_gpu_shared_hash_map.cpp:122-125). Not relevant on the Orin.
- Map type 1540 in step0b: not found in this tree's bpf_map_type enum (the file calls it a placeholder the host sets).
- Exact resident-warp count per SM for the warp_histogram key bound: not found (the results only say 8 SMs on this Orin).

## Reader
I read the code only. Nothing was built or run, and no git refs were touched. Paths are relative to /home/jetson/src/bpftime-proto-70f7.

## 0. Main finding for the Orin
The GPU never touches a hash map's memory itself. Every bpf_map_lookup/update/delete on a hash-like map is a host call: one round trip through one request slot, answered by the agent's single watcher thread.

On the Orin, BPF_MAP_TYPE_GPU_HASH_MAP (1501) is not created as the device hash map at all.
- At start the syscall server reads CU_DEVICE_ATTRIBUTE_INTEGRATED (`runtime/syscall-server/syscall_context.cpp:534-540`).
- When it creates the map it swaps the type: `gpu_map_type_without_cuda_ipc` turns 1501 into BPF_MAP_TYPE_HASH = 1 (`syscall_context.cpp:871-883`, mapping at `runtime/include/bpftime_shm.hpp:155-174`).
- Comment at `bpftime_shm.hpp:159`: "for the GPU hash map, whose every device operation is a host call, an ordinary hash map".

So on the Orin, every GPU hash use (1501, 1, 5, 9) ends in the host-side implementations below. `nv_gpu_shared_hash_map_impl` runs only on GPUs with CUDA IPC (x86, the NUC).

The trampoline has device-side paths only for array types:
- 1530, 1531, 1532, 1502, 1503/1504, 1512, 1513, and type 2 when it has a device address (`extra_buffer`) (`attach/nv_attach_impl/trampoline/default_trampoline.cu:281-341`, `353-435`, `450-461`).
- No hash type and no 1501 branch: 1501 appears only as a constant at `default_trampoline.cu:72`.
- Lookups, updates and deletes of hash maps fall through to `host_call` at `:342-350`, `:436-447` and `:462-470`.
- No PTX pass lowers hash maps either. The only type-specific code in `nv_attach_impl.cpp` is for 1531 and 1532 (`:1020-1036`).

## 1. One GPU-thread hash operation, step by step (any hash type; 1501 on the Orin)
**GPU side**, `host_call`, `default_trampoline.cu:141-181`:
- Load `map_info[map]` from `__constant__ MapBasicInfo map_info[1024]` (`:107`), indexed by map fd. Compare the type against the array types, then fall through.
- The lanes of a warp take turns: a loop of 32 turns with `__syncwarp`. A warp whose 32 lanes all call makes 32 round trips one after another.
- The lane whose turn it is:
  - spins on `atomicCAS(&__bpftime_module_comm_lock)`, a per-module lock in GPU global memory;
  - then spins on `atomicCAS(&comm->occupy_flag)`, in `CommSharedMem` in host memory (the shared segment), one per process;
  - `membar.sys`, then writes `request_id`, `map_id`, and the key with `simple_memcpy` (a byte loop, `:183-187`): `key_size` one-byte stores into host memory. An update also copies `value_size` bytes and the flags;
  - `membar.sys`, sets `flag1 = 1`, spins with volatile loads on `flag2` (host memory), clears it;
  - `membar.sys`, copies the 8-byte response, `membar.sys`, then `atomicExch` releases both locks.
- That is 4 system fences per call.

**Host side**, the single watcher thread of the agent process:
- Takes the request with a CAS of `flag1` from 1 to 0 (`runtime/src/attach/bpf_attach_ctx_cuda.cpp:309-321`).
- `serve_cuda_request` (`:126-165`) calls `bpftime_map_{lookup,update,delete}_elem_with_origin(fd, key, false)`, which goes through the `bpftime_shm` dispatch (`runtime/src/bpftime_shm_internal.cpp:301-393`) and `bpf_map_handler` to the implementation.
- A lookup also turns the host pointer into a device address with `cuda::gpu_address_of_map_value`: `cuPointerGetAttribute`, then `cuMemHostGetDevicePointer` (`bpf_attach_ctx_cuda.cpp:576-591`). That is two driver calls per found key. A value outside registered memory gives the GPU NULL.
- Then stores `flag2` (release).
- The agent registers the whole shared segment with `cuMemHostRegister(DEVICEMAP|PORTABLE)` (`runtime/src/bpftime_shm_internal.cpp:1557-1624`, call at `:1613`). So any value in the segment can be addressed from the GPU.

**Consequences:**
- All GPU hash operations of a process run one at a time on that one slot and its watcher. GPU threads queue on the module lock and `occupy_flag`, not on the map's own locks. The map's locks matter only against the CPU tool's `bpf()` calls.
- Errors reach the GPU as -1 (`resp.map_update.result` is an int); errno is not passed on. Kernel bpf would return -EEXIST, -E2BIG and so on.
- Recorded costs, for host calls in general (not hash-specific; none of them was measured here):
  - about 10 us per call at any contention: 2048 calls in about 20 ms (CLAUDE.md:2306; test bound at `runtime/unit-test/cuda/test_cuda_host_call.cpp:796-799`);
  - with the default watcher rules: a call 2 ms after the previous one about 69 us median, p90 108; 32-call bursts under full CPU load 15.7 us per call (CLAUDE.md:2333-2336);
  - with the opt-in WFE watcher, 14-16 us (CLAUDE.md:2379).

## 2. BPF_MAP_TYPE_HASH (1), and 1501 on the Orin
**Where it lives:** `fix_size_hash_map_impl` over `bpftime_hash_map`, in the boost shared segment (host memory), created in `map_init` (`runtime/src/handler/map_handler.cpp:1252-1258`).

**Layout:**
- `max_entries` preallocated elements `{word, next, key, value}`, plus one spare element per stripe.
- A power-of-two number of u32 chain heads (`runtime/src/bpf_map/bpftime_hash_map.hpp:95-146`; geometry at `:629-649`).
- Seeded multiplicative hash, separate chaining, per-CPU free lists.
- No per-thread copies.

**Synchronization:**
- `should_lock = false` (`runtime/src/bpf_map/userspace/fix_hash_map.hpp:33`): the handler takes no lock.
- Lookups take no lock: a generation word and the link are re-checked.
- Writers spin on the key's stripe lock (`bpftime_hash_map.hpp:715-745`).
- An insert pops from the free list chosen by the calling thread, which for GPU calls is always the watcher.

**Update of an existing key:** the new value goes into the stripe's spare element, which replaces the old one; the old element is retired and becomes the spare (`:510-538`). A value pointer from an earlier lookup therefore stays valid only until that key is updated or deleted (CLAUDE.md:966-972). A GPU thread writing through an older pointer writes into a retired or reused element.

**Flags (kernel semantics):** NOEXIST on an existing key gives EEXIST, EXIST on a missing key gives ENOENT, a full map gives E2BIG (`fix_hash_map.cpp:38-67`).

**Lookup from the GPU:** returns the device mapping of the element's value in host memory. The program then reads and writes host memory directly, with plain or atomic stores. The trampoline adds no fence after the program's own stores.

**What the CPU sees:**
- The tool's `bpf()` calls run in the syscall server against the same table in shared memory (`syscall_context.cpp:942-973`: lookup, then memcpy of the value). There is no delivery step and no copy.
- Inserts, updates and deletes made by the watcher are visible as soon as the host call has run.
- In-place GPU writes are visible once they reach memory: no explicit fence, but kernel completion or the next host call's `membar.sys` order them.

**Tests and benchmark:**
- Test "Every GPU thread's host call carries its own arguments" (`test_cuda_host_call.cpp:727`): 1024 threads, update plus lookup each.
- The watcher tests also use HASH (`:1248`, `:1387`).
- Benchmark cases hash-update, hash-lookup and hash-delete: `benchmark/gpu/micro/cuda_probe.bpf.c:33-38` and `:159-185`, config `micro_vec_add_config.json:360-373`.
- The only recorded result (`micro_vec_add_result.md:80-82`, about 33.4 ms against 5.23 ms) is from an RTX 5090 on 2025-10-16, not the Orin.

## 3. PERCPU_HASH (5) from a GPU probe
- `per_cpu_hash_map_impl`: keys in a `bpftime_hash_map`; values in slots of the shared segment, one per CPU (`map_handler.cpp:1301-1307`).
- Lookups and updates choose the CPU with `my_sched_getcpu()` (`runtime/src/bpf_map/userspace/per_cpu_hash_map.cpp:176-210`). For a GPU program that is the watcher thread's current CPU: every GPU thread shares whichever slot the watcher is on, and it changes when the watcher migrates. These are per-CPU copies, not per-GPU-thread copies.
- A lookup returns the device mapping of that CPU's value.
- The tool reads all CPUs in libbpf's layout. GPU contributions are scattered over the watcher's CPUs, so only a sum over CPUs is meaningful.

## 4. LRU_HASH (9) from a GPU probe
- `lru_var_hash_map_impl`: a boost unordered_map plus a linked list in the shared segment. Each element's value is its own `bytes_vec` (`runtime/src/bpf_map/userspace/lru_var_hash_map.hpp:46-78`).
- `should_lock = true`: every operation, tool lookups included, takes the handler's `map_lock` spinlock (`map_handler.cpp:224-227`).
- A lookup moves the key to the head and returns `&value[0]` (`lru_var_hash_map.cpp:28-48`).
- Inserting into a full map evicts the tail (`:81-86`); delete is at `:106-120`. A value pointer a GPU thread got earlier then points into freed segment memory.

LRU_PERCPU_HASH has no `map_init` case (only a usage-stats switch at `map_handler.cpp:2137`). Creation fails with EINVAL unless something registers it in `global_map_ops_table` (not found).

## 5. BPF_MAP_TYPE_GPU_HASH_MAP (1501) on GPUs with CUDA IPC (not the Orin)
**Creation** (`map_handler.cpp:1441-1465`): the server creates a CUDA context if it has none, then constructs `nv_gpu_shared_hash_map_impl(memory, max_entries, key_size, value_size)`. `check_map_attr` has no case for 1501 (`default: return 0`, `map_handler.cpp:1156`), so there are no size checks.

**Layout** (`runtime/src/bpf_map/gpu/nv_gpu_shared_hash_map.hpp:16-33`; .cpp line numbers below are of that file alone):
- Keys: a host `key_buffer` in the segment, `(4 + key_size) * next_prime(max_entries)` bytes. Each bucket is a u32 filled flag followed by the key (`.cpp:17-22`).
- Values: an `nv_gpu_shared_array_map_impl` with `max_entries` entries. Device memory from `cuMemAlloc`, zeroed, exported with `cuIpcGetMemHandle` (`nv_gpu_shared_array_map.cpp:38-68`); each agent maps it with `cuIpcOpenMemHandle` (`:263-389`). Bucket i's value is at `i * value_size`.
- Hash `bpftime_hasher::hash_func` (`bpftime_hash_map.hpp:79-92`), mod the bucket count, open addressing with linear probing (`.cpp:29-63`).
- No per-thread copies; `get_max_thread_count()` returns 1.

**Synchronization:**
- `should_lock = false` (`.hpp:70`). One `pthread_spinlock map_lock` per map covers lookup, update, delete and next_key (`.cpp:51, 72, 115, 142`).
- That lock is held across the device copy: `cuMemcpyDtoHAsync`/`HtoDAsync` plus `cuStreamSynchronize` on the agent's own stream, or `cuMemcpyDtoH`/`HtoD` in the server (`nv_gpu_shared_array_map.cpp:87-133`, `151-195`); gdrcopy when available.
- GPU operations are host calls (there is no trampoline branch for 1501), so the device value buffer changes only through host-side copies. The device address in `map_info` (`map_handler.cpp:1885-1888`) is never read by the trampoline.

**What the CPU sees:** every `bpf()` lookup is a device-to-host copy into one shared staging buffer, `value_buffer` (`nv_gpu_shared_array_map.cpp:35, 38`). Its address is returned after the lock is released, and the syscall server then copies it out (`syscall_context.cpp:962-970`), so a concurrent lookup can overwrite it. `get_next_key` walks `key_buffer` in bucket order.

**Defects found by reading** (none reproduced or run):
- (a) **GPU lookups never reach the map's value.** A GPU lookup gets the device address of the shared host staging copy (`elem_lookup` returns `value_buffer.data()`; translated at `bpf_attach_ctx_cuda.cpp:138-141`). Writes through it never reach the device buffer, and the next lookup overwrites the buffer. `*v += 1` or `__sync_fetch_and_add(v, 1)` after a lookup does nothing to the map.
- (b) **Inserting a new key with BPF_NOEXIST always fails.** The hash passes `flags` unchanged to `array_map.elem_update` (`.cpp:86-87`), which returns EEXIST for any in-range index when flags == BPF_NOEXIST (`nv_gpu_shared_array_map.cpp:143-146`). The threadscheduling example's first inserts depend on this.
- (c) **BPF_EXIST on a missing key inserts** instead of returning ENOENT.
- (d) **Buckets at or above `max_entries` have no value slot.** There are `next_prime(max_entries)` buckets but `max_entries` values, so a key whose first empty bucket is in that range fails with E2BIG (`nv_gpu_shared_array_map.cpp:147-150`): 4096 entries give 4099 buckets.
- (e) **A delete breaks probe runs.** Delete only clears the flag (`set_empty`, `.cpp:123`; no tombstone or shift), and lookups stop at the first empty bucket (`.cpp:54-56`). Keys later in the run become unreachable; updating such a key inserts a duplicate, and `get_next_key` lists both.
- (f) **`get_next_key` deadlocks for a missing key.** It calls itself (`.cpp:160`) while holding `map_lock` (`.cpp:142`). The lock is a non-reentrant spinlock: exchange 1 while held (`map_common_def.hpp:39-50`, guard at `map_handler.hpp:49-66`). A tool iterating with a deleted or missing key spins forever.
- (g) **Full map:** ENOSPC once `_count >= num_buckets` (`.cpp:79-81`, `107`); the kernel returns E2BIG.

## 6. Limits common to every hash map used from the GPU
- **Not supported from the GPU:**
  - get_next_key: protocol op 4 exists, but `serve_cuda_request` has no branch and logs "Unknown request id";
  - lookup_and_delete, push/pop, BPF_F_LOCK;
  - device-resident hashing for any hash type.
- **Map fd below 1024:** `map_info` has 1024 entries; `create_map_basic_info` returns an empty vector for a larger fd (`bpf_attach_ctx_cuda.cpp:525-545`).
- **Key and value size:** I found nothing that checks `key_size` or `value_size` against `GPU_HELPER_MAX_BUF` (1 MiB per buffer, `gpu_helper_protocol.h:12-43`) before the byte copies. Host hash creation allows key + value up to about 4 MiB.
- **Racing inserts:** `lookup || update(NOEXIST)` from many GPU threads loses the losers' first counts; they get -1 and nothing retries.

## 7. GPU map type numbers (GPU_MAP_OFFSET = 1500, `bpftime_shm.hpp:69-151`)
| Type | Name | What it is |
|---|---|---|
| 1501 | GPU_HASH_MAP | Keys in shared memory, values in device memory exported over IPC; every GPU op a host call; created as HASH (1) on integrated GPUs. |
| 1502 | PERGPUTD_ARRAY_MAP | Per-thread `[key][tid]` array in device memory over IPC; GPU accesses it directly; becomes 1512 without IPC. |
| 1503 | GPU_ARRAY_MAP | Single-copy array in device memory over IPC; GPU memcpy plus `membar.sys`; becomes 1513 without IPC. |
| 1504 | GPU_KERNEL_SHARED_ARRAY_MAP | Array shared with a kernel bpf array map (`array_map_kernel_gpu_impl`); GPU accesses it directly. |
| 1512 | PERGPUTD_ARRAY_HOST_MAP | Per-thread array in the registered shared segment; GPU accesses it directly. |
| 1513 | GPU_ARRAY_HOST_MAP | Single-copy array in the registered shared segment; GPU accesses it directly. |
| 1527 | GPU_RINGBUF_MAP | Per-thread record rings, drained by the host. |
| 1530 | GPU_THREAD_LOCAL_ARRAY_MAP (prototype) | Each thread's copy in local memory; the CPU sees nothing; at most 1 KiB. |
| 1531 | GPU_SUM_ARRAY_MAP (prototype) | u64 words in device memory, atomic adds only, (SM, lane) shards; the agent delivers their sum. |
| 1532 | GPU_DEVICE_PER_THREAD_ARRAY_MAP (prototype) | Per key and thread index in device memory, atomic adds only; the CPU sees the delivered copy. |

## 8. Tests for BPF_MAP_TYPE_GPU_HASH_MAP
- There is no unit test of `nv_gpu_shared_hash_map_impl`; the class is only compiled (`runtime/CMakeLists.txt:106`).
- The only test that names 1501 checks the type mapping 1501 → 1 (`runtime/unit-test/maps/test_gpu_map_without_ipc.cpp:16-33`, line 25).
- Two examples use 1501, and neither is a test: `example/gpu/gpu_shared_map/gpu_shared_map.bpf.c:23-28` and `example/gpu/threadscheduling_dynamic_hook/threadscheduling.bpf.c:27-48`, whose lookup + `__sync_fetch_and_add` + NOEXIST pattern runs into (a) and (b) on IPC GPUs.
- GPU host calls to hash maps are covered only through plain BPF_MAP_TYPE_HASH (section 2).

Open questions:
- Visibility of GPU writes made directly through a looked-up pointer into registered host memory on the Orin: plain st.global with no trailing membar.sys. When does the CPU tool see them before the kernel ends? Not checked.
- Do GPU atomics (__sync_fetch_and_add, i.e. atom.add) on registered host memory behave correctly on the Orin (host-native atomics, coherence)? Not checked in code; needs a measurement.
- Per-op cost of hash host calls on the Orin for lookup, update and delete, and how it scales with threads issuing them: only general host-call numbers exist (CLAUDE.md). The micro benchmark hash cases were not found in our 24-case subset or in Orin results.
- Defects (a)-(g) of the 1501 device implementation come from reading the code only; none was reproduced, and they apply only to GPUs with CUDA IPC (x86 NUC), not the Orin.
- Whether map_production_ab_mode (cached_handler/direct_map) is ever set for the watcher's from_syscall=false calls by default: not checked.

## Reader
Short answer: the device side has no hash code at all. In attach/nv_attach_impl/trampoline/default_trampoline.cu, helpers _bpf_helper_ext_0001/0002/0003 (lines 281-351, 353-448, 450-470) have branches only for array-shaped types: 2 with a device address, 1502, 1503/1504, 1512, 1513, 1530, 1531 and 1532. Every other type falls through to one host_call (lines 342-350, 436-447, 462-469). That includes BPF_MAP_TYPE_HASH (1), GPU_HASH_MAP (1501), PERCPU_HASH (5) and LRU_HASH (9). The shipped PTX does the same: trampoline_ptx.h:180-224 compares map_info[map].map_type and branches to $L__BB2_20 (line 458), the host-call block. So the GPU does no hashing, no probing loop and no bucket locks; the host map implementation does all of that inside the CUDA watcher thread.

On the Orin, 1501 never reaches the GPU as 1501. The syscall server sets cuda_gpu_integrated from CU_DEVICE_ATTRIBUTE_INTEGRATED (runtime/syscall-server/syscall_context.cpp:536-540) and then creates 1501 as an ordinary HASH (syscall_context.cpp:873-881, bpftime_shm.hpp:160-172). Its map_info.map_type is therefore 1. The x86-only 1501 implementation (nv_gpu_shared_hash_map.cpp) is described at the end.

What one GPU thread does per lookup/update/delete on a hash map (Orin, type 1):
(a) Dispatch: one ld.const of map_info[fd].map_type (map_info is __constant__ MapBasicInfo[1024], default_trampoline.cu:107), then about 4-6 compare/branch instructions (ptx 180-224).
(b) host_call (default_trampoline.cu:140-180, ptx 458-556). Lanes of a warp take turns. Every active lane runs a 32-iteration loop of activemask, a compare with its lane id and bar.warp.sync (ptx 466-479). Only lane == turn does the call, so A active lanes make A host calls one after another.
(c) The lane holding the turn does the following:
- It spins unbounded, with no backoff, on atom.global.cas.b32 of __bpftime_module_comm_lock. That lock is per module and lives in GPU memory (default_trampoline.cu:128, ptx 483-487).
- It spins on a generic atom.cas.b32 of comm->occupy_flag, which is per process and lives in the host shared segment (ptx 488-492).
- membar.sys.
- It stores request_id (u32) and map_id (u64), then copies the key byte by byte: ld.u8 from the eBPF stack (local memory) and st.u8 into host memory, key_size times (simple_memcpy, default_trampoline.cu:181-185, ptx 508-536). An update also copies value_size bytes to req.map_update.value (+1 MiB) and stores the 8-byte flags (default_trampoline.cu:438-445).
- membar.sys, then st.volatile flag1=1.
- It polls ld.volatile flag2 in host memory with no bound until the watcher answers (ptx 545-549). Every poll is a read of host memory over the bus (CLAUDE.md, host-call section).
- st.volatile flag2=0, membar.sys, ld of the 8-byte response, membar.sys.
- atom.exch occupy_flag=0, then atom.global.exch of the module lock (ptx 550-557).

Per call that is:
- 4 membar.sys
- 2 GPU-memory atomics, plus failed CAS spins
- 2 host-memory atomics, plus spins
- about key_size+4 host-memory stores (update: key_size+value_size+5)
- 1 + (number of polls) host-memory loads
- exactly 1 host round trip

(d) Host side (runtime/src/attach/bpf_attach_ctx_cuda.cpp):
- try_serve takes the request with a CAS of flag1 1->0 (acquire) (307-317).
- serve_cuda_request (126-191) calls bpftime_map_lookup_elem_with_origin(fd, key, false). For HASH that is fix_size_hash_map_impl -> bpftime_hash_map::lookup_read, lock-free. Update and delete lock the key's stripe (CLAUDE.md "Hash Map Concurrency").
- For a lookup hit, the watcher then calls cuda::gpu_address_of_map_value (581-596), which is cuPointerGetAttribute plus cuMemHostGetDevicePointer, two driver calls per hit.
- It stores flag2 with release (321).

Latencies documented in CLAUDE.md (not measured by me):
- about 10 us per call at any contention ("Every GPU thread's host call carries its own arguments": 1024 threads, 2048 calls in about 20 ms; test_cuda_host_call.cpp:727-812, comment at 798)
- 15.7 us per call in 32-call bursts under full CPU load
- median 69 us (p90 108) for a call 2 ms after the previous one
- up to the 1 ms park backstop for launches no hook noted
- 14-16 us with the opt-in WFE watcher

Shape: one slot per process and one lock per module, so every hash operation of every GPU thread in the process is serialized. Throughput is at most about one operation per host round trip (roughly 1e5/s for the whole GPU), and a kernel's added time grows with (threads x hash operations) regardless of SM count.

What a lookup returns (Orin, HASH): a device address of the element's value inside the bpftime shared segment. This is host memory that the agent registers whole with cuMemHostRegister(DEVICEMAP|PORTABLE) (bpftime_shm_internal.cpp:1611-1614). It is a pointer into the live element, not a copy (bpftime_hash_map.hpp:692-695; the header at 95-103 says values never move and pointers stay valid until the key is updated or deleted). The GPU then reads and writes the value directly in host memory with no further host call and no fence; the test kernel dereferences it with ld.u64 (test_cuda_host_call.cpp:106). A miss returns 0.

An atomic add on that pointer:
- llvmbpf lowers every BPF atomic as an LLVM atomicrmw with Monotonic ordering (vm/llvm-jit/src/compiler_utils.cpp:376-388). __ATOMIC_SEQ_CST in the source (example/gpu/cuda-counter/cuda_probe.bpf.c:50) becomes relaxed.
- The PTX is a generic atom.add.u64. The build uses LLVM 15 (build-cuda/CMakeCache.txt), and the routing tests model llvmbpf's output as "atom.add.u64 %rd56, [%rd55], 1;" (test_sum_map_routing.cpp:20).
- If a 1530/1531/1532 map exists, the routing pass rewrites each generic atomic. It tests isspacep.local/isspacep.global; a global address takes atom.global after cvta.to.global, a local one a plain ld/op/st, and anything else stays generic (nv_thread_local_routing.cpp:670-723).
- Without such a map the pass returns early (nv_attach_impl.cpp:1043-1044) and the raw generic atom stays.
- Either way it is one GPU atomic on host memory, with no .sys scope written in the code.

Do the routing pass or llvmbpf treat hash maps specially? No.
- Routing: only constant-fd lookups of 1531 (with a device copy) and 1530 are inlined (nv_thread_local_routing.cpp:1170-1239). Every other call of helpers 1-3, hash maps included, stays a call and only gets its 5th argument (eBPF r5) overwritten with the thread-local area address or 0 (1240-1256). It runs only when 1530/1531/1532 maps exist (nv_attach_impl.cpp:1017-1044).
- llvmbpf: generate_ptx (llvm_jit_context.cpp:903-958) does not call inline_array_map_lookup_helpers, which only the CPU JIT does (llvm_jit_context.cpp:537). The PTX compile in ptxpass_core registers helpers 1,2,3,6,14,25,501-511 with no array maps (pass/ptxpass_core/src/core.cpp:234-255). Every hash operation is a call to _bpf_helper_ext_000N.

Not supported or incorrect under concurrent use, as far as the code shows:
1. Same-key initialisation races, with a two-host-call window. cuda-counter's lookup -> NULL -> update(BPF_NOEXIST) (cuda_probe.bpf.c:48-52) drops the count of every thread that loses the insert (-1, no retry). BPF_ANY would overwrite instead.
2. Non-atomic read-modify-write. `*total += delta; update(..., total, BPF_EXIST)` (cuda_probe.bpf.c:75-77) loses updates between GPU threads.
3. Stale pointers. An update of an existing key replaces the element: the stripe's spare takes the new value and the old element is retired and becomes the spare (bpftime_hash_map.hpp:528-538). The next update of any key in that stripe reuses it, so a GPU thread still holding the old pointer adds into another key's value. A delete frees the element to a free list for any later insert (755ff). This includes updates that GPU threads make through host calls. The kernel has the same window, as CLAUDE.md notes.
4. Atomic adds are atomic only among GPU threads, under PTX's default .relaxed/.gpu scope. Nothing in the code makes them atomic against CPU writers of the same value: CPU probes using the map, or tools. See open questions.
5. PERCPU_HASH. The watcher answers with sched_getcpu() of the watcher thread (per_cpu_hash_map.cpp:176-190, 193-201). All GPU threads share one CPU's slot, chosen by wherever the watcher runs, so per-CPU semantics are lost.
6. LRU_HASH (should_lock=true, lru_var_hash_map.hpp:78):
   - A lookup moves the key to the head under the handler lock and returns &value[0] of a node vector (lru_var_hash_map.cpp:28-48).
   - An update of an existing key assigns in place (86-90), so GPU readers can see torn values.
   - Eviction and delete erase the node (81-84, 119-120), leaving the GPU pointer dangling into freed segment memory.
7. LRU_PERCPU_HASH has no case in map_init: creation is EINVAL unless allow_non_buildin_map_types (map_handler.cpp:1648-1663).
8. Missing helpers:
   - There is no bpf_map_get_next_key on the GPU. MAP_GET_NEXT_KEY exists only in the enum (gpu_helper_protocol.h:23-31), with no _bpf_helper_ext_0004 in the trampoline, no registration in core.cpp:234-255 and no case in serve_cuda_request.
   - There are no bpf_spin_lock helpers.
   - All errors come back as -1, not -errno: the response is an int (gpu_helper_protocol.h:56-58).
9. Test coverage: the only GPU hash test (test_cuda_host_call.cpp:727-812) uses distinct keys per thread, one update plus one lookup each. Nothing tests same-key contention or atomics on returned pointers.

x86-only 1501 (nv_gpu_shared_hash_map.cpp, never reached on the Orin):
- The device path is the same host call.
- The host lookup holds a pthread spinlock (line 51) and returns array_map.elem_lookup(&index) (58). That copies the value from device memory into the map's single value_buffer, resized to value_size at nv_gpu_shared_array_map.cpp:38 and returned at 91/133. A device copy runs under that spinlock: gdrcopy, or cuMemcpyDtoHAsync followed by cuStreamSynchronize.
- So the GPU gets a pointer to one per-map staging copy, which the next lookup of that map by any thread overwrites. Writes and atomics through it never reach the device array.
- Buckets are next_prime(max_entries) (line 19), but the value array has only max_entries entries. A key whose slot index is >= max_entries fails: update returns E2BIG (array 147) and lookup ENOENT (74).
- Inserting a new key with BPF_NOEXIST always fails with EEXIST, because of the array's flag check (array 143).
- The capacity check is _count >= _num_buckets (79), returning ENOSPC.
- Delete clears the slot with no tombstone (123), so keys later in the same probe run become unreachable.

Open questions:
- Is a GPU generic/global atom.add (default .relaxed .gpu scope) on host-registered segment memory atomic against CPU writers of the same value on the Orin? The code has no .sys scope and no test. The CUDA guide's note says atomics on mapped page-locked memory are not atomic from the host's point of view. Not verified here.
- Are such GPU atomics on host-mapped memory even atomic among GPU threads, and at what cost, on Orin's I/O-coherent sysmem path? No test in test_cuda_host_call.cpp exercises it.
- What do the two driver calls in gpu_address_of_map_value (cuPointerGetAttribute + cuMemHostGetDevicePointer) cost on each lookup hit? Not measured or found.
- Several traced processes appear to share one CommSharedMem per segment, and create_cuda_context memsets it (bpf_attach_ctx_cuda.cpp:637). Could one process's watcher serve another process's request and return a device address from its own context? Not tested (CLAUDE.md also lists several processes as not tested).
- map_info is indexed by fd with no bounds check (default_trampoline.cu:284). For a map not enabled in map_info, key_size is 0 and the host would read a stale key. Whether that can happen for hash maps after load-time substitution was not checked.
- Is the host hash lookup cost inside the watcher (bpftime_hash_map lookup_read, tens of ns) negligible against the watcher's wake-up latency? Not measured here.
