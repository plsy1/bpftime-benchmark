# Device hash map, stage one: build plan (2026-11-27)

Design: `results/gpu/hash-design-20261126/DESIGN.md` (updated 11-27, decisions 1-5).
Checks it rests on: `results/gpu/hash-round1-20261126`, `results/gpu/hash-round2-20261127`
(C5 protocol, C6 publish order, R2 review, `code-map-stage1.json`).
Base: `proto/map-model-70f7bc1f` at 7d02d677, worktree `/home/jetson/src/bpftime-proto-70f7`.
Nothing is built yet. Every step below names its test and a mutation that must make that test fail;
both are written before the step's code.

## 0. What is built

A map type `BPF_MAP_TYPE_GPU_DEVICE_HASH_MAP = 1533` that a tool declares (decision 1).
u32 keys, u64 values. From GPU probes: lookup, adds through the returned pointer, inserts with
`BPF_NOEXIST`. No GPU deletes, stores or other flags. Plain HASH and 1501 do not change.

## 1. Decisions this plan makes (each one is open to the user)

**D1. The 1533 device code is not in the shared trampoline.**
- Why: the trampoline is prepended to every patched module and is kept sm_61-legal today
  (no `.relaxed/.acquire/.release`, no `nanosleep`, no 64-bit `atom`). The insert needs those forms.
  Putting them in the trampoline would make every patched module fail to compile on a GPU below
  sm_70, whether or not it uses a hash map. 1531 keeps its sm_70 forms out of the trampoline the
  same way: they are in the routing pass's inline text and in the snapshot module, which exist only
  when a 1531 map does.
- So: the device functions (`devhash_lookup`, `devhash_insert`) are compiled by clang++-16 from one
  `.cu` file into a checked-in PTX text (`devhash_ptx.h`, as `trampoline_ptx.h` is made). The routing
  pass, which runs only for modules whose process has a 1533 map, appends them (`.func`, not
  `.visible`) to the module and calls them directly, with the map's geometry as immediates.
- The trampoline gets only sm_61-legal 1533 branches, never a host call:
  lookup NULL, update -1, delete -EINVAL. They serve calls the pass does not rewrite
  (a non-constant fd, or no device copy at patch time).
- Cost: constant-fd calls only. A 1533 call with a non-constant fd returns NULL or -1.
  This is documented. 1531's inline path has the same constant-fd requirement.

**D2. Hash and seed.** home(K) = ((u64)(K ^ SEED) * 0x9E3779B97F4A7C15) >> (64 - logS), as in C5,
with SEED a fixed constant in the layout header. There is no per-map seed: no field carries it to
the routing pass, and these maps are keyed by the tool's own counters.

**D3. Geometry from what MapBasicInfo already has.**
- S = next_pow2(2 x max_entries), at least 2.
- R = min(max_entries, 512).
- shards = max_thread_count, as for 1531.
- No new MapBasicInfo field, so the 40-byte struct and its three copies stay as they are.

**D4. Header states.**
- Header word = key << 32 | row << 3 | state.
- States: 0 empty, 1 claiming, 2 live, 3 refused.
- New: 4 opaque, meaning occupied with no key. Only the host writes it: in the CPU mirror (gap
  repair, section 4) and when seeding at owner takeover.
- A device walk matches K only when the state is 1, 2 or 3 and the key field is K. An opaque slot is
  walked past.

**D5. Loss counts on the device are sharded.**
- Counters, laid out [counter][shard]: EEXIST, E2BIG, unstable EEXIST, unstable E2BIG (full
  directory), bad operation (other flags, delete), full walks (lookup), full walks (insert).
- Each thread adds to its own (SM, lane) shard with `red.relaxed.gpu.global.add.u64`.
- The snapshot sums them.

**D6. Return codes from the GPU.**
- Insert: 0 won, -17 (EEXIST, including unstable), -7 (E2BIG: refused, capacity, full directory),
  -22 (EINVAL: flags other than BPF_NOEXIST).
- No device copy: -1. This is 1531's update value.
- Delete: -22.
- Lookup: a pointer or NULL.

**D7. The CPU side has three phases** (state word PHASE in the map object in shm):
- 0 CPU: bpf hash semantics on a CPU table.
- 1 SEALING: writes get EPERM; lookups still come from the CPU table.
- 2 DEVICE: writes get EPERM; lookups and get_next_key come from the delivered mirror.

**D8. Seal exclusion without a lock that a dead process can hold.**
- A CPU writer does `INFLIGHT.fetch_add(1)` (seq_cst), then loads PHASE (seq_cst):
  - if PHASE is not 0, it does `fetch_sub` and returns EPERM;
  - otherwise it does the operation, then `fetch_sub` (release).
- The sealer stores PHASE = 1 (seq_cst), then waits, bounded at 1 s, for INFLIGHT == 0 (acquire).
- Store-buffer argument (seq_cst on both sides): either the writer sees PHASE 1, or the sealer sees
  its INFLIGHT.
- On timeout (for example a writer killed inside an operation): PHASE goes back to 0, the copy
  fails closed (no device copy, so GPU lookups NULL and inserts -1), and it is logged.
- The CPU table's own locking (`hash_map_impl`, CLAUDE.md "Hash Map Concurrency") still orders
  writers among themselves.

**D9. The CPU table before the copy is the existing `hash_map_impl`.**
- It is the one BPF_MAP_TYPE_HASH uses, sized max_entries, so its bpf semantics are already tested.
- At the seal the agent iterates it and places each key into the device layout with the shared
  hash and linear probing on the host: rows 0..n-1 in iteration order, the value in shard 0 of its
  row (or its single word), count = n.
- The mirror is filled to match before PHASE = 2 is stored (release).
- Pointers from phase-0 lookups stay valid, because the table is frozen and never freed while the map
  lives. After the copy they show the sealed value, not the device's.

**D10. The mirror is its own index.**
- The mirror is shm arrays `mhdr[S]` (u64, atomic) and `mval[S]` (u64), indexed by slot.
- A CPU lookup walks from home over `mhdr` with acquire loads, at most S steps, and stops at empty.
  It returns `&mval[j]` only for live K.
- That walk would miss keys, because the snapshot reads slots one by one: slot i can be delivered
  empty while K is delivered live further on. So the delivery thread (the only writer, in the owner
  process) keeps the invariant "every non-empty slot's probe path from its home is non-empty" before
  it publishes:
  - **repair:** in staging, for each non-empty slot j with key K, mark every empty slot from home(K)
    to j as opaque. A mirror slot never goes back to empty: staged empty over a non-empty mirror
    slot keeps the mirror's value.
  - **pass A:** write changed values (relaxed).
  - **pass B:** for every slot that is empty in the mirror and non-empty in staging, store opaque
    (relaxed).
  - **fence (release).**
  - **pass C:** store each changed header (release).
  - Then the status words, then the release increment of DELIVERIES.
- A reader that acquires a live header sees its value (pass A) and the occupancy of every slot
  (pass B), whatever the slot order, wraparound included.
- An opaque slot from repair gets its real header in a later delivery. The device header was
  non-empty when K's walker passed it, which happened before K's release publish. The next snapshot
  launch is stream-ordered after the one that acquired K's header.
- Takeover is the exception: the mirror keeps its slots, so iteration order still never changes, but
  rows are renumbered (D11).

**D11. Owner takeover re-seeds at the same slots.**
- A process taking over from a dead owner seeds the fresh block from the mirror:
  - every live slot stays live at the same slot, with rows renumbered 0..L-1 in slot order and the
    delivered value in shard 0;
  - every other non-empty slot (claiming, refused, opaque) becomes opaque;
  - count = L;
  - the loss counters are seeded with the delivered totals.
- A claim that died in flight therefore does not hold capacity, and a key once refused can be
  inserted again if there is room, as in bpf.
- Adds made after the last delivery by the dead owner are lost. That is documented; 1531 loses
  everything at a takeover.

**D12. Companion status array (decision 3).**
- The tool declares it in its BPF object: an ARRAY, key 4, value 8, at least 16 entries.
- Its name is the hash map's name truncated to 12 bytes plus `_st`, 15 bytes at most, which fits
  BPF_OBJ_NAME_LEN.
- The owner binds it in `get_gpu_mem_buffer` by scanning the handlers. It requires exactly one
  match: none or two means no companion, logged once, and the status stays readable only in the
  map object. The binding is pinned like the map itself.
- Words:
  - 0-5: as 1531's status words (DELIVERIES, EXIT_DELIVERY, OWNER_PID, SYNCS_COVERED, SYNCS_AT_EXIT,
    LAUNCHES_AFTER_LAST_SYNC);
  - 6-12: the D5 counters;
  - 13: count;
  - 14: live slots;
  - 15: refused slots.
- The delivery writes words 1-15 relaxed, then word 0 with release.
- Note for the user: decision 3 means each hash tool declares two maps.

**D13. The "adds only" and 64-bit-only rules are a contract, as for 1531.**
- The routing pass rewrites other atomics through an inline 1533 result to its guarded form and
  does not refuse them.
- It cannot follow pointers that a function returned.
- DESIGN.md says so since 11-27.

**D14. A snapshot failure stops hash maps only.**
- The hash snapshot is its own entry and CUfunction with its own warm-up launch, and it has its own
  broken flag.
- A failing hash snapshot leaves 1531 and 1532 delivery running.

**D15. Delivery is set up before the address is returned.**
- If the staging allocation or registration fails, `get_gpu_mem_buffer` returns 0 (no device copy)
  and PHASE goes back to 0.
- 1531 instead returns the address undelivered.

## 2. Steps

Each step builds and its tests pass before the next one that depends on it.
Order: S0, S1, then S2, S3 and S6 in any order, then S4, S5, S7, S8.

### S0. Compile checks of the forms round one's C3 did not cover (rules first, `S0-RULES.md`)

1. **The snapshot module** (`.version 7.0 .target sm_70`) with `ld.acquire.gpu.global.b64`: ptxas
   for sm_87, and record the SASS.
2. **`devhash.cu`, the C5 bodies ported** (S3's first draft):
   - clang++-16 to PTX;
   - append it to the patched llama.cpp modules (the 11-26 by-value build's dump);
   - ptxas at sm_87.
   - Rules: the SASS shows the `.STRONG.GPU` forms and `MEMBAR`/`ERRBAR` where C6 found them, and
     no probed kernel loses warps per SM against the same build without the functions.
3. **C6, fifth variant:** release publish, `ld.acquire.gpu` reader, 10^8 instances.
   - Rule: 0 weak means the snapshot uses `ld.acquire`.
   - Any weak means it uses `ld.relaxed` + `fence.acq_rel.gpu`, the pair C6 measured, and the
     result is reported.

### S1. One layout header (`attach/nv_attach_impl/trampoline/gpu_devhash_layout.h`)

**Contents:**
- the type constant;
- the states;
- header encode/decode;
- SEED and `home_slot`;
- the geometry: S, logS, R and the region offsets from (max_entries, shards);
- the block layout:
  - control words: magic, max_entries, S, R, shards, count, reserved; 256 B;
  - loss counters: [7][shards];
  - directory: [S];
  - rows: [R][shards];
  - single rows: [max_entries - R];
  - regions 256-byte aligned;
- `static_assert`s.

It builds as host C++ (g++), as device code for clang CUDA, and is included by `devhash.cu`, the
routing emitter, the runtime map object, the snapshot set-up and the tests. The emitter's PTX text
takes its constants from these functions at patch time. The one place the hash is written a second
time, as PTX text, is the emitter.

**Tests:**
- Host test `[devhash-layout]`: offsets and sizes for max_entries 1, 2, 48, 512, 513, 1024 and 2^22;
  R; S a power of two and at least 2 x max_entries; no overlap; total size; preconditions (rows cover
  max_entries; R x shards x 8 fits 32-bit offsets).
- `home_slot` equals C5's for 10^4 keys at logS 1..23.
- Mutations that must fail: shift by 63 - logS; R = 513.

### S2. Runtime type and CPU object, no CUDA (`runtime/`)

**Code:**
- The enum value 1533 in `bpftime_shm.hpp`. It stays out of `gpu_map_type_without_cuda_ipc`.
- `check_map_attr`:
  - key 4, value 8;
  - 1 <= max_entries <= 2^22;
  - the CPU part fits in the segment.
- `map_init`/`map_free` and lookup, update, delete, next_key in `map_handler.cpp`, all under
  `BPFTIME_ENABLE_CUDA_ATTACH`.
- New class `nv_gpu_device_hash_map_impl`. It holds:
  - the phase-0 `hash_map_impl`;
  - PHASE and INFLIGHT;
  - the mirror (mhdr, mval);
  - its own status words (D12 0-15);
  - the delivered count.
- CPU operations by phase (D7-D10):
  - get_next_key goes in slot order over live slots: a NULL or unknown key gives the first live slot.
  - lookup_and_delete keeps the default (524).
  - Update and delete in phases 1 and 2 return EPERM.
- Host-only functions, so they can be tested without a GPU:
  - `seal_and_build_image()`: the D8 seal, then D9 placement, giving an image of headers, rows and
    count;
  - `apply_delivery(staging)`: the D10 repair and passes A, B, fence, C;
  - `takeover_image()`: D11.

**Tests** (`runtime/unit-test/maps/test_gpu_device_hash.cpp`, `[gpu-devhash-cpu]`, style of
`test_map_create_checks.cpp`):
- **create checks:** wrong sizes give EINVAL; too many entries give E2BIG.
- **phase 0:** NOEXIST, EXIST, ANY, E2BIG, ENOENT and delete behave as on a HASH map with the same
  calls.
- **seal race:** 8 writer threads insert distinct keys while the sealer seals. Every key is in the
  image or its writer got EPERM.
  - Mutation that must fail: the writer skips INFLIGHT.
  - Also a writer process forked and SIGKILLed inside an update (INFLIGHT left at 1): the seal times
    out, PHASE returns to 0, and later CPU updates work.
- **image:** n keys prefilled give rows 0..n-1, values in shard 0 and count n. Every key is found by
  a host walk of the image.
- **mirror:** a writer thread applies synthetic stagings, made from a host model of the device
  directory with gaps and wraparound at the end of the directory, while 4 reader threads look keys
  up.
  - Rules: once a reader has found a key live, it finds it on every later lookup; a found value is
    never below the value staged with that header.
  - Mutations that must fail: no repair; headers stored in one pass (no pass B); values after
    headers; a slot allowed back to empty.
  - Run it once more with the reader in a forked process on the same segment.
- **full directory:** lookup and get_next_key end within S steps when no slot is empty.
- **takeover image:** live slots keep their slots, rows are 0..L-1, everything else is opaque,
  count = L, and loss totals are seeded.

### S3. Device functions (`attach/nv_attach_impl/devhash/devhash.cu` -> `devhash_ptx.h`)

**Code:**
- A port of `c5.cu`'s v2 bodies, not of the code map's prose. It keeps:
  - the precheck with the slot re-read (control 5);
  - refused as the outcome of a wait;
  - the full-directory wait;
  - the S-step bound on every walk;
  - the wait bound: 64 ns doubling to 2,048, 100,000 ns requested in all.
- Every access goes through inline asm on `cvta.to.global` addresses. The qualifiers are written
  out: `ld.relaxed.gpu`, `ld.acquire.gpu`, `atom.relaxed.gpu.cas.b64`, `atom.acq_rel.gpu.add.u64`,
  `st.release.gpu`, `red.relaxed.gpu.add.u64`, `nanosleep.u32`.
- Opaque slots are walked past (D4). Loss counts are sharded (D5).
- Signatures take (base, key, value pointer, flags), and the geometry as immediates.
- clang++-16 `--cuda-device-only -O2 -S` at sm_70. A Makefile target `../devhash_ptx.h` strips the
  module header and `.visible`. The PTX is checked in, as `trampoline_ptx.h` is.
- The trampoline's sm_61 Makefile targets do not compile this file.
- The trampoline itself gets only the D1 branches. `trampoline_ptx.h` is regenerated, and a check
  confirms it still has no sm_70-only form.

**Tests** (GPU, `runtime/unit-test/cuda/`, `[gpu-devhash-device]`; skipped without a device):
- C5's runs a, b, c, e, g and h through `devhash_ptx.h` in a module with test kernels, with C5's
  checker ported. Fewer trials: 1 + 50 each for a, b, c, g, h; 1 + 5 for e.
- Pass rules as in `RULES-round2.md`.
- The controls are built from the same `.cu` with `-DBPFTIME_DEVHASH_FAULT=1/2/5/6` at test build
  time (clang++-16 on the board). Each must fail the check that round two names.
- **termination:** one key per thread at max 48 (directory full). Every kernel ends under a
  watchdog, and lookups NULL, inserts -7.
  - Mutation that must fail: no S bound, so the watchdog fires.
- **opaque:** a directory seeded with opaque slots on K's probe path. K is inserted past them, found,
  and has one slot.
- **flags and delete:** -22 and counted. With no copy (base 0): -1 and NULL.

### S4. Device copy in the agent (`nv_gpu_model_maps.cpp`, `map_handler.cpp`)

**Code:**
- `get_gpu_mem_buffer` for 1533, in this order:
  1. the owner CAS;
  2. allocate and zero;
  3. seal and image (S2);
  4. `cuMemcpyHtoDAsync` of headers, rows, count and counters on the same stream;
  5. synchronize;
  6. register for delivery (D15);
  7. PHASE = 2 (release), after the mirror holds the seeded image;
  8. return the address.
- Takeover uses `takeover_image()`.
- 1533 in `get_gpu_map_extra_buffer` and `get_gpu_map_max_thread_count`, and in the pin list.
- Companion binding and pin (D12). First check what `create_map_basic_info` does with an ARRAY
  map's values (`bpf_attach_ctx_cuda.cpp:532-533`) and that the delivery thread may write them.

**Tests** (GPU, in the style of `test_cuda_host_call.cpp:1016`):
- Prefilled keys are at the image's slots on the device; extra_buffer is non-zero; max_thread_count
  is the shard count; CPU updates afterwards give EPERM.
- A second process (not the owner) gets 0.
- Takeover: a forked owner exits after one delivery; the parent's copy has the same live slots and
  values.
- A failure injected after the seal (test hook) returns 0, and PHASE is 0 again.
- Companion: created before or after the hash map, it binds; two candidates bind neither.
- Mutation that must fail: 1533 left out of the pin list, so the "close while delivering" test sees
  a freed object (run under ASan).

### S5. Snapshot and delivery (`nv_gpu_model_maps.cpp`)

**Code:**
- A second snapshot entry, `bpftime_model_hash_snapshot`. Per slot:
  - load the header with the S0-decided acquire form;
  - if live, sum the row's shards (row < R) or read its single word;
  - stage the raw header and the value.
- It also sums the counters and reads the count. It has its own CUfunction, warm-up and broken flag
  (D14).
- `registered_map` gets a kind and the 1533 geometry. `deliver_locked` calls `apply_delivery()` and
  writes the companion.
- It uses the sum schedule. The exit delivery writes the exit words.

**Tests:**
- **Hand-built device block:** all five states, rows below and above R, known shards.
  - Staging must be exact for live slots and raw for the others.
  - Mutations that must fail: summing non-live rows; claiming staged as empty; row >= R read as
    sharded.
- **End to end:**
  - A kernel inserts N keys with NOEXIST and adds, then sync, then `deliver_now`.
  - The mirror and the companion must hold the exact sums minus the counted EEXIST, with
    SYNCS_COVERED = app syncs.
  - Repeat with `BPFTIME_GPU_DELIVERY_AT_SYNC=1`.
- **Failure isolation:** a test hook fails the hash launch; a 1531 map in the same process keeps
  being delivered.

### S6. Routing pass (`nv_thread_local_routing.{hpp,cpp}`, `nv_attach_impl.cpp`)

**Code:**
- A `hash_map_desc` (fd, base, max_entries, shards; the rest from S1) as a new defaulted parameter.
- 1533 in the gate at `nv_attach_impl.cpp:1042`.
- The pass appends `devhash_ptx.h`'s functions once per module that uses them.
- **Constant-fd lookup, base != 0, the inline hit path (C2):**
  - key (forwarded as for 1531), home, one `ld.relaxed.gpu.global.b64`;
  - live K at home: the shard address, as for 1531, or the single word;
  - empty: NULL, no call;
  - anything else: a call to `devhash_lookup` into a block-local register.
  - One `mov` at the join writes the program's register, so it has one definition per lookup and
    its adds take the `.global` path.
  - Labels are unique per function.
- **Constant-fd update:** the call block becomes a call to `devhash_insert`.
- **Constant-fd delete:** -22 inline.
- **Base 0:** NULL and -1 inline.
- A malformed block throws, as for 1531.
- Stats: hash lookups inlined, inserts rewritten. They are added to the INFO line.

**Tests** (`attach/nv_attach_impl/test/test_hash_map_routing.cpp`, `[hash-map-routing]`):
- Mirrors of the 1531 cases at `test_sum_map_routing.cpp` :76, :135, :185, :289, :514, :642, and the
  key-forwarding ones.
- New cases:
  - the program register has one definition and its adds are `.global`;
  - two inline lookups in one function have distinct labels;
  - empty gives NULL with no call;
  - a module whose only GPU-model map is 1533 is routed;
  - the functions are appended once, and not at all when unused;
  - by-value is kept beside lat's 1530 lookups (`[tl-byvalue]` :281 pattern);
  - the emitted hash equals `home_slot` (the text is run on the GPU in S3's test module for 10^4
    keys).
- Mutations that must fail: the fallback writes the program register directly (two definitions,
  adds guarded); a label without the sequence number.

### S7. Tools and documentation

**Tools:**
- `pt_common.h` gets `MT_DEV_HASH 1533`.
- `pt_hash.bpf.c` gets a type switch for CNT, PRE and MISS, as `pt_lat` switches with PT_OLD/PT_MIX,
  and declares the companion.
- `pt_loader` dumps 1533 by key with get_next_key, and the companion as an array.

**Documentation:**
- `runtime/src/bpf_map/gpu/README.md`, new, in the format of
  `attach/trap_uprobe_attach_impl/README.md`. For tool authors:
  - how to declare the map and the companion;
  - first touch gives EEXIST (counted);
  - the E2BIG/lookup window, as bpf's htab has it (decision 5);
  - unstable outcomes;
  - the refused-slot limit and an undersized map;
  - visibility within one launch rests on the hardware;
  - the contract of adds only, 64-bit;
  - constant fds;
  - prefill before the agent starts;
  - the CPU phases and EPERM;
  - takeover.
- A short CLAUDE.md section for the cross-cutting rules: the trampoline stays sm_61-legal, the
  layout header is the only definition, and the regeneration commands.

### S8. Measurement and correctness against today (rules first, `S8-RULES.md`)

**Set-up:** jetson_clocks checked (cpu5 1728000, GPU 1020 MHz), GPU lock, bounded runs, containers
and shm cleaned.

**pt_victim, the hash-current matrix** (D and G sizes, CNT/PRE/MISS, three passes), with 1533
against today's host path:
- **counts:** exact for PRE; for CNT, the count equals expected minus the counted EEXIST, and
  EEXIST + E2BIG + unstable are read from the companion;
- **per operation:** by slope, as in hash-current.

**llama.cpp:**
- **cost on top of 1531:** tg128, mul_mat_vec_q, every thread, constant key, against the same tool
  on 1531.
  - Expected 0.034-0.037 ns per thread over 1531 (C2).
  - Rule: above 0.06 ns in both passes means a SASS comparison with C2's `hh` form before anything
    is reported.
- **against today's host path:** rms_norm pp32, per thread and per block, against the hash-current
  rows.
- **the per-launch counter** on the four tg128 kernels (l3/l4 shape).

**Every unit test of S1-S6** runs once more on the final build.

## 3. Code-map risks and where each is handled

| risk (code map) | step |
|---|---|
| walks need an S bound (refused slots fill the directory) | S3 termination test; S2 full directory test |
| refused semantics at lookup, insert, wait | S3 (C5 port, ctrl 5/6) |
| loss counters hot spot | D5, S3 |
| layout copies disagree | S1, S6 hash-equality test |
| no device copy must not fall through | D1 branches, D6, S3 and S6 tests |
| new PTX forms, register and occupancy effect | S0 |
| return codes | D6, S7 README |
| result register defined twice | S6 test and mutation |
| malformed block throws (whole module unprobed) | as 1531, S7 README |
| no 1533 in the pass gate | S6 test |
| `.version` of original modules | S0 item 2 |
| mixed-size or non-add uses not refused | D13, S7 README |
| MapBasicInfo has one pointer | D3 |
| CPU-writable phase then seal, writers in flight | D7, D8, S2 seal race tests |
| CPU walk misses slot-by-slot deliveries | D10, S2 mirror test |
| header before value order in the mirror | D10, S2 mirror mutations |
| owner takeover drops keys | D11, S2 and S4 takeover tests |
| copy made at the first CUDA link; prefill must come first | S7 README; S4 EPERM test |
| companion naming and reachability | D12, S4 tests |
| pin list (use after free) | S4 ASan test |
| shared broken flag | D14, S5 test |
| staging failure leaves an undelivered map | D15, S4 test |
| second PTX entry compiled at first launch | S5 warm-up |
| snapshot size (R x 256 shards) | S8 measures delivery time |
| memcpy in the syscall server races the next delivery | per word, as 1531 (S7 README) |
| trampoline sm_61 test target | D1: trampoline unchanged in kind |
| direct run makes its own context | out of scope: not used by these tools; unverified for 1531 too |
| without CUDA attach, 1533 is EINVAL | as 1531 |

## 4. Not in stage one

The store layout, GPU deletes, other flags, multi-word values, per-thread keys (1530), a fused
lookup-or-insert, non-constant fds on the device path.
