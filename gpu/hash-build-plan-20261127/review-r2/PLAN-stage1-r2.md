# Device hash map, stage one: build plan (2026-11-27, revision 2)

Design: `results/gpu/hash-design-20261126/DESIGN.md` (updated 11-27, decisions 1-5).
Checks it rests on: `results/gpu/hash-round1-20261126`, `results/gpu/hash-round2-20261127`
(C5 protocol, C6 publish order, R2 review, `code-map-stage1.json`).
Base: `proto/map-model-70f7bc1f` at 7d02d677, worktree `/home/jetson/src/bpftime-proto-70f7`.
Nothing is built yet.

**Revision 2.** Revision 1 went to four independent reviewers (coverage, buildability,
cross-process CPU concurrency, device and routing). Each finding then went to a refuter.
83 findings came back and 80 held (`review-r1/plan-review.json`). This revision answers all 80;
section 6 maps each one to its change.

**Every test names how it can fail:** a mutation that must make it fail, or "(no mutation: reason)".
Both are written before the step's code.

## 0. What is built

A map type `BPF_MAP_TYPE_GPU_DEVICE_HASH_MAP = 1533` that a tool declares (decision 1).
- u32 keys, u64 values.
- From GPU probes: lookup, adds through the returned pointer, and inserts with `BPF_NOEXIST`.
- No GPU deletes, no stores, no other flags.
- Plain HASH and 1501 do not change.

## 1. Decisions this plan makes (each one is open to the user)

### D1. The 1533 device code is not in the shared trampoline

**Why.** The trampoline is prepended to every patched module, and today it uses no sm_70-only form
(`.relaxed/.acquire/.release`, `nanosleep`). 64-bit `atom`/`red` without `.sem`/`.scope` are
sm_61-legal and are not the limit. The insert needs the sm_70 forms. In the trampoline they would
make every patched module fail to compile on a GPU below sm_70, hash map or not.
1533 is the first map type whose sm_70 forms go into patched application modules. That happens only
in modules of a process that has a 1533 map, and only where the pass rewrites a 1533 call.
(1531's only sm_70 form is in its snapshot module; its routed text is sm_61-legal.)

**The functions.** They are compiled by clang++-16 from one file,
`attach/nv_attach_impl/devhash/devhash.cu`, into a checked-in PTX text (`devhash_ptx.h`). A
Makefile target uses the trampoline's `DEVICE_ONLY_FLAGS` (`-x cuda --cuda-device-only -nocudainc
-nocudalib`) at sm_70.
- There are exactly three `extern "C"` functions, all with u64 parameters and a u64 return:
  - `__bpftime_devhash_lookup(base, key, max_entries, shards)` returns a generic pointer
    (base + offset) or 0.
  - `__bpftime_devhash_insert(base, key, value_ptr, flags, max_entries, shards)` returns a signed
    result in a u64, so the program's r0 holds a 64-bit -17.
  - `__bpftime_devhash_layout_fp()` returns the layout fingerprint (S1).
- Everything else in the file is forced inline, so these are the only `.func`s.
- The Makefile strips the module header and `.visible`.

**Where the pass puts them.** The routing pass inserts the text once per module that needs it, at the
start of the module body: after the module's `.version`/`.target`/`.address_size`, before any
function. PTX needs a function declared before a call (`ptxas: Call to ... requires call prototype`).
Call sites use the prototype from that text.

**What stays in the trampoline.** Calls the pass does not rewrite go to the trampoline: a
non-constant fd, or no device copy at patch time (base 0), as 1531 keeps its call there. The
trampoline gets sm_61-legal 1533 branches with no host call and no counting:
- lookup: NULL;
- update: -1;
- delete: -22.

**Difference from 1531.** For 1531 the trampoline serves those calls, and a copy made later is found
through `map_info`. For 1533 they never reach the device map, even after a copy appears. README.

### D2. Hash

home(K) = ((u64)(K ^ SEED) * 0x9E3779B97F4A7C15) >> (64 - logS), as in C5. SEED is a fixed constant in
the layout header; no field carries a per-map seed to the routing pass. Tests vary the key set per
trial instead (S3), since the layout no longer varies with a seed.

### D3. Geometry from what MapBasicInfo already has

- S = next_pow2(2 x max_entries), at least 2.
- R = min(max_entries, 512).
- shards = max_thread_count, as for 1531.
- No new MapBasicInfo field.

### D4. Header states

Header word = key << 32 | row << 3 | state.

| state | meaning |
|---|---|
| 0 | empty |
| 1 | claiming |
| 2 | live |
| 3 | refused |
| 4 | opaque: occupied, no key |

- **Opaque is exactly the word 4** (key field 0, row 0). Only the host writes it: in the CPU mirror
  (gap repair, D10), and when seeding at owner takeover (D11).
- **The device walk decides on the state first:**
  - 0: stop;
  - 4: walk past;
  - 1, 2 or 3 with key field K: K's slot;
  - anything else: walk past.
- C5 compared the key field first. That order would take opaque for key 0's slot, so the port must
  not keep it (S3 opaque test, with K = 0).
- **The CPU walk** goes past every header that is not live K, and stops only at empty, at live K, or
  after S steps.

### D5. Loss counts on the device are sharded

- **Counters,** laid out [counter][shard]:
  - EEXIST;
  - E2BIG;
  - unstable EEXIST;
  - unstable E2BIG (full directory);
  - bad flags (an insert with flags other than BPF_NOEXIST);
  - lookups that walked all S slots;
  - inserts that walked all S slots.
- Each thread adds to its own (SM, lane) shard with `red.relaxed.gpu.global.add.u64`, and the
  snapshot sums them.
- **Deletes are not counted on the device:** they never reach devhash code. The pass counts rewritten
  delete call sites in its patch-time stats.

### D6. Return codes seen by GPU programs

- Insert:
  - 0: won;
  - -17: EEXIST, stable or unstable;
  - -7: E2BIG (refused, capacity, or full directory);
  - -22: flags other than BPF_NOEXIST.
- Through the trampoline (D1): update -1, delete -22.
- Lookup: a pointer or NULL.

GPU programs get kernel errno values only from 1533. A plain HASH map's GPU update through today's
host call returns the backend's raw -1. S8 therefore counts host-path outcomes as "non-zero", not by
value.

### D7. Three CPU phases

PHASE is a word in the map object in shm.
- **0 CPU:** bpf hash semantics on a CPU table.
- **1 SEALING:** writes get EPERM; lookups still come from the CPU table.
- **2 DEVICE:** writes get EPERM; lookups and get_next_key come from the delivered mirror.

Every CPU read path loads PHASE with acquire first. Ordering between the phases:
- lookup_and_delete stays 524 (ENOTSUPP), as today's default;
- a get_next_key dump that crosses the switch to phase 2 changes order (table bucket order, then
  slot order) and can skip keys (README: dump after the copy is made).

### D8. Seal exclusion without a lock a dead process can hold

**Writer** (CPU update or delete, any process):
1. `INFLIGHT.fetch_add(1, seq_cst)`;
2. `PHASE.load(seq_cst)`: if not 0, `fetch_sub(1, release)` and EPERM;
3. otherwise the table operation, then `INFLIGHT.fetch_sub(1, release)`.

**Sealer:**
1. `PHASE.store(1, seq_cst)`;
2. poll `INFLIGHT.load(seq_cst)` until 0, bounded at 1 s.

All four are in the single total order, so either the writer reads PHASE 1 or the sealer reads its
increment. An acquire load would not do: on an RCpc build it becomes `LDAPR`, which may pass the
earlier `STLR`. A code comment pins this, and S2 disassembles the seal once to check that the first
poll is `LDAR`. The S2 tests cannot see it.

**A writer that dies between steps 1 and 3 leaves INFLIGHT above 0 for the life of the segment.**
- Every seal then times out, and the map never gets a device copy. GPU lookups give NULL and inserts
  -1 through the trampoline (base 0); the CPU stays in phase 0.
- Each process records the failure and does not try again: one 1 s wait per process, not one per
  CUDA link, held under `map_binding_mutex`.
- The block allocated for the attempt is freed.
- README.

### D9. The CPU table before the copy is the existing `hash_map_impl`

It is the one BPF_MAP_TYPE_HASH uses, sized max_entries, so its bpf semantics are tested already.

**The image is built from the frozen table by a lock-free scan of its elements.**
- A new method on `bpftime_hash_map`, `for_each_in_use_unlocked`, reads each element's word
  (acquire), then the key and value. It runs only with PHASE >= 1 and INFLIGHT 0, when no writer can
  run.
- It takes no stripe lock, so a tool killed inside `next_key` (which holds a stripe lock that
  INFLIGHT does not show) cannot hang the seal.
- Keys are placed in scan order: rows 0..n-1, linear probing with the shared hash, the value in shard
  0 of its row (or its single word), count = n.

Pointers from phase-0 lookups stay valid: the table is frozen and lives as long as the map. After the
copy they show the sealed value. CPU programs must not write through 1533 lookup pointers (README).
Adds after the seal are lost; in phase 2 the next delivery overwrites them.

### D10. The mirror is its own index (replaces DESIGN.md's "own index of the delivered live headers")

- **Layout:** shm arrays `mhdr[S]` (u64, atomic) and `mval[S]` (u64), indexed by slot.
- **CPU lookup** (phase 2): walks from home with acquire loads (D4's CPU rule) and returns `&mval[j]`
  only for live K.
- **One writer at a time:**
  - the seeding thread, before the map is registered for delivery;
  - then deliveries, under `run_mutex`.

**The invariant.** A slot-by-slot snapshot can deliver slot i empty while K is live further on. So the
writer keeps this true before it publishes: "every non-empty slot's probe path from its home is
non-empty". Each delivery:
1. **Repair:** in staging, for each non-empty slot j with a key K, mark every empty slot from home(K)
   to j as opaque. A mirror slot never goes back to empty: staged empty over a non-empty mirror slot
   keeps the mirror's value.
2. **Pass A:** changed values (relaxed).
3. **Pass B:** opaque into every slot that is empty in the mirror and non-empty in staging (relaxed).
4. Release fence.
5. **Pass C:** each changed header (release).
6. The status words, then the release increment of DELIVERIES.

A reader that acquires a header from pass C sees its value (pass A) and every slot made non-empty by
this delivery (pass B), whatever the slot order, wraparound included.

**When an opaque slot from repair gets its real header** (PTX ISA 9.5 8.9.2, 8.9.4, 8.10.6):
- **Repair caused by a live or refused header:** the next delivery. The walker read the slot
  non-empty before its `st.release`. The snapshot that acquired the header synchronizes with that
  release, and the next snapshot is stream-ordered after it.
- **Repair caused by a claiming header:** that header comes from a relaxed CAS, so nothing follows.
  The slot resolves only after K is published. If the last delivery or the owner's death comes
  first, it stays opaque for good (D11).

### D11. Owner, phases and takeover

**Owner record:** OWNER_PID plus the owner's pid-namespace inode (`/proc/self/ns/pid`, as the perf
shards record it).

**The owner is alive if** `kill(pid, 0)` succeeds or fails with EPERM.
- An owner in another pid namespace is never taken over: return 0, logged.
- A record equal to this process's (pid, namespace) with no copy in this process's cache means the
  pid was reused, and the process takes over.

**Under ownership, PHASE chooses the path:**
- **0, first copy:** seal (D8), image (D9), copy, mirror seeded, registration, PHASE 2.
- **1, a sealer died:**
  - writers already get EPERM;
  - reset the mirror to empty;
  - poll INFLIGHT as in D8 (a writer that read PHASE 0 before the dead sealer's store may still run);
  - build the image from the frozen table;
  - continue as the first copy.
- **2, takeover:** seed a fresh block from the mirror.
  - **Live slots** stay live at the same slot, with rows renumbered 0..L-1 in slot order and the
    delivered value in shard 0.
  - **Every other non-empty slot** becomes opaque (claiming, refused, opaque).
  - **count = L,** or max_entries if the seeded directory has no empty slot. Otherwise every insert
    of a new key would wait out the full-directory bound for a count that cannot rise.
  - **The loss counters** are seeded with the delivered totals.
  - **The takeover does not write the mirror;** its deliveries do. Mirror slots stay non-empty, and
    slot order never changes.
  - **So:**
    - a claim that died in flight holds no capacity;
    - a key once refused can be inserted again if there is room, as in bpf;
    - keys inserted and adds made after the dead owner's last delivery are lost (README; 1531 loses
      everything at a takeover).

### D12. Companion status array (decision 3)

**Declaration.** The tool declares it in its BPF object: an ARRAY, key 4, value 8, at least 16
entries. Its name is the hash map's name cut to 12 bytes plus `_st`.

**Binding** happens in `bpf_map_handler::get_gpu_map_extra_buffer`, which has the handler's name and
does the pinning, under the `map_binding_mutex` already held there.
- It needs exactly one ARRAY candidate, and exactly one 1533 map that derives that name.
- Otherwise none binds, logged once, and the status stays readable only through the map object.
- The companion is pinned like the map. The 1533 object gets its impl pointer.

**Words:**

| word | content |
|---|---|
| 0-5 | as 1531's status words |
| 6-12 | the D5 counters |
| 13 | count |
| 14 | live slots |
| 15 | refused slots |

Each word is an independent counter, written relaxed. Words are not ordered against each other or
against the hash (README).

Note for the user: decision 3 means each hash tool declares two maps.

### D13. Contracts, as for 1531

- **Adds only, 64-bit only, on GPU pointers.** The pass rewrites other atomics through an inline 1533
  result to its guarded form and does not refuse them. It cannot follow pointers that a function
  returned.
- **The program's adds keep 1531's form** (`atom`/`red` `.global.add.u64`). They rely on the PTX
  defaults `.relaxed` and `.gpu`. Inside the devhash functions every qualifier is written out.
  DESIGN.md's "qualifiers written out" sentence is corrected to say so.

### D14. The hash snapshot is isolated for set-up, load and launch errors

- It is its own CUmodule and CUfunction, with its own warm-up launch and its own broken flag.
- A failure there stops hash delivery only.
- A device fault inside it is sticky for the whole context and would stop everything; that is not
  covered (README).

### D15. Registration reports failure

- Registration returns a result.
- The copy goes into the process cache only after it succeeds.
- **On failure:**
  - free the block;
  - on a first copy, reset the mirror to empty, then PHASE 0;
  - on a takeover, keep PHASE 2 and the mirror unchanged;
  - return 0.
- (1531 pushes to the cache first, and its registration returns void.)

### D16. Test conventions

- **Child processes.** A GPU test that takes ownership, registers a delivery, or can hang runs in its
  own exec'd child of the test binary. The pattern is a hidden tag, as `test_cuda_no_device.cpp:93-104`.
  - Why: the delivery state is a process-wide singleton, never reset; the copy cache is keyed by
    object address; `BPFTIME_GPU_DELIVERY_AT_SYNC` is read once per process; a forked child cannot use
    CUDA.
  - The parent kills a child at a total deadline and counts that as the result, which is the expected
    one for the hang mutations. Afterwards it checks that no child is left holding the GPU.
- **App syncs.** Tests that need them call `attach::launch_activity().note_app_sync()` after each of
  their own synchronizes. Only the agent's hooks count syncs otherwise.
- **Delivery.** A test that needs a delivery asserts that `deliver_now` delivered at least one map.
- **Lifetimes** are checked through refcounts and a destroy-counting test hook, not ASan. Map objects
  live in the boost segment, which ASan does not see, and the CUDA build has no ASan.
- **Test hooks** are test-only functions behind `BPFTIME_DEVHASH_TEST_HOOKS`, named in each step.
- **Test files** and the CMake lists they join are named in each step.

## 2. Steps

**Order:** S0, S1, S2, S3, then S4 and S6 (independent of each other), then S5, S7, S8. Each step
builds and passes its tests before any step that depends on it.

### S0. Compile checks (rules first, `S0-RULES.md`)

1. **The hash snapshot module** (`.version 7.0 .target sm_70`) with `ld.acquire.gpu.global.b64`:
   ptxas for sm_87, and record the SASS.
   - (no mutation: a record, no decision.)
   - The acquire rests on PTX ISA 8.8 case 1 and 8.9.4. C6 cannot tell `ld.acquire` from no acquire
     on this device (its variant C gave 0 weak), so no fifth C6 variant is run.
2. **The devhash functions called for real.**
   - Hand-route the hash tool's probe on mul_mat_vec_q the way round one's C2 `hcall` form was made:
     the inline hit path, the `__bpftime_devhash_lookup` fallback, and a `__bpftime_devhash_insert`
     call, all from S3's first draft of `devhash.cu`, placed by D1's rule.
   - ptxas at sm_87. Compare registers, stack and warps per SM of every probed kernel against the
     same tool on 1531.
   - **Rule:** a probed kernel that loses warps per SM is reported, with its register counts, and
     decided with the user before S6. The insert is a call either way; the fallback call is what
     may cost.
   - Run once more on the final `devhash_ptx.h` in S8.
   - Mutation: the functions placed after the probes; ptxas must fail with the prototype error.

### S1. One layout header (`attach/nv_attach_impl/trampoline/gpu_devhash_layout.h`)

**Contents:**
- the type constant;
- the states and the opaque word;
- header encode and decode;
- SEED and `home_slot`;
- geometry: S, logS, R, and the region offsets from (max_entries, shards);
- the block layout:
  - control words: magic, max_entries, S, R, shards, count, and reserved words (the test build's
    diagnostic counters, S3); 256 B;
  - loss counters [7][shards];
  - directory [S];
  - rows [R][shards];
  - single rows [max_entries - R];
  - regions 256-byte aligned;
- `DEVHASH_LAYOUT_FP`, a constexpr hash of all the constants above;
- `static_assert`s.

It builds as host C++ and for clang CUDA. It is included by `devhash.cu`, the routing emitter, the
runtime object, the snapshot set-up and the tests. The emitter takes its constants from it at patch
time.

**Tests:** `attach/nv_attach_impl/test/test_devhash_layout.cpp`, `[devhash-layout]`, in
`bpftime_nv_attach_tests` (TEST_SOURCES of `attach/nv_attach_impl/test/CMakeLists.txt`).
- Offsets and sizes for max_entries 1, 2, 48, 512, 513, 1024 and 2^22: R; S a power of two and at
  least 2 x max_entries; no overlap; total size; rows cover max_entries; R x shards x 8 fits 32-bit
  offsets.
  - Mutation: R = 513.
- `home_slot` equals C5's for 10^4 keys at logS 1..23.
  - Mutation: shift by 63 - logS.
- Header encode/decode round trip, and opaque decodes as state 4 with no key.
  - Mutation: row field 30 bits.

### S2. Runtime type and CPU object, no CUDA (`runtime/`)

**Code:**
- **Type and checks:**
  - enum 1533 in `bpftime_shm.hpp`, outside `gpu_map_type_without_cuda_ipc`;
  - `check_map_attr`: key 4, value 8, 1 <= max_entries <= 2^22, and the CPU part fits in the segment;
  - `map_init`/`map_free` and the lookup, update, delete and next_key switches in `map_handler.cpp`,
    all under `BPFTIME_ENABLE_CUDA_ATTACH`.
- **Class `nv_gpu_device_hash_map_impl`** holds:
  - the phase-0 `hash_map_impl`;
  - PHASE and INFLIGHT;
  - the owner record;
  - the mirror;
  - its status words (D12 0-15);
  - the bound companion pointer.
- **`bpftime_hash_map::for_each_in_use_unlocked`** (D9).
- **Host-only functions:**
  - `seal()` (D8);
  - `build_image()` (D9);
  - `apply_delivery(staging)` (D10);
  - `takeover_image()` (D11);
  - `choose_path()` (D11, given PHASE and the owner record).
- **Test hooks:**
  - park a writer after its PHASE load;
  - park a writer after its INFLIGHT increment;
  - a liveness stub;
  - a namespace stub.

**Tests:** `runtime/unit-test/maps/test_gpu_device_hash.cpp`, `[gpu-devhash-cpu]`, in TEST_SOURCES of
`runtime/unit-test/CMakeLists.txt`, the file guarded by `BPFTIME_ENABLE_CUDA_ATTACH` as
`test_gpu_map_without_ipc.cpp` is.

1. **Create checks:** wrong sizes give EINVAL; too many entries give E2BIG.
   - Mutation: value size 16 accepted.
2. **Phase 0:** NOEXIST, EXIST, ANY, E2BIG, ENOENT and delete behave exactly as on a HASH map given
   the same calls.
   - Mutation: phase 0 routed to the mirror.
3. **Seal race:** a writer is parked by the hook after its PHASE load, then the sealer stores
   PHASE 1, then the writer is released. Its key must be in the image, or its call must return EPERM.
   - 8 free-running writers also run, and every key is in the image or got EPERM.
   - Mutation: the writer skips INFLIGHT. The parked key is then lost with no EPERM, every time.
4. **Dead writer:** a forked writer is SIGKILLed at the hook after its INFLIGHT increment. Then:
   - the seal fails within its bound;
   - PHASE is 0;
   - later CPU updates of keys on every stripe work;
   - a second seal attempt in the same process returns at once (the failure is cached).
   - Mutation: no failure cache, so the second attempt waits 1 s.
   - (A kill inside the table's own update hangs that stripe, as documented for HASH; not run.)
5. **Stripe held:** a stripe is held with `lock_stripe_for_test` during the seal. The seal completes.
   - Mutation: the image built with `next_key`, so the child is killed at its deadline.
6. **Image:** n prefilled keys give rows 0..n-1, values in shard 0 (or the single word), count n.
   Every key is found by a host walk of the image, and one key in two slots fails.
   - Mutation: count left 0.
7. **Mirror.** A writer thread applies synthetic stagings while reader threads run.
   - **The stagings** come from a host model of the device directory, with:
     - wraparound at S-1;
     - K = 0;
     - opaque on probe paths;
     - claiming-caused repairs that persist over several stagings;
     - defensive stagings that show a repaired slot empty again.
   - **Rules:**
     - (a) once a reader has found a key live, it finds it on every later lookup;
     - (b) a found value is never below the value staged with that header;
     - (c) the writer bumps a release marker after each `apply_delivery`; a reader that acquires
       marker >= N finds by lookup every key live in staging N;
     - (d) a scanning reader finds by an immediate lookup every key that get_next_key returns.
   - **Mutations:**
     - no repair, caught by (c);
     - no pass B, caught by (d);
     - values after headers, caught by (b);
     - a slot allowed back to empty, caught by (a) with the defensive stagings.
   - Run once more with the readers in a forked process on the same segment.
8. **get_next_key:** live keys only, in slot order; NULL or an unknown key gives the first live
   slot; it ends within S steps on a full directory.
   - Mutation: refused returned as live.
9. **Takeover image:**
   - live slots keep their slots, with rows 0..L-1;
   - everything else is opaque;
   - count = L;
   - count = max_entries when no slot is empty;
   - loss totals are seeded.
   - Mutation: count = L on a full directory.
10. **choose_path:** PHASE 0, 1 and 2 with live, dead, EPERM-alive and other-namespace owners, and
    the same pid without a cached copy.
    - Mutation: EPERM taken as dead.
11. **Phase reads:** in phase 1 lookups come from the table; PHASE is read with acquire.
    - (no mutation: the order is fixed by the D8 disassembly check, not observable here.)

### S3. Device functions and trampoline branches

**Code:**
- **`devhash.cu`** is a port of `c5.cu`'s v2 bodies, not of the code map's prose. It keeps:
  - the precheck with the slot re-read (control 5);
  - refused as an outcome of the wait;
  - the full-directory wait;
  - the S-step bound on every walk;
  - the wait bound: 64 ns doubling to 2,048, 100,000 ns requested in all.
- **Changes from C5:**
  - D4's state-first walk;
  - sharded counters (D5);
  - qualifiers written in full with `.global`: `ld.relaxed.gpu.global`, `ld.acquire.gpu.global`,
    `atom.relaxed.gpu.global.cas.b64`, `atom.acq_rel.gpu.global.add.u64`,
    `st.release.gpu.global.b64`, `red.relaxed.gpu.global.add.u64`, `nanosleep.u32`, on
    `cvta.to.global` addresses;
  - the lookup returns base + offset (generic), as the inline path does.
- **Test builds,** under `BPFTIME_DEVHASH_TEST`:
  - a per-call outcome out-parameter with C5's OC codes;
  - C5's diagnostic counters (key changed, row overflow, refused published, next row and give-back
    for control 6) in reserved control words;
  - `BPFTIME_DEVHASH_WAIT_BOUND`;
  - `BPFTIME_DEVHASH_FAULT` 1/2/5/6.
- **Generated headers,** by the same Makefile and checked in, so CMake needs no clang:
  - `devhash_ptx.h`;
  - `devhash_test_ptx.h`, with the test build, the four fault builds and the wait-0 build.
- **The trampoline:** the D1 branches only. `trampoline_ptx.h` is regenerated.

**Tests.** GPU cases go in `runtime/unit-test/cuda/test_gpu_devhash_device.cpp` (the CUDA list in
`runtime/unit-test/CMakeLists.txt`), `[gpu-devhash-device]`. Each runs in a child (D16).
1. **C5 runs a, b, c, e, g (wait-0 build) and h,** with C5's checker ported and the outcome channel.
   - The key set is varied per trial: an odd multiplier and offset in the harness's key function.
   - Trials: 1 + 50 for a, b, c, g, h; 1 + 5 for e.
   - Pass rules as in `RULES-round2.md`.
   - (no mutation: the controls below are the mutations.)
2. **Run d, seeded:** run d with the image from S2's `build_image()`.
3. **Run d, takeover:** the same from `takeover_image()`: opaque slots on the probe paths, renumbered
   rows, count = L. The per-key rule holds on both:
   delivered = seeded + attempts - EEXIST - E2BIG.
   - Mutation: count not copied (rows reused).
4. **Controls 1, 2, 5 and 6,** with `RULES-round2.md`'s configurations and 1 + 200 trials each. Each
   must fail the check round two names.
5. **Termination:** one key per thread at max 48, so the directory fills. Every kernel ends; lookups
   give NULL and inserts -7.
   - Mutation: no S bound, so the child is killed at its deadline.
6. **Full directory, count below max:** a directory seeded full with count < max_entries. An insert
   of a new key returns -7, the unstable-E2BIG counter rises, and the kernel ends.
   - Mutation: an unbounded full-directory wait (killed).
7. **Opaque:** opaque slots on K's probe path, for K = 0 and K = 1000. K is inserted past them, found,
   and has one slot.
   - Mutation: key compared before state, so K = 0 stops at opaque.
8. **Flags:** an insert with BPF_ANY returns -22, and bad-flags rises.
   - Mutation: no count.
9. **ABI:** test kernels declare the call exactly as the pass will. -17, -7 and -22 arrive as 64-bit
   negatives.
   - Mutation: an int return.
10. **Trampoline branches** (the real trampoline, `test_module` and `counting_host` of
    `test_cuda_host_call.cpp`): a 1533 `map_info` with extra_buffer 0 and non-zero. Helpers 1, 2 and
    3 give NULL, -1 and -22, with `served == 0`.
    - Mutation: a branch removed, so `served > 0`.
11. **Host text checks** (no GPU), in `[devhash-layout]`'s file:
    - `devhash_ptx.h` has the qualifiers above at the publish, precheck, CAS, count and adds;
    - its fingerprint constant equals `DEVHASH_LAYOUT_FP`;
    - `trampoline_ptx.h` has none of the sm_70 forms, and ptxas `-arch=sm_75` accepts it with its
      `.target sm_61` kept.
    - Mutations: `.release` changed to `.relaxed`; a stale fingerprint.

### S4. Hash snapshot module (`nv_gpu_model_maps.cpp`)

**Code:** `bpftime_model_hash_snapshot` in its own PTX module (D14). Per slot:
- load the header with `ld.acquire.gpu.global`;
- if live, sum the row's shards (row < R) or read its single word;
- stage the raw header and the value.

It also sums the counters and reads the count. It has its own CUfunction, warm-up and broken flag.

**Tests** (child, D16):
- **A hand-built block** with all five states, rows below and above R, known shards and counters.
  Staging is exact for live slots and raw for the others.
  - Mutations: non-live rows summed; claiming staged as empty; a row >= R read as sharded.
- **Text check:** the header load is `ld.acquire.gpu`.
  - Mutation: `ld.relaxed`.

### S5. Device copy and delivery in the agent (`nv_gpu_model_maps.cpp`, `map_handler.cpp`)

**Read first:** what `create_map_basic_info` does with ARRAY values (`bpf_attach_ctx_cuda.cpp:532-533`),
and that the delivery thread may write a companion's values.

**`get_gpu_mem_buffer` for 1533:**
1. the owner record and `choose_path()` (D11);
2. allocate and zero;
3. seal and image, or the takeover image;
4. `cuMemcpyHtoDAsync` of headers, rows, count and counters on the same stream, then synchronize;
5. first copy only: write the seeded mirror;
6. register for delivery (a `registered_map` kind for 1533, with a result, D15);
7. first copy only: PHASE = 2 (release);
8. cache the copy and return the address.

**Also:**
- 1533 in `get_gpu_map_extra_buffer` (companion binding and pins, D12) and
  `get_gpu_map_max_thread_count`.
- `deliver_locked` calls the hash snapshot and then `apply_delivery()`, and writes the companion.
- The exit delivery writes the exit words.
- The sum schedule.
- **Test hooks:**
  - fail registration;
  - fail the hash module's load;
  - stop the owner after the CAS, after the seal, and between pass B and pass C;
  - a destroy counter.

**Tests** (each in a child, D16; `test_gpu_devhash_device.cpp`):
1. **First copy:**
   - prefilled keys sit at the image's slots on the device;
   - extra_buffer is non-zero, and max_thread_count is the shard count;
   - CPU updates afterwards give EPERM.
   - Mutation: 1533 missing from `get_gpu_map_max_thread_count` (returns 0).
2. **Not the owner:** a second child, with the owner alive, gets 0. So do an owner alive by EPERM
   (stub) and an owner in another namespace (stub).
   - Mutation: the inherited rule (any failure of `kill` means dead).
3. **Owner deaths:** the owner child is stopped by the hook and killed after the CAS, after the seal
   (PHASE 1 left), and between pass B and pass C. The next child keeps every prefilled and every
   delivered live key, and PHASE ends at 2.
   - Mutation: the path chosen by the owner CAS alone, so prefilled keys are lost after a death in
     phase 1.
4. **Takeover after a clean exit** that followed deliveries: the same live slots and values.
5. **Registration failure:**
   - on a first copy: 0 returned, PHASE 0, mirror empty, block freed; then a phase-0 delete and a
     successful seal, and the deleted key is absent from the mirror;
   - on a takeover: PHASE stays 2 and lookups are unchanged.
   - Mutation: PHASE 0 on the takeover failure.
6. **End to end:**
   - a kernel inserts N keys with NOEXIST and adds;
   - synchronize, then `note_app_sync`;
   - `deliver_now` (at least one map delivered).
   - The mirror and the companion hold exact sums minus the counted EEXIST and E2BIG, and
     SYNCS_COVERED equals the noted syncs (non-zero).
   - Repeat in a child with `BPFTIME_GPU_DELIVERY_AT_SYNC=1`.
   - Mutation: the delivery does not record SYNCS_COVERED.
7. **Prefill under GPU inserts:**
   - CPU prefill, copy, then GPU NOEXIST inserts and adds on the prefilled keys, then delivery;
   - one slot per key;
   - delivered = seeded + adds - counted EEXIST.
   - Mutation: count not copied.
8. **Refused-slot fill:** distinct keys past max_entries. The companion's E2BIG count equals the -7
   results, and refused slots are reported.
   - Mutation: refused slots not counted in word 15.
9. **Close while delivering:** the tool closes its fds during deliveries. The 1533 object's and the
   companion's refcounts stay above 0, and the destroy counter reads 0.
   - Mutation: 1533 left out of the pin list.
10. **Companion:**
    - binds when created before or after the hash map;
    - two candidate arrays: none binds;
    - two hash maps sharing a 12-byte prefix: none binds;
    - a 15-character hash map name binds to its 12-byte prefix plus `_st`;
    - the words are readable with `bpf_map_lookup_elem`.
    - Mutation: uniqueness checked one way only.
11. **Isolation:** the hash module's load fails (hook), and a 1531 map in the same child keeps being
    delivered.
    - Mutation: one broken flag.

### S6. Routing pass (`nv_thread_local_routing.{hpp,cpp}`, `nv_attach_impl.cpp`)

**Code:**
- A `hash_map_desc` (fd, base, max_entries, shards; the rest from S1), added as a new defaulted
  parameter. Stats: inline lookups, rewritten inserts, rewritten deletes.
- 1533 in the gate at `nv_attach_impl.cpp:1042`.
- **Placement:** the devhash text goes at the start of the module body, once, only when a 1533 call
  is rewritten (D1).
- **Constant-fd lookup, base != 0: the inline hit path.**
  - key (forwarded as for 1531), home, one `ld.relaxed.gpu.global.b64`;
  - state live and key K: the sharded address below R, or the single word;
  - state empty: NULL, no call;
  - otherwise: a call to `__bpftime_devhash_lookup` into a block-local register.
  - One `mov` at the join writes the program's register. Labels carry the per-function sequence
    number.
- **Constant-fd update, base != 0:** the call block becomes a call to `__bpftime_devhash_insert`.
- **Constant-fd delete:** -22 inline, counted in the stats.
- **Base 0, or a non-constant fd:** the trampoline call is kept, as for 1531 (D1).
- A malformed block throws, as for 1531 (the whole module is unprobed; README).

**Tests, text** (`attach/nv_attach_impl/test/test_hash_map_routing.cpp`, `[hash-map-routing]`, in
`bpftime_nv_attach_tests`):
- **Mirrors of the 1531 cases in `test_sum_map_routing.cpp`:**
  - :76 (exact block, `.global` add);
  - :135 (base 0 keeps the call);
  - :171/:242 (a register written elsewhere keeps the guarded atomic);
  - :185 (joined lookups);
  - :223 (unknown-origin atomics get the global branch);
  - :289 (only 64-bit adds take the fast path);
  - :514 (updates and deletes: rewritten here, kept at base 0);
  - :642 (a refused block throws);
  - the key-forwarding cases.
- **New:**
  - the program register has one definition, and its adds are `.global`;
    - mutation: the fallback writes the program register;
  - two inline lookups get distinct labels;
    - mutation: no sequence number;
  - the text is inserted once and before every function, and not at all when unused;
  - by-value is kept beside lat's 1530 lookups (the `[tl-byvalue]` :281 pattern).
- **The gate,** through `route_thread_local_maps_of_module` on an `nv_attach_impl`, as
  `test_ptx_failure.cpp` builds one: a map_basic_info whose only GPU-model map is 1533 is routed.
  - Mutation: 1533 not in the gate.
- **ptxas at sm_87** on a routed module with an inline lookup that falls back and a rewritten update.
  - Mutation: the text appended after the probes (prototype error).

**Tests, GPU** (`test_gpu_devhash_device.cpp`, child; the runtime test binary links
`bpftime_nv_attach_impl` and calls the emitter):
- The emitted inline block and call text run in S3's module over a seeded directory:
  - live at home, row < R and row >= R;
  - claiming, refused and opaque at home;
  - empty;
  - K = 0.
- Each thread's pointer equals `__bpftime_devhash_lookup`'s. The emitted hash equals `home_slot` for
  10^4 keys. The rewritten update returns -17, -7 and -22 as 64-bit negatives.
- Mutations: no state check; row >= R addressed as sharded; row bits shifted by one; the update's
  result zero-extended.

### S7. Tools and documentation

**Tools:**
- `pt_common.h` gets `MT_DEV_HASH 1533`.
- `pt_hash.bpf.c` gets a type switch for CNT, PRE and MISS, as `pt_lat` switches with PT_OLD/PT_MIX.
  It declares the companion.
- `pt_loader` dumps 1533 by key with get_next_key, and the companion as an array.

**Documentation:** `runtime/src/bpf_map/gpu/README.md`, new, in the format of
`attach/trap_uprobe_attach_impl/README.md`, for tool authors. It covers:
1. Declaring the map and the companion: the name rule, and that two maps are needed.
2. Return codes: -17, -7, -22 and -1. Only 1533 gives GPU programs errno values.
3. First touch gives EEXIST (counted). Unstable outcomes, and what each counter means.
4. The E2BIG/lookup window, as bpf's htab has it (decision 5). Visibility within one launch rests on
   the hardware.
5. The refused-slot limit and an undersized map. After a takeover, a full directory refuses at once.
6. Constant fds only. Base 0 and non-constant fds never reach the device map, also after a copy
   appears (unlike 1531).
7. Contracts: adds only, 64-bit; no CPU writes through lookup pointers.
8. Prefill before the agent's first CUDA link. The CPU phases and EPERM. A dump that crosses the copy
   may skip keys.
9. Takeover: same slots, renumbered rows; keys and adds after the last delivery are lost.
10. A writer killed inside an update disables the device copy for the life of the segment, and
    leaves its stripe locked if it died inside the table.
11. A malformed call block leaves the whole module unprobed.
12. Delivery: words are copied one at a time (a `bpf()` lookup can copy a value while the next
    delivery writes it, per word as 1531); companion words are independent counters.
13. A device fault in the hash snapshot stops the context.

A short CLAUDE.md section for the cross-cutting rules: the trampoline stays sm_61-legal, the layout
header is the only definition, the regeneration commands, and the D16 test conventions.

**Tests:** (no mutation: documentation.) The tools are exercised in S8.

### S8. Measurement and correctness against today (rules first, `S8-RULES.md`)

**Set-up:**
- jetson_clocks checked (cpu5 1728000, GPU 1020 MHz);
- the GPU lock;
- bounded runs;
- containers and shm cleaned.

**pt_victim, the hash-current matrix** (D and G sizes, CNT/PRE/MISS, three passes), with 1533
against today's host path:
- **counts:** exact for PRE; for CNT, delivered = expected - EEXIST - E2BIG, read from the companion;
  host-path outcomes counted as non-zero results (D6);
- **per operation:** by slope, as in hash-current.

**llama.cpp:**
- **cost on top of 1531:** tg128, mul_mat_vec_q, every thread, constant key, against the same tool
  on 1531.
  - Expected 0.034-0.037 ns per thread over 1531 (C2).
  - Rule: above 0.06 ns in both passes, compare the SASS with C2's `hh` form before reporting.
- **against today's host path:** rms_norm pp32, per thread and per block, against the hash-current
  rows.
- **the per-launch counter** on the four tg128 kernels (the l3/l4 shape).

**Delivery cost:** the hash snapshot's time per delivery and per run, from the per-map
ns_total/ns_max log lines, at max_entries 512 and 1024 with all rows live.
- Rule: a delivery above 2 ms (a tenth of the 20 ms busy period) is reported as a cost of the
  schedule.

**Re-checks on the final build:**
- S0 item 2 on the final `devhash_ptx.h`;
- S3's host text checks;
- every unit test of S1-S6.

## 3. Code-map risks and where each is handled

| risk (code map) | handled in | test |
|---|---|---|
| walks need an S bound | D4, S3 | S3.5, S2.8 |
| refused semantics at lookup, insert, wait | S3 (C5 port) | S3.1, S3.4 (ctrl 5/6) |
| loss counters hot spot | D5 | S3.1 (counts), S8 cost |
| layout copies disagree | S1, D1 fingerprint | S1, S3.11, S6 GPU |
| no device copy must not fall through | D1 | S3.10, S6 :135 mirror |
| new PTX forms, register and occupancy effect | S0.2 | S0.2, S8 re-check |
| return codes | D6 | S3.9, S6 GPU, README 2 |
| result register defined twice | S6 | S6 text mutation |
| malformed block throws | as 1531 | S6 :642 mirror, README 11 |
| no 1533 in the pass gate | S6 | S6 gate test |
| `.version` of original modules | S0.2 | S0.2 |
| mixed-size or non-add uses not refused | D13 | README 7 (no test: contract) |
| MapBasicInfo has one pointer | D3 | S5.1 |
| CPU-writable phase, then seal, writers in flight | D7, D8 | S2.3, S2.4, S2.5 |
| CPU walk misses slot-by-slot deliveries | D10 | S2.7 (c), (d) |
| header before value in the mirror | D10 | S2.7 (b) |
| owner takeover drops keys | D11 | S2.9, S5.3, S5.4 |
| copy made at the first CUDA link; prefill first | README 8 | S5.1 (EPERM after) |
| companion naming and reachability | D12 | S5.10 |
| pin list | D12, D16 | S5.9 |
| shared broken flag | D14 | S5.11 |
| staging or registration failure | D15 | S5.5 |
| second PTX entry compiled at first launch | D14 warm-up | S8 delivery cost (no unit test) |
| snapshot size (R x 256 shards) | | S8 delivery cost |
| `bpf()` copy races the next delivery | as 1531 | README 12 (no test) |
| trampoline sm_61 test target | D1 | S3.11 |
| direct run makes its own context | out of scope: not used by these tools; unverified for 1531 too | |
| without CUDA attach, 1533 is EINVAL | as 1531 | (no test) |

## 4. Not in stage one

- the store layout;
- GPU deletes;
- other flags;
- multi-word values;
- per-thread keys (1530);
- a fused lookup-or-insert;
- non-constant fds on the device path;
- the pid-namespace owner rule for 1531.

## 5. To change in DESIGN.md when the build lands

- **CPU view:** the CPU walks the delivered mirror, kept walkable by repair (D10), instead of its own
  index.
- **Adds through the returned pointer** keep 1531's `.global` form and rely on the `.relaxed .gpu`
  defaults (D13).
- **Takeover:** same slots, renumbered rows, opaque (D11).
- **Return codes** (D6); constant fds only (D1).

## 6. Review findings and where each is answered

(ids from `review-r1/plan-review.json`; c = coverage, b = buildability, p = cpu-concurrency,
d = device-and-routing)

- **Placement and ABI:** c01, b1, d01, d04 → D1 placement and signatures, S0.2, S6 ptxas test.
  d11 → D1 text.
- **Base 0:** c03, b15 → D1 (one rule: trampoline call kept), S6 :135.
- **Untested paths:**
  - c04 → S3.10;
  - c05, b14 → S6 gate test;
  - c06 → S3.11, S4 text check;
  - c11, d05 → S6 GPU test.
- **S0 and C6:**
  - c02, b4, d02 → S0.2 (functions called for real);
  - d03 → S0.1 (no fifth C6 variant).
- **The C5 port:**
  - c07, b11 → S3 test builds (outcome channel, wait-0 build, varied keys);
  - c08, d06 → D4 (opaque word, state first), S3.7;
  - c12, d08 → S3.2, S3.3, S5.7;
  - c16, d15 → D11 count rule, S3.6, S2.9;
  - c17 → D5 (deletes not counted on the device);
  - c28, d10 → S3.4 trial counts;
  - d09 → S3.1 varied keys;
  - d13 → S3 qualifiers, lookup returns generic;
  - d17, b19 → D16 children and deadlines.
- **The mirror:** c09, c10, p06 → S2.7 rules (c), (d) and defensive stagings. p10, d14 → D10
  resolution text. p11 (refuted) → D4 states the CPU rule anyway.
- **The seal:**
  - p01 → D8 orders and disassembly check;
  - p03, b09 → D9 lock-free scan, S2.5;
  - p04, c14, b08 → D8 dead writer, S2.4;
  - c13 → S2.3 hook.
- **Owner and phases:**
  - p02, c15 → D11 path by PHASE, S5.3;
  - p05 → D15;
  - p07 → D11 owner record, S5.2;
  - p12, b18 → D10 one writer at a time, S5 order (mirror before registration).
- **Process state:** c19, c20, b5, b6 → D16, S5.6.
- **Lifetimes:** c18, p08, b7 → D16, S5.9.
- **Step order:** c21, b2, b3 → section 2 order (S4 snapshot alone; S5 delivery with its tests; S3
  before S6).
- **Companion:** c22, p13, b16 → D12 (bind in the handler, uniqueness both ways), S5.10.
- **CPU-side wording:** p09 → D7, D12; p14 → D9, README 7; p15 → D7, README 8.
- **Measurement:** c23 → S8 delivery cost; d19 → D6, S8 counts.
- **Lists and tables:** c24, c27, c29 → section 3, README list, the S6 mirror list, "every test
  names how it can fail".
- **DESIGN.md:** c25, c26 → section 5, D13.
- **Build mechanics:** b12 → S3 generated headers checked in; b13 → test files and lists named;
  b17 → D14; b20 → fingerprint (S1, S3.11); d12 → D1 Makefile flags, S3.11 ptxas check.
- **Other:** d16, d07 (both refuted): no change. d18 → D4 reference fixed.
