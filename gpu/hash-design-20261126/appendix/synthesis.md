# The panel synthesis, as it came back (unverified model output; see ../DESIGN.md for what is taken from it)

## Device hash map: one key directory with a sum layout and a store layout

**Plain name:** "device hash map". It is a new server map type, **1533**. Number 1533 is free, because the enum in `runtime/include/bpftime_shm.hpp` ends at 1532.

Nothing here was built or run. Each number is tagged:
- **[measured]:** taken from our results;
- **[derived]:** arithmetic on measured numbers;
- **[inferred]:** not measured.

Code paths are in `/home/jetson/src/bpftime-proto-70f7`. Quotes from the PTX ISA (version 9.5) and from the CUDA programming guide are verbatim, from step0b's copies in `bpftime-gpu-work/step0b/c5-guarantees/docs/text/`.

### Terms used below
- **Key directory:** open addressing over permanent key slots in device memory. This is device-table's mechanism.
- **Sum layout:** used for maps that the GPU changes only with adds. Each key's value is a 1531-style row of (SM, lane) shards.
- **Store layout:** one versioned value per key, also from device-table. It has two forms:
  - **In-place form:** a lookup returns the value cell itself.
  - **Copy form:** a lookup returns a private copy in the thread's 1530 area.
- **Use check:** a decision on the eBPF bytecode that picks the layout for each map, once, at attach time.
- **Covered synchronize:** an application synchronize whose count a delivery read before it took its snapshot. This is 1531's rule.
- **R:** the number of rows that get shards in a sum map.

### 1. What it is
**Today** (hash-current):
- Every lookup, update and delete of a hash map from a GPU probe is a host call.
- One call takes 10-13 us when calls follow each other and 17.9-20.9 us when they are sparse [measured].
- The whole process is served one call at a time.

**The design.** A hash map that only GPU programs use lives in device memory:
1. **Key directory.** Each key gets a permanent slot, claimed by a 64-bit CAS inside trampoline code. max_entries is enforced at insert, with E2BIG as in bpf.
2. **One value layout per map**, chosen by the use check:
   - **Sum layout:** the map's lookup results only feed 64-bit atomic adds. Values are 1531's sharded rows. The hit path for constant map fds is inline and uses relaxed loads only.
   - **Store layout:** every other map the check admits. One value per key. An 8-byte update is one strong store; a longer value uses a versioned write.
   - **Host call:** today's HASH path, unchanged, for every map the check refuses. The reason is logged once.
3. **CPU side.**
   - Until the agent makes the device copy, the map is today's CPU hash table, so loaders can prefill it.
   - After that, the CPU reads delivered snapshots (1531's delivery thread), and CPU writes get EPERM.
4. **Per-thread state is not this map's job.**
   - Entry-to-exit scratch stays in 1530, which carries it by value.
   - Per-thread results stay in 1532.
   - The tool picks these by the map type it declares. That is how the 11-15 goal "every thread its own copy" is met, measured on `lat`.

### 2. Which maps get it
- **1501 on the Orin**, when key_size is 4 and value_size is a multiple of 8, becomes 1533. Today it becomes HASH.
  - The swap goes at `runtime/syscall-server/syscall_context.cpp:871-883`, where the sizes are known.
  - `runtime/unit-test/maps/test_gpu_map_without_ipc.cpp:16-33` must change.
- **Plain HASH (1)** only by an opt-in (open question 1). A HASH map may be shared with CPU programs.
- **Other sizes, PERCPU_HASH and LRU_HASH:** today's paths.
- **Without the opt-in, coverage is small.** Only gpu_shared_map and threadscheduling_dynamic_hook are served: 4 maps, 6 of the 38 hash call sites. Neither program has run on the Orin yet (blocked on 11-09).

**The server-side 1533 object.** It is today's `bpftime_hash_map`, with bpf semantics, plus a "device-owned" phase.
- The agent switches the map into that phase once, when it creates the device copy, under the table's writer lock.
- If the use check refuses the map, or no GPU process ever makes a copy, the map stays exactly today's HASH, served by host calls.

### 3. The use check (decides the layout)
**Where it runs:** `bpf_attach_ctx` init, over every `cuda__` program in the segment, before the first `create_map_basic_info`.
- That call is at `runtime/src/attach/bpf_attach_ctx.cpp:642`.
- So map_info is written once with the layout, as an internal map_info type per layout and form.
- MapBasicInfo stays 40 B.
- The decision is frozen. A later attach that does not fit is refused, and the refusal is logged.

**Why bytecode:** helper calls and the control flow are explicit there (route-first's argument). The line-based PTX scans failed open in two review rounds (map-lowering).

**It fails closed.** A lookup result that escapes makes the map a host-call map. Escapes are:
- the pointer stored to memory, including a spill to the stack;
- the pointer passed to a helper, except as the value of the update allowed below;
- a pointer derived with a non-constant offset.

**Sum layout.** All of these must hold in every GPU program:
- every update is BPF_NOEXIST, with constant flags;
- there is no delete;
- lookup results feed only null tests and 64-bit atomic adds at constant 8-aligned in-value offsets. An add is XADD, or FETCH whose result register is dead;
- no CPU-side program references the map.

**Store layout, in-place form.** Lookup results feed only:
- null tests;
- 64-bit atomics (results may be used);
- the value argument of an update on the same map and key.

No plain load or store goes through them.

**Store layout, copy form.** Lookup results feed only:
- null tests;
- plain loads inside the value;
- plain stores, but only if every path afterwards passes that same pointer as the value of update(same map, same key bytes), with no other operation on that map in between. This is route-first's write-back rule.

No atomics go through the pointer. Every lookup site is in a kprobe or kretprobe program of a kernel, which receives the 1530 area.

**Everything else** is a host-call map. That covers mixed atomic and plain uses, a write with no write-back, and copy-form lookups in a direct run.

**At patch time** the routing pass enforces the same rules on PTX, with `plan_by_value`'s allowlist pattern (`attach/nv_attach_impl/nv_thread_local_routing.cpp:878-1010`): every statement that names a lookup-result register must be on the list.
- The pass must run when 1533 maps exist. Today `attach/nv_attach_impl/nv_attach_impl.cpp:1042` returns early.
- If the two checks disagree, one of them has a bug. The module is refused loudly: `fail()`, so the module loads without probes and the reason is logged.
- A falling-back pass cannot exist here: once map_info says "device", a host call would reach a stale table.

### 4. Device memory (one cuMemAlloc block per map)
**Zeroing.** cuMemAlloc says "The memory is not cleared". The block is zeroed with cuMemsetD8Async on the agent's own stream, in relaxed capture mode, and waited for before its address goes into map_info. This is 1531's chain.

**Contents:**
- **Control block, 64 B:**
  - seed (random per map; 0 with BPF_F_ZERO_SEED, as the CPU table);
  - geometry;
  - `bound`: reservations of slots;
  - `next_row`: rows handed to claim winners (sum layout).
- **Loss counters, one per SM:** E2BIG, EEXIST, ENOENT, waits and unstable snapshots, 8 u64 words each, changed with `red.global.add`. Spreading them keeps a full table from turning every thread into a read-modify-write on one control line (cost judge).
- **Directory:**
  - S_slots = next_pow2(2 x max_entries) headers of 8 B.
  - Hash: (key ^ seed) x 0x9E3779B97F4A7C15, top log2(S_slots) bits, linear probing.
  - At most max_entries slots are ever claimed, so at least half stay empty and every walk ends.
- **Sum header:** key (bits 63..32), row (31..3), state (empty / claiming / live).
- **Store header:** key, version, state (empty / writing-new / live / writing / deleted).

**Sum rows** are dense, numbered in claim order.
- A row below R holds S shards x value_size. On the Orin S = 256, and the shard is (%smid & 7) x 32 + %laneid, as 1531's `sum_array_offset`.
- A row at R or above is one copy.
- R per map defaults to min(max_entries, 512) [proposal].
- Correctness never depends on R, S, %smid or %laneid; they only spread the adds.

**Store values:** S_slots x value_size, indexed by slot, 8-aligned.

**Why dense rows:** only the CAS winner takes `next_row`, so rows never leak. Rows indexed by slot would double the rows. Rows numbered from `bound` would need a give-back that can duplicate a row.

**Memory per map** [arithmetic]:

| map | device | sum rows |
|---|---:|---:|
| cuda-counter call_count (1,024 x u64) | 16 KiB directory + rows | 512 x 2 KiB + 512 x 8 B, about 1 MiB in all |
| sm_histogram (128) | 2 KiB directory + rows | about 256 KiB |
| thread_mapping (4,096 x 80 B, store) | 64 KiB + 640 KiB = 704 KiB | none |
| a store map of 1,024 x u64 | 32 KiB | none |

- **Host side:** two mirrors and pinned staging of S_slots x (8 + value_size) each, for example 32 KiB for 1,024 x u64.
- **For comparison:**
  - 1531's `lat`: 532 KB [measured];
  - shards' per-shard tables: 295 KB at 1,024 entries;
  - approach D (the hardware-position storage from the 11-15 design space): 96 KiB per u64 entry.
- Memory is bounded by max_entries and R, never by the grid.

### 5. Device operations
**Entry points:** trampoline helpers 1, 2 and 3 get branches for the new map_info types ahead of the host_call fall-through. Constant-fd sum lookups are also inlined by the routing pass.

**Rules that hold for every access:**
- **One address form.** Every header and value access goes through the `cvta.to.global` address: helpers, the inline path, routed program atomics and the snapshot. PTX 8.6: "since using distinct virtual addresses behaves as if using different proxies, they require a proxy fence".
- **New inline PTX:** `atom.{relaxed,acquire}.gpu.global.cas.b64`, `ld.{relaxed,acquire}.gpu.global.b64`, `st.{relaxed,release}.gpu.global.b64`, `fence.acq_rel.gpu` and `nanosleep`. The existing `atomicCAS` is 32-bit and generic (`default_trampoline.cu:17-20`).
- **sm_70.** "Support for scope qualifier, .relaxed, .acquire, .weak qualifiers require sm_70 or higher." and "nanosleep requires sm_70 or higher." The patched module's .target is rewritten to the device's arch (`nv_attach_fatbin_record.cpp:174`). Compile check C3 confirms it.
- **No warp collectives and no activemask.** Agreement on an activemask mask is not documented (step0b C5 claim 9), and C2 measured violations. Every lane takes its own path.
- **No CAS in tool code.** llvmbpf's CMPXCHG returns a load taken before the cmpxchg (`vm/llvm-jit/src/compiler.cpp:1690-1718`). step0 e5 counted 102 claims of 122,880.
- **Update values** are read from the eBPF stack bytewise, because 8-byte alignment of a generic local address is not stated (probe-cost section 3). They are stored with 64-bit strong stores.
- **Errors are -errno**, as in bpf. Today's host call returns -1.

**Sum layout**
- **lookup(K):**
  - Walk with `ld.relaxed.gpu.global`.
  - A live K gives the calling thread's shard of K's row (or the single copy when the row is R or above).
  - A claiming or missing K gives NULL. A lookup that meets an insert in flight is ordered before that insert, and bpf answers NULL for a key not yet inserted.
  - It never waits.
- **Inline hit path** (constant fd; the key is forwarded from its register, 0890dbdd): hash, one header load at the home slot, compare key and state, row to address. A miss or a non-home hit calls helper 1.
  - Seed, S_slots, R, S and value_size are written into the PTX as constants. The device copy exists before the passes run, as for 1531.
- **Pointer uses:** a null test, and a 64-bit add with an unused result, which becomes `red.global.add.u64` (1531's lowering, with provenance).
- **update(K, V, BPF_NOEXIST):**
  1. Walk. A live K gives -EEXIST.
  2. A claiming K: wait until it is live, then -EEXIST.
  3. At an empty slot: if `ld.relaxed(bound)` is at least max_entries, -E2BIG. Otherwise `atom.add(bound, 1)`; if the old value is at least max_entries, give it back and return -E2BIG.
  4. `CAS empty -> {K, claiming}`.
     - **Won:** row = `atom.add(next_row, 1)`; `red.add` V's words into the caller's shard; `st.release {K, row, live}`; return 0.
     - **Lost to K:** give the reservation back, wait for live, return -EEXIST.
     - **Lost to another key:** keep the reservation and walk on.
- **Why the wait:** a thread that got EEXIST must not then get NULL from its own lookup. No bpf interleaving gives EEXIST and then NULL without a delete in between. After it has waited, PTX 8.10.5 (sequential consistency per location) keeps its later reads at live.

**Store layout** (device-table's protocol, with the judges' fixes)
- **lookup(K):**
  - Walk; `ld.acquire` on K's header.
  - **live:**
    - In-place form returns the cell.
    - Copy form copies the value. An 8-byte value is one `ld.relaxed.gpu.global.b64`. A longer value is a versioned read: v1 = acquire, words relaxed, `fence.acq_rel.gpu`, v2 = relaxed, retry unless v1 = v2 and live. The copy goes into the site's slot in the 1530 area, and its address is returned.
  - **writing-new, deleted or empty:** NULL.
  - **writing** (a multi-word write of a live key): retry, then wait.
- **update(K, V, flags):** the low 32 bits must be ANY, NOEXIST or EXIST; anything else is -EINVAL, and BPF_F_LOCK is -EINVAL.
  - **K live:**
    - NOEXIST: -EEXIST.
    - In-place form, when V is K's own cell: a no-op returning 0. Copying a value onto itself is the identity at the update.
    - 8-byte value: one `st.relaxed.gpu.global.b64`; it never waits.
    - Longer value: `CAS live(v) -> writing(v+1)` (acquire); `fence.acq_rel.gpu`; word stores; `st.release live(v+1)`.
  - **K writing-new or writing:** wait, then decide again.
  - **K deleted:** EXIST gives -ENOENT. Otherwise `CAS deleted(v) -> writing-new(v+1)`; fence; stores; release live.
  - **Empty slot reached:** EXIST gives -ENOENT. Otherwise the same `bound` check as the sum layout; `CAS empty -> {K, writing-new}`; fence; stores; `st.release live`. A CAS lost to K gives the reservation back and decides as for "K found".
- **delete(K):** live gives `CAS -> deleted(v+1)` and 0. writing-new or writing: wait. deleted or empty: -ENOENT.
- **The rule for writes in flight:** any operation that reports "present" for a key whose write is in flight waits; lookups report absent instead.

**Waits.** Only for a slot whose owner has already executed its CAS.
- Basis: CPG 5.9 "aims to provide parallel forward progress [intro.progress.9]", where "once this thread has executed a step, it provides concurrent forward progress guarantees". Spins use atomic reads plus nanosleep (device.threads.4: "perform a synchronization operation or an atomic read operation").
- This is partly documented: a CUDA C++-level text read onto inline PTX [inference].
- The sum hot path never waits.

**Not from the GPU:** get_next_key, lookup-and-delete and BPF_F_LOCK return -EINVAL, as today.

### 6. Semantics and their basis
**Exactness criterion.** Every result a GPU thread gets, and every state the tool sees after a covered synchronize, is one the same program could get from bpf's hash map under some interleaving the PTX memory model allows. Real-time linearizability is not claimed.

**One slot per key** [documented].
- PTX 8.10.3: "When an atomic operation A and a write W overlap and are morally strong, then the following two communications cannot both exist ...: A reads any byte from a write W' that precedes W in coherence order. A follows W in coherence order."
- Every header access is a 64-bit strong .gpu operation on one .global address. That makes them morally strong (8.7), and so totally ordered (8.9.6: "Two overlapping write operations are related in coherence order if they are morally strong").
- A claimed key never changes, and a walk CASes only the first slot it read as empty. So K cannot hold a second slot (device-table's derivation, checked by the correctness judge). This holds with relaxed loads, because the CAS returns the coherent value.

**Sum hot path without acquire** [documented, under stated preconditions].
- Rows are zeroed once, then changed only by `red`/`atom` adds. The inserted V is an add, the prefill is written before any probe runs, and there are no GPU deletes or stores.
- RMW atomicity then makes every add land whenever the adder saw the key, so no ordering is needed. (Device-table's "acquire even for adds" was about an insert that stores.)
- The zeroing is ordered before every probe by PTX 8.9.4: "Start of a CUDA API call to enqueue a task synchronizes with start of the task" and "Completion of the last task queued to a stream, if any, synchronizes with return from cudaStreamSynchronize".

**Delivery** shows only live keys.
- The snapshot kernel's relaxed header read followed by `fence.acq_rel.gpu` is an acquire pattern (8.8: "a strong read on M followed by an acquire memory fence in program order").
- The inserter's `st.release` (8.9.4: "A release pattern X synchronizes with an acquire pattern Y, if a write operation in X precedes a read operation in Y in observation order ...") puts the inserted V into the delivered sum.

**Store layout** [documented]: fence then strong write is a release pattern (8.8: "a release or acquire-release memory fence followed by a strong write on M"), paired with the lookup's acquire. Each 8-byte word is whole (8.10.3: "Conflicting morally strong operations are performed with single-copy atomicity"). The versioned read for longer values is built from 8.8 and 8.10.6 [inference built on documents].

**Mixed sizes** (the main hole the judges found). PTX 8.7.2: "The axioms in the memory consistency model do not apply if a PTX program contains one or more mixed-size data-races."
- The tool's code never touches a shared value cell with anything but 64-bit strong atomics:
  - sum pointers feed only adds;
  - in-place pointers feed only 64-bit atomics;
  - copy-form plain accesses touch private `.local` memory (PTX 5.1.5: "private memory for each thread").
- Why plain accesses must not reach shared cells [inference, code read]: llvmbpf builds the IR before it sets the NVPTX data layout (`vm/llvm-jit/src/llvm_jit_context.cpp:933-946`). Plain i64 loads and stores therefore carry LLVM's default i64 ABI alignment of 4 B, which NVPTX splits into two 32-bit accesses. map-lowering saw exactly that for 1530's u64. Atomics get 8 B alignment from their size. Check C1 confirms or refutes this.

**Differences from bpf's hash map.** All are visible or counted; none is silent.
1. Capacity counts distinct keys ever claimed. Slots are permanent; bpf counts live keys.
2. Copy form: a lookup returns the value as of the lookup. Writes reach the map only through the write-back update; maps that write without one are refused.
3. Sum pointers address one shard. Reads through them are refused at load.
4. Store lookups wait during a multi-word write in progress. bpf would return the old element.
5. A lookup that meets an insert in flight returns NULL.
6. Errors are -errno. Today the host call returns -1.
7. CPU update and delete return EPERM once the device copy exists.

### 7. Idioms and rates: storage and path
The 14 device-side hash maps in bpftime's GPU examples, from the survey's source lines:

| program: map | operations | layout | result |
|---|---|---|---|
| micro hash_map (HASH, 1,024), update mode | BPF_ANY on key 0 from every thread | store, in-place | exact; last writer wins, 8-byte words whole |
| micro, lookup mode | `(*val)++` with no write-back; the map is empty, so lookups return NULL | host call | today's |
| micro, delete mode | delete key 0 | store | exact (-ENOENT) |
| cuda-counter start_ts (HASH) | every thread: BPF_ANY with key blockIdx.x; exit: lookup and plain read; delete with &pid | store, copy | bpf semantics. Which start each exit reads is the tool's race. The delete misses, as it does on the CPU |
| cuda-counter call_count | lookup, `__atomic_add_fetch`; else NOEXIST of 1 | sum if the add's result is dead (C1), else store in-place | bpf semantics; the first-touch loss is counted as EEXIST |
| cuda-counter total_time_ns | lookup; `*total += delta`; update BPF_EXIST(total); else NOEXIST | store, copy, write-back | bpf semantics. The plain read-modify-write race stays the tool's own. Today's element-replacement loss does not arise |
| cudagraph start_ts, call_count, total_time_ns | leader thread: BPF_ANY, lookup and read, delete | store, copy | exact (one writer); no host step in graph replays |
| gpu_shared_map counter_per_thread (1501) | one thread: BPF_ANY with key pid | store, in-place | exact; automatic |
| threadscheduling thread_mapping (HASH, 4,096 x 80 B) | per thread: BPF_ANY with key (bx<<16)\|tx, update only | store, versioned | exact while distinct keys are at most 4,096; beyond that E2BIG, as in bpf (nothing deletes) |
| threadscheduling sm_histogram, warp_histogram | lookup, `__sync_fetch_and_add`; else NOEXIST | sum | bpf semantics; first-touch loss counted |
| threadscheduling_dynamic_hook, the same three maps (1501) | as above | as above; automatic | as above |

Our own tools:
- th-hostcall threadhist (prefilled key 0, lookup and add): sum, exact.
- pt_hash:
  - CNT, PRE and MISS: sum;
  - UPD and DEL: store;
  - RMW (atomic add, then update EXIST with the same pointer): store in-place, exact, because the self-aliased update is a no-op;
  - TID: store, with lifetime capacity, so E2BIG once distinct thread keys pass max_entries. This shape belongs in 1530 (section 9).

### 8. CPU view
**Before the device copy.** Today's table: bpf semantics for prefill. threadhist.c:76 and PT_PREFILL insert before attach.

**At device-copy creation**, the agent, under the table's writer lock:
1. copies the table's live keys into the zeroed block. In the store layout that is a live header and the value. In the sum layout it is a live header, a row in table order, and the value in shard 0 of that row;
2. waits for the copy;
3. sets the owner pid;
4. publishes the address in map_info.

From then on, CPU update and delete return EPERM (logged once), as 1531 and 1532 do. Lookup-and-delete and the batch operations return EINVAL.

**Delivery.** 1531's thread and schedule:
- within 2 ms of an uncovered synchronize, every 20 ms while values change, otherwise every 200 ms;
- inside the synchronize call with BPFTIME_GPU_DELIVERY_AT_SYNC=1.

The snapshot kernel reads each slot (summing a sum row's shards, or a versioned read of a store value) into staging. The agent then writes a layout-neutral slot-order mirror (key, live, value).

**Mirror seqlock**, with the correctness judge's fix:
- **Writer:** sequence s+1 stored relaxed; release fence; relaxed atomic stores into the inactive mirror; s+2 stored with release.
- **Reader** (the server's bpf() lookup): acquire-read s; copy the value with relaxed atomic loads into a per-thread buffer, because `syscall_context.cpp:956-968` memcpys from the returned pointer after the call; acquire fence; re-read s; retry if it changed.
- This is a CPU-side C++ seqlock. The hand-over between processes stays partly documented (step0b claim 8), as for 1531.

**lookup** returns ENOENT unless the key is live.

**get_next_key** goes in slot order.
- NULL gives the first live key.
- For K, it gives the next live key after K's slot. K's slot exists even after a delete.
- A key never bound gives the first live key, which is bpf's rule.
- Slots never move, so a swap between deliveries neither repeats nor reorders keys.

**Completeness.** Exact for kernels that a covered synchronize waited for. An application that exits without synchronizing loses its last launches: `pt_exitfast` delivered 24,576 of 73,728 exits [measured].

**Status** goes into the mirror's control area and is logged when it changes and at exit:
- E2BIG, EEXIST, ENOENT, waits, unstable slots, fallbacks;
- slots claimed, live keys;
- deliveries, covered synchronizes.

1531's status keys at max_entries + i cannot work, because every u32 is a legal key. The channel is open question 3.

**Processes.** One process delivers each map (1531's owner rule). A second GPU process's operations are refused visibly and counted.

### 9. Capacity and loss accounting
**max_entries bounds distinct keys ever claimed.** E2BIG is returned at insert and counted.
- This matches bpf for tools that never delete: counters, thread_mapping, cuda-counter's start_ts.
- It differs for insert/delete churn. With per-thread start times, bpf needs about 12,288 resident keys plus those not yet deleted. Here every distinct thread key of the largest grid needs a slot: 720,896 to 4,096,000 threads on llama.cpp [measured census]. Such tools get counted E2BIG and belong in 1530.

**Near the limit,** racing reservations and give-backs can produce a spurious E2BIG. It is counted, like the CPU table's per-CPU free-list window.

**Idiom losses are kept, as in bpf, and counted:**
- **First-touch counter:** NULL from the lookup, then a NOEXIST insert that gets EEXIST. The device size is unmeasured [inferred: up to 31 per converged warp, plus warps whose lookups ran before the claim]. Today it is 2,169-5,957 of 100,096 [measured]. Fusing lookup and insert would remove it (open question 2).
- **Plain read-modify-write written back:** lost updates, the tool's race.

**Delivery** loses nothing for completed, covered kernels. A store slot that stays unstable after R retries keeps its previous value and is counted.

### 10. Expected costs
Baselines [measured]:
- tg128 runs 3,137,339,392 mul_mat_vec_q threads per run at about 1.885-1.89 s per run (67.9 t/s).
- A 5% overhead allows about 0.03 ns per thread.

| idiom, rate | path | anchors | expected |
|---|---|---|---|
| counter on a constant or pid key, every thread | sum, inline | `red.global` to a (SM, lane) shard: 0.010-0.015 ns [measured addmin]. The dependent header load is unmeasured; the closest analog, the wait for an atomic's reply, is 0.028-0.033 [measured] | 0.02-0.06 ns per thread, 0.06-0.19 s per run, 65.7-61.7 t/s, -3% to -9% [inferred]. Unsharded, the same work is 1.09-1.10 ns [measured merge-cost] |
| SM keys (8), warp keys (384), every thread | sum, rows below R | as above, plus collisions | 0.02-0.08 ns [inferred]. Warp keys beyond R: 0.275 ns per thread [measured shape, step0 e2, 32 lanes on one word] |
| block key, every thread, table full (5,632 keys per launch against 1,024) | E2BIG path: walk, relaxed `bound` read, per-SM counter | | about 0.05-0.1 ns [inferred]. bpf also gives E2BIG |
| counter with a used fetch result, every thread | store, in-place, one word | merge-cost one word | about 1.0-1.1 ns per thread [measured shape]. Exact but slow |
| one thread per block (rms_norm, 1,378 calls per 32-token prefill) | helpers | today 13.08-13.67 us per call, 0.0233 -> 0.0413-0.0421 s [measured] | at most about 1 us per call, at most 1.4 ms per prefill [inferred] |
| one thread per launch (15,616 calls per tg128 run) | helpers | today +0.3256 / +0.3270 s per run, 20.85-20.94 us per call [measured] | at most 16 ms per run [inferred]. The rate filter's 16 points are outside the map |
| leader state per launch (cudagraph's 7 operations, gpu_shared_map's 1) | store helpers | 17.87-20.94 us per sparse call today; about 70 us during graph launches on the old base [measured] | about 0.5-3 us per operation [inferred] |
| per-thread start time (update at entry; lookup and delete at exit) | store, copy | | 0.3-1.0 ns per thread plus counted E2BIG [inferred]. In 1530 the carry costs about 0 [measured, probe-cost section 6] and `lat` costs 0.125-0.127 ns per thread by value [measured] |
| per-thread 80-B record | store, versioned | `fence.acq_rel.gpu` 1.0 ns per warp [measured, C3] | about 0.5-1 ns per thread [inferred] |
| first touch of a key | CAS, `bound`, `next_row` | CAS loop: 41 ns per operation over 8 words [derived, worst case] | about 1 us once per key, plus tens of ns per warp [inferred] |
| delivery | snapshot kernel | `lat` (532 KB): 0.30-0.65 ms average [measured] | similar for 32-704 KiB of staging [inferred] |

Further costs:
- **Copy form** forces the kernel's 1530 area into the address form: +0.025 ns per thread on mul_mat_vec_q [measured, by-value t1/t2].
- **The use check** is one bytecode pass per program. Start-up is already 76-155 s per llama-bench invocation [measured].
- **Code size:** about 2,000-3,000 lines with tests [inferred], in three stages:
  1. sum layout and the use check;
  2. store layout;
  3. options the user decides.

### 11. Out of scope
- keys other than 4 bytes (8-byte keys need a second key word claimed under "claiming");
- values that are not a multiple of 8;
- PERCPU_HASH, LRU_HASH, BPF_F_LOCK;
- get_next_key and lookup-and-delete from the GPU;
- CPU writes after the device copy exists;
- several GPU processes per map;
- reusing deleted slots for other keys (live-key capacity);
- lowering thread-keyed HASH entries into 1530 automatically;
- the fused counter;
- in-call delivery for large maps;
- host-cache's validated hits.

### 12. Relation to 1530, 1531 and 1532
- **1531.** The sum layout is a key directory in front of 1531's rows. It reuses:
  - `sum_array_offset`;
  - the `red.global` lowering and provenance (`sum_ptrs`, `nv_thread_local_routing.cpp:1259-1266`, extended to helper-1 results);
  - the snapshot's shard sum;
  - the delivery thread, its schedule, the owner pid, the zeroing chain and relaxed capture mode.

  `registered_map` (`runtime/src/bpf_map/gpu/nv_gpu_model_maps.cpp:186-200, 345-395`) gains a map kind and a second snapshot entry.
- **1530.** Copy-form lookups keep their copies in the 1530 area. Per-thread scratch stays in 1530, declared by the tool.
- **1532.** Per-thread results stay in 1532.
- **What the hash map adds:** CAS-claimed keys and a store layout. Neither existed in the model before.

## Rejected

- **host-cache** (CPU-owned table with validated device hits), rejected as the path:
  - It is the research scheme dropped on 11-14 (RESEARCH_DIRECTIONS origin/main, Direction 2).
  - The tool's atomics land in uncached registered memory: 59.8 ns per thread when lanes share a word, and 82-106 ns per operation serialized on one word per SM [measured, step0 e2].
  - Writes and first touches stay 10-21 us host calls.
  - The correctness judge found defects:
    - an entry with ref = 0 validates against the never-used, zero-filled element 0;
    - the miss-reuse bump is published after the response, not before;
    - device-answered lanes are mixed into `__syncwarp(__activemask())`.
- **device-table as written:**
  - Its unsharded values cost 1.09-1.10 ns per thread on a hot key [measured merge-cost], about -64% on tg128 [projection]. They are replaced by the sum layout.
  - It hands out in-place pointers for plain accesses and only logs 32-bit and plain uses. That is a mixed-size race (PTX 8.7.2), so those uses now go to copies or are refused.
  - Its lookups wait on new inserts; here they return NULL.
  - Kept from it: the directory, the reserve-then-CAS order, the versioned store protocol, delete keeping the slot, the self-aliased no-op, and the CPU view.
- **shards** (per-(SM, lane) insert-only tables):
  - A lookup creates the key and BPF_NOEXIST adds. Both change what the program observes, so they are at most a user opt-in.
  - Device reads return one shard's partial sum.
  - max_entries is enforced only at merge, so which keys are shown depends on delivery timing.
  - A full spill table is probed in full on every miss (the block-key cliff).
  - It refuses every store idiom and needs its own merge path.
  - Kept from it: its "directory variant" is this design's sum layout.
- **route-first:**
  - Key-class decoders are dropped: the directory serves any key, block ids included.
  - Its word-cell lock costs 41-350 ns per operation under contention [derived]; it is replaced by lock-free 8-byte stores and versioned words.
  - CPU writes queued and rebased against a lagging table can give two successful NOEXIST inserts.
  - Upserts into a host table sized max_entries + D are not buildable: the table is sized at BPF_MAP_CREATE (`map_handler.cpp:1252-1256`).
  - Classification inside nv_attach_impl runs after map_info is written (`bpf_attach_ctx.cpp:642`).
  - Its E2BIG rollback undoes a state other threads may already have seen.
  - Its single-word (S = 1) fetch ignores the base value.
  - Plain read-modify-writes on private copies are not bpf-legal for atomics.
  - Kept from it: the exactness criterion, the bytecode use check (moved to attach init), and private copies with the write-back rule. Its thread-private class is left to the user.
- **Warp aggregation:** agreement on an activemask mask is not documented, and C2 measured millions of violations.
- **CAS written in the tool:** llvmbpf's CMPXCHG bug.
- **Rows indexed by slot:** twice the memory.
- **Rows numbered from `bound`:** giving a row back can duplicate it.
- **Lookups that wait on inserts in flight:** NULL is bpf's answer for a key not yet inserted.
- **Slot reuse for deleted keys:** needs locks on each new key's home slot. Not in the first version.
- **llvmbpf data-layout change, as the main path:** it affects every GPU program and makes unaligned 8-byte eBPF accesses undefined. Left as open question 7.

## Checks

### C1, compile only, no GPU. Run the bytecode use check and llvmbpf's PTX over the 7 example programs (cuda-counter, cudagr

C1, compile only, no GPU. Run the bytecode use check and llvmbpf's PTX over the 7 example programs (cuda-counter, cudagraph, gpu_shared_map, threadscheduling, threadscheduling_dynamic_hook, micro in all 3 modes), th-hostcall, hostwake, hostspin and the pt_hash variants. Print, per map: its verdict (sum / store in-place / store copy / host call) with the reason. Print, per access through a lookup result: its width and whether it is generic or .global. Print whether `__atomic_add_fetch` and `__sync_fetch_and_add` compile to XADD or to FETCH, and whether a FETCH result is live. Print whether plain u64 loads and stores come out as two 32-bit accesses.

Why: It decides which layout each idiom gets, and it confirms or refutes the code-read basis of the mixed-size argument (llvmbpf builds IR under LLVM's default data layout, with i64 aligned to 4 B, before it sets the NVPTX layout). It is cheap and needs no board time.

Decision rule: - Any map whose verdict differs from the design's per-map table stops the work until the difference is explained.
- If cuda-counter's call_count FETCH result is live, that map moves to store in-place; update the table.
- If plain u64 accesses come out 64-bit, record that. Keep the copy form for v1, and list in-place lowering of 64-bit plain accesses as an option for the user.
- If any atomic through a lookup result is not 64-bit, that map goes to the host call.

### C2, hand-written hit path on mul_mat_vec_q, through BPFTIME_CUDA_PTX_OVERRIDE_DIR. tg128 with only mul_mat_vec_q probed,

C2, hand-written hit path on mul_mat_vec_q, through BPFTIME_CUDA_PTX_OVERRIDE_DIR. tg128 with only mul_mat_vec_q probed, at least two passes, clocks checked per item, items shuffled, rules written before the runs. Variants:
- addmin: the 1531 reference;
- hh: hash of a runtime key, one dependent ld.relaxed.gpu.global header load at the home slot, compare, row from the header, shard address, red.global;
- hh2: the hit at the second slot;
- hpre: a constant key whose row is fixed at patch time, so the header load only decides presence and is off the address chain;
- hone: a row at or beyond R (one word per key);
- hcall: the walk in a noinline helper;
- hacq: hh with ld.acquire.gpu.

Why: The dependent header load is the one unmeasured part of the sum layout's hot path. It decides whether hash counters cost close to 1531 arrays on fine-grained kernels, whether inlining is worth building, the default R, and the price of acquire for the store layout.

Decision rule: - (a) hh - addmin <= 0.02 ns in both passes: build the inline path as designed.
- 0.02 < hh - addmin <= 0.05: build it inline, and also build hpre's pre-bound constant keys if hpre saves at least 0.006 against hh in both passes.
- hh - addmin > 0.05: build hpre if it brings constant keys within 0.05 of addmin. Otherwise report to the user that hash counters cost that much more than 1531 arrays.
- (b) Inline only if hcall - hh >= 0.006 in both passes.
- (c) If hone - hh >= 0.3 ns, the default R must cover every hot key C1 finds (constant, pid, SM, warp).
- (d) hacq - hh is the price of acquire per thread, used in C5.
- Any difference that moves more than 0.006 between passes gets a third pass.

### C3, compile only. Build the trampoline's new inline PTX for sm_61 (as today) and for sm_70: CAS .b64 in relaxed and acqu

C3, compile only. Build the trampoline's new inline PTX for sm_61 (as today) and for sm_70: CAS .b64 in relaxed and acquire forms, ld relaxed and acquire, st relaxed and release, fence.acq_rel.gpu, nanosleep, red.global.add.u64. Merge it, with C2's inline path, into the patched llama.cpp modules (rms_norm, mul_mat_q, mul_mat_vec_q, rope). Read the SASS, and compare registers and local frame per kernel with today's patched modules.

Why: It gates the build. The .relaxed and .acquire qualifiers and nanosleep require sm_70. The argument that all accesses use one address requires .global forms. mul_mat_vec_q sits at a warp-count boundary, where a few registers moved results by 0.03 ns per thread.

Decision rule: - If ptxas rejects the sm_61 build, move the trampoline to sm_70.
- The SASS must show ATOMG.E.CAS.64.STRONG.GPU, LDG.E.64.STRONG.GPU and RED on the cvta.to.global address. Any generic form blocks the build until it is fixed.
- Any kernel that gains registers or loses warps per SM with the inline path keeps the helper call.

### C4, a standalone stress test in hand-written PTX, before any bpftime code: a CUDA test program with the helper bodies as

C4, a standalone stress test in hand-written PTX, before any bpftime code: a CUDA test program with the helper bodies as inline assembly.
- (a) Claims: 10^6 threads x 10 launches inserting with BPF_NOEXIST from 64 heavily duplicated keys, with lookups and adds racing. Afterwards, scan every header.
- (b) Sum exactness for pre-inserted and first-touch keys.
- (c) Store layout: 80-B values whose words carry the writer id; BPF_ANY from many threads on a few keys; deletes and re-inserts; a snapshot kernel every 1 ms; at least 10^8 slot reads.
- (d) NOEXIST losers waiting on 'claiming' while a second process forces time-slicing.
- Controls: no fence after the 'writing' CAS; no version re-read; a claim without the bound reservation.

Why: Correctness gate for the claim, publication, versioned-read and wait arguments. Several of them are derived from PTX 8.8-8.10 by us, not stated by NVIDIA.

Decision rule: - Stop the build on any key in two slots, any count different from the expected value, or any torn delivery with the protocol.
- Every control must fail. If one does not, the test cannot see the fault and is redesigned.
- Zero violations read as 'consistent with the model', not as proof.

### C5, store-layout reads at thread rate. Run the pt_hash TID shape on pt_victim, and the same hand-written on mul_mat_vec_

C5, store-layout reads at thread rate. Run the pt_hash TID shape on pt_victim, and the same hand-written on mul_mat_vec_q: a per-thread lookup with ld.acquire and an 8-B copy into the 1530 area, against 1530 by value. Use C2's hacq for the acquire part.

Why: It decides which rates the store layout claims to serve. ld.acquire.gpu lowers to LDG.E.64.STRONG.GPU plus CCTL.IVALL (step0b C4 SASS), and that cost has not been measured on a real kernel.

Decision rule: - If the store lookup costs at least 0.2 ns per thread more than 1530 in both passes, the design states that per-thread entry and exit state belongs in 1530 (declared), and that the store layout serves block, launch and leader rates. Correctness is unchanged either way.
- Otherwise, record the price per thread.

### C6, a full table at thread rate, hand-written on mul_mat_vec_q. Every thread misses on a table already at max_entries: s

C6, a full table at thread rate, hand-written on mul_mat_vec_q. Every thread misses on a table already at max_entries: start_ts's shape of 1,024 entries against 5,632 blocks per launch. Compare the designed path (walk, relaxed bound read, per-SM E2BIG counter) with a control that does atom.add on bound plus one shared counter.

Why: A full table must not turn every thread into read-modify-writes on one control line. The cost judge priced that at about 2.5 ns per thread.

Decision rule: - The designed path must cost at most hh + 0.03 ns in both passes.
- If it does not, add a 'full' flag that is read before the walk, and measure again.

### C7, independent review of the bytecode use check and the PTX allowlist before any GPU run of built code. Use two lenses,

C7, independent review of the bytecode use check and the PTX allowlist before any GPU run of built code. Use two lenses, with one negative fixture per refusal reason:
- a stack spill;
- a derived pointer;
- a plain store without write-back;
- an atomic through a copy-form pointer;
- a live FETCH on a sum map;
- a CPU program referencing the map;
- a copy-form lookup in a direct run;
- the two checks disagreeing.

Why: One wrong allowlist entry gives silently wrong results. The routing pass's earlier scans failed open in two review rounds (9 + 9 confirmed findings).

Decision rule: - Every confirmed fail-open finding is fixed and reviewed again before any GPU run.
- A disagreement between the bytecode verdict and the PTX verdict must refuse the module loudly in its test row.

### C8, first touch with exact sum semantics on pt_victim: pt_kdiv, and pt_kflat in a graph. Run the counter idiom with one 

C8, first touch with exact sum semantics on pt_victim: pt_kdiv, and pt_kflat in a graph. Run the counter idiom with one key, a key per SM, a key per block, and one thread per launch, three passes each. Count lost increments and EEXIST returns.

Why: The first-touch loss is the idiom's own and is kept under bpf semantics. Its size on the device is unknown [inferred: up to 31 per converged warp plus early warps]. Whether to offer the fused counter is the user's call.

Decision rule: - In every pass, the counted value must equal thread executions minus EEXIST returns exactly, so that every loss is accounted for.
- Report the losses against today's 2,169-5,957 of 100,096 (per thread) and 3-8 of 391 (per block), and hand them to the user for the opt-in decision.
- The default does not change.

### C9, store layout at leader rate. Run cudagraph and gpu_shared_map on their victims under CUDA graphs, plus pt_hash UPD, 

C9, store layout at leader rate. Run cudagraph and gpu_shared_map on their victims under CUDA graphs, plus pt_hash UPD, DEL and RMW, with exact expected values.

Why: It confirms the store protocol end to end, and the gain at the rates where real tools use leader state.

Decision rule: - Any inexact count or value stops the work.
- Report ns per operation against 17.87-20.94 us per sparse call today and about 70 us during graph launches (old base).

### C10, the CPU view.

C10, the CPU view.
- A loader iterates with get_next_key and lookup during and after runs.
- It prefills the way th-hostcall's key 0 and PT_PREFILL do.
- It tries update and delete while kernels run.
- A torn-read stress on the mirror seqlock, with a control that has no fence.

Why: The device-owned switch, prefill import, EPERM and the fixed mirror seqlock are new, and the seqlock fix came from the correctness judge.

Decision rule: - At every covered synchronize, the key set and values must equal a scan of the device state.
- CPU writes return EPERM after the device copy exists.
- 0 torn mirror reads in at least 10^8 reads while the control fails. Any violation stops the work.

### C11, delivery cost against table size on tg128, default and in-call modes: a sum map with R = 512 rows (about 1 MiB of r

C11, delivery cost against table size on tg128, default and in-call modes: a sum map with R = 512 rows (about 1 MiB of rows) and a 704-KiB store map, beside lat's 0.30-0.65 ms average (default) and 0.14-0.15 ms (in-call).

Why: In-call delivery ran 10,244 times per tg128 item for lat. A slow snapshot makes that mode too costly.

Decision rule: - Offer in-call delivery for hash maps only if their in-call average is at most 2x lat's.
- Otherwise, first make the snapshot read only claimed rows, then measure again.

### C12, end to end on llama.cpp, with the HASH opt-in:

C12, end to end on llama.cpp, with the HASH opt-in:
- th-hostcall threadhist on rms_norm (every thread, key 0 prefilled);
- the per-launch counter on tg128's four kernels;
- a lat variant on mul_mat_vec_q whose histogram is a hash map in the sum layout.

Why: The real-application result against today's measured numbers.

Decision rule: - Counts must equal the census counts exactly in every item.
- Report overheads against today's 341-351x slowdown, +0.3256 / +0.3270 s per run, and lat's 0.125-0.127 ns per thread (1531).

## Open questions

- HASH opt-in. Without it, only the 1501 maps are served: gpu_shared_map and threadscheduling_dynamic_hook, 4 maps and 6 of 38 hash call sites, neither run on the Orin yet. Should plain HASH maps get the device map through a per-process list of map names, through every HASH map the use check admits that no CPU program references, or only when the tool declares 1533?
- First-touch counter. Keep bpf's semantics, where losses are counted as EEXIST? Or offer an opt-in fused counter (lookup or insert as one step, so the lookup's NULL branch never runs for an admitted counter idiom)? The fused counter removes the idiom's own loss, which changes what the program observes. C8 gives the size.
- Status channel for loss counts and delivery status (E2BIG, EEXIST, ENOENT, waits, fallbacks, covered synchronizes). Should it be a bpftime-only reader found with dlsym, as 1527's poll is, or a companion status array named after the map? Reserved keys cannot work in a hash key space.
- Capacity. Permanent slots count distinct keys ever claimed, so tools that insert and delete keys keyed by thread get E2BIG where bpf would not. Is that acceptable, or should deleted slots be reusable through a lock on each new key's home slot, giving bpf's live-key capacity?
- Per-thread hash entries. Should a HASH map keyed one-to-one by the thread within its block, inserted and deleted within the thread, be lowered into 1530 automatically (route-first's thread-private class)? Its basis is an inference: guide 3.2 says 'It must be possible to execute blocks in any order, in parallel or in series.' The argument holds per map, not jointly with other maps, and cooperative launches break it. Or do tools declare 1530, as in v1?
- Sum-layout memory. Is R (rows with (SM, lane) shards) per map, default min(max_entries, 512), at most 1 MiB for a u64 map, acceptable? Should it be a tool-visible parameter (for example map_extra) or an environment setting?
- llvmbpf's GPU path builds IR under LLVM's default data layout, so plain u64 accesses come out as two 32-bit accesses [code read; C1 confirms]. Should it set the NVPTX data layout first? That would let the store layout lower plain accesses in place without copies. It affects every GPU program and makes unaligned 8-byte eBPF accesses undefined on the GPU.
- CPU writes while kernels run. Keep EPERM (v1), or later add shards' base-value trick for sum maps (the tool sees delivered minus base), so that print-and-clear tools can delete or lookup-and-delete without writing device memory?