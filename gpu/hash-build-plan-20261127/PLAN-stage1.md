# Device hash map, stage one: build plan (2026-11-27, revision 4)

Design: `results/gpu/hash-design-20261126/DESIGN.md` (updated 11-27, decisions 1-5).
Checks it rests on: `results/gpu/hash-round1-20261126`, `results/gpu/hash-round2-20261127`
(C5 protocol, C6 publish order, R2 review, `code-map-stage1.json`).
Base: `proto/map-model-70f7bc1f` at 7d02d677, worktree `/home/jetson/src/bpftime-proto-70f7`.
Nothing is built yet.

**Reviews.** Each review round sent the plan to independent reviewers; each finding then went to a
refuter.
- **Round 1** (revision 1, four lenses): 83 findings, 80 held (`review-r1/`).
- **Round 2** (revision 2: two checked that round 1 was answered, two read afresh): 53 findings,
  49 held (`review-r2/`). One was a blocker: the copy ran under an interprocess mutex that a dead
  process breaks for good.
- **Round 3** (revision 3: one checked round 2, one read afresh): 23 findings, all held
  (`review-r3/`). One was a blocker: on the late path, modules were patched before the copy existed.
- **This revision** answers round 3; section 8 maps each one. Section 7 keeps round 2's map.
- **Changed during the build (S2 code review, 11-27):** D8 (writer slots per process, dead
  processes discounted), D11c (liveness over all tasks, start time), D11 (the claim is given back
  on every path without a copy), D10 (repair only for newly seen keys). Section 9.

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

**Why.** The trampoline is prepended to every patched module and today uses no sm_70-only form
(`.relaxed/.acquire/.release`, `nanosleep`). 64-bit `atom`/`red` without `.sem`/`.scope` are
sm_61-legal and are not the limit. The insert needs the sm_70 forms; in the trampoline they would
make every patched module fail to compile on a GPU below sm_70, hash map or not.

1533 is the first map type whose sm_70 forms go into patched application modules: only modules of a
process with a 1533 map, and only where the pass rewrites a 1533 call. (1531's only sm_70 form is in
its snapshot module; its routed text is sm_61-legal.)

**The functions.** They live in `attach/nv_attach_impl/devhash/devhash.cu`. A new
`attach/nv_attach_impl/devhash/Makefile` compiles it:
- `CLANG ?= clang++-16`;
- the trampoline's device-only flags (`-x cuda --cuda-device-only -nocudainc -nocudalib`) at sm_70;
- targets `../devhash_ptx.h` and `../devhash_test_ptx.h` (both checked in, included as
  `attach/nv_attach_impl/devhash_ptx.h` and `attach/nv_attach_impl/devhash_test_ptx.h`);
- a strip rule (sed) that drops `.version`, `.target` and `.address_size` and turns `.visible .func`
  into `.func`.

There are exactly three `extern "C"` functions, with u64 parameters and a u64 return:
- `__bpftime_devhash_lookup(base, key, max_entries, shards)` returns a generic pointer
  (base + offset) or 0.
- `__bpftime_devhash_insert(base, key, value_ptr, flags, max_entries, shards)` returns a signed
  result in a u64, so the program's r0 holds a 64-bit -17.
- `__bpftime_devhash_layout_fp()` returns the layout fingerprint (S1).

Everything else in the file is forced inline. The test build keeps the same three signatures (S3).

**Placement.** The routing pass inserts the text once per module that needs it, at the start of the
module body: after the module's `.version`/`.target`/`.address_size` and before any function. A PTX
call needs a prototype first. Call sites use the prototype from that text.

**The trampoline.** Calls the pass does not rewrite go there: a non-constant fd, or no device copy at
patch time (base 0), as 1531 keeps its call there. The trampoline gets sm_61-legal 1533 branches
with no host call and no counting:
- lookup: NULL;
- update: -1;
- delete: -22.

For 1531 the trampoline serves those calls, and a copy made later is found through `map_info`. For
1533 they never reach the device map, even after a copy appears (README).

### D2. Hash

home(K) = ((u64)(K ^ SEED) * 0x9E3779B97F4A7C15) >> (64 - logS), as in C5. SEED is a fixed constant in
the layout header. Tests vary the key set per trial, since no seed varies the layout (S3).

### D3. Geometry from what MapBasicInfo already has

S = next_pow2(2 x max_entries), at least 2. R = min(max_entries, 512). shards = max_thread_count, as
for 1531. No new MapBasicInfo field.

### D4. Header states

Header word = key << 32 | row << 3 | state.
- **States:** 0 empty, 1 claiming, 2 live, 3 refused, 4 opaque (occupied, no key).
- **Opaque** is exactly the word 4 (key field 0, row 0). Only the host writes it: in the CPU mirror
  (gap repair, D10) and when seeding at owner takeover (D11).
- **The device walk decides on the state first:**
  - 0: stop;
  - 4: walk past;
  - 1, 2 or 3 with key field K: K's slot;
  - anything else: walk past.
- **Every header load in the walks and in the inline hit path** is `ld.relaxed.gpu.global` (strong).
  The precheck's count load is `ld.acquire.gpu.global`. D10's argument needs strong loads
  (PTX ISA 8.7, 8.9.2); a plain `ld.global` is weak.
- **The CPU walk** goes past every header that is not live K. It stops only at empty, at live K, or
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
- **Deletes never reach devhash code;** the pass counts rewritten delete call sites in its patch-time
  stats.

### D6. Return codes seen by GPU programs

- **Insert:**
  - 0: won;
  - -17: EEXIST, stable or unstable;
  - -7: E2BIG (refused, capacity, or full directory);
  - -22: flags other than BPF_NOEXIST.
- **Through the trampoline (D1):** update -1, delete -22.
- **Lookup:** a pointer or NULL.

GPU programs get kernel errno values only from 1533. A plain HASH map's GPU update through today's
host call returns the backend's raw -1. S8 therefore counts host-path outcomes as "non-zero".

### D7. Three CPU phases

PHASE is a word in the map object in shm.
- **0 CPU:** bpf hash semantics on a CPU table.
- **1 SEALING:** writes get EPERM; lookups still come from the CPU table.
- **2 DEVICE:** writes get EPERM; lookups and get_next_key come from the delivered mirror.

Every CPU read path loads PHASE with acquire first (checked by disassembly, S2.12).
- lookup_and_delete stays 524 (ENOTSUPP).
- A get_next_key dump that crosses the switch to phase 2 changes order and can skip keys (README).

### D8. The seal

**Writer slots.** INFLIGHT is not one word: the map object has 256 writer slots (each a count of a
process's writers in flight, on a 64-byte line of its own) and one overflow slot.
- **A slot is held by a kernel lock.** A writing process holds a slot by holding an open file
  description lock (`F_OFD_SETLK`, write lock, non-blocking) on that slot's byte of the shared
  segment's file.
  - The file is found from the mapping that holds the object (`/proc/self/maps`); there is no
    guessed path, because another file's locks are seen by nobody else.
  - It is opened `O_RDWR | O_CLOEXEC` at a number of 3 or more: a host started with its standard
    streams closed would otherwise write its output into the segment.
  - The bytes lie beyond the file's end, 4096 per map at 2^44 + an offset from the map's generation.
    Two live maps share bytes with a chance of about 2^-26 per pair, and a shared byte only makes a
    slot or the claim look held.
  - All of it is raw system calls with no allocation (it runs in probes, and the syscall server
    interposes `close`). A thread already inside these paths takes the overflow slot.
- **The kernel releases the lock** when the process exits, is killed, or execs, in any pid
  namespace, and not while any of its threads runs (results `s2/ofd/`, items 1-7).
- **The holder hint.** The lock is also released when a host closes the description while the
  process runs (`close_range`, item 8). That is not a death, and its writers keep counting in the
  slot. So the holder writes a hint into the slot once it holds the lock: its pid namespace << 32 |
  pid. A free lock whose hint names a process of this pid namespace that still runs and maps the
  segment's file (`/proc/<pid>/maps`) is treated as held: it is not taken and not discounted.
  - A zombie maps nothing.
  - A process that exec'd into an image without the segment maps nothing.
  - An unreadable maps file counts as alive.
  - Another namespace's hint counts as gone (its pid means nothing here).
- **Taking a slot.** At its first write, a process tries the slots from its pid's slot onwards and
  takes the first byte it can lock whose holder is gone. It writes its hint, then clears the count
  the gone holder left. A slot whose hint is its own keeps its count: the process lost its
  description, and its writers in flight count there.
- **A description the host closed** is noticed on the slow paths (the owner's check before each
  delivery, a later claim, a new map). The description must still hold the slot's or claim's lock,
  read from `/proc/self/fdinfo/<fd>`; matching the file alone is not enough, because the number may
  now name another description of the same file. The process then opens a new description and
  takes its slot and claim back at the same bytes. The hint keeps others off them meanwhile.
- **All slots held by live processes:** it writes in the overflow slot, and tries again for a slot
  of its own after 100 ms. The decision is cached per process, so a write never runs the scan.
- **Fork children** close the inherited description in a fork handler (`register_fork_handlers`).
  The child's copy shares the parent's lock and would keep a dead parent's slot held (item 5b). The
  child takes a slot of its own at its first write.
  - The handler closes a number only if it still names a description of the segment's file.
  - It also closes the copy of a seal's probe description.
  - A vfork-style child (shared memory, no handler) is told apart by its pid. It neither records
    descriptions in the shared table nor claims; it writes in the parent's slot or the overflow
    slot.

**Writer** (CPU update or delete, any process):
1. its slot's `count.fetch_add(1, seq_cst)`;
2. `PHASE.load(seq_cst)`: not 0 → `fetch_sub(1, release)`, EPERM;
3. otherwise the table operation, then `count.fetch_sub(1, release)`.

**Sealer:** `PHASE.store(1, seq_cst)`, then spin-polls every slot's count (seq_cst loads) until all
are 0, with `sched_yield` every 4,096 polls and one last poll after a 1 s deadline.

All four operations are in the single total order, so either the writer reads PHASE 1 or the sealer
reads its increment. An acquire load on either side would not do: on an RCpc target it becomes
`LDAPR`, which may pass the earlier `STLR` or `LDADDAL`. A poll that reads 0 must also at least
acquire, to see the writers' table writes before the image's scan. S2.12 checks all of it by
disassembly with an RCpc target.

**Dead writers are discounted.** An exit or an exec kills a process's other threads wherever they
are, so either can leave a count raised (2-23 of 40 exits of a multi-threaded writer did, with one
shared counter).
- While it polls, the sealer tries to lock the byte of every slot whose count is above 0. It uses a
  description of its own, which conflicts with its own process's writer lock as well (item 2), so
  no slot is skipped as "self".
- Taking the lock means the holder is gone, unless its hint names a process that still runs and
  maps the segment (above). For a gone holder the sealer clears the count, unlocks, and goes on.
- A sealer killed in between leaves the lock to the kernel and the count to the next seal.
- The overflow slot is never discounted.
- A process that died inside the table's own operation leaves its stripe locked, as documented for
  HASH; the image's lock-free scan does not need that lock.

**Limits.**
- On the alive side (the copy is delayed, never wrong; README):
  - a writer in the overflow slot that dies in flight;
  - a vfork-style child, which shares its parent's description, killed in flight while the parent
    lives;
  - no segment file to lock: the process writes in the overflow slot.
- On the alive side too:
  - a holder in this namespace whose lock a host released and whose slot keeps a count it will
    never drop (it is held as alive);
  - children made without `fork` (`clone`, `clone3`) keep their parent's descriptions, and with
    them its slot and claim, while they run.
- On the dead side: a writer of another pid namespace whose description a host closed while it
  writes: its hint counts as gone.

**A live writer stuck for 1 s:** `SEAL_DEAD = 1` in the map object.
- On timeout: PHASE goes back to 0, `SEAL_DEAD` is stored, the attempt's block is freed, the claim
  is given back, and 0 is returned.
- A later attempt (any process) that sees `SEAL_DEAD` discounts dead writers and reads the counts
  once:
  - 0: the stalled writer has finished (or died), so it clears `SEAL_DEAD` and seals normally;
  - above 0: it returns 0 at once, with no 1 s wait and no EPERM window.
- While the flag stands, the map has no device copy: GPU lookups give NULL and inserts -1 through the
  trampoline; the CPU stays in phase 0 (README).
- A writer in the overflow slot cannot be discounted: if it dies in flight, the flag stands until
  the segment is recreated (README).

**No seal or copy runs under `map_binding_mutex`** (D11a). The 1 s wait therefore blocks only the
sealing process's own attach.

### D9. The CPU table before the copy is the existing `fix_size_hash_map_impl`

This is BPF_MAP_TYPE_HASH's table, sized max_entries; its bpf semantics are already tested.

**The image comes from a lock-free scan.**
- `bpftime_hash_map::for_each_in_use_unlocked` scans all elements, `max_entries + num_stripes`, the
  stripes' spare elements included. An update of an existing key moves the key into a spare, so live
  keys can sit at indices >= max_entries.
- For each element it reads the word (acquire), then the key and the value.
- It runs only with PHASE >= 1 and INFLIGHT 0, so no writer runs, and it takes no stripe lock: a tool
  killed inside `next_key` cannot hang the seal.
- `fix_size_hash_map_impl::for_each_in_use_unlocked` forwards to it, because the table is a private
  member.

**Placement:** scan order, rows 0..n-1, linear probing with the shared hash, the value in shard 0 of
its row (or its single word), count = n.

**Pointers.** Pointers from phase-0 lookups stay valid: the table is frozen and lives as long as the
map. CPU programs must not write through 1533 lookup pointers (README).

### D10. The mirror is its own index (replaces DESIGN.md's "own index")

**Layout:** shm arrays `mhdr[S]` (u64, atomic) and `mval[S]` (u64), indexed by slot.

**CPU lookup** (phase 2) walks from home with acquire loads, by D4's CPU rule, and returns `&mval[j]`
only for live K.

**One writer at a time:**
- the seeding thread, before the map is registered for delivery;
- then deliveries, under `run_mutex`.

**The invariant.** The slot-by-slot snapshot can deliver slot i empty while K is live further on. So
the writer keeps "every non-empty slot's probe path from its home is non-empty" true before it
publishes. Each delivery:
1. **Guard, then repair.**
   - **Guard:** where the mirror slot is non-empty and the staging is empty, the staging takes the
     mirror's header and value. A live mirror header is replaced only by a live header of the same
     key: a takeover may change its row. Device facts allow nothing else: slot keys never change,
     live stays live, and only a takeover turns claiming or refused into opaque.
   - **Repair:** for each non-empty staged slot j with a key K, mark every empty staged slot from
     home(K) to j as opaque.
   - In the other order, a staging that shows live K1's slot empty while that slot lies on K2's path
     would have the repair write opaque over live K1.
2. **Pass A:** changed values (relaxed).
3. **Pass B:** opaque into every slot that is empty in the mirror and non-empty in staging (relaxed).
4. Release fence.
5. **Pass C:** each changed header (release), in ascending slot order.
6. The status words, then the release increment of DELIVERIES.

A reader that acquires a header from pass C sees its value (pass A) and every slot made non-empty by
this delivery (pass B), whatever the slot order.

**When an opaque slot gets its real header** (PTX ISA 9.5 8.7, 8.9.2, 8.9.4, 8.9.5, 8.10.6):
- **Repair caused by a live or refused header:** the next delivery. The walker's strong read of the
  slot found it non-empty before its `st.release`. The snapshot that acquired the header
  synchronizes with that release, and the next snapshot is stream-ordered after it.
- **Repair caused by a claiming header** (a relaxed CAS): resolved only after K is published. It is
  permanent if the last delivery or the owner's death comes first (D11).

### D11. Owner, phases and takeover

**D11a. Where the work runs: a pre-pass before any CUDA link.**

`init_attach_ctx_from_handlers` holds `map_binding_mutex`, a robust interprocess mutex, across its
handler loops. A process that dies holding it breaks it for good: Boost throws on every later lock.
On the late path, each CUDA link's `create_attach_with_ebpf_callback` also patches loaded modules
right away, inside that loop. So every 1533 copy has to exist before the loop, and must be made
outside the mutex.

**`prepare_device_hash_copies()`** runs at the start of `init_attach_ctx_from_handlers`, inside
`ctx_mutex` and before the binding guard is taken.
- **It acts only when the init will instantiate a CUDA link:**
  - the ctx has a CUDA context (made in the `bpf_attach_ctx` constructor, so it exists at the first
    init);
  - it handles nv attach: an attach implementation is registered for the CUDA attach types (8 and
    9; the agent registers nv_attach_impl, so does the direct run);
  - the scan finds a link handler whose program is a CUDA program (`is_cuda()`).
- **Why the gate:** the seal happens at the same point as 1531's copy, the first CUDA link. An agent
  with a GPU but no CUDA link (a CPU-only traced process on the same segment) never claims or seals
  the map, so a tool's prefill made after it is not refused.
- **Not the direct run:** `bpftimetool cuda` sets a flag. There 1533 calls go to the trampoline with
  base 0, and the map is not claimed or sealed.
1. **Under the guard,** quick and with no waits:
   - if the scan finds no CUDA link, stop here;
   - find the 1533 handlers that have no copy in this process;
   - pin each one, recording its process-local impl pointer;
   - bind and pin its companion (D12);
   - release the guard.
2. **Without the guard,** `make_device_hash_copies()` runs for each recorded impl pointer, under a
   process-wide mutex held from the owner claim to the cache insert (as 1531 holds its own):
   1. the claim;
   2. `choose_path()`;
   3. seal and image, or the takeover image;
   4. the copy;
   5. registration;
   6. PHASE 2;
   7. the cache insert.
3. Then the normal transaction runs. In its CUDA-link loop, `get_gpu_map_extra_buffer` and
   `get_gpu_map_max_thread_count` return the cached base and shard count for 1533, as 1531's
   `get_gpu_mem_buffer` returns its cached copy. Every later link (auto-refresh, a second tool, an
   epoch retry) gets the same base, so nothing ever overwrites a base with 0.

**Consequences:**
- No `map_basic_info` is changed after a link exists, so application threads that read it under
  `load_substitution_mutex` see only finished entries.
- A 1533 map created between the pre-pass and the loop gets base 0 at this init, and its copy at the
  next init.
- The copy cache is keyed by the impl pointer plus a per-map generation word, stored in the shm
  object when it is created. A new session's object at an old address does not hit an old entry.
- 1531 keeps its copy under the mutex, as today. That is an existing hazard, not widened here.

**D11b. The owner claim is the owner byte's lock:** an OFD write lock, non-blocking, on the byte
after the writer slots' (D8), through the process's description of the segment file.
- Taking it is the claim. Cross-process exclusion rests on the kernel alone, with no CAS, no
  record and start time to pair, and no window between them.
- The owner writes its hint (D8) after taking the lock. A release clears the hint before unlocking.
- The OWNER word records the owner's pid, for the status array and logs.
- An owner's fork child closes the inherited description (D8), so it never shares the claim.

**D11c. Liveness is the lock, guarded by the hint.** The kernel releases the lock when the owner
exits, is killed or execs, in any pid namespace.
- A claimant that takes a free lock whose hint names another process that still runs and maps the
  segment gives the lock back: that owner lost its description to a host and still delivers.
  Return 0 (path `not_owner`).
- The hint may be the claimant's own pid: its new image after an exec, or itself after losing its
  description. Then it takes the claim.
- Before each delivery the owner calls `still_holds_claim()`. Its description must hold the claim's
  lock (`/proc/self/fdinfo`). If not, it opens a new description and takes the lock back; no one
  else can have taken it while it runs. Only an owner of another pid namespace whose description a
  host closed can be taken over while it runs (README).

**D11d. Claims that are not taken.**
- The lock is held by another description: another live owner. Return 0 (path `not_owner`).
- A process with a cached copy holds the lock already. Its description takes it again
  successfully, and `choose_path()` gives `have_copy`. With a cached copy, the pre-pass skips the
  map anyway.
- No segment file to lock: no claim, logged. The map then has no device copy in that process.
- Every path that ends without a copy gives the claim back (unlocks): a seal that timed out, a failed
  first copy or takeover (D15), and an exception between the seal and the copy. `make_image()` does
  it for the host part. An exception after the seal also unseals (PHASE 0).

**D11e. Under ownership, PHASE chooses the path.**
- **`SEAL_DEAD` set:** by D8. The counts read 0 after discounting dead writers: clear the flag and
  continue by PHASE. Otherwise give the claim back and return 0.
- **0, first copy:** seal (D8), image (D9), copy, mirror seeded, registration, PHASE 2.
- **1, a sealer died:**
  - writers already get EPERM;
  - reset the mirror to empty;
  - poll the counts as in D8, because a writer that read PHASE 0 before the dead sealer's store may
    still run;
  - on timeout, do as D8 (PHASE 0, `SEAL_DEAD`, give the claim back, return 0);
  - otherwise build the image and continue as the first copy.
- **2, takeover:** seed a fresh block from the mirror.
  - Live slots stay live at the same slot, with rows renumbered 0..L-1 in slot order and the
    delivered value in shard 0.
  - Every other non-empty slot becomes opaque.
  - count = max(L, max_entries - min(E, max_entries)), a saturating subtraction, where E is the number
    of empty slots in the seeded directory.
    - Each new key takes one empty slot and adds 1 to the count. So with count = L the directory
      could fill while count < max_entries, and every later insert of a new key would wait out the
      full-directory bound.
    - Rows L..count-1 stay unused, and companion word 13 then shows reserved capacity, not live keys.
  - The loss counters are seeded with the delivered totals.
  - The takeover does not write the mirror: its deliveries do.
- **Consequences:**
  - a claim that died in flight holds no capacity;
  - a key once refused can be inserted again if there is room;
  - keys and adds after the dead owner's last delivery are lost (README).

### D12. Companion status array (decision 3)

**Declaration.** The tool declares it in its BPF object: an ARRAY, key 4, value 8, at least 16
entries, named the hash map's name cut to 12 bytes plus `_st`.

**Binding** happens in the pre-pass (D11a step 1), under the guard, which reads the handlers' names
and does the pinning.
- It needs exactly one ARRAY candidate, and exactly one 1533 map that derives that name. Otherwise
  none binds, logged once.
- The shm object stores the companion's handler index, the same in every process. **Only the
  process whose copy is made stores it** (or clears it), after the copy: the pre-pass chooses and
  pins under the guard before it knows whether this process becomes the owner, and another agent's
  choice must not change the link (S5 review: with two candidates, one closed later, a non-owner
  would have bound the one left).
- The binding process records the pinned companion impl pointer, process-local.
- A process that resolves the stored index later (a takeover) first checks that slot: an ARRAY, with
  the derived name, key 4, value 8 and at least 16 entries. Otherwise it unbinds (once its copy is
  made) and logs, so a closed and reused slot is never written.
- Only the owner's delivery writes the words.

**Words:**

| word | content |
|---|---|
| 0-5 | as 1531's status words |
| 6-12 | the D5 counters |
| 13 | count |
| 14 | live slots |
| 15 | refused slots |

Each word is an independent counter, written relaxed, not ordered against the others or the hash
(README). Note for the user: decision 3 means each hash tool declares two maps.

### D13. Contracts, as for 1531

- **Adds only, 64-bit only, through GPU pointers.** Other atomics through an inline 1533 result are
  rewritten to the guarded form, not refused. Pointers a function returned are not followed.
- **The program's adds keep 1531's `.global` form** and rely on the `.relaxed` `.gpu` defaults.
  Inside the devhash functions every qualifier is written out.

### D14. The hash snapshot is isolated for set-up, load and launch errors

- It has its own CUmodule and CUfunction, its own warm-up and its own broken flag.
- A device fault inside it is sticky for the whole context and stops everything (README).

### D15. Any failure after the seal rolls back

- **What counts:** the copy, the synchronize, and registration (which now returns a result).
- **The cache:** the copy goes into the process cache only after everything succeeds.
- **On failure:**
  - free the block;
  - on a first copy, reset the mirror to empty, then PHASE 0;
  - on a takeover, keep PHASE 2 and the mirror;
  - give the claim back (unlock the owner byte, clear the hint; D11b);
  - return 0.
- **The copy runs as 1531's zeroing does:**
  - in relaxed stream-capture mode;
  - on a stream of the agent's own, created `CU_STREAM_NON_BLOCKING`;
  - its synchronize under `agent_sync_scope`, so that it is not counted as an application sync and
    does not wait behind application kernels.

### D16. Test conventions

- **Children.** A GPU test that takes ownership, registers a delivery, or can hang runs in its own
  exec'd child of the test binary (a hidden tag, as in `test_cuda_no_device.cpp:93-104`). So does a
  CPU test that can hang. The parent kills a child at a total deadline and counts that as its result,
  which is the expected one for hang mutations. Afterwards it checks that no child holds the GPU, and
  it reaps a killed owner before starting the next child.
- **Delivery and syncs.**
  - Tests that need deliveries call `attach::launch_activity().note_app_sync()` after each
    synchronize. Children set `BPFTIME_GPU_DELIVERY_SYNC_GAP_MS=0`.
  - n is `launch_activity().app_sync_count()`, read after the last note. It counts hooked syncs
    too.
  - A test waits, bounded at 1 s, until SYNCS_COVERED >= n. It then waits for one more DELIVERIES
    increment (acquire) before reading the mirror or the companion, so it has acquired a delivery
    that covered n. Which thread delivered does not matter.
  - The AT_SYNC child checks right after `note_app_sync` returns, with no `deliver_now`.
  - `deliver_now(true)` is used only for a final delivery.
- **Mutex depth.** `map_binding_guard` keeps a thread-local depth counter, which the "mutex not held"
  check reads. The mutex is recursive, so a same-thread `try_lock` cannot tell.
- **Lifetimes** are checked through refcounts and a destroy counter, not ASan.
- **Test hooks** follow the repo's always-compiled `*_for_test` pattern: setters, off (null) by
  default, named in each step, with no build option.
- **GPU harness.** `runtime/unit-test/cuda/cuda_test_support.hpp` (new, S3) holds `test_module`
  (taking a PTX string), `counting_host` with its `scratch_segment`, `agent_segment`,
  `use_primary_context`, `device_sm_arch` and `device_buffer`, moved from `test_cuda_host_call.cpp`'s
  anonymous namespace.
- **Test files** and the CMake lists they join are named in each step.

## 2. Order

S0.1 → S1 → S2 → S3 code (first draft) → S0.2 → S3 tests → S4 and S6 (independent of each other)
→ S5 → S7 → S8.

Each step builds and passes its tests before any step that depends on it. S0.2's decision comes
before S6.

## 3. Steps

### S0. Compile checks (rules first, `S0-RULES.md`)

**S0.1. The hash snapshot module** (`.version 7.0 .target sm_70`) with `ld.acquire.gpu.global.b64`:
ptxas for sm_87, and record the SASS.
- (no mutation: a record, no decision.)
- The acquire rests on PTX ISA 8.8 case 1 and 8.9.4. C6 cannot tell `ld.acquire` from no acquire on
  this device (its variant C gave 0 weak).

**S0.2. The devhash functions called for real.** It runs after S1 and S3's first draft.
- Hand-route the hash tool's probe on mul_mat_vec_q, the way round one's C2 `hcall` form was made:
  - the inline hit path;
  - the `__bpftime_devhash_lookup` fallback;
  - a `__bpftime_devhash_insert` call;
  - all placed by D1's rule.
- ptxas at sm_87. Compare registers, stack and warps per SM of every probed kernel against the same
  tool on 1531. Record the SASS forms in the called functions.
- **Rule:** a probed kernel that loses warps per SM is reported and decided with the user before S6.
- **Timing.** Time this hand-routed form on tg128 against 1531 (two passes) with C2's harness. This
  sets S8's expected cost; C2 measured `hh` (an inline walk with no call), not this form.
  - Rules first, in `S0-RULES.md`.
  - jetson_clocks checked (cpu5 1728000, GPU 1020 MHz), the GPU lock, bounded runs.
- Mutation: the functions placed after the probes, so ptxas fails with the prototype error.
- Run once more on the final `devhash_ptx.h` in S8.

### S1. One layout header (`attach/nv_attach_impl/trampoline/gpu_devhash_layout.h`)

**Contents:**
- the type constant;
- the states and the opaque word;
- header encode and decode;
- SEED and `home_slot`;
- geometry: S, logS, R, region offsets;
- the block layout:
  - control words (magic, max_entries, S, R, shards, count, reserved; the test build's diagnostic
    counters and outcome-array address use reserved words), 256 B;
  - counters [7][shards];
  - directory [S];
  - rows [R][shards];
  - single rows [max_entries - R];
  - 256-byte aligned regions;
- **the staging layout** of the snapshot (per slot: raw header and value; then the summed counters
  and the count), shared by S2's `apply_delivery` and S4's snapshot;
- `DEVHASH_LAYOUT_FP`: a constexpr hash of every constant and of the layout-offsets function's results
  (region offsets and total size) at S1's max_entries list, for shards 256 and 512;
- `static_assert`s;
- `DEVHASH_HD`: `__attribute__((host, device))` when compiled as CUDA (`__CUDA__`), empty otherwise,
  so the helpers build under `-nocudainc`.

**Tests:** `attach/nv_attach_impl/test/test_devhash_layout.cpp`, `[devhash-layout]`, in
`bpftime_nv_attach_tests` (TEST_SOURCES of `attach/nv_attach_impl/test/CMakeLists.txt`).
1. **Offsets and sizes** for max_entries 1, 2, 48, 512, 513, 1024 and 2^22: R; S a power of two and
   at least 2 x max_entries; no overlap; total size; rows cover max_entries; R x shards x 8 fits
   32-bit offsets.
   - Mutation: R = 513.
2. **`home_slot`** equals C5's for 10^4 keys at logS 1..23.
   - Mutation: shift by 63 - logS.
3. **Header and staging round trip;** opaque decodes as state 4 with no key.
   - Mutation: row field 30 bits.
4. **The fingerprint** equals a value the test computes itself, by its own hash, from the
   layout-offsets function at the sample geometries and the constants.
   - Mutation: a fingerprint of the constants only (then it differs, whatever the constants are).

### S2. Runtime type and CPU object, no CUDA (`runtime/`)

**Code:**
- Enum 1533 in `bpftime_shm.hpp`, outside `gpu_map_type_without_cuda_ipc`.
- `check_map_attr`: key 4, value 8, 1 <= max_entries <= 2^22, and the CPU part fits.
- `map_init`/`map_free` and the lookup, update, delete and next_key switches in `map_handler.cpp`,
  all under `BPFTIME_ENABLE_CUDA_ATTACH`.
- **Class `nv_gpu_device_hash_map_impl`,** in new files
  `runtime/src/bpf_map/gpu/nv_gpu_device_hash_map.{hpp,cpp}`, added to the CUDA-only source list in
  `runtime/CMakeLists.txt` beside `nv_gpu_model_maps.cpp`. It holds:
  - the phase-0 `fix_size_hash_map_impl`;
  - PHASE, the writer slots' counts, `SEAL_DEAD` and the owner's pid;
  - the mirror;
  - its status words (D12 0-15);
  - the companion's handler index.
- **`bpftime_hash_map::for_each_in_use_unlocked`,** plus its forwarder on `fix_size_hash_map_impl`
  (D9).
- **Host-only functions:**
  - `seal()` (D8);
  - `build_image()` (D9);
  - `apply_delivery(staging)` (D10, S1's staging layout);
  - `takeover_image()` (D11e);
  - `choose_path(phase, got_lock, has_cached_copy)` (D11);
  - the segment file's description and the OFD locks of the writer slots and the owner (D8,
    D11b), closed in fork children by a fork handler.
- **Test hooks** (`*_for_test`):
  - park a writer after its PHASE load;
  - park a writer after its INFLIGHT increment;
  - park `apply_delivery` right after its pass-C store of a slot j that the test names;
  - park it after pass C and before pass A, for the values-after-headers mutant;
  - a limit on the usable writer slots, a scan counter, and a throwing point in `build_image`
    (S2.14).

**Tests:** `runtime/unit-test/maps/test_gpu_device_hash.cpp`, `[gpu-devhash-cpu]`, in TEST_SOURCES of
`runtime/unit-test/CMakeLists.txt`, guarded by `BPFTIME_ENABLE_CUDA_ATTACH` as
`test_gpu_map_without_ipc.cpp` is.

1. **Create checks:** wrong sizes EINVAL, too many entries E2BIG.
   - Mutation: value 16 accepted.
2. **Phase 0:** NOEXIST, EXIST, ANY, E2BIG, ENOENT and delete behave as on a HASH map given the same
   calls.
   - Mutation: phase 0 routed to the mirror.
3. **Seal race.**
   - A sealer thread runs `seal()` and then `build_image()`.
   - A writer is parked after its PHASE load (it read 0). The sealer then stores PHASE 1.
   - The test releases the writer when the sealer thread has finished, or after 50 ms, whichever
     comes first.
   - Correct code: the sealer is still polling at 50 ms, the writer completes, and its key is in the
     image.
   - Mutation, the writer skips INFLIGHT: the image is built within 50 ms and the key is missing
     with no EPERM, every time.
   - 8 free-running writers also run; every key is in the image or got EPERM.
4. **Dead and stopped writers** (as built after the code review):
   - **Killed in flight** (a forked writer SIGKILLed at the hook after its increment; reaped): the
     seal discounts it and succeeds; its key is not in the image; the counts read 0.
     - Mutation: no discount, so the seal fails.
   - **Killed by an ordinary exit** of a multi-threaded process (writer threads loop, the process
     exits after 5 ms; repeated until an exit has left a count raised): the seal succeeds.
   - **Stopped in flight** (SIGSTOP after the increment): `make_image` times out, sets `SEAL_DEAD`,
     gives the claim back (OWNER 0); another process's attempt returns at once; after SIGCONT the
     writer finishes and the next attempt clears `SEAL_DEAD` and seals.
     - Mutations: `SEAL_DEAD` not stored; `SEAL_DEAD` unconditional; the claim kept after a failed
       seal.
5. **Stripe held** (in a forked child with a deadline): the stripe of a prefilled key k
   (`stripe_of_key(k)`) is held with `lock_stripe_for_test` during the seal. The seal completes.
   - Mutation: the image built with `next_key`, so the child is killed at its deadline.
6. **Image.**
   - Prefill with inserts, then `BPF_ANY` and `BPF_EXIST` updates of existing keys, then some
     deletes, so live keys sit at spare indices and between free elements.
   - The image holds exactly the live keys: rows 0..n-1, values in shard 0 or the single word,
     count n.
   - Every key is found by a host walk; one key in two slots fails.
   - Mutation: the scan stops at max_entries, so updated keys are missing.
7. **Mirror.** A writer thread applies synthetic stagings while reader threads run.
   - **Stagings** come from a host model of the device directory, with:
     - a live K with a staged-empty slot on its probe path, both mid-directory and across S-1 (the
       slot read before its header was stored);
     - a claiming-caused repair whose K turns live while the repaired slot is still staged empty;
     - K = 0;
     - opaque on probe paths;
     - claiming-caused repairs that persist;
     - defensive stagings that show a keyed slot empty again (K's own slot, or every keyed slot
       whose path covers a repaired slot).
   - **Rules:**
     - (a) a key once found live is found on every later lookup;
     - (b) a found value is never below the value staged with that header;
     - (c) the writer bumps a release marker after each `apply_delivery`; a reader that acquires
       marker >= N finds every key live in staging N;
     - (d) every key that get_next_key returns is found by an immediate lookup.
   - **Deterministic windows:**
     - A wraparound staging puts a new live K at a low slot j, with its home near S-1.
     - Its delivery is parked right after the pass-C store of slot j. Pass C is ascending and the gap
       slots are near S-1, so they come later.
     - While it is parked, a reader runs get_next_key and then lookup (d).
     - The values-after-headers mutant is parked between pass C and pass A while a reader checks (b).
   - **Mutations:**
     - no repair, caught by (c);
     - no pass B, caught by (d) while parked;
     - values after headers, caught by (b) while parked;
     - a slot allowed back to empty (no guard), caught by (a) with the keyed-slot defensive
       stagings;
     - repair before guard, caught by (a) with a keyed-slot staging whose slot lies on another key's
       path.
   - The keyed-slot stagings come in both placements: on another key's path and off it.
   - Run once more with the readers in a forked process on the same segment.
8. **get_next_key:** live keys only, in slot order; NULL or an unknown key gives the first live
   slot; it ends within S steps on a full directory.
   - Mutation: refused returned as live.
9. **Takeover image:** live slots keep their slots with rows 0..L-1; the rest is opaque; the
   counters are seeded.
   - count = max(L, max_entries - min(E, max_entries)):
     - a mirror with E = 0 seeds max_entries;
     - a mirror with E = 1 and L = max_entries - 2 seeds max_entries - 1;
     - a mirror with E > max_entries seeds L.
   - Then a host model inserts new keys until the directory is full. Every insert after that is an
     immediate E2BIG, never a full-directory wait.
   - Mutations: count = L; an unsigned subtraction without the min.
   - Each live row equals its slot-order rank.
     - Mutation: rows not renumbered (the mirror has a row gap from a dead claim).
10. **choose_path(phase, got_lock, has_cached_copy):** all PHASE values, lock taken or not, with and
    without a cached copy.
    - Mutation: a cached copy treated as a takeover.
11. **The PHASE-1 recovery.**
    - A writer parked after its PHASE load reads 0. A sealer stores PHASE 1 and is abandoned (a
      simulated death).
    - The PHASE-1 path then runs on its own thread. The test releases the writer when that thread
      has finished, or after 50 ms, whichever comes first. It asserts that the thread had not
      finished by then, and that the writer's key is in the image.
    - A second case: a writer stopped in flight (alive). The PHASE-1 path times out to PHASE 0 and
      `SEAL_DEAD`, gives the claim back, and after the writer resumes CPU writes work.
    - Mutation: the PHASE-1 path skips the poll.
12. **Disassembly check,** `runtime/unit-test/maps/check_devhash_orders.sh`, run in S2 and S8. It
    compiles `nv_gpu_device_hash_map.cpp` with `-mcpu=cortex-a78ae` (RCpc) and checks:
    - every count load after the sealer's PHASE `STLR` is `LDAR`, and so is every load of its later
      polls;
    - the writer's PHASE load after its count's `LDADDAL` is `LDAR` (the last memory load: a test
      hook may come between);
    - the readers' PHASE load is `LDAR` or `LDAPR`, not `LDR`;
    - elem_update and elem_delete call the writer's helper, elem_lookup and map_get_next_key the
      reader's.
    - Mutations: acquire sealer loads; relaxed polls; an acquire writer load; a relaxed reader load;
      a lookup that loads PHASE itself.
13. **Phase 1 lookups** come from the table.
    - Mutation: phase-1 lookups sent to the mirror.
14. **Writer slots and the owner lock** (D8, D11b-d; each case in forked processes with a deadline):
    - **exec in flight:** a writer process with four updating threads is parked in flight, and its
      main thread execs `/bin/sleep`. The seal discounts the slot and succeeds while the exec'd image
      runs.
      - Mutation: the description opened without `O_CLOEXEC`, so the seal fails.
    - **Fork child:** a writer forks an idle child and is then killed in flight. The seal discounts
      the parent's slot while the child lives.
      - Mutation: no fork handler, so the seal fails.
    - **Slot reuse:** a slot left with a count by a killed writer is taken by the next writing
      process, and the seal then succeeds with that process idle.
      - Mutation: the leftover count not cleared.
    - **Own process in flight:** a thread of the sealing process is parked in flight. It is not
      discounted, and its key is in the image once released.
      - Mutation: the sealer probes through the writer's own description, so the key is missing.
    - **Slots exhausted** (a test limit of 4 usable slots): four live writer processes hold them. A
      fifth writes in the overflow slot and scans at most once per 100 ms (a scan counter).
      - Mutation: no cache, so the fifth process scans on every update.
    - **Owner:** two processes claim at once, 200 times, and exactly one gets the lock each time.
      Then:
      - an owner killed, or exec'd into `/bin/sleep`, is taken over while the new image runs;
      - an owner's fork child does not hold the claim;
      - an owner whose descriptor was closed (`close_range`) stops delivering at its next delivery
        (the `fstat` check).
      - Mutations: a claim that always succeeds (two owners); no `fstat` check.
    - **An exception between the seal and the copy** (a throwing hook in `build_image`): PHASE is 0
      and the claim is free afterwards.
      - Mutation: no catch.

### S3. Device functions, test harness and trampoline branches

**Code:**
- **`devhash.cu`:** a port of `c5.cu`'s v2 bodies, not of the code map's prose. It keeps:
  - the precheck with the slot re-read;
  - refused as an outcome of the wait;
  - the full-directory wait;
  - the S-step bound on every walk;
  - the wait bound: 64 ns doubling to 2,048, 100,000 ns requested.
- **Changes from C5:**
  - D4's state-first walk and strong header loads;
  - sharded counters (D5);
  - every qualifier written in full with `.global` on `cvta.to.global` addresses;
  - the lookup returns base + offset.
- **The test build** (`BPFTIME_DEVHASH_TEST`) keeps the three signatures.
  - It writes each call's outcome (C5's OC codes) into a per-thread array whose address the harness
    puts in a reserved control word.
  - It adds C5's diagnostic counters (key changed, row overflow, refused published; next row and
    give-back for control 6) in reserved control words.
  - It adds `BPFTIME_DEVHASH_WAIT_BOUND` and `BPFTIME_DEVHASH_FAULT` 1/2/5/6.
- **`devhash_test_ptx.h`** holds whole modules: C5's harness kernels (`k_ops`, `k_reader`, ported to
  call the three functions) compiled together with the test build. There is one module each for the
  test build, the four faults and wait 0.
- **`runtime/unit-test/cuda/cuda_test_support.hpp`** (D16).
- **The trampoline** gets only the D1 branches. `trampoline_ptx.h` is regenerated.

**Tests.** GPU cases go in `runtime/unit-test/cuda/test_gpu_devhash_device.cpp`, `[gpu-devhash-device]`,
added to the CUDA list in `runtime/unit-test/CMakeLists.txt`; each runs in a child (D16). Which text
each case loads:
- S3.1-S3.4 load `devhash_test_ptx.h`;
- S3.5-S3.9 load the shipped `devhash_ptx.h` with hand-written test kernels after it;
- S3.10 loads the trampoline.

1. **C5 runs a, b, c, e, g (wait 0) and h,** with C5's checker ported.
   - The key set is varied per trial: an odd multiplier and offset in the harness's key function, for
     the controls too.
   - Trials: 1 + 50 for a, b, c, g, h; 1 + 5 for e.
   - Rules as in `RULES-round2.md`.
   - (no mutation: S3.4's controls.)
2. **Run d seeded** with S2's `build_image()`.
   - Mutation: `build_image()` leaves count 0.
3. **Run d from `takeover_image()`.**
   - The mirror it starts from has a dead claim that held a low row, so the live rows have a gap.
   - Opaque on the probe paths, renumbered rows, count by D11e.
   - Per-key rule on S3.2 and S3.3: delivered = seeded + attempts - EEXIST - E2BIG.
   - Mutation: `takeover_image()` does not renumber rows, so a new key's row collides with a live
     one.
4. **Controls 1, 2, 5 and 6,** with `RULES-round2.md`'s configurations and 1 + 200 trials each. Each
   must fail the check round two names.
5. **Shipped build, runs a and b,** with the checks that need no outcome channel: no key in two slots;
   count = live + refused; live = min(keys, max_entries); E2BIG 0 when keys fit; no slot left
   claiming.
   - Mutation: the shipped build compiled with control 6's order.
6. **Termination:** one key per thread at max 48, so the directory fills. Every kernel ends; lookups
   give NULL and inserts -7.
   - Mutation: no S bound (killed at the deadline).
7. **Full directory with count below max:** seeded full with count < max_entries. An insert of a new
   key returns -7, unstable E2BIG rises, and the kernel ends.
   - Mutation: an unbounded full-directory wait (killed).
8. **Opaque:** opaque on K's probe path for K = 0 and K = 1000. K is inserted past them, found, and has
   one slot.
   - Mutation: key compared before state.
9. **Flags and ABI:** BPF_ANY gives -22 and bad-flags rises. -17, -7 and -22 arrive as 64-bit
   negatives in callers that declare the call as the pass does.
   - Mutations: no count; an int return.
10. **Trampoline branches** (`cuda_test_support.hpp`). A kernel hand-written in the new file calls
    helpers 1, 2 and 3 on a 1533 fd, with a 1533 `map_info` whose extra_buffer is 0 and non-zero.
    They give NULL, -1 and -22, with `served == 0`.
    - Mutation: a branch removed, so `served > 0`.
11. **Host text checks** (no GPU, in `test_devhash_layout.cpp`):
    - `devhash_ptx.h` has the written-out qualifiers at the publish, precheck, CAS, count and adds;
    - every header load in its walks is `ld.relaxed.gpu.global`;
    - its fingerprint equals `DEVHASH_LAYOUT_FP`;
    - `trampoline_ptx.h` has no sm_70-only form, and ptxas `-arch=sm_75` accepts it with its
      `.target sm_61` kept.
    - Mutations: `.release` changed to `.relaxed`; one walk load made `ld.global`; a stale
      fingerprint; a `.relaxed` load in the trampoline's 1533 branch.

### S4. Hash snapshot module (`nv_gpu_model_maps.cpp`)

**Code:**
- `bpftime_model_hash_snapshot` in its own PTX module (D14). Per slot:
  - load the header with `ld.acquire.gpu.global`;
  - if live, sum the row's shards (row < R) or read its single word;
  - stage per S1's staging layout.
- It also sums the counters and reads the count.
- Exported test entries: `gpu_model::hash_snapshot_for_test(block, geometry, staging)` loads the
  module and launches it once; `gpu_model::hash_snapshot_ptx_for_test()` returns its text.

**Tests** (child, D16):
1. **A hand-built block** with all five states, rows below and above R, known shards and counters.
   Staging is exact for live slots and raw for the others.
   - Mutations: non-live rows summed; claiming staged as empty; a row >= R read as sharded.
2. **Text:** the header load is `ld.acquire.gpu`.
   - Mutation: `ld.relaxed`.

### S5. Device copy and delivery in the agent

**Read first:** what `create_map_basic_info` does with ARRAY values (`bpf_attach_ctx_cuda.cpp:532-533`),
and that the owner's delivery thread may write a companion's values.

**Code:**
- **`prepare_device_hash_copies()` in `bpf_attach_ctx`,** called at the start of
  `init_attach_ctx_from_handlers` (D11a). Under the guard it records, pins and binds; then it calls
  `make_device_hash_copies()` without the guard.
- **`make_device_hash_copies()`,** under the process-wide mutex, for each recorded impl pointer:
  1. the owner claim and `choose_path()`;
  2. allocate and zero (relaxed capture mode, the agent's own non-blocking stream);
  3. seal and image, or the takeover image;
  4. `cuMemcpyHtoDAsync` of headers, rows, count and counters on that stream; synchronize under
     `agent_sync_scope`;
  5. first copy: write the seeded mirror;
  6. registration, with a 1533 `registered_map` kind and a result;
  7. first copy: PHASE = 2 (release);
  8. cache the copy.
  
  Any failure after the seal rolls back by D15.
- **1533 in `get_gpu_map_extra_buffer` and `get_gpu_map_max_thread_count`:** the cached base and
  shard count, or 0 before a copy.
- **`bpftimetool cuda`:** sets the direct-run flag, which skips the pre-pass.
- **Delivery:** `deliver_locked` calls the hash snapshot, then `apply_delivery()`, and writes the
  companion. The exit delivery writes the exit words. The sum schedule.
- **Test hooks:**
  - fail the copy;
  - fail registration;
  - fail the hash module's load;
  - stop the owner after the claim, during the seal wait, after the seal, and between pass B and
    pass C;
  - a destroy counter;
  - a check that `map_binding_mutex`'s thread-local depth is 0 during `make_device_hash_copies()`
    (D16).

**Tests** (each in a child, D16; `test_gpu_devhash_device.cpp`). The owner children call what init
calls, in init's order:
1. `prepare_device_hash_copies()`, which takes and drops the guard itself and makes the copies;
2. `create_map_basic_info` under the guard.

So the kill points match production.
1. **First copy:**
   - prefilled keys (inserted, updated and some deleted) sit at the image's slots on the device;
   - `create_map_basic_info`'s 1533 entry has a non-zero extra_buffer and max_thread_count equal to
     the shard count;
   - CPU updates afterwards give EPERM.
   - Mutation: 1533 missing from `get_gpu_map_max_thread_count` (the entry reads 0).
1b. **Later links keep the base:** a second `create_map_basic_info`, and a second
    `prepare_device_hash_copies()`, return the same base and register no second map (the
    registered count and the destroy counter are unchanged).
    - Mutation: the pre-pass ignores the cache.
1c. **The late path:** an `nv_attach_impl` with load substitution off, given the `map_basic_info` from
    1b, routes a loaded module's 1533 lookup inline (its inline-lookup stat > 0), and a probe's lookup
    returns non-NULL.
    - Mutation: the copies made at the end of init, as revision 3 had it, so the module is routed
      with base 0.
    - Built as (S5): the test agent registers a stub implementation of the CUDA attach types that
      records the `map_basic_info` init's own link loop hands the CUDA link (the 1533 base and
      shards, where nv_attach_impl would patch modules with them). Routing a module with a given
      base is S6's GPU test; a late-path load in the runtime tests was not built.
2. **Not the owner:** a second child gets 0 while the owner holds its claim (alive, or stopped);
   once the owner is killed or execs, the next child takes the map over.
   - Mutation: a claim that always succeeds (section 10).
3. **Owner deaths:** the owner is killed (and reaped) after the claim, during the seal wait, after the
   seal (PHASE 1 left), and between pass B and pass C.
   - The next child keeps every prefilled and every delivered live key, and PHASE ends at 2.
   - After the kill during the seal wait, the next child can create maps (`map_binding_mutex`
     intact).
   - Mutations:
     - the path chosen by the claim alone (prefill lost after a death in phase 1);
     - the copy made under `map_binding_mutex` (the next child's map creation throws).
4. **Takeover after a clean exit** that followed deliveries: the same live slots and values, and each
   live row equal to its slot-order rank (checked directly on the device block).
   - Mutation: rows not renumbered.
5. **Failures:**
   - a copy failure and a registration failure on a first copy each return 0 with PHASE 0, the
     mirror empty and the block freed; then a phase-0 delete and a successful seal leave the deleted
     key absent;
   - a registration failure on a takeover keeps PHASE 2 and unchanged lookups;
   - after each failure, OWNER is 0 and a second child becomes the owner.
   - Mutations: PHASE 0 on the takeover failure; OWNER not released.
6. **End to end:**
   - a kernel inserts N keys with NOEXIST and adds; synchronize; `note_app_sync`;
   - wait by D16's rule (SYNCS_COVERED >= n, then one more DELIVERIES increment);
   - the mirror and the companion hold exact sums minus the counted EEXIST and E2BIG.
   - The AT_SYNC child checks right after `note_app_sync`.
   - Mutation: the delivery does not store SYNCS_COVERED.
7. **Prefill under GPU inserts:** CPU prefill, copy, then GPU NOEXIST inserts and adds on the
   prefilled keys, then delivery. One slot per key; delivered = seeded + adds - counted EEXIST.
   - Mutation: count not copied.
8. **Refused-slot fill:** distinct keys past max_entries. The companion's E2BIG equals the -7
   results; refused slots are in word 15.
   - Mutation: word 15 not written.
9. **Close while delivering:** the tool closes its fds. The refcounts of the 1533 object and the
   companion stay above 0, and the destroy counter reads 0.
   - Mutation: 1533 left out of the pin list.
10. **Companion:**
    - created before or after the hash map, it binds;
    - two candidates: none binds;
    - two hash maps sharing a 12-byte prefix: none binds;
    - a 15-character name binds to its 12-byte prefix plus `_st`;
    - words are readable with `bpf_map_lookup_elem`;
    - a non-owner child's binding does not change the link;
    - the companion is closed and its slot reused by another map before a takeover: the new owner
      does not bind it.
    - Mutations: uniqueness checked one way only; the slot not checked at resolve.
11. **Isolation:** the hash module fails to load (hook), and a 1531 map in the same child keeps being
    delivered.
    - Mutation: one broken flag.

### S6. Routing pass (`nv_thread_local_routing.{hpp,cpp}`, `nv_attach_impl.cpp`)

**Code:**
- `hash_map_desc` (fd, base, max_entries, shards), a new defaulted parameter.
- Stats: inline lookups, rewritten inserts, rewritten deletes.
- 1533 in the gate at `nv_attach_impl.cpp:1042`.
- **Placement:** the devhash text at the start of the module body, once, only when a 1533 call is
  rewritten (D1).
- **Constant-fd lookup, base != 0: the inline hit path.**
  - key (forwarded as for 1531), home, one `ld.relaxed.gpu.global.b64`;
  - live K: the sharded address below R, or the single word;
  - empty: NULL, no call;
  - otherwise: a call to `__bpftime_devhash_lookup` into a block-local register.
  - One `mov` at the join writes the program's register. Labels carry the per-function sequence
    number.
- **Constant-fd update, base != 0:** a call to `__bpftime_devhash_insert`.
- **Constant-fd delete:** -22 inline.
- **Base 0 or a non-constant fd:** the trampoline call is kept.
- A malformed block throws, as for 1531.

**Tests, text** (`attach/nv_attach_impl/test/test_hash_map_routing.cpp`, `[hash-map-routing]`, in
`bpftime_nv_attach_tests`):
- **Mirrors of `test_sum_map_routing.cpp`:**
  - :76;
  - :135 (base 0 keeps the call);
  - :171/:242;
  - :185;
  - :223;
  - :289;
  - :514 (rewritten here, kept at base 0);
  - :642;
  - key forwarding.
  - (no mutation: the same assertions as the 1531 originals.)
- **One definition:** the program register has one definition and its adds are `.global`.
  - Mutation: the fallback writes the program register.
- **Labels:** two inline lookups get distinct labels.
  - Mutation: no sequence number.
- **Placement:** the text is inserted once, before every function, and not when unused.
  - Mutations: inserted per call site; inserted when no 1533 call is rewritten.
- **By-value** is kept beside lat's 1530 lookups.
  - Mutation: the inserted call counted as an address use.
- **The gate,** through `route_thread_local_maps_of_module` on an `nv_attach_impl` built as
  `test_ptx_failure.cpp` does: a map_basic_info whose only GPU-model map is 1533 is routed.
  - Mutation: 1533 not in the gate.
- **ptxas at sm_87** on a routed module with an inline lookup that falls back and a rewritten update.
  - Mutation: the text appended after the probes.
- **The emitted inline header load** is `ld.relaxed.gpu.global`.
  - Mutation: `ld.global`.

**Tests, GPU** (`test_gpu_devhash_device.cpp`, child; the runtime test binary links
`bpftime_nv_attach_impl` and calls the emitter):
- The routed output, wrapped with the trampoline, is loaded as a whole module. It is not combined
  with S3's module, which already holds the text.
- It runs over a seeded directory:
  - live at home, row < R and row >= R;
  - claiming, refused and opaque at home;
  - empty;
  - K = 0.
- Each thread's pointer equals `__bpftime_devhash_lookup`'s. The emitted hash equals `home_slot` for
  10^4 keys. The rewritten update returns -17, -7 and -22 as 64-bit negatives.
- Mutations: no state check; row >= R addressed as sharded; row bits shifted by one; the result
  zero-extended.

### S7. Tools and documentation

**Tools:**
- `pt_common.h` gets `MT_DEV_HASH 1533`.
- `pt_hash.bpf.c` gets a type switch for CNT, PRE and MISS (as `pt_lat`'s PT_OLD/PT_MIX) and
  declares the companion.
- `pt_loader` dumps 1533 by key with get_next_key, and the companion as an array.

**`runtime/src/bpf_map/gpu/README.md`** (new, format of `attach/trap_uprobe_attach_impl/README.md`),
for tool authors:
1. Declaring the map and the companion: the name rule; two maps.
2. Return codes -17, -7, -22, -1; only 1533 gives GPU programs errno values.
3. First touch gives EEXIST (counted); unstable outcomes; what each counter means. Word 13 is
   reserved capacity after a takeover.
4. The E2BIG/lookup window, as bpf's htab has it (decision 5). Visibility within one launch rests on
   the hardware.
5. The refused-slot limit and an undersized map.
6. Constant fds only. A map without a copy when the process initialized its attach (another owner,
   a failed copy, a map created during that init), and non-constant fds, never reach the device map
   from modules patched then (unlike 1531, which the trampoline serves). `bpftimetool cuda` never
   uses the device map.
7. Contracts: adds only, 64-bit; no CPU writes through lookup pointers.
8. Prefill before the first agent init that instantiates a CUDA link (the pre-pass); the CPU phases and EPERM; a dump that crosses the copy
   may skip keys.
9. Takeover: same slots, renumbered rows. Keys and adds after the last delivery are lost.
10. A writer of a running process that stays inside an update for more than 1 s during a seal
    delays the device copy (`SEAL_DEAD`) until it has finished. A writer whose process died, was
    killed or exec'd (into an image without the segment) is discounted (the kernel releases its
    slot's lock). Not discounted, so the copy stays disabled until the writer's process exits or
    the segment is recreated, if one dies in flight: a writer in the overflow slot (more than 256
    writing processes at once, a process without the segment file, or one whose locks the system
    refuses), a vfork-style child killed in flight while its parent lives, the threads an exec
    killed when the new image maps the segment again (unless it takes the same slot, as it usually
    does: the scan starts at its pid), and a holder whose lock a host released while it runs. A
    writer that died inside the table also leaves its stripe locked (as for HASH). Each process
    keeps one descriptor of the segment's file per 1533 map it writes or owns, until it exits or
    destroys the map.
11. An owner that died, was killed or exec'd is replaced at the next agent init that needs the map,
    in any pid namespace. An owner whose descriptor a host closed keeps its claim: no process of its
    pid namespace takes over while it runs and maps the segment, and it takes its lock back at its
    next delivery check. An owner of another pid namespace whose descriptor a host closed can be
    taken over while it runs, and both deliver until its next check. Children made without `fork`
    (`clone`, `clone3`) keep their parent's descriptors: a dead owner's claim and a dead writer's
    slot stay held while such a child runs. The claim stays with the process until it exits or
    execs, even after the agent detaches.
12. A malformed call block leaves the whole module unprobed.
13. Delivery copies words one at a time: a `bpf()` lookup can copy a value while the next delivery
    writes it, per word as 1531. Companion words are independent counters.
14. A device fault in the hash snapshot stops the context.

**CLAUDE.md** gets a short section:
- the trampoline stays sm_61-legal;
- the layout header is the only definition;
- the regeneration commands;
- 1533 copies are made in the pre-pass, never under `map_binding_mutex`, and never after a link
  exists;
- the D16 test conventions.

**Tests:** (no mutation: documentation.) S8 exercises the tools.

### S8. Measurement and correctness against today (rules first, `S8-RULES.md`)

(no mutation: measurement.)

**Set-up:** jetson_clocks checked (cpu5 1728000, GPU 1020 MHz), the GPU lock, bounded runs, and
containers and shm cleaned.

**pt_victim, the hash-current matrix** (D and G, CNT/PRE/MISS, three passes), with 1533 against the
host path:
- counts: exact for PRE; for CNT, delivered = expected - EEXIST - E2BIG, from the companion;
  host-path outcomes counted as non-zero;
- per operation: by slope.

**llama.cpp:**
- **Cost on top of 1531:** tg128, mul_mat_vec_q, every thread, constant key.
  - Expected: S0.2's timing of the routed form. C2's `hh` number (0.034-0.037 ns) was an inline form
    with no call.
  - Rule: more than 0.02 ns above S0.2's number in both passes, compare the SASS with S0.2's
    before reporting.
- **Against today's host path:** rms_norm pp32, per thread and per block.
- **The per-launch counter** on the four tg128 kernels.

**Delivery cost:** the hash snapshot's time per delivery and per run (ns_total/ns_max lines) at
max_entries 512 and 1024 with all rows live.
- Rule: above 2 ms per delivery (a tenth of the busy period), report it as a cost of the schedule.

**Re-checks on the final build:**
- S0.2;
- S2.12;
- S3.11;
- every unit test of S1-S6.

## 4. Code-map risks and where each is handled

| risk (code map) | handled in | test |
|---|---|---|
| walks need an S bound | D4, S3 | S3.6, S2.8 |
| refused semantics | S3 (C5 port) | S3.1, S3.4 |
| loss counters hot spot | D5 | S3.1, S8 |
| layout copies disagree | S1, D1 fingerprint | S1.4, S3.11, S6 GPU |
| no device copy must not fall through | D1 | S3.10, S6 :135 |
| new PTX forms, registers, occupancy | S0.2 | S0.2, S8 |
| return codes | D6 | S3.9, S6 GPU |
| result register defined twice | S6 | S6 text |
| malformed block throws | as 1531 | S6 :642, README 12 |
| no 1533 in the pass gate | S6 | S6 gate |
| `.version` of original modules | S0.2 | S0.2 |
| mixed-size or non-add uses | D13 | README 7 (no test: contract) |
| MapBasicInfo has one pointer | D3, D11a | S5.1 |
| CPU-writable phase, then seal | D7, D8, D9 | S2.3-S2.6, S2.11, S2.12 |
| CPU walk misses slot-by-slot deliveries | D10 | S2.7 |
| header before value in the mirror | D10 | S2.7 |
| owner takeover drops keys | D11 | S2.9-S2.11, S5.3, S5.4 |
| copy made at the first CUDA link; prefill first | D11a, README 8 | S5.1 |
| companion naming and reachability | D12 | S5.10 |
| pin list | D12, D16 | S5.9 |
| shared broken flag | D14 | S5.11 |
| failures after the seal | D15 | S5.5 |
| second PTX entry compiled at first launch | D14 | S8 delivery cost |
| snapshot size | | S8 delivery cost |
| `bpf()` copy races the next delivery | as 1531 | README 13 (no test) |
| trampoline sm_61 target | D1 | S3.11 |
| direct run makes its own context | out of scope for the device path: `bpftimetool cuda` skips the pre-pass, so it neither claims nor seals (D11a) | README 6 (no test) |
| without CUDA attach, 1533 is EINVAL | as 1531 | (no test) |

## 5. Not in stage one

- the store layout;
- GPU deletes;
- other flags;
- multi-word values;
- per-thread keys (1530);
- a fused lookup-or-insert;
- non-constant fds on the device path;
- moving 1531's copy out of `map_binding_mutex`;
- a pid-namespace owner rule for 1531.

## 6. To change in DESIGN.md when the build lands

- **CPU view:** the CPU walks the delivered mirror, kept walkable by repair (D10).
- **Adds through the returned pointer:** they keep 1531's `.global` form and rely on the defaults
  (D13).
- **Takeover:** same slots, renumbered rows, opaque, and the count rule (D11e).
- **Return codes:** as D6; constant fds only (D1).
- **Seal:** `SEAL_DEAD` (D8).

## 7. Round-two findings and where each is answered (historical)

Ids are from `review-r2/plan-review.json`. This is how revision 3 answered them. Revision 4 changed
D11a (copies in a pre-pass) and D12 (binding in the pre-pass) again: section 8.

- **new-1 (blocker):** D11a, D11b, D8 (no seal under the mutex), S5 code and S5.3 (kill during the
  seal wait; the "under the mutex" mutation).
- **The count rule:** ans-coverage-16 and new-5 → D11e and S2.9.
- **The mirror test:**
  - ans-coverage-10 → S2.7 keyed-slot defensive stagings;
  - ans-coverage-09 and bld-4 → S2.7 parked windows and S2 hooks;
  - ans-buildability-10 → S2.7 live-K gap stagings.
- **The seal race:** ans-coverage-13, new-3 and bld-7 → S2.3 release rule.
- **Delivery tests:** ans-buildability-6, new-4 and bld-2 → D16 delivery rule, S5.6.
- **Disassembly checks:**
  - ans-cpu-concurrency-1 → S2.12 (RCpc target);
  - ans-cpu-concurrency-9 and new-11 → S2.12 (PHASE loads), D7.
- **Which text and which harness:**
  - ans-coverage-07 → S3.5;
  - ans-buildability-11 and bld-9 → S3 test build keeps the signatures, S3 text per case, S6 GPU
    whole module;
  - bld-12 → S3 `devhash_test_ptx.h` with the harness kernels;
  - ans-coverage-04, ans-buildability-3 and bld-3 → `cuda_test_support.hpp` (D16, S3).
- **Order:** ans-coverage-21, ans-buildability-4 and bld-8 → section 2.
- **Mutations:** ans-coverage-29 and bld-10 → each test now has one or a reason.
- **S8's expectation:** ans-device-and-routing-02 → S0.2 timing, S8 rule.
- **Snapshot entry:** ans-buildability-2 and bld-1 → S4 test entries.
- **Failure state:**
  - ans-buildability-5, bld-5 and new-8 → `SEAL_DEAD` in the map object (D8);
  - new-7 → D11e PHASE-1 timeout, S2.11.
- **Rollback:** ans-cpu-concurrency-5 → D15 (any failure after the seal), S5.5.
- **The PHASE-1 path:** ans-cpu-concurrency-2 → S2.11.
- **Fingerprint:** ans-buildability-20 → S1 (offsets at sample geometries), S1.4.
- **Companion link:** ans-buildability-16 and bld-16 → D12 (handler index; mutex held by the caller;
  tests take it).
- **Table scan:** new-2 → D9 (all elements), S2.6.
- **Owner liveness:** new-6 → D11c, D11d, README 11, D16 reaping.
- **Strong loads:** new-10 → D4, D10 (8.7, 8.9.5), S3.11, S6 text.
- **S2.5:** bld-6 → the stripe of a prefilled key, forked child.
- **Hooks:** bld-11 → always-compiled `*_for_test` (D16).
- **S2 code:** bld-13 → forwarder, class files and CMake list; bld-14 → staging layout in S1,
  `choose_path` input.
- **Sync scope:** bld-17 → D15, S5 step 4.
- **Makefile:** bld-18 → D1 Makefile, S1 `DEVHASH_HD`.
- **Refuted, no change:** ans-device-and-routing-09, new-9, new-12, bld-15.

## 8. Round-three findings and where each is answered

Ids are from `review-r3/plan-review.json`.

- **The late path and later links:** r3-1 (blocker), ans3-new-1 and r3-2 → D11a pre-pass (copies
  before any CUDA link, cached base on every later link), S5 1b and 1c.
- **Concurrent readers of `map_basic_info`:** r3-3 → D11a (it is never changed after a link exists).
- **S5 children:** r3-4 → S5 preamble (init's order), S5.1 (reads `create_map_basic_info`'s entry).
- **Writer-side disassembly:** r3-5 and ans3-ans-cpu-concurrency-1 → D8, S2.12.
- **`SEAL_DEAD` recovery and wording:** r3-6 and ans3-new-8 → D8 (spin-poll, last poll, recovery when
  INFLIGHT reads 0), S2.4 stopped writer, README 10.
- **Count rule:** r3-7 → D11e saturating rule, S2.9.
- **Claims and OWNER:** r3-8 → D11a process-wide mutex, D11d, D15 OWNER release, S5.5.
- **Handler indices:** r3-9 → D11a pinned pointers, D12 resolve check, S5.10.
- **The delivery wait:** r3-10 → D16 (n from `app_sync_count()`, one more DELIVERIES increment).
- **Pass C order:** r3-11 and ans3-ans-coverage-09 → D10 ascending pass C, S2 hook on a named slot,
  S2.7.
- **Copy stream:** r3-12 → D15, S5 code.
- **Mutex depth:** r3-13 → D16 thread-local depth.
- **Direct run:** r3-14 → D11a flag, S5 code, section 4, README 6.
- **Guard before repair:** ans3-ans-coverage-10 → D10 step 1, S2.7 mutation.
- **PHASE-1 release rule:** ans3-ans-cpu-concurrency-2 → S2.11.
- **Fingerprint test:** ans3-ans-buildability-20 → S1, S1.4.
- **Row gap:** ans3-bld-10 → S2.9, S3.3, S5.4.
- **S3.10 kernel:** ans3-ans-coverage-04 → S3.10, D16 (`scratch_segment`).

- **After round 3, a narrow check of the pre-pass against the code** (not a review round):
  - the CUDA context exists from the `bpf_attach_ctx` constructor, so the first init's pre-pass
    sees it;
  - the pre-pass is gated on the init instantiating a CUDA link (D11a). Otherwise any agent with a
    GPU would claim and seal the map at its first init, before a tool's prefill.

## 9. Changes from the S2 code review (11-27, after S1 and S2 were built)

A three-lens review of the S1-S2 code (22 findings, 21 held; `s2/review/code-review.json`)
changed these decisions:

- **D8, writer slots.** INFLIGHT is one count per writing process (64 slots: owner record, start
  time, count; one shared overflow slot that is never discounted). The review measured that an
  ordinary exit of a multi-threaded writer process leaks a single shared counter in 2-23 of 40 exits
  (the exit kills the other threads wherever they are), after which no seal could ever succeed. The
  sealer now discounts the slot of a process that is gone (same pid namespace only) and seals; only
  a live writer stuck for 1 s sets SEAL_DEAD. The sealer's later polls are seq_cst loads in a
  function of their own (a poll that reads 0 must acquire the writers' decrements).
- **D11c, liveness.** `/proc/<pid>/stat` shows the thread-group leader only: a process whose main
  thread exited while others run is alive. A process is gone when kill(pid, 0) fails with ESRCH,
  when every task in `/proc/<pid>/task` is Z or X, or when its start time differs from the one
  recorded (the pid was reused). Writer slots match on the start time too.
- **D11d/D15, the claim.** Every path that ends without a copy gives the claim back, a seal
  timeout included (`make_image`). Otherwise a live sealer kept every other process out.
- **D10, repair cost.** Repair walks only keys the mirror does not hold at their slot yet (a key
  already there had its path made non-empty when it came, and mirror slots never go back to
  empty); the delivery's scratch is kept per thread. Re-walking every key cost seconds per delivery
  on a full directory at large max_entries.
- **D7.** A null next_key is EINVAL in both phases (phase 2 wrote through it).
- **Build.** The 1533 creation check is under BPFTIME_ENABLE_CUDA_ATTACH: the default build did not
  compile.


## 10. Changes from the independent read of the S2 fixes (11-27, after S3 was built)

A two-lens read of c84dd0ff and 0b9b02af (writer slots and the seal; owner and liveness), each
finding checked by a refuter (19 findings, all upheld; `s2/review/reread-fixes.json`). They fall in
one class: liveness from (pid, start time from /proc) cannot be read together with the slot or the
claim it belongs to (a CAS, then a store), and cannot see exec at all. The fix of OWNER's window in
0b9b02af had left the same window in the writer slots (slots-1, owner-3: a live writer's count
discounted, the seal true while it writes; reproduced). So D8 and D11b-d rest on the kernel: a
writer slot and the owner claim are OFD locks on bytes of the segment file, which the kernel
releases at exit, kill and exec, in any pid namespace (`s2/ofd/RULES-ofd.md`, all items passed).
Pid liveness came back after the next review (section 11) only as a second guard: a free lock whose
holder still runs and maps the segment counts as held. It matters only after a host has released
a live holder's lock; otherwise the lock alone decides.

| finding | answer |
|---|---|
| slots-1 = owner-3: CAS, then start time; a second claimant steals a live slot; count wraps | gone: a slot is held by its lock; the sealer clears a count only while it holds the slot's lock |
| slots-2 = owner-1, owner-8: exec keeps pid and start time | exec releases the lock (`O_CLOEXEC`); test S2.14 exec |
| slots-3 = owner-7: sealer killed between marking and clearing | the kernel releases the sealer's lock; the next seal clears the count |
| slots-4 = owner-2: dead in-flight slots not reusable, foreign namespaces never freed, overflow reached | a free lock is reusable at once, leftover count cleared; no namespaces; 256 slots |
| slots-5: 64 liveness probes per update when slots are exhausted | the overflow decision is cached per process for 100 ms; test S2.14 |
| slots-6 = owner-11: no test reaches slot claims, overflow, start time | S2.14 |
| slots-7 = owner-6: own-pid predecessor never discounted | gone: the sealer probes through its own description, no "self" case |
| slots-8: start-time cache shared with vfork children | gone: no start times; a vfork-style child records nothing in the parent's table and writes in the parent's slot or the overflow slot (section 11) |
| owner-4: same-record claim without CAS | gone: the claim is the lock |
| owner-5: /proc and time namespaces | the lock decides; /proc is read only for the hint guard (section 11), and another namespace's hint counts as gone |
| owner-9: exception after the seal leaves PHASE 1 and the claim | caught in `make_image`: unseal, unlock; test S2.14 |
| owner-10: release_claim's start-time order | gone |

Kept from the old design: the seal's orders (D8, S2.12), `SEAL_DEAD`, the overflow slot. A host
closing the agent's descriptors was first left on the dead side; section 11 moved it to the alive
side with the holder hints (README 10, 11).

## 11. Changes from the review of the lock rework (11-28, after S4 and S6 were built)

A four-lens review of 84ce9813d (slots, claim, tests, host safety), each finding checked by a refuter
(38 findings, 37 upheld; `s2/review/review-ofd-rework.json`). Answered in 3b6842fc8:

| finding | answer |
|---|---|
| a host closing the agent's description: writes went on in a slot whose lock was free, for good; `still_holds_claim()` matched the file only, so the number reused by another description of the file passed (claim-1, claim-4, slots-3, host-2, host-4, tests-1) | holder hints (D8, D11c): a free lock whose holder still runs and maps the segment is held; the description must hold its lock (`/proc/self/fdinfo`), and the process takes its slot and claim back |
| a description at fd 0-2 took a host's output into the segment (claim-2, slots-4, host-5) | descriptions at 3 or above |
| the fork handler, the destructor and a new map at an old address closed numbers that might be the host's (claim-3, slots-2, host-3) | closed only while the number still names a description of the segment's file |
| a vfork-style child recorded its own descriptions in the parent's table (slots-1, host-1) | the table's pid, set at load and in fork children: a vfork child records nothing |
| the fork handler's close went through the syscall server's interposed close; writers waited on local_mutex inside probes, signal handlers and fork children (host-6, host-11) | raw system calls; writers only try the mutex (overflow slot otherwise) |
| a wrapped count could cancel another slot in the seal's sum (slots-1) | the seal checks each slot for zero |
| the seal's probe description kept in fork children (claim-5) | closed by the fork handler |
| exec into an image that maps the segment again: its earlier image's leftover count (the mutation "own slot cleared by its own rescan" survived) | an image nonce beside the hint; test "An exec'd image of a writer clears what its earlier image left" |
| logs and rescans every 100 ms for good (claim-7, slots-6, host-7) | logged once per map; no file or refused locks retry after 10 s |
| locks refused by the system (seccomp) read as held (host-8) | a refused lock stops the scan (overflow slot); a filter that kills on these calls kills the host at its first write (README) |
| descriptions of maps destroyed by other processes are kept; at most 1023 maps per process (claim-9) | stated in README 10; the claim's warning names the cause |
| children made without `fork` keep the descriptions (claim-6, host-10) | stated in README 10/11 (alive side) |
| tests: overflow writer in flight, scan past a live holder, new map at an old address, SEAL_DEAD then death, the closed-number check, flaky claims, unchecked forks, pipe deadlines, periodic discount, destructor (tests-2..10) | each has a case now (20 cases); the mutation script runs a baseline first (tests-11) |
| plan and code disagreed on the lock bytes and the file fallback (claim-8, slots-5, host-9) | D8 rewritten |
| (found after the review, by an advisor's read) a holder whose main thread exited shows empty `/proc/<pid>/maps`, so the hint guard took it for gone; a vfork child took the mutex before its pid was checked; a fork from inside these paths waited for its own mutex | the threads' maps decide when the pid's are empty (test: a holder whose main thread exited); the pid is checked before the mutex; the fork handler does not wait for a mutex its own thread holds |

Refuted: claim-10 (the claim is not given back when the agent detaches; README 11 now says the
claim stays with the process until it exits or execs).

## 12. Changes found while building S5 and S7 (11-28)

| found | change | where |
|---|---|---|
| the kernel merges one description's adjacent locks of one type: writer slot 255 and the claim's byte 256 show as one fdinfo line from 255; the claim check compared the line's start, so such an owner (1 in 256 of those that write before they claim) took its claim for lost and stopped delivering | the byte must lie in a line's range (end parsed, EOF included); a number naming another file is checked first; fdinfo unreadable with the segment's file at that number counts as held | D11c, `description_holds`; `s2/ofd/RULES-ofd.md` (found later); CPU test "The claim stays held beside this process's writer slot 255" |
| a non-owner's pre-pass stored the companion's index before it knew it would own the map: with two candidates (none bound) and one closed later, another agent bound the one left | only the process whose copy is made stores (or clears) the index | D12 |
| exceptions after the image (the mirror's seeding, registration's thread, the cache) escaped with the map sealed and the claim held | one catch-all from the image to registration rolls back (D15); the cache's room reserved before PHASE 2; registration removes its entry when it throws; nothing leaves init | D15, S5 code |
| "it handles nv attach" was in the gate's text but not in the code | the gate needs an attach implementation registered for the CUDA attach types (8, 9) | D11a |
| test 1c (late path) needs nv_attach_impl in the runtime tests | a stub CUDA attach in the test agents records what init's link loop gives a CUDA link | S5 test 1c |
| S7's first run of a real tool and victim: ptxas refused the patched module ("requires call prototype"), which ran unprobed; the passes put the probe functions ahead of the module's own header, so the devhash text placed after `.address_size` came after its callers (S6's tests routed modules whose probes followed the header) | the text goes before the module's first function; tests mirror the attach's assembly (trampoline, filtered headers, device target) | D1, S6 code; `s7/S7-result.md` |
| the deliveries hold the 1533 object in the segment: a process that unmaps its segment while a device map is registered (a test switching segments) crashes the delivery thread; 1531 maps have the same exposure; the agent unmaps only at exit, after the exit delivery | not handled (as 1531); tests keep one segment per delivering process | README limits |
